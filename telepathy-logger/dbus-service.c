/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
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
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "config.h"
#include "dbus-service.h"

#include <glib.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/log-entry-text.h>
#include <telepathy-logger/log-manager.h>
#include <telepathy-logger/util.h>

#include <extensions/extensions.h>

#define DEBUG_FLAG TPL_DEBUG_DBUS_SERVICE
#include <telepathy-logger/debug.h>

static void tpl_logger_iface_init (gpointer iface, gpointer iface_data);

#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplDBusService)
struct _TplDBusServicePriv
{
  TplLogManager *manager;
};

G_DEFINE_TYPE_WITH_CODE (TplDBusService, tpl_dbus_service, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_SVC_LOGGER, tpl_logger_iface_init));

static void
tpl_dbus_service_class_init (TplDBusServiceClass *klass)
{
  GObjectClass* object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (object_class, sizeof (TplDBusServicePriv));
}


static void
tpl_dbus_service_init (TplDBusService *self)
{
  TplDBusServicePriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_DBUS_SERVICE, TplDBusServicePriv);

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));

  self->priv = priv;
  priv->manager = tpl_log_manager_dup_singleton ();
}


TplDBusService *
tpl_dbus_service_new (void)
{
  return g_object_new (TPL_TYPE_DBUS_SERVICE, NULL);
}


typedef struct
{
  TplDBusService *self;
  TpAccount *account;
  char *identifier;
  gboolean is_chatroom;
  guint lines;
  DBusGMethodInvocation *context;
  GPtrArray *packed;
  GList *dates, *ptr;
} RecentMessagesContext;

static void _lookup_next_date (RecentMessagesContext *ctx);

static void
_get_messages_return (GObject *manager,
    GAsyncResult *res,
    gpointer user_data)
{
  RecentMessagesContext *ctx = user_data;
  GList *messages, *ptr;
  GError *error = NULL;

  messages = tpl_log_manager_get_messages_for_date_async_finish (res, &error);
  if (error != NULL)
    {
      DEBUG ("Failed to get messages: %s", error->message);

      g_clear_error (&error);
      messages = NULL; /* just to be sure */
    }

  /* from the most recent message, backward */
  for (ptr = g_list_last (messages);
       ptr != NULL && ctx->lines > 0;
       ptr = g_list_previous (ptr))
    {
      TplLogEntry *log = ptr->data;
      const char *message = tpl_log_entry_text_get_message (
          TPL_LOG_ENTRY_TEXT (log));
      const char *sender = tpl_contact_get_identifier (
          tpl_log_entry_text_get_sender (TPL_LOG_ENTRY_TEXT (log)));
      gint64 timestamp = tpl_log_entry_get_timestamp (log);

      DEBUG ("Message: %" G_GINT64_FORMAT " <%s> %s",
          timestamp, sender, message);

      g_ptr_array_add (ctx->packed, tp_value_array_build (3,
          G_TYPE_STRING, sender,
          G_TYPE_STRING, message,
          G_TYPE_INT64, timestamp,
          G_TYPE_INVALID));

      ctx->lines--;
    }

  g_list_foreach (messages, (GFunc) g_object_unref, NULL);
  g_list_free (messages);

  _lookup_next_date (ctx);
}


static void
_lookup_next_date (RecentMessagesContext *ctx)
{
  TplDBusServicePriv *priv = GET_PRIV (ctx->self);

  if (ctx->ptr != NULL && ctx->lines > 0)
    {
      char *date = ctx->ptr->data;

      DEBUG ("Looking up date %s", date);

      tpl_log_manager_get_messages_for_date_async (priv->manager,
          ctx->account, ctx->identifier, ctx->is_chatroom, date,
          _get_messages_return, ctx);

      ctx->ptr = g_list_previous (ctx->ptr);
    }
  else
    {
      /* return and release */
      DEBUG ("complete, returning");

      g_list_foreach (ctx->dates, (GFunc) g_free, NULL);
      g_list_free (ctx->dates);

      tpl_svc_logger_return_from_get_recent_messages (ctx->context,
          ctx->packed);

      g_ptr_array_free (ctx->packed, TRUE);
      g_free (ctx->identifier);
      g_object_unref (ctx->account);
      g_slice_free (RecentMessagesContext, ctx);
    }
}


static void
_get_dates_return (GObject *manager,
    GAsyncResult *res,
    gpointer user_data)
{
  RecentMessagesContext *ctx = user_data;
  GError *error = NULL;

  ctx->dates = tpl_log_manager_get_dates_async_finish (res, &error);
  if (ctx->dates == NULL)
    {
      DEBUG ("Failed to get dates: %s", error->message);

      dbus_g_method_return_error (ctx->context, error);

      g_clear_error (&error);

      g_free (ctx->identifier);
      g_object_unref (ctx->account);
      g_slice_free (RecentMessagesContext, ctx);

      return;
    }

  ctx->ptr = g_list_last (ctx->dates);
  ctx->packed = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_value_array_free);

  _lookup_next_date (ctx);
}


static void
tpl_dbus_service_get_recent_messages (TplSvcLogger *self,
    const gchar *account_path,
    const gchar *identifier,
    gboolean is_chatroom,
    guint lines,
    DBusGMethodInvocation *context)
{
  TplDBusServicePriv *priv = GET_PRIV (self);
  TpDBusDaemon *tp_dbus;
  TpAccount *account;
  RecentMessagesContext *ctx;
  GError *error = NULL;

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));
  g_return_if_fail (context != NULL);

  tp_dbus = tp_dbus_daemon_dup (&error);
  if (tp_dbus == NULL)
    {
      DEBUG ("Unable to acquire the bus daemon: %s", error->message);
      dbus_g_method_return_error (context, error);
      goto out;
    }

  account = tp_account_new (tp_dbus, account_path, &error);
  if (account == NULL)
    {
      DEBUG ("Unable to acquire the account for %s: %s", account_path,
          error->message);
      dbus_g_method_return_error (context, error);
      goto out;
    }

  ctx = g_slice_new (RecentMessagesContext);
  ctx->self = TPL_DBUS_SERVICE (self);
  ctx->account = account;
  ctx->identifier = g_strdup (identifier);
  ctx->is_chatroom = is_chatroom;
  ctx->lines = lines;
  ctx->context = context;

  tpl_log_manager_get_dates_async (priv->manager,
      account, identifier, is_chatroom,
      _get_dates_return, ctx);

out:

  if (tp_dbus != NULL)
    g_object_unref (tp_dbus);

  g_clear_error (&error);
}


static void
tpl_logger_iface_init (gpointer iface,
    gpointer iface_data)
{
  TplSvcLoggerClass *klass = (TplSvcLoggerClass *) iface;

#define IMPLEMENT(x) tpl_svc_logger_implement_##x (klass, tpl_dbus_service_##x)
  IMPLEMENT (get_recent_messages);
#undef IMPLEMENT
}
