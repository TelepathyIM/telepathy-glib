/*
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
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#ifndef __TPL_TEXT_CHANNEL_H__
#define __TPL_TEXT_CHANNEL_H__

/*
 * http://telepathy.freedesktop.org/doc/telepathy-glib/telepathy-glib-channel-text.html#tp-cli-channel-type-text-connect-to-received
 */

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

#define TPL_TYPE_TEXT_CHANNEL             (_tpl_text_channel_get_type ())
#define TPL_TEXT_CHANNEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_TEXT_CHANNEL, TplTextChannel))
#define TPL_TEXT_CHANNEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_TEXT_CHANNEL, TplTextChannelClass))
#define TPL_IS_TEXT_CHANNEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_TEXT_CHANNEL))
#define TPL_IS_TEXT_CHANNEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_TEXT_CHANNEL))
#define TPL_TEXT_CHANNEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_TEXT_CHANNEL, TplTextChannelClass))


#define TPL_TEXT_CHANNEL_ERROR \
  g_quark_from_static_string ("tpl-text-channel-error-quark")

typedef enum
{
  /* generic error */
  TPL_TEXT_CHANNEL_ERROR_FAILED,
  TPL_TEXT_CHANNEL_ERROR_NEED_MESSAGE_INTERFACE,
} TplTextChannelError;

#define TPL_TEXT_CHANNEL_FEATURE_CORE \
  _tpl_text_channel_get_feature_quark_core ()
GQuark _tpl_text_channel_get_feature_quark_core (void) G_GNUC_CONST;

typedef struct _TplTextChannelPriv TplTextChannelPriv;
typedef struct
{
  TpTextChannel parent;

  /* private */
  TplTextChannelPriv *priv;
} TplTextChannel;

typedef struct
{
  TpTextChannelClass parent_class;
} TplTextChannelClass;

GType _tpl_text_channel_get_type (void);

TplTextChannel * _tpl_text_channel_new (TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    GError **error);

TplTextChannel * _tpl_text_channel_new_with_factory (
    TpSimpleClientFactory *factory,
    TpConnection *conn,
    const gchar *object_path,
    const GHashTable *tp_chan_props,
    GError **error);

G_END_DECLS
#endif /* __TPL_TEXT_CHANNEL_H__ */
