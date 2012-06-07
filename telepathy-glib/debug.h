#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TP_DEBUG_H__
#define __TP_DEBUG_H__

#include <glib.h>
#include <telepathy-glib/defs.h>

G_BEGIN_DECLS

void tp_debug_set_flags (const gchar *flags_string);

void tp_debug_set_persistent (gboolean persistent);

void tp_debug_divert_messages (const gchar *filename);

void tp_debug_timestamped_log_handler (const gchar *log_domain,
    GLogLevelFlags log_level, const gchar *message, gpointer ignored);

G_END_DECLS

#endif
