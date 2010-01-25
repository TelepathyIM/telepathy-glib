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

#include <glib.h>
#include <gio/gio.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/account.h>

#include <telepathy-logger/log-manager.h>

#define ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/gabble/jabber/cosimo_2ealfarano_40collabora_2eco_2euk0"
#define ID "echo@test.collabora.co.uk"

static GMainLoop *loop = NULL;

static
void cb(GObject *obj,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;
  GList *lst;

  lst = tpl_log_manager_async_operation_finish (result, &error);
  if (error != NULL)
    g_debug ("%s", (gchar*)error->message);

  for(;lst;lst=g_list_next (lst)) {
    g_debug ("LST: %s", (gchar*) lst->data);
  }
}

static void foo(TplLogEntry *f)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (f));

  if (tpl_log_entry_is_text() == TRUE)
    tpl_log_entry_text_some_op ( TPL_LOG_ENTRY_TEXT (f), ...);
  TPL_LOG_ENTRY_CALL (f)
}

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  TplLogManager *manager;
  TpDBusDaemon *dbus;
  TpAccount *acc;

  TplLogEntryText *t = tpl_log_entry_text_new ();

  g_type_init ();

  foo(TPL_LOG_ENTRY (t));

  g_debug ("FOOOO");
  dbus = tp_dbus_daemon_dup (&error);
  if (error != NULL) {
      g_debug ("%s", error->message);
  }
  g_debug ("FOOOO2");
  acc = tp_account_new (dbus, ACCOUNT_PATH, NULL);
  g_debug ("FOOOO3");
  manager = tpl_log_manager_dup_singleton ();

  g_debug ("FOOOO4");
  tpl_log_manager_get_dates_async (manager, acc, ID, FALSE, cb, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  return 0;
}
