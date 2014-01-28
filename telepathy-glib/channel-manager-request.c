/*
 * channel-manager-request.c
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

#include "config.h"

#include "channel-manager-request.h"
#include "channel-manager-request-internal.h"

#define DEBUG_FLAG TP_DEBUG_DISPATCHER
#include "telepathy-glib/debug-internal.h"

#include <telepathy-glib/telepathy-glib-dbus.h>

/**
 * SECTION: channel-manager-request
 * @title: TpChannelManagerRequest
 * @short_description: TODO
 *
 * TODO
 */

/**
 * TpChannelManagerRequest:
 *
 * Data structure representing a #TpChannelManagerRequest.
 *
 * Since: UNRELEASED
 */

/**
 * TpChannelManagerRequestClass:
 *
 * The class of a #TpChannelManagerRequest.
 *
 * Since: UNRELEASED
 */

/**
 * TpChannelManagerRequestMethod:
 * @TP_CHANNEL_MANAGER_REQUEST_METHOD_CREATE_CHANNEL: a CreateChannel() call
 * @TP_CHANNEL_MANAGER_REQUEST_METHOD_ENSURE_CHANNEL: a EnsureChannel() call
 *
 * The method associated with a #TpChannelManagerRequest
 */

G_DEFINE_TYPE (TpChannelManagerRequest, tp_channel_manager_request, G_TYPE_OBJECT)

static void
tp_channel_manager_request_finalize (GObject *object)
{
  TpChannelManagerRequest *self = TP_CHANNEL_MANAGER_REQUEST (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) tp_channel_manager_request_parent_class)->finalize;

  g_assert (self->context == NULL);

  DEBUG("Freeing channel request at %p: ctype=%s htype=%d handle=%d",
        self, self->channel_type, self->handle_type,
        self->handle);

  g_free (self->channel_type);

  chain_up (object);
}

static void
tp_channel_manager_request_class_init (
    TpChannelManagerRequestClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = tp_channel_manager_request_finalize;
}

static void
tp_channel_manager_request_init (TpChannelManagerRequest *self)
{
}

TpChannelManagerRequest *
_tp_channel_manager_request_new (
    DBusGMethodInvocation *context,
    TpChannelManagerRequestMethod method,
    const char *channel_type,
    TpHandleType handle_type,
    TpHandle handle)
{
  TpChannelManagerRequest *result;

  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (channel_type != NULL, NULL);
  g_return_val_if_fail (method < TP_NUM_CHANNEL_MANAGER_REQUEST_METHODS, NULL);

  result = g_object_new (TP_TYPE_CHANNEL_MANAGER_REQUEST,
      NULL);

  result->context = context;
  result->method = method;
  result->channel_type = g_strdup (channel_type);
  result->handle_type = handle_type;
  result->handle = handle;
  result->yours = FALSE;

  DEBUG ("New channel request at %p: ctype=%s htype=%d handle=%d",
        result, channel_type, handle_type, handle);

  return result;
}

void
_tp_channel_manager_request_cancel (TpChannelManagerRequest *self)
{
  GError error = { TP_ERROR, TP_ERROR_DISCONNECTED,
      "unable to service this channel request, we're disconnecting!" };

  g_return_if_fail (self->context != NULL);

  DEBUG ("cancelling request at %p for %s/%u/%u", self,
      self->channel_type, self->handle_type, self->handle);

  dbus_g_method_return_error (self->context, &error);
  self->context = NULL;
}

void
_tp_channel_manager_request_satisfy (TpChannelManagerRequest *self,
    TpExportableChannel *channel)
{
  gchar *object_path;
  GHashTable *properties;

  g_return_if_fail (TP_IS_EXPORTABLE_CHANNEL (channel));
  g_return_if_fail (self->context != NULL);

  DEBUG ("completing queued request %p with success, "
      "channel_type=%s, handle_type=%u, "
      "handle=%u", self, self->channel_type, self->handle_type, self->handle);

  g_object_get (channel,
      "object-path", &object_path,
      "channel-properties", &properties,
      NULL);

  switch (self->method)
    {
      case TP_CHANNEL_MANAGER_REQUEST_METHOD_CREATE_CHANNEL:
        tp_svc_connection_interface_requests_return_from_create_channel (
            self->context, object_path, properties);
        break;

      case TP_CHANNEL_MANAGER_REQUEST_METHOD_ENSURE_CHANNEL:
        tp_svc_connection_interface_requests_return_from_ensure_channel (
            self->context, self->yours, object_path, properties);
        break;

      default:
        g_assert_not_reached ();
    }

  self->context = NULL;

  g_free (object_path);
  g_hash_table_unref (properties);
}

void
_tp_channel_manager_request_fail (TpChannelManagerRequest *self,
    GError *error)
{
  g_return_if_fail (self->context != NULL);

  DEBUG ("completing queued request %p with error, channel_type=%s, "
      "handle_type=%u, handle=%u",
      self, self->channel_type, self->handle_type, self->handle);

  dbus_g_method_return_error (self->context, error);
  self->context = NULL;
}
