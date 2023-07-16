/*
 * Copyright (C) 2015-2023  Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Vratislav Podzimek <vpodzime@redhat.com>
 *         Vojtech Trefny <vtrefny@redhat.com>
 */

#include <stdio.h>
#include <blockdev/utils.h>

#include "libzram.h"


static volatile gboolean have_zram = FALSE;
G_LOCK_DEFINE_STATIC (have_zram);

/**
 * zram_error_quark: (skip)
 */
GQuark zram_error_quark (void)
{
    return g_quark_from_static_string ("g-zram-error-quark");
}

/**
 * zram_stats_copy: (skip)
 *
 * Creates a new copy of @data.
 */
ZramStats* zram_stats_copy (ZramStats *data) {
    if (data == NULL)
        return NULL;

    ZramStats *new = g_new0 (ZramStats, 1);
    new->disksize = data->disksize;
    new->num_reads = data->num_reads;
    new->num_writes = data->num_writes;
    new->invalid_io = data->invalid_io;
    new->zero_pages = data->zero_pages;
    new->max_comp_streams = data->max_comp_streams;
    new->comp_algorithm = g_strdup (data->comp_algorithm);
    new->orig_data_size = data->orig_data_size;
    new->compr_data_size = data->compr_data_size;
    new->mem_used_total = data->mem_used_total;

    return new;
}

/**
 * zram_stats_free: (skip)
 *
 * Frees @data.
 */
void zram_stats_free (ZramStats *data) {
    if (data == NULL)
        return;

    g_free (data->comp_algorithm);
    g_free (data);
}

G_DEFINE_BOXED_TYPE (ZramStats, zram_stats, zram_stats_copy, zram_stats_free);

static gboolean check_deps (GError **error) {
    gboolean ret = FALSE;

    G_LOCK (have_zram);

    if (have_zram) {
        G_UNLOCK (have_zram);
        return TRUE;
    }

    ret = bd_utils_have_kernel_module ("zram", error);
    have_zram = ret;
    G_UNLOCK (have_zram);
    return have_zram;
}

/**
 * zram_create_devices:
 * @num_devices: number of devices to create
 * @sizes: (array zero-terminated=1): requested sizes (in bytes) for created zRAM
 *                                    devices
 * @nstreams: (allow-none) (array zero-terminated=1): numbers of streams for created
 *                                                    zRAM devices
 * @error: (out): place to store error (if any)
 *
 * Returns: whether @num_devices zRAM devices were successfully created or not
 *
 * **Lengths of @size and @nstreams (if given) have to be >= @num_devices!**
 */
gboolean zram_create_devices (guint64 num_devices, const guint64 *sizes, const guint64 *nstreams, GError **error) {
    g_autofree gchar *opts = NULL;
    gboolean success = FALSE;
    guint64 i = 0;
    g_autofree gchar *num_str = NULL;
    g_autofree gchar *file_name = NULL;

    if (!check_deps (error))
        return FALSE;

    opts = g_strdup_printf ("num_devices=%"G_GUINT64_FORMAT, num_devices);
    success = bd_utils_load_kernel_module ("zram", opts, error);

    /* maybe it's loaded? Try to unload it first */
    if (!success && g_error_matches (*error, BD_UTILS_MODULE_ERROR, BD_UTILS_MODULE_ERROR_FAIL)) {
        g_clear_error (error);
        success = bd_utils_unload_kernel_module ("zram", error);
        if (!success) {
            g_prefix_error (error, "zram module already loaded: ");
            return FALSE;
        }
        success = bd_utils_load_kernel_module ("zram", opts, error);
        if (!success)
            return FALSE;
    }

    if (!success)
        /* error is already populated */
        return FALSE;

    /* compression streams have to be specified before the device is activated
       by setting its size */
    if (nstreams)
        for (i=0; i < num_devices; i++) {
            file_name = g_strdup_printf ("/sys/block/zram%"G_GUINT64_FORMAT"/max_comp_streams", i);
            num_str = g_strdup_printf ("%"G_GUINT64_FORMAT, nstreams[i]);
            success = bd_utils_echo_str_to_file (num_str, file_name, error);
            if (!success) {
                g_prefix_error (error, "Failed to set number of compression streams for '/dev/zram%"G_GUINT64_FORMAT"': ",
                                i);
                return FALSE;
            }
        }

    /* now activate the devices by setting their sizes */
    for (i=0; i < num_devices; i++) {
        file_name = g_strdup_printf ("/sys/block/zram%"G_GUINT64_FORMAT"/disksize", i);
        num_str = g_strdup_printf ("%"G_GUINT64_FORMAT, sizes[i]);
        success = bd_utils_echo_str_to_file (num_str, file_name, error);
        if (!success) {
            g_prefix_error (error, "Failed to set size for '/dev/zram%"G_GUINT64_FORMAT"': ", i);
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * zram_destroy_devices:
 * @error: (out): place to store error (if any)
 *
 * Returns: whether zRAM devices were successfully destroyed or not
 *
 * The only way how to destroy zRAM device right now is to unload the 'zram'
 * module and thus destroy all of them. That's why this function doesn't allow
 * specification of which devices should be destroyed.
 */
gboolean zram_destroy_devices (GError **error) {
    if (!check_deps (error))
        return FALSE;

    return bd_utils_unload_kernel_module ("zram", error);
}

static guint64 get_number_from_file (const gchar *path, GError **error) {
    g_autofree gchar *content = NULL;
    gboolean success = FALSE;
    guint64 ret = 0;

    success = g_file_get_contents (path, &content, NULL, error);
    if (!success) {
        /* error is already populated */
        return 0;
    }

    ret = g_ascii_strtoull (content, NULL, 0);

    return ret;
}

/**
 * zram_add_device:
 * @size: size of the zRAM device to add
 * @nstreams: number of streams to use for the new device (or 0 to use the defaults)
 * @device: (allow-none) (out): place to store the name of the newly added device
 * @error: (out): place to store error (if any)
 *
 * Returns: whether a new zRAM device was added or not
 */
gboolean zram_add_device (guint64 size, guint64 nstreams, gchar **device, GError **error) {
    g_autofree gchar *path = NULL;
    gboolean success = FALSE;
    guint64 dev_num = 0;
    g_autofree gchar *num_str = NULL;

    if (!check_deps (error))
        return FALSE;

    if (access ("/sys/class/zram-control/hot_add", R_OK) != 0) {
        success = bd_utils_load_kernel_module ("zram", NULL, error);
        if (!success) {
            g_prefix_error (error, "Failed to load the zram kernel module: ");
            return FALSE;
        }
    }

    dev_num = get_number_from_file ("/sys/class/zram-control/hot_add", error);
    if (*error) {
        g_prefix_error (error, "Failed to add new zRAM device: ");
        return FALSE;
    }

    if (nstreams > 0) {
        path = g_strdup_printf ("/sys/block/zram%"G_GUINT64_FORMAT"/max_comp_streams", dev_num);
        num_str = g_strdup_printf ("%"G_GUINT64_FORMAT, nstreams);
        success = bd_utils_echo_str_to_file (num_str, path, error);
        if (!success) {
            g_prefix_error (error, "Failed to set number of compression streams: ");
            return FALSE;
        }
    }

    path = g_strdup_printf ("/sys/block/zram%"G_GUINT64_FORMAT"/disksize", dev_num);
    num_str = g_strdup_printf ("%"G_GUINT64_FORMAT, size);
    success = bd_utils_echo_str_to_file (num_str, path, error);
    if (!success) {
        g_prefix_error (error, "Failed to set device size: ");
        return FALSE;
    }

    if (device)
        *device = g_strdup_printf ("/dev/zram%"G_GUINT64_FORMAT, dev_num);

    return TRUE;
}

/**
 * zram_remove_device:
 * @device: zRAM device to remove
 * @error: (out): place to store error (if any)
 *
 * Returns: whether the @device was successfully removed or not
 */
gboolean zram_remove_device (const gchar *device, GError **error) {
    gchar *dev_num_str = NULL;
    gboolean success = FALSE;

    if (!check_deps (error))
        return FALSE;

    if (g_str_has_prefix (device, "/dev/zram"))
        dev_num_str = (gchar *) device + 9;
    else if (g_str_has_prefix (device, "zram"))
        dev_num_str = (gchar *) device + 4;
    else {
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Invalid zRAM device given: '%s'", device);
        return FALSE;
    }

    success = bd_utils_echo_str_to_file (dev_num_str, "/sys/class/zram-control/hot_remove", error);
    if (!success)
        g_prefix_error (error, "Failed to remove device '%s': ", device);

    return success;
}

/* Get the zRAM stats using the "old" sysfs files --  /sys/block/zram<id>/num_reads,
   /sys/block/zram<id>/invalid_io etc. */
static gboolean get_zram_stats_old (const gchar *device, ZramStats* stats, GError **error) {
    gchar *path = NULL;

    path = g_strdup_printf ("/sys/block/%s/num_reads", device);
    stats->num_reads = get_number_from_file (path, error);
    g_free (path);
    if (*error) {
        g_clear_error (error);
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'num_reads' for '%s' zRAM device", device);
        return FALSE;
    }

    path = g_strdup_printf ("/sys/block/%s/num_writes", device);
    stats->num_writes = get_number_from_file (path, error);
    g_free (path);
    if (*error) {
        g_clear_error (error);
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'num_writes' for '%s' zRAM device", device);
        return FALSE;
    }

    path = g_strdup_printf ("/sys/block/%s/invalid_io", device);
    stats->invalid_io = get_number_from_file (path, error);
    g_free (path);
    if (*error) {
        g_clear_error (error);
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'invalid_io' for '%s' zRAM device", device);
        return FALSE;
    }

    path = g_strdup_printf ("/sys/block/%s/zero_pages", device);
    stats->zero_pages = get_number_from_file (path, error);
    g_free (path);
    if (*error) {
        g_clear_error (error);
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'zero_pages' for '%s' zRAM device", device);
        return FALSE;
    }

    path = g_strdup_printf ("/sys/block/%s/orig_data_size", device);
    stats->orig_data_size = get_number_from_file (path, error);
    g_free (path);
    if (*error) {
        g_clear_error (error);
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'orig_data_size' for '%s' zRAM device", device);
        return FALSE;
    }

    path = g_strdup_printf ("/sys/block/%s/compr_data_size", device);
    stats->compr_data_size = get_number_from_file (path, error);
    g_free (path);
    if (*error) {
        g_clear_error (error);
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'compr_data_size' for '%s' zRAM device", device);
        return FALSE;
    }

    path = g_strdup_printf ("/sys/block/%s/mem_used_total", device);
    stats->mem_used_total = get_number_from_file (path, error);
    g_free (path);
    if (*error) {
        g_clear_error (error);
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'mem_used_total' for '%s' zRAM device", device);
        return FALSE;
    }

    return TRUE;
}

/* Get the zRAM stats using the "new" sysfs files -- /sys/block/zram<id>/stat,
  /sys/block/zram<id>/io_stat etc. */
static gboolean get_zram_stats_new (const gchar *device, ZramStats* stats, GError **error) {
    gchar *path = NULL;
    gboolean success = FALSE;
    gint scanned = 0;
    gchar *content = NULL;

    path = g_strdup_printf ("/sys/block/%s/stat", device);
    success = g_file_get_contents (path, &content, NULL, error);
    g_free (path);
    if (!success) {
        /* error is already populated */
        return FALSE;
    }

    scanned = sscanf (content,
                      "%*[ \t]%" G_GUINT64_FORMAT "%*[ \t]%*[0-9]%*[ \t]%*[0-9]%*[ \t]%*[0-9]%" G_GUINT64_FORMAT "",
                      &stats->num_reads, &stats->num_writes);
    g_free (content);
    if (scanned != 2) {
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'stat' for '%s' zRAM device", device);
        return FALSE;
    }

    path = g_strdup_printf ("/sys/block/%s/io_stat", device);
    success = g_file_get_contents (path, &content, NULL, error);
    g_free (path);
    if (!success) {
        /* error is already populated */
        return FALSE;
    }

    scanned = sscanf (content,
                      "%*[ \t]%*[0-9]%*[ \t]%*[0-9]%*[ \t]%" G_GUINT64_FORMAT "",
                      &stats->invalid_io);
    g_free (content);
    if (scanned != 1) {
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'io_stat' for '%s' zRAM device", device);
        return FALSE;
    }

    path = g_strdup_printf ("/sys/block/%s/mm_stat", device);
    success = g_file_get_contents (path, &content, NULL, error);
    g_free (path);
    if (!success) {
        /* error is already populated */
        return FALSE;
    }

    scanned = sscanf (content,
                      "%*[ \t]%" G_GUINT64_FORMAT "%*[ \t]%" G_GUINT64_FORMAT "%*[ \t]%" G_GUINT64_FORMAT \
                      "%*[ \t]%*[0-9]%*[ \t]%" G_GUINT64_FORMAT "",
                      &stats->orig_data_size, &stats->compr_data_size, &stats->mem_used_total, &stats->zero_pages);
    g_free (content);
    if (scanned != 4) {
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'mm_stat' for '%s' zRAM device", device);
        return FALSE;
    }

    return TRUE;
}


/**
 * zram_get_stats:
 * @device: zRAM device to get stats for
 * @error: (out): place to store error (if any)
 *
 * Returns: (transfer full): statistics for the zRAM device
 */
ZramStats* zram_get_stats (const gchar *device, GError **error) {
    gchar *path = NULL;
    gboolean success = FALSE;
    ZramStats *ret = NULL;

    if (!check_deps (error))
        return FALSE;

    ret = g_new0 (ZramStats, 1);

    if (g_str_has_prefix (device, "/dev/"))
        device += 5;

    path = g_strdup_printf ("/sys/block/%s", device);
    if (access (path, R_OK) != 0) {
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_NOEXIST,
                     "Device '%s' doesn't seem to exist", device);
        g_free (path);
        g_free (ret);
        return NULL;
    }
    g_free (path);

    path = g_strdup_printf ("/sys/block/%s/disksize", device);
    ret->disksize = get_number_from_file (path, error);
    g_free (path);
    if (*error) {
        g_clear_error (error);
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'disksize' for '%s' zRAM device", device);
        g_free (ret);
        return NULL;
    }

    path = g_strdup_printf ("/sys/block/%s/max_comp_streams", device);
    ret->max_comp_streams = get_number_from_file (path, error);
    g_free (path);
    if (*error) {
        g_clear_error (error);
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'max_comp_streams' for '%s' zRAM device", device);
        g_free (ret);
        return NULL;
    }

    path = g_strdup_printf ("/sys/block/%s/comp_algorithm", device);
    success = g_file_get_contents (path, &(ret->comp_algorithm), NULL, error);
    g_free (path);
    if (!success) {
        g_clear_error (error);
        g_set_error (error, ZRAM_ERROR, ZRAM_ERROR_INVAL,
                     "Failed to get 'comp_algorithm' for '%s' zRAM device", device);
        g_free (ret);
        return NULL;
    }
    /* remove the trailing space and newline */
    g_strstrip (ret->comp_algorithm);

    /* We need to read stats from different files on new and old kernels.
       e.g. "num_reads" exits only on old kernels and "stat" (that replaces
       "num_reads/writes/etc.") exists only on newer kernels.
    */
    path = g_strdup_printf ("/sys/block/%s/num_reads", device);
    if (g_file_test (path, G_FILE_TEST_EXISTS))
      success = get_zram_stats_old (device, ret, error);
    else
      success = get_zram_stats_new (device, ret, error);
    g_free (path);

    if (!success) {
        /* error is already populated */
        g_free (ret);
        return NULL;
    }

    return ret;
}
