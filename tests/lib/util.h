/* Simple utility code used by the regression tests.
 *
 * Copyright © 2008-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef TEST_LIB_UTIL_H
#define TEST_LIB_UTIL_H

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/base-connection.h>

TpDBusDaemon *test_dbus_daemon_dup_or_die (void);

void test_proxy_run_until_dbus_queue_processed (gpointer proxy);

#define test_connection_run_until_dbus_queue_processed(c) \
  (test_proxy_run_until_dbus_queue_processed (c))

TpHandle test_connection_run_request_contact_handle (TpConnection *connection,
    const gchar *id);

void test_proxy_run_until_prepared (gpointer proxy,
    const GQuark *features);
gboolean test_proxy_run_until_prepared_or_failed (gpointer proxy,
    const GQuark *features,
    GError **error);

void test_connection_run_until_ready (TpConnection *conn);
void test_connection_manager_run_until_ready (TpConnectionManager *cm);
void test_connection_manager_run_until_readying_fails (TpConnectionManager *cm,
    GError **error);

#define test_assert_no_error(e) _test_assert_no_error (e, __FILE__, __LINE__)

void _test_assert_no_error (const GError *error, const char *file, int line);

#define test_assert_empty_strv(strv) \
  _test_assert_empty_strv (__FILE__, __LINE__, strv)
void _test_assert_empty_strv (const char *file, int line, gconstpointer strv);

#define test_assert_strv_equals(actual, expected) \
  _test_assert_strv_equals (__FILE__, __LINE__, \
      #actual, actual, \
      #expected, expected)
void _test_assert_strv_equals (const char *file, int line,
  const char *actual_desc, gconstpointer actual_strv,
  const char *expected_desc, gconstpointer expected_strv);

#define test_clear_object(op) \
  G_STMT_START \
    { \
      gpointer _test_clear_object_obj = *(op); \
      \
      *(op) = NULL; \
      \
      if (_test_clear_object_obj != NULL) \
        g_object_unref (_test_clear_object_obj); \
    } \
  G_STMT_END


void test_create_and_connect_conn (GType conn_type,
    const gchar *account,
    TpBaseConnection **service_conn,
    TpConnection **client_conn);

gpointer test_object_new_static_class (GType type,
    ...) G_GNUC_NULL_TERMINATED;

#endif
