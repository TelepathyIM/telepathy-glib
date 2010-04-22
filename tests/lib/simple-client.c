/*
 * simple-client.c - a simple client
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "simple-client.h"

#include <string.h>

#include <dbus/dbus-glib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/handle-repo-dynamic.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

G_DEFINE_TYPE (SimpleClient, simple_client, TP_TYPE_BASE_CLIENT)

static void
simple_observe_channels (
    TpBaseClient *client,
    const gchar *account,
    const gchar *connection,
    const GPtrArray *channels,
    const gchar *dispatch_operation,
    const GPtrArray *requests,
    TpObserveChannelsContext *context)
{
  SimpleClient *self = SIMPLE_CLIENT (client);

  if (!tp_strdiff (account, "/INVALID"))
    {
      GError error = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Invalid account" };

      tp_observe_channels_context_fail (context, &error);
      return;
    }

  if (self->observe_ctx != NULL)
    g_object_unref (self->observe_ctx);

  self->observe_ctx = g_object_ref (context);
  tp_observe_channels_context_accept (context);
}

static void
simple_client_init (SimpleClient *self)
{
}

static void
simple_client_dispose (GObject *object)
{
  SimpleClient *self = SIMPLE_CLIENT (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (simple_client_parent_class)->dispose;

  if (self->observe_ctx != NULL)
    {
      g_object_unref (self->observe_ctx);
      self->observe_ctx = NULL;
    }

  if (dispose != NULL)
    dispose (object);
}

static void
simple_client_class_init (SimpleClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TpBaseClientClass *base_class = TP_BASE_CLIENT_CLASS (klass);

  object_class->dispose = simple_client_dispose;

  tp_base_client_implement_observe_channels (base_class,
      simple_observe_channels);
}

SimpleClient *
simple_client_new (TpDBusDaemon *dbus_daemon,
    const gchar *name,
    gboolean uniquify_name)
{
  return g_object_new (SIMPLE_TYPE_CLIENT,
      "dbus-daemon", dbus_daemon,
      "name", name,
      "uniquify-name", uniquify_name,
      NULL);
}
