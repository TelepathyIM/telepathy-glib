/* Test support for shortening connection service names.
 *
 * Copyright (C) 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <string.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>

#include "examples/cm/echo/chan.h"
#include "examples/cm/echo/conn.h"
#include "tests/lib/myassert.h"
#include "tests/lib/util.h"

/* 256 characters */
#define LONG_ACCOUNT_IS_LONG \
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" \
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" \
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" \
  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

int
main (int argc,
      char **argv)
{
  ExampleEchoConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  GError *error = NULL;
  gchar *name;
  gchar *conn_path;

  g_type_init ();

  MYASSERT (strlen (LONG_ACCOUNT_IS_LONG) == 256, "");
  service_conn = EXAMPLE_ECHO_CONNECTION (g_object_new (
        EXAMPLE_TYPE_ECHO_CONNECTION,
        "account", LONG_ACCOUNT_IS_LONG,
        "protocol", "example",
        NULL));
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "example",
        &name, &conn_path, &error), "");
  test_assert_no_error (error);
  /* Name is too long to be used unmodified; check that it's shortened to 255
   * characters.
   */
  MYASSERT (strlen (name) == 255, "");

  g_object_unref (service_conn);
  g_free (name);
  g_free (conn_path);
  return 0;
}
