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
simple_client_init (SimpleClient *self)
{
}

static void
simple_client_class_init (SimpleClientClass *klass)
{
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
