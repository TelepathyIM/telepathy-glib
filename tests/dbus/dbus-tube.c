/* Tests of TpDbusTubeChannel
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <string.h>

#include <telepathy-glib/dbus-tube-channel.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/dbus.h>

#include "tests/lib/util.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/dbus-tube-chan.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    TpTestsDBusTubeChannel *tube_chan_service;
    TpHandleRepoIface *contact_repo;
    TpHandleRepoIface *room_repo;

    /* Client side objects */
    TpConnection *connection;
    TpDBusTubeChannel *tube;

    GError *error /* initialized where needed */;
    gint wait;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->error = NULL;

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  tp_clear_object (&test->tube_chan_service);
  tp_clear_object (&test->tube);

  tp_cli_connection_run_disconnect (test->connection, -1, &test->error, NULL);
  g_assert_no_error (test->error);

  g_object_unref (test->connection);
  g_object_unref (test->base_connection);
}

static void
create_tube_service (Test *test,
    gboolean requested,
    gboolean contact)
{
  gchar *chan_path;
  TpHandle handle, alf_handle;
  GHashTable *props;
  GType type;
  TpSimpleClientFactory *factory;

  /* If previous tube is still preparing, refs are kept on it. We want it to be
   * destroyed now otherwise it will get reused by the factory. */
  if (test->tube != NULL)
    tp_tests_proxy_run_until_prepared (test->tube, NULL);

  tp_clear_object (&test->tube_chan_service);
  tp_clear_object (&test->tube);

  /* Create service-side tube channel object */
  chan_path = g_strdup_printf ("%s/Channel",
      tp_proxy_get_object_path (test->connection));

  test->contact_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_CONTACT);
  g_assert (test->contact_repo != NULL);

  test->room_repo = tp_base_connection_get_handles (test->base_connection,
      TP_HANDLE_TYPE_ROOM);
  g_assert (test->room_repo != NULL);

  if (contact)
    {
      handle = tp_handle_ensure (test->contact_repo, "bob", NULL, &test->error);
      type = TP_TESTS_TYPE_CONTACT_DBUS_TUBE_CHANNEL;
    }
  else
    {
      handle = tp_handle_ensure (test->room_repo, "#test", NULL, &test->error);
      type = TP_TESTS_TYPE_ROOM_DBUS_TUBE_CHANNEL;
    }

  g_assert_no_error (test->error);

  alf_handle = tp_handle_ensure (test->contact_repo, "alf", NULL, &test->error);
  g_assert_no_error (test->error);

  test->tube_chan_service = g_object_new (
      type,
      "connection", test->base_connection,
      "handle", handle,
      "requested", requested,
      "object-path", chan_path,
      "initiator-handle", alf_handle,
      NULL);

  /* Create client-side tube channel object */
  g_object_get (test->tube_chan_service, "channel-properties", &props, NULL);

  factory = tp_proxy_get_factory (test->connection);
  test->tube = (TpDBusTubeChannel *) tp_simple_client_factory_ensure_channel (
      factory, test->connection, chan_path, props, &test->error);
  g_assert (TP_IS_DBUS_TUBE_CHANNEL (test->tube));

  g_assert_no_error (test->error);

  g_free (chan_path);
  g_hash_table_unref (props);

  if (contact)
    tp_handle_unref (test->contact_repo, handle);
  else
    tp_handle_unref (test->room_repo, handle);
}

/* Test Basis */

static void
test_creation (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  create_tube_service (test, TRUE, TRUE);

  g_assert (TP_IS_DBUS_TUBE_CHANNEL (test->tube));
  g_assert (TP_IS_CHANNEL (test->tube));

  create_tube_service (test, FALSE, FALSE);

  g_assert (TP_IS_DBUS_TUBE_CHANNEL (test->tube));
  g_assert (TP_IS_CHANNEL (test->tube));
}

static void
check_parameters (GHashTable *parameters)
{
  g_assert (parameters != NULL);

  g_assert_cmpuint (g_hash_table_size (parameters), ==, 1);
  g_assert_cmpuint (tp_asv_get_uint32 (parameters, "badger", NULL), ==, 42);
}

static void
test_properties (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gchar *service;
  GHashTable *parameters;

  /* Outgoing tube */
  create_tube_service (test, TRUE, TRUE);

  /* Service */
  g_assert_cmpstr (tp_dbus_tube_channel_get_service_name (test->tube), ==,
      "com.test.Test");
  g_object_get (test->tube, "service-name", &service, NULL);
  g_assert_cmpstr (service, ==, "com.test.Test");
  g_free (service);

  /* Parameters */
  parameters = tp_dbus_tube_channel_get_parameters (test->tube);
  /* NULL as the tube has not be offered yet */
  g_assert (parameters == NULL);
  g_object_get (test->tube, "parameters", &parameters, NULL);
  g_assert (parameters == NULL);

  /* Incoming tube */
  create_tube_service (test, FALSE, FALSE);

  /* Parameters */
  parameters = tp_dbus_tube_channel_get_parameters (test->tube);
  check_parameters (parameters);
  g_object_get (test->tube, "parameters", &parameters, NULL);
  check_parameters (parameters);
  g_hash_table_unref (parameters);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/dbus-tube/creation", Test, NULL, setup, test_creation,
      teardown);
  g_test_add ("/dbus-tube/properties", Test, NULL, setup, test_properties,
      teardown);

  return g_test_run ();
}
