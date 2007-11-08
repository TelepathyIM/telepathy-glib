#ifndef __TP_DEBUG_H__
#define __TP_DEBUG_H_

#include <glib.h>

G_BEGIN_DECLS

void tp_debug_set_flags (const gchar *flags_string);

void tp_debug_set_persistent (gboolean persistent);

void tp_debug_set_flags_from_string (const gchar *flags_string)
  G_GNUC_DEPRECATED;
void tp_debug_set_flags_from_env (const gchar *var)
  G_GNUC_DEPRECATED;
void tp_debug_set_all_flags (void) G_GNUC_DEPRECATED;

G_END_DECLS

#endif
