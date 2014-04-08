/* Feature test for the user's self-handle/self-contact changing.
 *
 * Copyright (C) 2009-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-generic.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/debug.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

typedef struct {
    TpTestsContactsConnection parent;
    gboolean change_self_handle_after_get_all;
} MyConnection;

typedef struct {
    TpTestsContactsConnectionClass parent_class;
} MyConnectionClass;

static GType my_connection_get_type (void);

#define MY_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), my_connection_get_type (), MyConnection))

static void props_iface_init (TpSvcDBusPropertiesClass *);

G_DEFINE_TYPE_WITH_CODE (MyConnection, my_connection,
    TP_TESTS_TYPE_CONTACTS_CONNECTION,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES, props_iface_init))

static void
my_connection_init (MyConnection *self)
{
}

static void
my_connection_class_init (MyConnectionClass *cls)
{
}

static void
get (TpSvcDBusProperties *iface,
    const gchar *interface_name,
    const gchar *property_name,
    GDBusMethodInvocation *context)
{
  GObject *self = G_OBJECT (iface);
  GValue value = { 0 };
  GError *error = NULL;

  if (tp_dbus_properties_mixin_get (self, interface_name, property_name,
        &value, &error))
    {
      tp_svc_dbus_properties_return_from_get (context, &value);
      g_value_unset (&value);
    }
  else
    {
      g_dbus_method_invocation_return_gerror (context, error);
      g_error_free (error);
    }
}

static void
set (TpSvcDBusProperties *iface,
    const gchar *interface_name,
    const gchar *property_name,
    const GValue *value,
    GDBusMethodInvocation *context)
{
  GObject *self = G_OBJECT (iface);
  GError *error = NULL;

  if (tp_dbus_properties_mixin_set (self, interface_name, property_name, value,
          &error))
    {
      tp_svc_dbus_properties_return_from_set (context);
    }
  else
    {
      g_dbus_method_invocation_return_gerror (context, error);
      g_error_free (error);
    }
}

static void
get_all (TpSvcDBusProperties *iface,
    const gchar *interface_name,
    GDBusMethodInvocation *context)
{
  MyConnection *self = MY_CONNECTION (iface);
  TpBaseConnection *base = TP_BASE_CONNECTION (iface);
  GHashTable *values = tp_dbus_properties_mixin_dup_all (G_OBJECT (iface),
      interface_name);

  tp_svc_dbus_properties_return_from_get_all (context, values);
  g_hash_table_unref (values);

  if (self->change_self_handle_after_get_all &&
      tp_base_connection_get_status (base) == TP_CONNECTION_STATUS_CONNECTED)
    {
      TpTestsSimpleConnection *simple = TP_TESTS_SIMPLE_CONNECTION (iface);
      TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (base,
          TP_ENTITY_TYPE_CONTACT);

      DEBUG ("changing my own identifier to something else");
      self->change_self_handle_after_get_all = FALSE;
      tp_tests_simple_connection_set_identifier (simple, "myself@example.org");
      g_assert_cmpstr (tp_handle_inspect (contact_repo,
            tp_base_connection_get_self_handle (base)), ==,
          "myself@example.org");
    }
}

/* This relies on the assumption that every interface implemented by
 * TpTestsContactsConnection (or at least those exercised by this test)
 * is hooked up to TpDBusPropertiesMixin, which is true in practice. The
 * timing is quite subtle: to work as intended, test_change_inconveniently()
 * needs to change the self-handle *immediately* after the GetAll call. */
static void
props_iface_init (TpSvcDBusPropertiesClass *iface)
{
#define IMPLEMENT(x) \
  tp_svc_dbus_properties_implement_##x (iface, x)
  IMPLEMENT (get);
  IMPLEMENT (set);
  IMPLEMENT (get_all);
#undef IMPLEMENT
}

typedef struct {
  GDBusConnection *dbus;
  TpTestsSimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  gchar *name;
  gchar *conn_path;
  GError *error /* zero-initialized */;
  TpConnection *client_conn;
  TpHandleRepoIface *contact_repo;
  GAsyncResult *result;
} Fixture;

static void
setup (Fixture *f,
    gconstpointer arg)
{
  gboolean ok;

  f->dbus = tp_tests_dbus_dup_or_die ();

  f->service_conn = TP_TESTS_SIMPLE_CONNECTION (
      tp_tests_object_new_static_class (my_connection_get_type (),
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  f->service_conn_as_base = TP_BASE_CONNECTION (f->service_conn);
  g_object_ref (f->service_conn_as_base);
  g_assert (f->service_conn != NULL);
  g_assert (f->service_conn_as_base != NULL);

  f->contact_repo = tp_base_connection_get_handles (f->service_conn_as_base,
      TP_ENTITY_TYPE_CONTACT);

  ok = tp_base_connection_register (f->service_conn_as_base, "simple",
        &f->name, &f->conn_path, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  f->client_conn = tp_tests_connection_new (f->dbus, f->name, f->conn_path,
      &f->error);
  g_assert_no_error (f->error);
  g_assert (f->client_conn != NULL);

  if (!tp_strdiff (arg, "round-trip"))
    {
      /* Make sure preparing the self-contact requires a round-trip */
      TpClientFactory *factory = tp_proxy_get_factory (f->client_conn);

      tp_client_factory_add_contact_features_varargs (factory,
          TP_CONTACT_FEATURE_CAPABILITIES,
          0);
    }
}

static void
setup_and_connect (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  GQuark connected_feature[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  setup (f, unused);

  tp_cli_connection_call_connect (f->client_conn, -1, NULL, NULL, NULL, NULL);
  tp_tests_proxy_run_until_prepared (f->client_conn, connected_feature);
}

/* we'll get more arguments, but just ignore them */
static void
swapped_counter_cb (gpointer user_data)
{
  guint *times = user_data;

  ++*times;
}

static void
test_self_handle (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpContact *before, *after;
  guint contact_times = 0;

  g_signal_connect_swapped (f->client_conn, "notify::self-contact",
      G_CALLBACK (swapped_counter_cb), &contact_times);

  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "me@example.com");

  g_object_get (f->client_conn,
      "self-contact", &before,
      NULL);
  g_assert_cmpuint (tp_contact_get_handle (before), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpstr (tp_contact_get_identifier (before), ==, "me@example.com");

  g_assert_cmpuint (contact_times, ==, 0);

  /* similar to /nick in IRC */
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "myself@example.org");
  tp_tests_proxy_run_until_dbus_queue_processed (f->client_conn);

  while (contact_times < 1)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (contact_times, ==, 1);

  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "myself@example.org");

  g_object_get (f->client_conn,
      "self-contact", &after,
      NULL);
  g_assert (before != after);
  g_assert_cmpuint (tp_contact_get_handle (after), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpstr (tp_contact_get_identifier (after), ==,
      "myself@example.org");

  g_object_unref (before);
  g_object_unref (after);
}

static void
test_change_early (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  TpContact *after;
  guint contact_times = 0;
  gboolean ok;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  g_signal_connect_swapped (f->client_conn, "notify::self-contact",
      G_CALLBACK (swapped_counter_cb), &contact_times);

  tp_proxy_prepare_async (f->client_conn, features, tp_tests_result_ready_cb,
      &f->result);
  g_assert (f->result == NULL);

  /* act as though someone else called Connect; emit signals in quick
   * succession, so that by the time the TpConnection tries to investigate
   * the self-handle, it has already changed */
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTING,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "me@example.com");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "me@example.com");
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "myself@example.org");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "myself@example.org");

  /* now run the main loop and let the client catch up */
  tp_tests_run_until_result (&f->result);
  ok = tp_proxy_prepare_finish (f->client_conn, f->result, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  /* the self-handle and self-contact change once during connection */
  g_assert_cmpuint (contact_times, ==, 1);

  g_object_get (f->client_conn,
      "self-contact", &after,
      NULL);
  g_assert_cmpuint (tp_contact_get_handle (after), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpstr (tp_contact_get_identifier (after), ==,
      "myself@example.org");

  g_object_unref (after);
}

static void
test_change_inconveniently (Fixture *f,
    gconstpointer arg)
{
  TpContact *after;
  guint contact_times = 0;
  gboolean ok;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };

  MY_CONNECTION (f->service_conn)->change_self_handle_after_get_all = TRUE;

  /* This test exercises what happens if the self-contact changes
   * between obtaining its handle for the first time and having the
   * TpContact fully prepared. In Telepathy 1.0, that can only happen
   * if you are preparing non-core features, because we get the self-handle
   * and the self-ID at the same time. */
  g_assert_cmpstr (arg, ==, "round-trip");

  g_signal_connect_swapped (f->client_conn, "notify::self-contact",
      G_CALLBACK (swapped_counter_cb), &contact_times);

  tp_proxy_prepare_async (f->client_conn, features, tp_tests_result_ready_cb,
      &f->result);
  g_assert (f->result == NULL);

  /* act as though someone else called Connect */
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTING,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
  tp_tests_simple_connection_set_identifier (f->service_conn,
      "me@example.com");
  g_assert_cmpstr (tp_handle_inspect (f->contact_repo,
        tp_base_connection_get_self_handle (f->service_conn_as_base)), ==,
      "me@example.com");
  tp_base_connection_change_status (f->service_conn_as_base,
      TP_CONNECTION_STATUS_CONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);

  /* now run the main loop and let the client catch up */
  tp_tests_run_until_result (&f->result);
  ok = tp_proxy_prepare_finish (f->client_conn, f->result, &f->error);
  g_assert_no_error (f->error);
  g_assert (ok);

  /* the self-contact changes once during connection */
  g_assert_cmpuint (contact_times, ==, 1);

  g_assert_cmpuint (
      tp_contact_get_handle (tp_connection_get_self_contact (f->client_conn)),
      ==, tp_base_connection_get_self_handle (f->service_conn_as_base));

  g_object_get (f->client_conn,
      "self-contact", &after,
      NULL);
  g_assert_cmpuint (tp_contact_get_handle (after), ==,
      tp_base_connection_get_self_handle (f->service_conn_as_base));
  g_assert_cmpstr (tp_contact_get_identifier (after), ==,
      "myself@example.org");

  g_object_unref (after);
}

static void
teardown (Fixture *f,
    gconstpointer unused G_GNUC_UNUSED)
{
  g_clear_error (&f->error);

  if (f->client_conn != NULL)
    tp_tests_connection_assert_disconnect_succeeds (f->client_conn);

  tp_clear_object (&f->result);
  tp_clear_object (&f->client_conn);
  tp_clear_object (&f->service_conn_as_base);
  tp_clear_object (&f->service_conn);
  tp_clear_pointer (&f->name, g_free);
  tp_clear_pointer (&f->conn_path, g_free);
  tp_clear_object (&f->dbus);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_set_prgname ("self-handle");
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/self-handle", Fixture, NULL, setup_and_connect,
      test_self_handle, teardown);
  g_test_add ("/self-handle/round-trip", Fixture, "round-trip",
      setup_and_connect, test_self_handle, teardown);
  g_test_add ("/self-handle/change-early", Fixture, NULL, setup,
      test_change_early, teardown);
  g_test_add ("/self-handle/change-early/round-trip", Fixture, "round-trip",
      setup, test_change_early, teardown);
  g_test_add ("/self-handle/change-inconveniently", Fixture,
      "round-trip", setup, test_change_inconveniently, teardown);

  return tp_tests_run_with_bus ();
}
