/*
 * debugger.h - header for Telepathy debug interface implementation
 * Copyright (C) 2009 Collabora Ltd.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __TP_DEBUGGER_H__
#define __TP_DEBUGGER_H__

#include <glib-object.h>

#include "properties-mixin.h"
#include "dbus-properties-mixin.h"

G_BEGIN_DECLS

typedef struct _TpDebugger TpDebugger;
typedef struct _TpDebuggerClass TpDebuggerClass;
typedef struct _TpDebugMessage TpDebugMessage;

#define TP_TYPE_DEBUGGER tp_debugger_get_type()
#define TP_DEBUGGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_DEBUGGER, TpDebugger))
#define TP_DEBUGGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_DEBUGGER, TpDebuggerClass))
#define TP_IS_DEBUGGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_DEBUGGER))
#define TP_IS_DEBUGGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_DEBUGGER))
#define TP_DEBUGGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_DEBUGGER, TpDebuggerClass))

/* On the basis that messages are around 60 bytes on average, and that 50kb is
 * a reasonable maximum size for a frame buffer.
 */

#define DEBUG_MESSAGE_LIMIT 800

struct _TpDebugMessage {
  /*<public>*/
  gdouble timestamp;
  gchar *domain;
  TpDebugLevel level;
  gchar *string;
};

struct _TpDebugger {
  GObject parent;

  /*<private>*/
  gboolean enabled;
  GQueue *messages;
};

struct _TpDebuggerClass {
  /*<private>*/
  GObjectClass parent_class;
  TpDBusPropertiesMixinClass dbus_props_class;
};

GType tp_debugger_get_type (void);

TpDebugger *tp_debugger_get_singleton (void);

void tp_debugger_add_message (TpDebugger *self,
    GTimeVal *timestamp,
    const gchar *domain,
    GLogLevelFlags level,
    const gchar *string);

G_END_DECLS

#endif /* __TP_DEBUGGER_H__ */
