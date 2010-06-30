/*
 * account-channels.c - high level API to request channels
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

#include "telepathy-glib/account-internal.h"
#include "telepathy-glib/account.h"

#include <telepathy-glib/channel-dispatcher.h>
#include <telepathy-glib/channel-request.h>
#include <telepathy-glib/simple-handler.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_ACCOUNTS
#include "telepathy-glib/debug-internal.h"

typedef struct
{
  TpDBusDaemon *dbus;
  gboolean ensure;
  TpBaseClient *handler;
  GSimpleAsyncResult *result;
  TpChannelRequest *chan_request;
} request_ctx;

static request_ctx *
request_ctx_new (TpDBusDaemon *dbus,
    gboolean ensure)
{
  request_ctx *ctx = g_slice_new0 (request_ctx);

  ctx->dbus = g_object_ref (dbus);
  ctx->ensure = ensure;
  return ctx;
}

static void
request_ctx_free (request_ctx *ctx)
{
  tp_clear_object (&ctx->dbus);
  tp_clear_object (&ctx->handler);
  tp_clear_object (&ctx->result);
  tp_clear_object (&ctx->chan_request);

  g_slice_free (request_ctx, ctx);
}

static void
request_ctx_fail (request_ctx *ctx,
    const GError *error)
{
  g_simple_async_result_set_from_error (ctx->result, error);
  g_simple_async_result_complete (ctx->result);
}

static void
handle_channels (TpSimpleHandler *handler,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    GList *requests_satisfied,
    gint64 user_action_time,
    TpHandleChannelsContext *context,
    gpointer user_data)
{
  request_ctx *ctx = user_data;
  TpChannel *channel;

  if (g_list_length (channels) != 1)
    {
      GError error = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "We are supposed to handle only one channel" };

      tp_handle_channels_context_fail (context, &error);
      /* FIXME: Should we fail the operation ? */
      return;
    }

  /* Request succeed */
  channel = channels->data;

  g_simple_async_result_set_op_res_gpointer (ctx->result,
      g_object_ref (channel), g_object_unref);

  g_simple_async_result_complete (ctx->result);
  request_ctx_free (ctx);

  tp_handle_channels_context_accept (context);
}

static void
channel_request_failed_cb (TpChannelRequest *request,
  const gchar *error,
  const gchar *message,
  gpointer user_data,
  GObject *weak_object)
{
  request_ctx *ctx = user_data;
  GError *err = NULL;

  DEBUG ("ChannelRequest failed: %s", message);

  tp_proxy_dbus_error_to_gerror (TP_PROXY (request),
    error, message, &err);

  request_ctx_fail (ctx, err);
  request_ctx_free (ctx);
  g_error_free (err);
}

static void
channel_request_proceed_cb (TpChannelRequest *request,
  const GError *error,
  gpointer user_data,
  GObject *weak_object)
{
  request_ctx *ctx = user_data;

  if (error != NULL)
    {
      DEBUG ("Proceed failed: %s", error->message);

      request_ctx_fail (ctx, error);
      request_ctx_free (ctx);
      return;
    }

  DEBUG ("Proceed sucess; waiting for the channel to be handled");
}

static void
request_and_handle_channel_cb (TpChannelDispatcher *cd,
    const gchar *channel_request_path,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  request_ctx *ctx = user_data;
  GError *err = NULL;

  if (error != NULL)
    {
      DEBUG ("%s failed: %s", ctx->ensure ? "EnsureChannel" : "CreateChannel",
          error->message);

      request_ctx_fail (ctx, error);
      request_ctx_free (ctx);
      return;
    }

  DEBUG ("Got ChannelRequest: %s", channel_request_path);

  ctx->chan_request = tp_channel_request_new (ctx->dbus,
      channel_request_path, NULL, &err);

  if (ctx->chan_request == NULL)
    {
      DEBUG ("Failed to create ChannelRequest: %s", err->message);
      goto fail;
    }

  if (tp_cli_channel_request_connect_to_failed (ctx->chan_request,
      channel_request_failed_cb, ctx, NULL,
      G_OBJECT (ctx->result), &err) == NULL)
    {
      DEBUG ("Failed to connect the 'Failed' signal: %s", err->message);
      goto fail;
    }

  /* No need to connect the 'Succeeded' signal; we terminate the async call
   * once the handler has received the channel for handling. */

  DEBUG ("Calling ChannelRequest.Proceed()");

  tp_cli_channel_request_call_proceed (ctx->chan_request, -1,
      channel_request_proceed_cb, ctx, NULL, G_OBJECT (ctx->result));

  return;

fail:
  request_ctx_fail (ctx, err);
  request_ctx_free (ctx);
  g_error_free (err);
}

static void
request_and_handle_channel_async (TpAccount *account,
    GHashTable *request,
    gint64 user_action_time,
    GAsyncReadyCallback callback,
    gpointer user_data,
    gboolean ensure)
{
  GError *error = NULL;
  TpDBusDaemon *dbus;
  TpChannelDispatcher *cd;
  request_ctx *ctx;

  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (request != NULL);

  dbus = tp_proxy_get_dbus_daemon (account);

  ctx = request_ctx_new (dbus, ensure);

  /* Create a temp handler */
  ctx->handler = tp_simple_handler_new (dbus, TRUE, FALSE,
      "TpGlibTempHandler", TRUE, handle_channels, ctx, NULL);

  if (!tp_base_client_register (ctx->handler, &error))
    {
      DEBUG ("Failed to register temp handler: %s", error->message);

      g_simple_async_report_gerror_in_idle (G_OBJECT (account), callback,
          user_data, error);

      g_error_free (error);
      return;
    }

  cd = tp_channel_dispatcher_new (dbus);

  if (ensure)
    {
      /* FIXME */
    }
  else
    {
      ctx->result = g_simple_async_result_new (G_OBJECT (account),
          callback, user_data, tp_account_create_and_handle_channel_async);

      tp_cli_channel_dispatcher_call_create_channel (cd, -1,
          tp_proxy_get_object_path (account), request, user_action_time,
          tp_base_client_get_bus_name (ctx->handler),
          request_and_handle_channel_cb, ctx, NULL, NULL);
    }

  g_object_unref (cd);
}

/**
 * tp_account_create_and_handle_channel_async:
 * @account: a #TpAccount
 * @request: (transfer none) (element-type utf8 GObject.Value): the requested
 * properties of the channel
 * @user_action_time: the user action time to pass to the channel dispatcher
 * when requesting the channel
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Asynchronously calls CreateChannel on the ChannelDispatcher to create a
 * channel with the properties provided in @request that you are going to handle
 * yourself.
 * When the operation is finished, @callback will be called. You can then call
 * tp_account_create_and_handle_channel_finish () to get the result of
 * the operation.
 *
 * Since: 0.11.UNRELEASED
 */
void
tp_account_create_and_handle_channel_async (TpAccount *account,
    GHashTable *request,
    gint64 user_action_time,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  request_and_handle_channel_async (account, request, user_action_time,
      callback, user_data, FALSE);
}

static gboolean
request_and_handle_channel_finish (TpAccount *account,
    GAsyncResult *result,
    TpChannel **channel,
    gpointer source_tag,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TP_IS_ACCOUNT (account), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (channel != NULL, FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
          G_OBJECT (account), source_tag),
      FALSE);

  *channel = g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));

  return TRUE;
}

/**
 * tp_account_create_and_handle_channel_finish:
 * @account: a #TpAccount
 * @result: a #GAsyncResult
 * @channel: (out): a pointer used to return a reference on the newly created
 * #TpChannel having #TP_CHANNEL_FEATURE_CORE prepared if possible
 * @error: a #GError to fill
 *
 * Finishes an async channel creation started using
 * tp_account_create_and_handle_channel_async().
 *
 * Returns: %TRUE if the channel was successful created and you are handling
 * it, otherwise %FALSE
 *
 * Since: 0.11.UNRELEASED
 */
gboolean
tp_account_create_and_handle_channel_finish (TpAccount *account,
    GAsyncResult *result,
    TpChannel **channel,
    GError **error)
{
  return request_and_handle_channel_finish (account, result, channel,
      tp_account_create_and_handle_channel_async, error);
}
