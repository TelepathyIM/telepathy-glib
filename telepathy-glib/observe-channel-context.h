/*
 * Context objects for TpBaseClient calls
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

#ifndef __TP_OBSERVE_CHANNEL_CONTEXT_H__
#define __TP_OBSERVE_CHANNEL_CONTEXT_H__

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TpObserveChannelContext TpObserveChannelContext;
typedef struct _TpObserveChannelContextClass TpObserveChannelContextClass;
typedef struct _TpObserveChannelContextPrivate TpObserveChannelContextPrivate;

GType tp_observe_channel_context_get_type (void);

#define TP_TYPE_OBSERVE_CHANNELS_CONTEXT \
  (tp_observe_channel_context_get_type ())
#define TP_OBSERVE_CHANNEL_CONTEXT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_OBSERVE_CHANNELS_CONTEXT, \
                               TpObserveChannelContext))
#define TP_OBSERVE_CHANNEL_CONTEXT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_OBSERVE_CHANNELS_CONTEXT, \
                            TpObserveChannelContextClass))
#define TP_IS_OBSERVE_CHANNELS_CONTEXT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_OBSERVE_CHANNELS_CONTEXT))
#define TP_IS_OBSERVE_CHANNELS_CONTEXT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_OBSERVE_CHANNELS_CONTEXT))
#define TP_OBSERVE_CHANNEL_CONTEXT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_OBSERVE_CHANNELS_CONTEXT, \
                              TpObserveChannelContextClass))

void tp_observe_channel_context_accept (TpObserveChannelContext *self);

void tp_observe_channel_context_fail (TpObserveChannelContext *self,
    const GError *error);

void tp_observe_channel_context_delay (TpObserveChannelContext *self);

gboolean tp_observe_channel_context_is_recovering (
    TpObserveChannelContext *self);

GList * tp_observe_channel_context_get_requests (
    TpObserveChannelContext *self);

G_END_DECLS

#endif
