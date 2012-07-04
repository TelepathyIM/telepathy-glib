/*
 * Copyright (C) 2011 Collabora Ltd.
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
 * Authors: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#ifndef __TPL_CALL_CHANNEL_H__
#define __TPL_CALL_CHANNEL_H__

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS
#define TPL_TYPE_CALL_CHANNEL            (_tpl_call_channel_get_type ())
#define TPL_CALL_CHANNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CALL_CHANNEL, TplCallChannel))
#define TPL_CALL_CHANNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CALL_CHANNEL, TplCallChannelClass))
#define TPL_IS_CALL_CHANNEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CALL_CHANNEL))
#define TPL_IS_CALL_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CALL_CHANNEL))
#define TPL_CALL_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CALL_CHANNEL, TplCallChannelClass))

#define TPL_CALL_CHANNEL_ERROR \
  g_quark_from_static_string ("tpl-call-channel-error-quark")

typedef enum
{
  /* generic error */
  TPL_CALL_CHANNEL_ERROR_FAILED,
  TPL_CALL_CHANNEL_ERROR_MISSING_TARGET_CONTACT,
} TplCallChannelError;

#define TPL_CALL_CHANNEL_FEATURE_CORE \
  _tpl_call_channel_get_feature_quark_core ()
GQuark _tpl_call_channel_get_feature_quark_core (void) G_GNUC_CONST;

typedef struct _TplCallChannelPriv TplCallChannelPriv;
typedef struct
{
  TpCallChannel parent;

  /* private */
  TplCallChannelPriv *priv;
} TplCallChannel;

typedef struct
{
  TpCallChannelClass parent_class;
} TplCallChannelClass;

GType _tpl_call_channel_get_type (void);

TplCallChannel * _tpl_call_channel_new (TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    GError **error);

TplCallChannel * _tpl_call_channel_new_with_factory (
    TpSimpleClientFactory *factory,
    TpConnection *conn,
    const gchar *object_path,
    const GHashTable *tp_chan_props,
    GError **error);

G_END_DECLS
#endif /* __TPL_CALL_CHANNEL_H__ */
