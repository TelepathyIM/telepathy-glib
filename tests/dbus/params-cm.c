/* Test CM parameters
 *
 * Copyright © 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2009 Nokia Corporation
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
 */

#include "config.h"

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#include "tests/lib/util.h"
#include "tests/lib/params-cm.h"

#define CLEAR_OBJECT(o) \
  G_STMT_START { \
      if (*(o) != NULL) \
        { \
          g_object_unref (*(o)); \
          *(o) = NULL; \
        } \
  } G_STMT_END

typedef struct
{
  GMainLoop *mainloop;
  TpDBusDaemon *dbus;
  GError *error /* statically initialized to NULL */ ;

  TpTestsParamConnectionManager *service_cm;

  TpConnectionManager *cm;
  TpConnection *conn;
} Test;

static void
setup (Test *test,
       gconstpointer data G_GNUC_UNUSED)
{
  TpBaseConnectionManager *service_cm_as_base;
  gboolean ok;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->service_cm = TP_TESTS_PARAM_CONNECTION_MANAGER (
    tp_tests_object_new_static_class (
        TP_TESTS_TYPE_PARAM_CONNECTION_MANAGER,
        NULL));
  g_assert (test->service_cm != NULL);
  service_cm_as_base = TP_BASE_CONNECTION_MANAGER (test->service_cm);
  g_assert (service_cm_as_base != NULL);

  ok = tp_base_connection_manager_register (service_cm_as_base);
  g_assert (ok);

  test->cm = tp_connection_manager_new (test->dbus, "params_cm",
      NULL, &test->error);
  g_assert (test->cm != NULL);
  tp_tests_proxy_run_until_prepared (test->cm, NULL);
}

static void
teardown (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  CLEAR_OBJECT (&test->cm);
  CLEAR_OBJECT (&test->service_cm);

  CLEAR_OBJECT (&test->dbus);
  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
test_set_params (Test *test,
                 gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *parameters;
  TpTestsCMParams *params;
  gchar *array_of_strings[] = { "Telepathy", "rocks", "!", NULL };
  guint i;
  GArray *array_of_bytes;
  guint8 bytes[] = { 0x1, 0x10, 0xA, 0xB, 0xC };

  array_of_bytes = g_array_new (FALSE, FALSE, sizeof (guint8));
  g_array_append_vals (array_of_bytes, bytes, sizeof (bytes));

  parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_hash_table_insert (parameters, "a-string",
      tp_g_value_slice_new_static_string ("a string"));
  g_hash_table_insert (parameters, "a-int16",
      tp_g_value_slice_new_int (G_MININT16));
  g_hash_table_insert (parameters, "a-int32",
      tp_g_value_slice_new_int (G_MININT32));
  g_hash_table_insert (parameters, "a-uint16",
      tp_g_value_slice_new_uint (G_MAXUINT16));
  g_hash_table_insert (parameters, "a-uint32",
      tp_g_value_slice_new_uint (G_MAXUINT32));
  g_hash_table_insert (parameters, "a-int64",
      tp_g_value_slice_new_int64 (G_MAXINT64));
  g_hash_table_insert (parameters, "a-uint64",
      tp_g_value_slice_new_uint64 (G_MAXUINT64));
  g_hash_table_insert (parameters, "a-boolean",
      tp_g_value_slice_new_boolean (TRUE));
  g_hash_table_insert (parameters, "a-double",
      tp_g_value_slice_new_double (G_MAXDOUBLE));
  g_hash_table_insert (parameters, "a-array-of-strings",
      tp_g_value_slice_new_static_boxed (G_TYPE_STRV, array_of_strings));
  g_hash_table_insert (parameters, "a-array-of-bytes",
      tp_g_value_slice_new_boxed (DBUS_TYPE_G_UCHAR_ARRAY,
        array_of_bytes));
  g_hash_table_insert (parameters, "a-object-path",
      tp_g_value_slice_new_static_boxed (DBUS_TYPE_G_OBJECT_PATH,
        "/A/Object/Path"));
  g_hash_table_insert (parameters, "lc-string",
      tp_g_value_slice_new_static_string ("Filter Me"));
  g_hash_table_insert (parameters, "uc-string",
      tp_g_value_slice_new_static_string ("Filter Me"));

  tp_cli_connection_manager_run_request_connection (test->cm, -1,
      "example", parameters, NULL, NULL, &test->error, NULL);
  g_assert (test->error != NULL);
  g_assert (test->error->code == TP_ERROR_NOT_IMPLEMENTED);
  g_clear_error (&test->error);

  params = tp_tests_param_connection_manager_steal_params_last_conn ();
  g_assert (params != NULL);

  g_assert (!tp_strdiff (params->a_string, "a string"));
  g_assert_cmpint (params->a_int16, ==, G_MININT16);
  g_assert_cmpint (params->a_int32, ==, G_MININT32);
  g_assert_cmpuint (params->a_uint16, ==, G_MAXUINT16);
  g_assert_cmpuint (params->a_uint32, ==, G_MAXUINT32);
  g_assert_cmpuint (params->a_int64, ==, G_MAXINT64);
  g_assert_cmpuint (params->a_uint64, ==, G_MAXUINT64);
  g_assert (params->a_boolean);
  g_assert_cmpfloat (params->a_double, ==, G_MAXDOUBLE);

  g_assert_cmpuint (g_strv_length (params->a_array_of_strings), ==,
      g_strv_length (array_of_strings));
  for (i = 0; array_of_strings[i] != NULL; i++)
    g_assert (!tp_strdiff (params->a_array_of_strings[i], array_of_strings[i]));

  g_assert_cmpuint (params->a_array_of_bytes->len, ==, array_of_bytes->len);
  for (i = 0; i < array_of_bytes->len; i++)
    g_assert (params->a_array_of_bytes->data[i] == array_of_bytes->data[i]);

  g_assert (!tp_strdiff (params->a_object_path, "/A/Object/Path"));
  g_assert (!tp_strdiff (params->lc_string, "filter me"));
  g_assert (!tp_strdiff (params->uc_string, "FILTER ME"));

  tp_tests_param_connection_manager_free_params (params);
  g_hash_table_unref (parameters);
  g_array_unref (array_of_bytes);
}

static void
test_defaults (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *parameters;
  TpTestsCMParams *params;

  parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_hash_table_insert (parameters, "a-boolean",
      tp_g_value_slice_new_boolean (FALSE));

  tp_cli_connection_manager_run_request_connection (test->cm, -1,
      "example", parameters, NULL, NULL, &test->error, NULL);
  g_assert (test->error != NULL);
  g_assert_cmpint (test->error->code, ==, TP_ERROR_NOT_IMPLEMENTED);
  g_clear_error (&test->error);

  params = tp_tests_param_connection_manager_steal_params_last_conn ();
  g_assert (params->would_have_been_freed);
  g_assert_cmpstr (params->a_string, ==, "the default string");
  g_assert_cmpint (params->a_int16, ==, 42);
  g_assert_cmpint (params->a_int32, ==, 42);
  tp_tests_param_connection_manager_free_params (params);

  g_hash_table_unref (parameters);
}

static void
test_missing_required (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *parameters;
  TpTestsCMParams *params;

  parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

  tp_cli_connection_manager_run_request_connection (test->cm, -1,
      "example", parameters, NULL, NULL, &test->error, NULL);
  g_assert (test->error != NULL);
  g_assert_cmpint (test->error->code, ==, TP_ERROR_INVALID_ARGUMENT);
  g_clear_error (&test->error);

  params = tp_tests_param_connection_manager_steal_params_last_conn ();

  if (params != NULL)
    {
      g_assert (params->would_have_been_freed);
      tp_tests_param_connection_manager_free_params (params);
    }

  g_hash_table_unref (parameters);
}

static void
test_fail_filter (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *parameters;
  TpTestsCMParams *params;

  parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_hash_table_insert (parameters, "a-boolean",
      tp_g_value_slice_new_boolean (FALSE));
  /* The lc-string and uc-string parameters have a filter which rejects
   * anything outside ASCII, like these gratuitous umlauts */
  g_hash_table_insert (parameters, "uc-string",
      tp_g_value_slice_new_static_string ("M\xc3\xb6t\xc3\xb6rhead"));

  tp_cli_connection_manager_run_request_connection (test->cm, -1,
      "example", parameters, NULL, NULL, &test->error, NULL);
  g_assert (test->error != NULL);
  g_assert_cmpint (test->error->code, ==, TP_ERROR_INVALID_ARGUMENT);
  g_clear_error (&test->error);

  params = tp_tests_param_connection_manager_steal_params_last_conn ();

  if (params != NULL)
    {
      g_assert (params->would_have_been_freed);
      tp_tests_param_connection_manager_free_params (params);
    }

  g_hash_table_unref (parameters);
}

static void
test_wrong_type (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *parameters;
  TpTestsCMParams *params;

  parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_hash_table_insert (parameters, "a-boolean",
      tp_g_value_slice_new_string ("FALSE"));

  tp_cli_connection_manager_run_request_connection (test->cm, -1,
      "example", parameters, NULL, NULL, &test->error, NULL);
  g_assert (test->error != NULL);
  g_assert_cmpint (test->error->code, ==, TP_ERROR_INVALID_ARGUMENT);
  g_clear_error (&test->error);

  params = tp_tests_param_connection_manager_steal_params_last_conn ();

  if (params != NULL)
    {
      g_assert (params->would_have_been_freed);
      tp_tests_param_connection_manager_free_params (params);
    }

  g_hash_table_unref (parameters);
}

static void
test_unwelcome (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GHashTable *parameters;
  TpTestsCMParams *params;

  parameters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_hash_table_insert (parameters, "a-boolean",
      tp_g_value_slice_new_boolean (FALSE));
  g_hash_table_insert (parameters, "a-piece-of-cheese",
      tp_g_value_slice_new_boolean (TRUE));

  tp_cli_connection_manager_run_request_connection (test->cm, -1,
      "example", parameters, NULL, NULL, &test->error, NULL);
  g_assert (test->error != NULL);
  g_assert_cmpint (test->error->code, ==, TP_ERROR_INVALID_ARGUMENT);
  g_clear_error (&test->error);

  params = tp_tests_param_connection_manager_steal_params_last_conn ();

  if (params != NULL)
    {
      g_assert (params->would_have_been_freed);
      tp_tests_param_connection_manager_free_params (params);
    }

  g_hash_table_unref (parameters);
}

static void
test_get_parameters_bad_proto (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  GPtrArray *out = NULL;

  tp_cli_connection_manager_run_get_parameters (test->cm, -1,
      "not-example", &out, &test->error, NULL);
  g_assert (out == NULL);
  g_assert (test->error != NULL);
  g_assert_cmpint (test->error->code, ==, TP_ERROR_NOT_IMPLEMENTED);
  g_clear_error (&test->error);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/params-cm/set-params", Test, NULL, setup, test_set_params,
      teardown);
  g_test_add ("/params-cm/defaults", Test, NULL, setup, test_defaults,
      teardown);
  g_test_add ("/params-cm/fail-filter", Test, NULL, setup, test_fail_filter,
      teardown);
  g_test_add ("/params-cm/missing-required", Test, NULL, setup,
      test_missing_required, teardown);
  g_test_add ("/params-cm/wrong-type", Test, NULL, setup, test_wrong_type,
      teardown);
  g_test_add ("/params-cm/unwelcome", Test, NULL, setup, test_unwelcome,
      teardown);
  g_test_add ("/params-cm/get-parameters-bad-proto", Test, NULL, setup,
      test_get_parameters_bad_proto, teardown);

  return g_test_run ();
}
