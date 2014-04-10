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
 * Since: 0.99.7
 */

/**
 * TpChannelManagerRequestClass:
 *
 * The class of a #TpChannelManagerRequest.
 *
 * Since: 0.99.7
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
        self, self->channel_type, self->entity_type,
        self->handle);

  g_free (self->channel_type);
  g_object_unref (self->skeleton);

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
    GDBusMethodInvocation *context,
    _TpGDBusConnectionInterfaceRequests *skeleton,
    TpChannelManagerRequestMethod method,
    const char *channel_type,
    TpEntityType entity_type,
    TpHandle handle)
{
  TpChannelManagerRequest *result;

  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (channel_type != NULL, NULL);
  g_return_val_if_fail (method < TP_NUM_CHANNEL_MANAGER_REQUEST_METHODS, NULL);

  result = g_object_new (TP_TYPE_CHANNEL_MANAGER_REQUEST,
      NULL);

  result->context = context;
  result->skeleton = g_object_ref (skeleton);
  result->method = method;
  result->channel_type = g_strdup (channel_type);
  result->entity_type = entity_type;
  result->handle = handle;
  result->yours = FALSE;

  DEBUG ("New channel request at %p: ctype=%s htype=%d handle=%d",
        result, channel_type, entity_type, handle);

  return result;
}

void
_tp_channel_manager_request_cancel (TpChannelManagerRequest *self)
{
  GError error = { TP_ERROR, TP_ERROR_DISCONNECTED,
      "unable to service this channel request, we're disconnecting!" };

  g_return_if_fail (self->context != NULL);

  DEBUG ("cancelling request at %p for %s/%u/%u", self,
      self->channel_type, self->entity_type, self->handle);

  g_dbus_method_invocation_return_gerror (self->context, &error);
  self->context = NULL;
}

void
_tp_channel_manager_request_satisfy (TpChannelManagerRequest *self,
    TpBaseChannel *channel)
{
  gchar *object_path;
  GVariant *properties;

  g_return_if_fail (TP_IS_BASE_CHANNEL (channel));
  g_return_if_fail (self->context != NULL);

  DEBUG ("completing queued request %p with success, "
      "channel_type=%s, entity_type=%u, "
      "handle=%u", self, self->channel_type, self->entity_type, self->handle);

  g_object_get (channel,
      "object-path", &object_path,
      "channel-properties", &properties,
      NULL);

  switch (self->method)
    {
      case TP_CHANNEL_MANAGER_REQUEST_METHOD_CREATE_CHANNEL:
        _tp_gdbus_connection_interface_requests_complete_create_channel (
            self->skeleton, self->context, object_path, properties);
        break;

      case TP_CHANNEL_MANAGER_REQUEST_METHOD_ENSURE_CHANNEL:
        _tp_gdbus_connection_interface_requests_complete_ensure_channel (
            self->skeleton, self->context, self->yours, object_path,
            properties);
        break;

      default:
        g_assert_not_reached ();
    }

  self->context = NULL;

  g_free (object_path);
  g_variant_unref (properties);
}

void
_tp_channel_manager_request_fail (TpChannelManagerRequest *self,
    GError *error)
{
  g_return_if_fail (self->context != NULL);

  DEBUG ("completing queued request %p with error, channel_type=%s, "
      "entity_type=%u, handle=%u",
      self, self->channel_type, self->entity_type, self->handle);

  g_dbus_method_invocation_return_gerror (self->context, error);
  self->context = NULL;
}
