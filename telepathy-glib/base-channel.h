/*
 * base-channel.h - Header for TpBaseChannel
 *
 * Copyright © 2009-2010 Collabora Ltd.
 * Copyright © 2009-2010 Nokia Corporation
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

#ifndef __TP_BASE_CHANNEL_H__
#define __TP_BASE_CHANNEL_H__

#include <glib-object.h>

#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/base-connection.h>

#include "connection.h"

G_BEGIN_DECLS

typedef struct _TpBaseChannel TpBaseChannel;
typedef struct _TpBaseChannelClass TpBaseChannelClass;
typedef struct _TpBaseChannelPrivate TpBaseChannelPrivate;

typedef void (*TpBaseChannelCloseFunc) (TpBaseChannel *chan);
typedef void (*TpBaseChannelFillPropertiesFunc) (TpBaseChannel *chan,
    GHashTable *properties);
typedef gchar *(*TpBaseChannelGetPathFunc) (TpBaseChannel *chan);

struct _TpBaseChannelClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  TpDBusPropertiesMixinClass dbus_props_class;

  const gchar *channel_type;
  TpHandleType target_handle_type;
  const gchar **interfaces;

  TpBaseChannelCloseFunc close;
  TpBaseChannelFillPropertiesFunc fill_immutable_properties;
  TpBaseChannelGetPathFunc get_object_path_suffix;

  /*< private >*/
  GCallback _reserved[10];
};

struct _TpBaseChannel
{
  /*< private >*/
  GObject parent;

  TpBaseChannelPrivate *priv;
};

void tp_base_channel_register (TpBaseChannel *chan);
void tp_base_channel_close (TpBaseChannel *chan);
void tp_base_channel_destroyed (TpBaseChannel *chan);
void tp_base_channel_reopened (TpBaseChannel *chan, TpHandle initiator);

const gchar *tp_base_channel_get_object_path (TpBaseChannel *chan);
TpBaseConnection *tp_base_channel_get_connection (TpBaseChannel *chan);
TpHandle tp_base_channel_get_target_handle (TpBaseChannel *chan);
TpHandle tp_base_channel_get_initiator (TpBaseChannel *chan);
gboolean tp_base_channel_is_requested (TpBaseChannel *chan);
gboolean tp_base_channel_is_registered (TpBaseChannel *chan);
gboolean tp_base_channel_is_destroyed (TpBaseChannel *chan);

GType tp_base_channel_get_type (void);

/* TYPE MACROS */
#define TP_TYPE_BASE_CHANNEL \
  (tp_base_channel_get_type ())
#define TP_BASE_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_BASE_CHANNEL, \
                              TpBaseChannel))
#define TP_BASE_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_BASE_CHANNEL, \
                           TpBaseChannelClass))
#define TP_IS_BASE_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_BASE_CHANNEL))
#define TP_IS_BASE_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_BASE_CHANNEL))
#define TP_BASE_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_BASE_CHANNEL, \
                              TpBaseChannelClass))

G_END_DECLS

#endif /* #ifndef __TP_BASE_CHANNEL_H__*/
