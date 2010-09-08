/*
 * stream-tube.h - a view for the TpChannel proxy
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

#ifndef __TP_STREAM_TUBE_H__
#define __TP_STREAM_TUBE_H__

#include <telepathy-glib/channel.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TP_TYPE_STREAM_TUBE	(tp_stream_tube_get_type ())
#define TP_STREAM_TUBE(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_STREAM_TUBE, TpStreamTube))
#define TP_STREAM_TUBE_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), TP_TYPE_STREAM_TUBE, TpStreamTubeClass))
#define TP_IS_STREAM_TUBE(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_STREAM_TUBE))
#define TP_IS_STREAM_TUBE_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), TP_TYPE_STREAM_TUBE))
#define TP_STREAM_TUBE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_STREAM_TUBE, TpStreamTubeClass))

typedef struct _TpStreamTube TpStreamTube;
typedef struct _TpStreamTubeClass TpStreamTubeClass;
typedef struct _TpStreamTubePrivate TpStreamTubePrivate;

struct _TpStreamTube
{
  TpChannel parent;
  TpStreamTubePrivate *priv;
};

struct _TpStreamTubeClass
{
  TpChannelClass parent_class;
  /*<private>*/
  GCallback _padding[7];
};

GType tp_stream_tube_get_type (void);

TpStreamTube *tp_stream_tube_new (TpConnection *conn,
    const gchar *object_path,
    const GHashTable *immutable_properties,
    GError **error);

const gchar * tp_stream_tube_get_service (TpStreamTube *self);

GHashTable * tp_stream_tube_get_parameters (TpStreamTube *self);

/* Incoming tube methods */

void tp_stream_tube_accept_async (TpStreamTube *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

GIOStream *tp_stream_tube_accept_finish (TpStreamTube *self,
    GAsyncResult *result,
    GError **error);

/* Outgoing tube methods */

void tp_stream_tube_offer_async (TpStreamTube *self,
    GHashTable *params,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_stream_tube_offer_finish (TpStreamTube *self,
    GAsyncResult *result,
    GError **error);

void tp_stream_tube_offer_existing_async (TpStreamTube *self,
    GHashTable *params,
    GSocketAddress *address,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_stream_tube_offer_existing_finish (TpStreamTube *self,
    GAsyncResult *result,
    GError **error);

G_END_DECLS

#endif
