/* Tests of TpTLSCertificate
 *
 * Copyright © 2012 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>

#include "tests/lib/contacts-conn.h"
#include "tests/lib/tls-certificate.h"
#include "tests/lib/util.h"

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    /* Service side objects */
    TpBaseConnection *base_connection;
    TpTestsTLSCertificate *service_cert;

    /* Client side objects */
    TpConnection *connection;
    TpTLSCertificate *cert;

    GError *error /* initialized where needed */;
    gint wait;
} Test;


static void
setup (Test *test,
       gconstpointer data)
{
  gchar *path;
  GPtrArray *chain_data;
  GArray *cert;

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->error = NULL;

  /* Create (service and client sides) connection objects */
  tp_tests_create_and_connect_conn (TP_TESTS_TYPE_CONTACTS_CONNECTION,
      "me@test.com", &test->base_connection, &test->connection);

  path = g_strdup_printf ("%s/TlsCertificate",
      tp_proxy_get_object_path (test->connection));

  chain_data = g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);

  cert = g_array_new (TRUE, TRUE, sizeof (guchar));
  g_array_append_vals (cert, "BADGER", 6);
  g_ptr_array_add (chain_data, cert);

  test->service_cert = g_object_new (TP_TESTS_TYPE_TLS_CERTIFICATE,
      "object-path", path,
      "certificate-type", "x509",
      "certificate-chain-data", chain_data,
      "dbus-daemon", test->dbus,
      NULL);

  g_ptr_array_unref (chain_data);

  test->cert = tp_tls_certificate_new (TP_PROXY (test->connection), path,
      &test->error);
  g_assert_no_error (test->error);

  g_free (path);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  g_clear_error (&test->error);

  tp_clear_object (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;

  tp_clear_object (&test->service_cert);
  tp_clear_object (&test->cert);

  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  g_object_unref (test->connection);
  g_object_unref (test->base_connection);
}

static void
test_creation (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_assert (TP_IS_TLS_CERTIFICATE (test->cert));
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/tls-certificate/creation", Test, NULL, setup,
      test_creation, teardown);

  return g_test_run ();
}
