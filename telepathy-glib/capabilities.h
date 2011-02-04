/* Object representing the capabilities a Connection or a Contact supports.
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
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

#ifndef __TP_CAPABILITIES_H__
#define __TP_CAPABILITIES_H__

#include <glib-object.h>

#include <telepathy-glib/enums.h>

G_BEGIN_DECLS

typedef struct _TpCapabilities TpCapabilities;
typedef struct _TpCapabilitiesClass TpCapabilitiesClass;
typedef struct _TpCapabilitiesPrivate TpCapabilitiesPrivate;

GType tp_capabilities_get_type (void) G_GNUC_CONST;

#define TP_TYPE_CAPABILITIES \
  (tp_capabilities_get_type ())
#define TP_CAPABILITIES(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CAPABILITIES, \
                               TpCapabilities))
#define TP_CAPABILITIES_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_CAPABILITIES, \
                            TpCapabilitiesClass))
#define TP_IS_CAPABILITIES(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_CAPABILITIES))
#define TP_IS_CAPABILITIES_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_CAPABILITIES))
#define TP_CAPABILITIES_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CAPABILITIES, \
                              TpCapabilitiesClass))

GPtrArray * tp_capabilities_get_channel_classes (TpCapabilities *self);

gboolean tp_capabilities_is_specific_to_contact (TpCapabilities *self);

gboolean tp_capabilities_supports_text_chats (TpCapabilities *self);
gboolean tp_capabilities_supports_text_chatrooms (TpCapabilities *self);

gboolean tp_capabilities_supports_stream_tubes (TpCapabilities *self,
    TpHandleType handle_type,
    const gchar *service);

gboolean tp_capabilities_supports_dbus_tubes (TpCapabilities *self,
    TpHandleType handle_type,
    const gchar *service_name);

gboolean tp_capabilities_supports_contact_search (TpCapabilities *self,
    gboolean *with_limit,
    gboolean *with_server);

gboolean tp_capabilities_supports_room_list (TpCapabilities *self,
    gboolean *with_server);

G_END_DECLS

#endif
