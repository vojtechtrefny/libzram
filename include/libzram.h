#include <glib.h>
#include <glib-object.h>

#ifndef ZRAM_H
#define ZRAM_H

GQuark zram_error_quark (void);
#define ZRAM_ERROR zram_error_quark ()
typedef enum {
    ZRAM_ERROR_INVAL,
    ZRAM_ERROR_NOEXIST,
} ZramError;

/**
 * ZramStats:
 *
 * see zRAM kernel documentation for details
 * (https://www.kernel.org/doc/Documentation/blockdev/zram.txt)
 */
typedef struct ZramStats {
    guint64 disksize;
    guint64 num_reads;
    guint64 num_writes;
    guint64 invalid_io;
    guint64 zero_pages;
    guint64 max_comp_streams;
    gchar* comp_algorithm;
    guint64 orig_data_size;
    guint64 compr_data_size;
    guint64 mem_used_total;
} ZramStats;

ZramStats* zram_stats_copy (ZramStats *data);
void zram_stats_free (ZramStats *data);
GType zram_stats_get_type (void);

gboolean zram_create_devices (guint64 num_devices, const guint64 *sizes, const guint64 *nstreams, GError **error);
gboolean zram_destroy_devices (GError **error);
gboolean zram_add_device (guint64 size, guint64 nstreams, gchar **device, GError **error);
gboolean zram_remove_device (const gchar *device, GError **error);
ZramStats* zram_get_stats (const gchar *device, GError **error);

#endif  /* ZRAM_H */
