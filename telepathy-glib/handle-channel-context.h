/*
 * object for HandleChannels calls context
 *
 * Copyright © 2010 Collabora Ltd.
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

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TP_HANDLE_CHANNEL_CONTEXT_H__
#define __TP_HANDLE_CHANNEL_CONTEXT_H__

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TpHandleChannelContext TpHandleChannelContext;
typedef struct _TpHandleChannelContextClass \
          TpHandleChannelContextClass;
typedef struct _TpHandleChannelContextPrivate \
          TpHandleChannelContextPrivate;

GType tp_handle_channel_context_get_type (void);

#define TP_TYPE_HANDLE_CHANNELS_CONTEXT \
  (tp_handle_channel_context_get_type ())
#define TP_HANDLE_CHANNEL_CONTEXT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_HANDLE_CHANNELS_CONTEXT, \
                               TpHandleChannelContext))
#define TP_HANDLE_CHANNEL_CONTEXT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_HANDLE_CHANNELS_CONTEXT, \
                            TpHandleChannelContextClass))
#define TP_IS_HANDLE_CHANNELS_CONTEXT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_HANDLE_CHANNELS_CONTEXT))
#define TP_IS_HANDLE_CHANNELS_CONTEXT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_HANDLE_CHANNELS_CONTEXT))
#define TP_HANDLE_CHANNEL_CONTEXT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_HANDLE_CHANNELS_CONTEXT, \
                              TpHandleChannelContextClass))

void tp_handle_channel_context_accept (
    TpHandleChannelContext *self);

void tp_handle_channel_context_fail (
    TpHandleChannelContext *self,
    const GError *error);

void tp_handle_channel_context_delay (
    TpHandleChannelContext *self);

GVariant * tp_handle_channel_context_dup_handler_info (
    TpHandleChannelContext *self);

GList * tp_handle_channel_context_get_requests (
    TpHandleChannelContext *self);

G_END_DECLS

#endif
