#include <glib.h>
#include <glib-object.h>

#ifndef LIBZRAM_H
#define LIBZRAM_H

GQuark libzram_error_quark (void);
#define LIBZRAM_ERROR libzram_error_quark ()
typedef enum {
    LIBZRAM_ERROR_INVAL,
    LIBZRAM_ERROR_NOEXIST,
} LibzramError;

/**
 * LibzramStats:
 *
 * see zRAM kernel documentation for details
 * (https://www.kernel.org/doc/Documentation/blockdev/zram.txt)
 */
typedef struct LibzramStats {
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
} LibzramStats;

LibzramStats* libzram_stats_copy (LibzramStats *data);
void libzram_stats_free (LibzramStats *data);
GType libzram_stats_get_type (void);

gboolean libzram_create_devices (guint64 num_devices, const guint64 *sizes, const guint64 *nstreams, GError **error);
gboolean libzram_destroy_devices (GError **error);
gboolean libzram_add_device (guint64 size, guint64 nstreams, gchar **device, GError **error);
gboolean libzram_remove_device (const gchar *device, GError **error);
LibzramStats* libzram_get_stats (const gchar *device, GError **error);

#endif  /* LIBZRAM_H */
