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
#include <telepathy-logger/log-manager.h>

#include <telepathy-logger/datetime.h>

#define ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/gabble/jabber/cosimo_2ealfarano_40collabora_2eco_2euk0"
#define ID "echo@test.collabora.co.uk"

static GMainLoop *loop = NULL;

static void
get_messages_cb (TplLogManager * manager, gpointer result, GError * error,
		 gpointer user_data)
{
  guint len;
  if (result)
    len = g_list_length ((GList *) result);
  else
    len = 0;
  g_message ("GOTCHA: %d\n", len);

  if(error) {
	  g_error("get messages: %s", error->message);
	  return;
  }

  for (guint i = g_list_length (result); i > 0; --i)
    {
      TplLogEntry *entry = (TplLogEntry *) g_list_nth_data (result, i - 1);
      time_t t = tpl_log_entry_get_timestamp (entry);
      g_print ("LIST msgs(%d): %s\n", i,
	       tpl_time_to_string_utc (t, "%Y%m%d %H%M-%S"));
    }
}

static void
get_dates_cb (TplLogManager * manager, gpointer result, GError * error,
	      gpointer user_data)
{
  guint len;

  if(error) {
	  g_error("get dates: %s", error->message);
	  g_clear_error(&error);
	  g_error_free(error);
	  return;
  }

  if (result)
    len = g_list_length ((GList *) result);
  else
    len = 0;
  g_message ("GOTCHAi: %d\n", len);

  for (guint i = g_list_length (result); i > 0; --i)
    g_print ("LIST dates(%d): %s\n", i, (gchar *) g_list_nth_data (result, i - 1));
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  TpDBusDaemon *tpbus;
  TpAccount *acc;

  TplLogManager *manager = tpl_log_manager_dup_singleton ();

  tpbus = tp_dbus_daemon_dup (NULL);
  acc = tp_account_new (tpbus, ACCOUNT_PATH, NULL);

  tpl_log_manager_get_dates_async (manager, acc, ID, FALSE,
				   get_dates_cb, NULL, NULL);

  tpl_log_manager_get_messages_for_date_async (manager, acc, ID,
					       FALSE, "20091230",
					       get_messages_cb, NULL, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  return 0;
}
