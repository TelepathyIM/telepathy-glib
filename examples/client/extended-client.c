/*
 * telepathy-example-client-extended - use an extended connection manager
 *
 * Usage:
 *
 * telepathy-example-client-extended
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <stdio.h>

#include <telepathy-glib/connection.h>
#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/util.h>

/* Usually this'd be a top-level extensions/ directory in practice */
#include "examples/extensions/extensions.h"

static guint timer;
static int main_ret = 1;
static GMainLoop *mainloop;

static gboolean
die_if (const GError *error, const gchar *context)
{
  if (error != NULL)
    {
      g_warning ("%s: %s", context, error->message);
      g_main_loop_quit (mainloop);
      return TRUE;
    }

  return FALSE;
}

static void
conn_ready (TpConnection *conn,
            gpointer user_data)
{
  GError *error = NULL;
  GArray *handles;
  const gchar *names[] = { "myself@server", "other@server", NULL };
  GPtrArray *hats;
  guint i;
  GHashTable *asv;
  GValue *value;

  if (!tp_proxy_has_interface_by_id (conn,
        EXAMPLE_IFACE_QUARK_CONNECTION_INTERFACE_HATS))
    {
      g_warning ("Connection does not support Hats interface");
      g_main_loop_quit (mainloop);
      return;
    }

  /* Get handles for myself and someone else */

  tp_cli_connection_run_request_handles (conn, -1, TP_HANDLE_TYPE_CONTACT,
      names, &handles, &error);

  if (die_if (error, "RequestHandles()"))
    {
      g_error_free (error);
      return;
    }

  asv = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_static_string (value, "Shadowman");
  g_hash_table_insert (asv, "previous-owner",
      value);
  example_cli_connection_interface_hats_run_set_hat (conn, -1,
      "red", EXAMPLE_HAT_STYLE_FEDORA, asv, &error);
  g_hash_table_destroy (asv);

  if (die_if (error, "SetHat()"))
    {
      g_error_free (error);
      return;
    }

  example_cli_connection_interface_hats_run_get_hats (conn, -1,
      handles, &hats, &error);

  if (die_if (error, "GetHats()"))
    {
      g_error_free (error);
      return;
    }

  for (i = 0; i < hats->len; i++)
    {
      GValueArray *vals = g_ptr_array_index (hats, i);

      g_message ("Contact #%u has hat style %u, color \"%s\", with %u "
          "properties",
          g_value_get_uint (g_value_array_get_nth (vals, 0)),
          g_value_get_uint (g_value_array_get_nth (vals, 2)),
          g_value_get_string (g_value_array_get_nth (vals, 1)),
          g_hash_table_size (g_value_get_boxed (g_value_array_get_nth (vals,
                3))));
    }

  g_array_free (handles, TRUE);
  g_boxed_free (EXAMPLE_ARRAY_TYPE_CONTACT_HAT_LIST, hats);

  tp_cli_connection_run_disconnect (conn, -1, &error);

  if (die_if (error, "Disconnect()"))
    {
      g_error_free (error);
      return;
    }

  main_ret = 0;
  g_main_loop_quit (mainloop);
}

static void
conn_status_changed (TpConnection *conn,
                     guint status,
                     guint reason,
                     gpointer user_data,
                     GObject *weak_object)
{
  g_message ("Connection status changed to %u because %u", status, reason);

  if (status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      g_message ("Disconnected - exiting");
      g_main_loop_quit (mainloop);
    }
}

static void
cm_requested_connection (TpConnectionManager *manager,
                         const gchar *bus_name,
                         const gchar *object_path,
                         const GError *error,
                         gpointer user_data,
                         GObject *weak_object)
{
  GError *e = NULL;
  TpConnection *conn;
  TpProxy *proxy = (TpProxy *) manager;

  if (die_if (error, "RequestConnection()"))
    return;

  /* FIXME: there should be convenience API for this */
  conn = tp_connection_new (proxy->dbus_daemon,
      bus_name, object_path, &e);

  if (conn == NULL)
    {
      g_warning ("tp_connection_new(): %s", error->message);
      g_main_loop_quit (mainloop);
      return;
    }

  g_signal_connect (conn, "connection-ready", G_CALLBACK (conn_ready), NULL);
  /* the connection hasn't had a chance to become invalid yet, so we can
   * assume that this signal connection will work */
  tp_cli_connection_connect_to_status_changed (conn, conn_status_changed,
      NULL, NULL, NULL, NULL);
  tp_cli_connection_call_connect (conn, -1, NULL, NULL, NULL, NULL);
}

static void
connection_manager_got_info (TpConnectionManager *cm,
                             guint source,
                             GMainLoop *mainloop)
{
  g_message ("Emitted got-info (source=%d)", source);

  if (source > 0)
    {
      GHashTable *params;
      GValue value = { 0 };

      if (timer != 0)
        {
          g_source_remove (timer);
          timer = 0;
        }

      params = g_hash_table_new (g_str_hash, g_str_equal);
      g_value_init (&value, G_TYPE_STRING);
      g_value_set_static_string (&value, "myself@server");
      g_hash_table_insert (params, "account", &value);

      tp_cli_connection_manager_call_request_connection (cm,
          -1, "example", params, cm_requested_connection, NULL, NULL, NULL);

      g_hash_table_destroy (params);
    }
}

gboolean
time_out (gpointer mainloop)
{
  g_warning ("Timed out trying to get CM info");
  g_main_loop_quit (mainloop);
  return FALSE;
}

int
main (int argc,
      char **argv)
{
  TpConnectionManager *cm;
  GError *error;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  example_cli_init ();

  mainloop = g_main_loop_new (NULL, FALSE);

  cm = tp_connection_manager_new (tp_dbus_daemon_new (tp_get_bus ()),
      "example_extended", NULL, &error);

  if (cm == NULL)
    {
      g_warning ("%s", error->message);
      return 1;
    }

  g_signal_connect (cm, "got-info",
      G_CALLBACK (connection_manager_got_info), mainloop);

  timer = g_timeout_add (5000, time_out, mainloop);

  g_main_loop_run (mainloop);

  g_object_unref (cm);
  g_main_loop_unref (mainloop);
  return main_ret;
}

