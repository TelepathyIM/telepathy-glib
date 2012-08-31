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
disconnect_conn (Test *test)
{
  if (test->connection == NULL)
    return;

  tp_tests_connection_assert_disconnect_succeeds (test->connection);
  tp_clear_object (&test->connection);
  tp_clear_object (&test->base_connection);
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

  disconnect_conn (test);
}

static void
test_creation (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_assert (TP_IS_TLS_CERTIFICATE (test->cert));
}

static void
proxy_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_proxy_prepare_finish (source, result, &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
prepare_cert (Test *test,
    TpTLSCertificate *cert)
{
  GQuark features[] = { TP_TLS_CERTIFICATE_FEATURE_CORE, 0 };

  tp_proxy_prepare_async (cert, features, proxy_prepare_cb, test);

  test->wait = 1;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);
}

static void
test_core (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GPtrArray *cert_data;
  GBytes *d;

  /* Properties are not valid yet */
  g_assert_cmpstr (tp_tls_certificate_get_cert_type (test->cert), ==, NULL);
  g_assert (tp_tls_certificate_get_cert_data (test->cert) == NULL);
  g_assert_cmpuint (tp_tls_certificate_get_state (test->cert), ==,
      TP_TLS_CERTIFICATE_STATE_PENDING);

  prepare_cert (test, test->cert);

  g_assert_cmpstr (tp_tls_certificate_get_cert_type (test->cert), ==, "x509");
  g_assert_cmpuint (tp_tls_certificate_get_state (test->cert), ==,
      TP_TLS_CERTIFICATE_STATE_PENDING);

  cert_data = tp_tls_certificate_get_cert_data (test->cert);
  g_assert (cert_data != NULL);
  g_assert_cmpuint (cert_data->len, ==, 1);
  d = g_ptr_array_index (cert_data, 0);
  tp_tests_assert_bytes_equals (d, "BADGER", 6);
}

static void
notify_cb (GObject *object,
    GParamSpec *spec,
    Test *test)
{
  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
accept_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_tls_certificate_accept_finish (TP_TLS_CERTIFICATE (source), result,
      &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_accept (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_signal_connect (test->cert, "notify::state",
      G_CALLBACK (notify_cb), test);

  tp_tls_certificate_accept_async (test->cert, accept_cb, test);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (tp_tls_certificate_get_state (test->cert), ==,
      TP_TLS_CERTIFICATE_STATE_ACCEPTED);
}

static void
reject_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  Test *test = user_data;

  tp_tls_certificate_reject_finish (TP_TLS_CERTIFICATE (source), result,
      &test->error);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_reject (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GVariant *details;
  const GError *error;
  TpTLSCertificateRejectReason reason;
  const gchar *dbus_error;
  gboolean enabled;
  TpTLSCertificate *cert;
  TpTLSCertificateRejection *rej;
  GError *err = NULL;

  g_signal_connect (test->cert, "notify::state",
      G_CALLBACK (notify_cb), test);


  tp_tls_certificate_add_rejection (test->cert,
      TP_TLS_CERTIFICATE_REJECT_REASON_REVOKED, NULL,
      g_variant_new_parsed ("{ 'user-requested': <%b> }", TRUE));
  tp_tls_certificate_add_rejection (test->cert,
      TP_TLS_CERTIFICATE_REJECT_REASON_UNKNOWN,
      TP_ERROR_STR_CAPTCHA_NOT_SUPPORTED, NULL);

  tp_tls_certificate_reject_async (test->cert, reject_cb, test);

  test->wait = 2;
  g_main_loop_run (test->mainloop);
  g_assert_no_error (test->error);

  g_assert_cmpuint (tp_tls_certificate_get_state (test->cert), ==,
      TP_TLS_CERTIFICATE_STATE_REJECTED);

  rej = tp_tls_certificate_get_rejection (test->cert);
  g_assert (TP_IS_TLS_CERTIFICATE_REJECTION (rej));
  error = tp_tls_certificate_rejection_get_error (rej);
  dbus_error = tp_tls_certificate_rejection_get_dbus_error (rej);
  reason = tp_tls_certificate_rejection_get_reason (rej);
  details = tp_tls_certificate_rejection_get_details (rej);

  g_assert_error (error, TP_ERROR, TP_ERROR_CERT_REVOKED);
  g_assert_cmpstr (dbus_error, ==, TP_ERROR_STR_CERT_REVOKED);
  g_assert_cmpuint (reason, ==, TP_TLS_CERTIFICATE_REJECT_REASON_REVOKED);
  g_assert (g_variant_is_of_type (details, G_VARIANT_TYPE_VARDICT));
  g_assert_cmpuint (g_variant_n_children (details), ==, 1);
  g_assert (g_variant_lookup (details, "user-requested", "b", &enabled));
  g_assert (enabled);

  g_assert (!tp_tls_certificate_rejection_raise_error (rej, &err));
  g_assert_error (err, TP_ERROR, TP_ERROR_CERT_REVOKED);
  g_error_free (err);

  rej = tp_tls_certificate_get_nth_rejection (test->cert, 1);
  g_assert (TP_IS_TLS_CERTIFICATE_REJECTION (rej));
  error = tp_tls_certificate_rejection_get_error (rej);
  dbus_error = tp_tls_certificate_rejection_get_dbus_error (rej);
  details = tp_tls_certificate_rejection_get_details (rej);

  g_assert_error (error, TP_ERROR, TP_ERROR_CAPTCHA_NOT_SUPPORTED);
  g_assert_cmpstr (dbus_error, ==, TP_ERROR_STR_CAPTCHA_NOT_SUPPORTED);
  g_assert (g_variant_is_of_type (details, G_VARIANT_TYPE_VARDICT));
  g_assert_cmpuint (g_variant_n_children (details), ==, 0);

  /* Test if we cope with an empty rejections list */
  tp_tests_tls_certificate_clear_rejection (test->service_cert);

  cert = tp_tls_certificate_new (TP_PROXY (test->connection),
      tp_proxy_get_object_path (test->cert), &test->error);
  g_assert_no_error (test->error);

  prepare_cert (test, cert);

  rej = tp_tls_certificate_get_rejection (cert);
  g_assert (TP_IS_TLS_CERTIFICATE_REJECTION (rej));
  error = tp_tls_certificate_rejection_get_error (rej);
  dbus_error = tp_tls_certificate_rejection_get_dbus_error (rej);
  details = tp_tls_certificate_rejection_get_details (rej);

  g_assert_error (error, TP_ERROR, TP_ERROR_CERT_INVALID);
  g_assert_cmpstr (dbus_error, ==, TP_ERROR_STR_CERT_INVALID);
  g_assert (g_variant_is_of_type (details, G_VARIANT_TYPE_VARDICT));
  g_assert_cmpuint (g_variant_n_children (details), ==, 0);

  g_object_unref (cert);
}

static void
invalidated_cb (TpProxy *cert,
    guint domain,
    gint code,
    const gchar *message,
    Test *test)
{
  g_clear_error (&test->error);
  test->error = g_error_new_literal (domain, code, message);

  test->wait--;
  if (test->wait <= 0)
    g_main_loop_quit (test->mainloop);
}

static void
test_invalidated (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  g_signal_connect (test->cert, "invalidated",
      G_CALLBACK (invalidated_cb), test);

  disconnect_conn (test);

  g_assert_error (test->error, TP_ERROR, TP_ERROR_CANCELLED);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/tls-certificate/creation", Test, NULL, setup,
      test_creation, teardown);
  g_test_add ("/tls-certificate/core", Test, NULL, setup,
      test_core, teardown);
  g_test_add ("/tls-certificate/accept", Test, NULL, setup,
      test_accept, teardown);
  g_test_add ("/tls-certificate/reject", Test, NULL, setup,
      test_reject, teardown);
  g_test_add ("/tls-certificate/invalidated", Test, NULL, setup,
      test_invalidated, teardown);

  return g_test_run ();
}
