/*
 * channel-view.h - a view for the TpChannel proxy
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

#ifndef __TP_CHANNEL_VIEW_H__
#define __TP_CHANNEL_VIEW_H__

#include <telepathy-glib/channel.h>

G_BEGIN_DECLS

#define TP_TYPE_CHANNEL_VIEW	(tp_channel_view_get_type ())
#define TP_CHANNEL_VIEW(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CHANNEL_VIEW, TpChannelView))
#define TP_CHANNEL_VIEW_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), TP_TYPE_CHANNEL_VIEW, TpChannelViewClass))
#define TP_IS_CHANNEL_VIEW(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_CHANNEL_VIEW))
#define TP_IS_CHANNEL_VIEW_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), TP_TYPE_CHANNEL_VIEW))
#define TP_CHANNEL_VIEW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CHANNEL_VIEW, TpChannelViewClass))

typedef struct _TpChannelView TpChannelView;
typedef struct _TpChannelViewClass TpChannelViewClass;
typedef struct _TpChannelViewPrivate TpChannelViewPrivate;

struct _TpChannelView
{
  GObject parent;
  TpChannelViewPrivate *priv;
};

struct _TpChannelViewClass
{
  GObjectClass parent_class;
};

GType tp_channel_view_get_type (void);
TpChannel *tp_channel_view_borrow_channel (TpChannelView *self);

G_END_DECLS

#endif
