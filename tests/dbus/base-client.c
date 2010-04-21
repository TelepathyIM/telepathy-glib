/* Tests of TpBaseClient
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/base-client.h>
#include <telepathy-glib/client.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>

#include "tests/lib/util.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    TpBaseClient *base_client;
    /* client side object */
    TpClient *client;

    GError *error /* initialized where needed */;
    GStrv interfaces;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = test_dbus_daemon_dup_or_die ();
  g_assert (test->dbus != NULL);

  test->base_client = tp_base_client_new (test->dbus, "Test", FALSE);
  g_assert (test->base_client);

  test->client = g_object_new (TP_TYPE_CLIENT,
          "dbus-daemon", test->dbus,
          "dbus-connection", ((TpProxy *) test->dbus)->dbus_connection,
          "bus-name", "org.freedesktop.Telepathy.Client.Test",
          "object-path", "/org/freedesktop/Telepathy/Client/Test",
          NULL);

  g_assert (test->client != NULL);

  test->error = NULL;
  test->interfaces = NULL;
}

static void
teardown (Test *test,
          gconstpointer data)
{
  if (test->base_client != NULL)
    {
      g_object_unref (test->base_client);
      test->base_client = NULL;
    }

  if (test->client != NULL)
    {
      g_object_unref (test->client);
      test->client = NULL;
    }

  g_object_unref (test->dbus);
  test->dbus = NULL;
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  if (test->interfaces != NULL)
    {
      g_strfreev (test->interfaces);
      test->interfaces = NULL;
    }
}

static void
test_basis (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  TpDBusDaemon *dbus;
  gchar *name;
  gboolean unique;

  g_object_get (test->base_client,
      "dbus-daemon", &dbus,
      "name", &name,
      "uniquify-name", &unique,
      NULL);

  g_assert (test->dbus == dbus);
  g_assert_cmpstr ("Test", ==, name);
  g_assert (!unique);

  g_object_unref (dbus);
  g_free (name);
}

static void
get_client_prop_cb (TpProxy *proxy,
    GHashTable *properties,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  Test *test = user_data;

  if (error != NULL)
    {
      test->error = g_error_copy (error);
      goto out;
    }

  g_assert_cmpint (g_hash_table_size (properties), == , 1);

  test->interfaces = g_strdupv ((GStrv) tp_asv_get_strv (
        properties, "Interfaces"));

out:
  g_main_loop_quit (test->mainloop);
}

static void
test_register (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  tp_base_client_be_a_handler (test->base_client);

  /* Client is not registered yet */
  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_error (test->error, DBUS_GERROR, DBUS_GERROR_SERVICE_UNKNOWN);
  g_error_free (test->error);
  test->error = NULL;

  tp_base_client_register (test->base_client);

  tp_cli_dbus_properties_call_get_all (test->client, -1,
      TP_IFACE_CLIENT, get_client_prop_cb, test, NULL, NULL);
  g_main_loop_run (test->mainloop);

  g_assert_no_error (test->error);
}

int
main (int argc,
      char **argv)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/base-client/basis", Test, NULL, setup, test_basis, teardown);
  g_test_add ("/base-client/register", Test, NULL, setup, test_register,
      teardown);

  return g_test_run ();
}
