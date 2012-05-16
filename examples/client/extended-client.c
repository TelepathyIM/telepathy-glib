/*
 * telepathy-example-client-extended - use an extended connection manager
 *
 * Usage:
 *
 * telepathy-example-client-extended
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <stdio.h>

#include <telepathy-glib/telepathy-glib.h>

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
disconnect_cb (TpConnection *conn,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  if (die_if (error, "Disconnect()"))
    return;

  main_ret = 0;
  g_main_loop_quit (mainloop);
}

typedef struct {
    TpContact *contacts[2];
} ContactPair;

static void
got_hats_cb (TpConnection *conn,
    const GPtrArray *hats,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  guint i;

  if (die_if (error, "GetHats()"))
    return;

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

  tp_cli_connection_call_disconnect (conn, -1, disconnect_cb,
      NULL, NULL, NULL);
}

static void
set_hat_cb (TpConnection *conn,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  ContactPair *pair = user_data;
  GArray *handles = NULL;
  TpHandle handle;

  if (die_if (error, "SetHat()"))
    return;

  handles = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 2);
  handle = tp_contact_get_handle (pair->contacts[0]);
  g_array_append_val (handles, handle);
  handle = tp_contact_get_handle (pair->contacts[1]);
  g_array_append_val (handles, handle);

  example_cli_connection_interface_hats_call_get_hats (conn, -1,
      handles, got_hats_cb, NULL, NULL, NULL);
}

static void
contact_pair_free (gpointer p)
{
  ContactPair *pair = p;

  g_object_unref (pair->contacts[0]);
  g_object_unref (pair->contacts[1]);
  g_slice_free (ContactPair, pair);
}

static void
contact_ready_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpConnection *conn = (TpConnection *) object;
  GHashTable *asv;
  ContactPair *pair;
  GError *error = NULL;

  pair = g_slice_new0 (ContactPair);
  pair->contacts[0] = tp_connection_dup_contact_by_id_finish (conn,
      result, &error);
  pair->contacts[1] = g_object_ref (tp_connection_get_self_contact (conn));

  if (die_if (error, "tp_connection_dup_contact_by_id_async()"))
    {
      g_clear_error (&error);
      contact_pair_free (pair);
      return;
    }

  asv = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_hash_table_insert (asv, "previous-owner",
      tp_g_value_slice_new_static_string ("Shadowman"));

  example_cli_connection_interface_hats_call_set_hat (conn, -1,
      "red", EXAMPLE_HAT_STYLE_FEDORA, asv,
      set_hat_cb, pair, contact_pair_free, NULL);

  g_hash_table_unref (asv);
}

static void
conn_ready (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  GError *error = NULL;
  TpConnection *conn = TP_CONNECTION (source);

  if (!tp_proxy_prepare_finish (conn, result, &error))
    {
      g_warning ("%s", error->message);
      g_main_loop_quit (mainloop);
      g_clear_error (&error);
      return;
    }

  if (!tp_proxy_has_interface_by_id (conn,
        EXAMPLE_IFACE_QUARK_CONNECTION_INTERFACE_HATS))
    {
      g_warning ("Connection does not support Hats interface");
      g_main_loop_quit (mainloop);
      return;
    }

  /* Get contact object for someone else */
  tp_connection_dup_contact_by_id_async (conn, "other@server", 0, NULL,
      contact_ready_cb, NULL);
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
  TpSimpleClientFactory *factory;
  GError *e = NULL;
  TpConnection *conn;

  if (die_if (error, "RequestConnection()"))
    return;

  /* FIXME: there should be convenience API for this */
  factory = tp_simple_client_factory_new (NULL);
  conn = tp_simple_client_factory_ensure_connection (factory, object_path, NULL,
      &e);
  g_object_unref (factory);

  if (conn == NULL)
    {
      g_warning ("tp_connection_new(): %s", error->message);
      g_main_loop_quit (mainloop);
      return;
    }

  /* the connection hasn't had a chance to become invalid yet, so we can
   * assume that this signal connection will work */
  tp_cli_connection_connect_to_status_changed (conn, conn_status_changed,
      NULL, NULL, NULL, NULL);

  tp_proxy_prepare_async (conn, NULL, conn_ready, NULL);
  tp_cli_connection_call_connect (conn, -1, NULL, NULL, NULL, NULL);
}

static void
connection_manager_got_info (TpConnectionManager *cm,
                             guint source,
                             gpointer unused)
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

      g_hash_table_unref (params);
    }
}

static gboolean
time_out (gpointer unused)
{
  g_warning ("Timed out trying to get CM info");
  g_main_loop_quit (mainloop);
  return FALSE;
}

int
main (int argc,
      char **argv)
{
  TpConnectionManager *cm = NULL;
  GError *error = NULL;
  TpDBusDaemon *dbus = NULL;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  example_cli_init ();

  dbus = tp_dbus_daemon_dup (&error);

  if (dbus == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      goto out;
    }

  mainloop = g_main_loop_new (NULL, FALSE);

  cm = tp_connection_manager_new (dbus, "example_extended", NULL, &error);

  if (cm == NULL)
    {
      g_warning ("%s", error->message);
      goto out;
    }

  g_signal_connect (cm, "got-info",
      G_CALLBACK (connection_manager_got_info), NULL);

  timer = g_timeout_add (5000, time_out, NULL);

  g_main_loop_run (mainloop);

out:
  if (cm != NULL)
    g_object_unref (cm);

  if (dbus != NULL)
    g_object_unref (dbus);

  if (mainloop != NULL)
    g_main_loop_unref (mainloop);

  return main_ret;
}

