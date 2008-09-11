/*
 * exportable-channel.h - A channel usable with the Channel Manager
 *
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008 Nokia Corporation
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

#ifndef TP_EXPORTABLE_CHANNEL_H
#define TP_EXPORTABLE_CHANNEL_H

#include <glib-object.h>

G_BEGIN_DECLS

#define TP_TYPE_EXPORTABLE_CHANNEL (tp_exportable_channel_get_type ())

#define TP_EXPORTABLE_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TP_TYPE_EXPORTABLE_CHANNEL, TpExportableChannel))

#define TP_IS_EXPORTABLE_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_TYPE_EXPORTABLE_CHANNEL))

#define TP_EXPORTABLE_CHANNEL_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
  TP_TYPE_EXPORTABLE_CHANNEL, TpExportableChannelIface))

/**
 * TpExportableChannel:
 *
 * Opaque typedef representing a channel with several standard properties.
 */
typedef struct _TpExportableChannel TpExportableChannel;

typedef struct _TpExportableChannelIface TpExportableChannelIface;


/**
 * TpExportableChannelFunc:
 * @channel: An object implementing the exportable channel interface
 * @user_data: Arbitrary user-supplied data
 *
 * A callback for functions which act on exportable channels.
 */
typedef void (*TpExportableChannelFunc) (TpExportableChannel *channel,
    gpointer user_data);

/**
 * TpExportableChannelIface:
 * @parent: The parent interface
 *
 * The interface for #TpExportableChannel objects.
 */
struct _TpExportableChannelIface {
    GTypeInterface parent;
};

GType tp_exportable_channel_get_type (void);

G_END_DECLS

#endif
