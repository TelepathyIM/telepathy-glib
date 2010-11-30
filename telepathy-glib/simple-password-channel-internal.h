/*
 * simple-password-channel.h - Header for TpSimplePasswordChannel
 * Copyright (C) 2010 Collabora Ltd.
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

#ifndef __TP_SIMPLE_PASSWORD_CHANNEL_H__
#define __TP_SIMPLE_PASSWORD_CHANNEL_H__

#include <glib-object.h>

#include <telepathy-glib/base-channel.h>
#include <telepathy-glib/dbus-properties-mixin.h>

G_BEGIN_DECLS

typedef struct _TpSimplePasswordChannel TpSimplePasswordChannel;
typedef struct _TpSimplePasswordChannelPrivate TpSimplePasswordChannelPrivate;
typedef struct _TpSimplePasswordChannelClass TpSimplePasswordChannelClass;

struct _TpSimplePasswordChannelClass
{
  TpBaseChannelClass parent_class;

  TpDBusPropertiesMixinClass properties_class;
};

struct _TpSimplePasswordChannel
{
  TpBaseChannel parent;

  TpSimplePasswordChannelPrivate *priv;
};

GType _tp_simple_password_channel_get_type (void);

/* TYPE MACROS */
#define TP_TYPE_SIMPLE_PASSWORD_CHANNEL \
  (_tp_simple_password_channel_get_type ())
#define TP_SIMPLE_PASSWORD_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_SIMPLE_PASSWORD_CHANNEL,\
                              TpSimplePasswordChannel))
#define TP_SIMPLE_PASSWORD_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_SIMPLE_PASSWORD_CHANNEL,\
                           TpSimplePasswordChannelClass))
#define TP_IS_SIMPLE_PASSWORD_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_SIMPLE_PASSWORD_CHANNEL))
#define TP_IS_SIMPLE_PASSWORD_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_SIMPLE_PASSWORD_CHANNEL))
#define TP_SIMPLE_PASSWORD_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_SIMPLE_PASSWORD_CHANNEL, \
                              TpSimplePasswordChannelClass))

G_END_DECLS

#endif /* #ifndef __TP_SIMPLE_PASSWORD_CHANNEL_H__*/
