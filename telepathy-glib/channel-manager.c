/*
 * channel-manager.c - factory and manager for channels relating to a
 *  particular protocol feature
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

/**
 * SECTION:channel-manager
 * @title: TpChannelManager
 * @short_description: interface for creating and tracking channels
 * @see_also: #TpSvcConnection
 *
 * A channel manager is attached to a connection. It carries out channel
 * requests from the connection, and responds to channel-related events on the
 * underlying network connection, for particular classes of channel (for
 * example, incoming and outgoing calls, respectively). It also tracks
 * currently-open channels of the relevant kinds.
 *
 * The connection has an array of channel managers. In response to a call to
 * CreateChannel or RequestChannel, the channel request is offered to each
 * channel manager in turn, until one accepts the request. In a trivial
 * implementation there might be a single channel manager which handles all
 * requests and all incoming events, but in general, there will be multiple
 * channel managers handling different types of channel.
 *
 * For example, at the time of writing, Gabble has a roster channel manager
 * which handles contact lists and groups, an IM channel manager which
 * handles one-to-one messaging, a MUC channel manager which handles
 * multi-user chat rooms and the index of chat rooms, and a media channel
 * manager which handles VoIP calls.
 */

#include "config.h"
#include "channel-manager.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/exportable-channel.h>
#include <telepathy-glib/util.h>

#include "_gen/signals-marshal.h"

enum {
    S_NEW_CHANNELS,
    S_REQUEST_ALREADY_SATISFIED,
    S_REQUEST_FAILED,
    S_CHANNEL_CLOSED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};


static void
channel_manager_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;

      /* FIXME: should probably have a better GType for @channels */
      /**
       * TpChannelManager::new-channels:
       * @self: the channel manager
       * @channels: a #GHashTable where the keys are
       *  #TpExportableChannel instances (hashed and compared
       *  by g_direct_hash() and g_direct_equal()) and the values are
       *  linked lists (#GSList) of request tokens (opaque pointers) satisfied
       *  by these channels
       *
       * Emitted when new channels have been created. The Connection should
       * generally emit NewChannels (and NewChannel) in response to this
       * signal, and then return from pending CreateChannel, EnsureChannel
       * and/or RequestChannel calls if appropriate.
       */
      signals[S_NEW_CHANNELS] = g_signal_new ("new-channels",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__POINTER,
          G_TYPE_NONE, 1, G_TYPE_POINTER);

      /**
       * TpChannelManager::request-already-satisfied:
       * @self: the channel manager
       * @request_token: opaque pointer supplied by the requester,
       *  representing a request
       * @channel: the existing #TpExportableChannel that satisfies the
       *  request
       *
       * Emitted when a channel request is satisfied by an existing channel.
       * The Connection should generally respond to this signal by returning
       * success from EnsureChannel or RequestChannel.
       */
      signals[S_REQUEST_ALREADY_SATISFIED] = g_signal_new (
          "request-already-satisfied",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
          0,
          NULL, NULL,
          _tp_marshal_VOID__POINTER_OBJECT,
          G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_OBJECT);

      /**
       * TpChannelManager::request-failed:
       * @self: the channel manager
       * @request_token: opaque pointer supplied by the requester,
       *  representing a request
       * @domain: the domain of a #GError indicating why the request
       *  failed
       * @code: the error code of a #GError indicating why the request
       *  failed
       * @message: the string part of a #GError indicating why the request
       *  failed
       *
       * Emitted when a channel request has failed. The Connection should
       * generally respond to this signal by returning failure from
       * CreateChannel, EnsureChannel or RequestChannel.
       */
      signals[S_REQUEST_FAILED] = g_signal_new ("request-failed",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
          0,
          NULL, NULL,
          _tp_marshal_VOID__POINTER_UINT_INT_STRING,
          G_TYPE_NONE, 4, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_INT,
          G_TYPE_STRING);

      /**
       * TpChannelManager::channel-closed:
       * @self: the channel manager
       * @path: the channel's object-path
       *
       * Emitted when a channel has been closed. The Connection should
       * generally respond to this signal by emitting ChannelClosed.
       */
      signals[S_CHANNEL_CLOSED] = g_signal_new ("channel-closed",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__STRING,
          G_TYPE_NONE, 1, G_TYPE_STRING);

    }
}

GType
tp_channel_manager_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo info = {
        sizeof (TpChannelManagerIface),
        channel_manager_base_init,   /* base_init */
        NULL,   /* base_finalize */
        NULL,   /* class_init */
        NULL,   /* class_finalize */
        NULL,   /* class_data */
        0,
        0,      /* n_preallocs */
        NULL    /* instance_init */
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
          "TpChannelManager", &info, 0);
    }

  return type;
}


/* Signal emission wrappers */


/**
 * tp_channel_manager_emit_new_channels:
 * @instance: An object implementing #TpChannelManager
 * @channels: a #GHashTable where the keys are
 *  #TpExportableChannel instances (hashed and compared
 *  by g_direct_hash() and g_direct_equal()) and the values are
 *  linked lists (#GSList) of request tokens (opaque pointers) satisfied by
 *  these channels
 *
 * If @channels is non-empty, emit the #TpChannelManager::new-channels
 * signal indicating that those channels have been created.
 */
void
tp_channel_manager_emit_new_channels (gpointer instance,
                                      GHashTable *channels)
{
  g_return_if_fail (TP_IS_CHANNEL_MANAGER (instance));

  if (g_hash_table_size (channels) == 0)
    return;

  g_signal_emit (instance, signals[S_NEW_CHANNELS], 0, channels);
}


/**
 * tp_channel_manager_emit_new_channel:
 * @instance: An object implementing #TpChannelManager
 * @channel: A #TpExportableChannel
 * @request_tokens: the request tokens (opaque pointers) satisfied by this
 *                  channel
 *
 * Emit the #TpChannelManager::new-channels signal indicating that the
 * channel has been created. (This is a convenient shortcut for calling
 * tp_channel_manager_emit_new_channels() with a one-entry hash table.)
 */
void
tp_channel_manager_emit_new_channel (gpointer instance,
                                     TpExportableChannel *channel,
                                     GSList *request_tokens)
{
  GHashTable *channels;

  g_return_if_fail (TP_IS_CHANNEL_MANAGER (instance));
  g_return_if_fail (TP_IS_EXPORTABLE_CHANNEL (channel));

  channels = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, NULL);
  g_hash_table_insert (channels, channel, request_tokens);
  g_signal_emit (instance, signals[S_NEW_CHANNELS], 0, channels);
  g_hash_table_destroy (channels);
}


/**
 * tp_channel_manager_emit_channel_closed:
 * @instance: An object implementing #TpChannelManager
 * @path: A channel's object-path
 *
 * Emit the #TpChannelManager::channel-closed signal indicating that
 * the channel at the given object path has been closed.
 */
void
tp_channel_manager_emit_channel_closed (gpointer instance,
                                        const gchar *path)
{
  g_return_if_fail (TP_IS_CHANNEL_MANAGER (instance));
  g_return_if_fail (tp_dbus_check_valid_object_path (path, NULL));

  g_signal_emit (instance, signals[S_CHANNEL_CLOSED], 0, path);
}


/**
 * tp_channel_manager_emit_channel_closed_for_object:
 * @instance: An object implementing #TpChannelManager
 * @channel: A #TpExportableChannel
 *
 * Emit the #TpChannelManager::channel-closed signal indicating that
 * the given channel has been closed. (This is a convenient shortcut for
 * calling tp_channel_manager_emit_channel_closed() with the
 * #TpExportableChannel:object-path property of @channel.)
 */
void
tp_channel_manager_emit_channel_closed_for_object (gpointer instance,
    TpExportableChannel *channel)
{
  gchar *path;

  g_return_if_fail (TP_IS_EXPORTABLE_CHANNEL (channel));
  g_object_get (channel,
      "object-path", &path,
      NULL);
  tp_channel_manager_emit_channel_closed (instance, path);
  g_free (path);
}


/**
 * tp_channel_manager_emit_request_already_satisfied:
 * @instance: An object implementing #TpChannelManager
 * @request_token: An opaque pointer representing the request that
 *  succeeded
 * @channel: The channel that satisfies the request
 *
 * Emit the #TpChannelManager::request-already-satisfied signal indicating
 * that the pre-existing channel @channel satisfies @request_token.
 */
void
tp_channel_manager_emit_request_already_satisfied (gpointer instance,
    gpointer request_token,
    TpExportableChannel *channel)
{
  g_return_if_fail (TP_IS_EXPORTABLE_CHANNEL (channel));
  g_return_if_fail (TP_IS_CHANNEL_MANAGER (instance));

  g_signal_emit (instance, signals[S_REQUEST_ALREADY_SATISFIED], 0,
      request_token, channel);
}


/**
 * tp_channel_manager_emit_request_failed:
 * @instance: An object implementing #TpChannelManager
 * @request_token: An opaque pointer representing the request that failed
 * @domain: a #GError domain
 * @code: a #GError code appropriate for @domain
 * @message: the error message
 *
 * Emit the #TpChannelManager::request-failed signal indicating that
 * the request @request_token failed for the given reason.
 */
void
tp_channel_manager_emit_request_failed (gpointer instance,
                                        gpointer request_token,
                                        GQuark domain,
                                        gint code,
                                        const gchar *message)
{
  g_return_if_fail (TP_IS_CHANNEL_MANAGER (instance));

  g_signal_emit (instance, signals[S_REQUEST_FAILED], 0, request_token,
      domain, code, message);
}


/**
 * tp_channel_manager_emit_request_failed_printf:
 * @instance: An object implementing #TpChannelManager
 * @request_token: An opaque pointer representing the request that failed
 * @domain: a #GError domain
 * @code: a #GError code appropriate for @domain
 * @format: a printf-style format string for the error message
 * @...: arguments for the format string
 *
 * Emit the #TpChannelManager::request-failed signal indicating that
 * the request @request_token failed for the given reason.
 */
void
tp_channel_manager_emit_request_failed_printf (gpointer instance,
                                               gpointer request_token,
                                               GQuark domain,
                                               gint code,
                                               const gchar *format,
                                               ...)
{
  va_list ap;
  gchar *message;

  va_start (ap, format);
  message = g_strdup_vprintf (format, ap);
  va_end (ap);

  tp_channel_manager_emit_request_failed (instance, request_token,
      domain, code, message);

  g_free (message);
}


/* Virtual-method wrappers */


/**
 * tp_channel_manager_foreach_channel:
 * @manager: an object implementing #TpChannelManager
 * @func: A function
 * @user_data: Arbitrary data to be passed as the second argument of @func
 *
 * Calls func(channel, user_data) for each channel managed by @manager.
 */
void
tp_channel_manager_foreach_channel (TpChannelManager *manager,
                                    TpExportableChannelFunc func,
                                    gpointer user_data)
{
  TpChannelManagerIface *iface = TP_CHANNEL_MANAGER_GET_INTERFACE (
      manager);
  TpChannelManagerForeachChannelFunc method = iface->foreach_channel;

  if (method != NULL)
    {
      method (manager, func, user_data);
    }
  /* ... else assume it has no channels, and do nothing */
}


/**
 * tp_channel_manager_foreach_channel_class:
 * @manager: An object implementing #TpChannelManager
 * @func: A function
 * @user_data: Arbitrary data to be passed as the final argument of @func
 *
 * Calls func(manager, fixed, allowed, user_data) for each channel class
 * understood by @manager.
 */
void
tp_channel_manager_foreach_channel_class (TpChannelManager *manager,
    TpChannelManagerChannelClassFunc func,
    gpointer user_data)
{
  TpChannelManagerIface *iface = TP_CHANNEL_MANAGER_GET_INTERFACE (
      manager);
  TpChannelManagerForeachChannelClassFunc method =
      iface->foreach_channel_class;

  if (method != NULL)
    {
      method (manager, func, user_data);
    }
  /* ... else assume it has no classes of requestable channel */
}


/**
 * tp_channel_manager_create_channel:
 * @manager: An object implementing #TpChannelManager
 * @request_token: An opaque pointer representing this pending request.
 * @request_properties: A table mapping (const gchar *) property names to
 *  GValue, representing the desired properties of a channel requested by a
 *  Telepathy client.
 *
 * Offers an incoming CreateChannel call to @manager.
 *
 * Returns: %TRUE if this request will be handled by @manager; else %FALSE.
 */
gboolean
tp_channel_manager_create_channel (TpChannelManager *manager,
                                   gpointer request_token,
                                   GHashTable *request_properties)
{
  TpChannelManagerIface *iface = TP_CHANNEL_MANAGER_GET_INTERFACE (
      manager);
  TpChannelManagerRequestFunc method = iface->create_channel;

  /* A missing implementation is equivalent to one that always returns FALSE,
   * meaning "can't do that, ask someone else" */
  if (method != NULL)
    return method (manager, request_token, request_properties);
  else
    return FALSE;
}


/**
 * tp_channel_manager_request_channel:
 * @manager: An object implementing #TpChannelManager
 * @request_token: An opaque pointer representing this pending request.
 * @request_properties: A table mapping (const gchar *) property names to
 *  GValue, representing the desired properties of a channel requested by a
 *  Telepathy client.
 *
 * Offers an incoming RequestChannel call to @manager.
 *
 * Returns: %TRUE if this request will be handled by @manager; else %FALSE.
 */
gboolean
tp_channel_manager_request_channel (TpChannelManager *manager,
                                    gpointer request_token,
                                    GHashTable *request_properties)
{
  TpChannelManagerIface *iface = TP_CHANNEL_MANAGER_GET_INTERFACE (
      manager);
  TpChannelManagerRequestFunc method = iface->request_channel;

  /* A missing implementation is equivalent to one that always returns FALSE,
   * meaning "can't do that, ask someone else" */
  if (method != NULL)
    return method (manager, request_token, request_properties);
  else
    return FALSE;
}


/**
 * tp_channel_manager_asv_has_unknown_properties:
 * @properties: a table mapping (const gchar *) property names to GValues,
 *              as passed to methods of #TpChannelManager
 * @fixed: a %NULL-terminated array of property names
 * @allowed: a %NULL-terminated array of property names
 * @error: an address at which to store an error suitable for returning from
 *         the D-Bus method when @properties contains unknown properties
 *
 * Checks whether the keys of @properties are elements of one of @fixed and
 * @allowed.  This is intended to be used by implementations of
 * #TpChannelManager::create_channel which have decided to accept a request,
 * to conform with the specification's requirement that unknown requested
 * properties must cause a request to fail, not be silently ignored.
 *
 * On encountering unknown properties, this function will return %FALSE, and
 * set @error to a #GError that could be used as a D-Bus method error.
 *
 * Returns: %TRUE if all of the keys of @properties are elements of @fixed or
 *          @allowed; else %FALSE.
 */
gboolean
tp_channel_manager_asv_has_unknown_properties (GHashTable *properties,
                                               const gchar * const *fixed,
                                               const gchar * const *allowed,
                                               GError **error)
{
  GHashTableIter iter;
  gpointer key;
  const gchar *property_name;

  g_hash_table_iter_init (&iter, properties);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      property_name = key;
      if (!tp_strv_contains (fixed, property_name) &&
          !tp_strv_contains (allowed, property_name))
        {
          g_set_error (error, TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
              "Request contained unknown property '%s'", property_name);
          return TRUE;
        }
    }
  return FALSE;
}
