/*
 * channel-manager-request-internal.h
 *
 * Copyright (C) 2014 Collabora Ltd. <http://www.collabora.co.uk/>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TP_CHANNEL_MANAGER_REQUEST_INTERNAL_H__
#define __TP_CHANNEL_MANAGER_REQUEST_INTERNAL_H__

#include "channel-manager-request.h"

#include <glib-object.h>

#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

typedef enum {
    TP_CHANNEL_MANAGER_REQUEST_METHOD_CREATE_CHANNEL,
    TP_CHANNEL_MANAGER_REQUEST_METHOD_ENSURE_CHANNEL,
    TP_NUM_CHANNEL_MANAGER_REQUEST_METHODS,
} TpChannelManagerRequestMethod;

struct _TpChannelManagerRequestClass
{
  /*<private>*/
  GObjectClass parent_class;
};

struct _TpChannelManagerRequest
{
  /*<private>*/
  GObject parent;

  GDBusMethodInvocation *context;
  TpChannelManagerRequestMethod method;

  gchar *channel_type;
  TpEntityType handle_type;
  TpHandle handle;

  /* only meaningful for METHOD_ENSURE_CHANNEL; only true if this is the first
   * request to be satisfied with a particular channel, and no other request
   * satisfied by that channel has a different method.
   */
  gboolean yours;
};

GType tp_channel_manager_request_get_type (void);

/* TYPE MACROS */
#define TP_TYPE_CHANNEL_MANAGER_REQUEST \
  (tp_channel_manager_request_get_type ())
#define TP_CHANNEL_MANAGER_REQUEST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    TP_TYPE_CHANNEL_MANAGER_REQUEST, \
    TpChannelManagerRequest))
#define TP_CHANNEL_MANAGER_REQUEST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    TP_TYPE_CHANNEL_MANAGER_REQUEST, \
    TpChannelManagerRequestClass))
#define TP_IS_CHANNEL_MANAGER_REQUEST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    TP_TYPE_CHANNEL_MANAGER_REQUEST))
#define TP_IS_CHANNEL_MANAGER_REQUEST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    TP_TYPE_CHANNEL_MANAGER_REQUEST))
#define TP_CHANNEL_MANAGER_REQUEST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    TP_TYPE_CHANNEL_MANAGER_REQUEST, \
    TpChannelManagerRequestClass))

TpChannelManagerRequest * _tp_channel_manager_request_new (
    GDBusMethodInvocation *context,
    TpChannelManagerRequestMethod method,
    const char *channel_type,
    TpEntityType handle_type,
    TpHandle handle);

void _tp_channel_manager_request_cancel (TpChannelManagerRequest *self);

void _tp_channel_manager_request_satisfy (TpChannelManagerRequest *self,
    TpExportableChannel *channel);

void _tp_channel_manager_request_fail (TpChannelManagerRequest *self,
    GError *error);

G_END_DECLS

#endif /* #ifndef __TP_CHANNEL_MANAGER_REQUEST_INTERNAL_H__*/
