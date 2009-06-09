/* Simple utility code used by the regression tests.
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "tests/lib/util.h"

void
test_proxy_run_until_dbus_queue_processed (gpointer proxy)
{
  tp_cli_dbus_introspectable_run_introspect (proxy, -1, NULL, NULL, NULL);
}

void
test_connection_run_until_dbus_queue_processed (TpConnection *connection)
{
  tp_cli_connection_run_get_protocol (connection, -1, NULL, NULL, NULL);
}

void
_test_assert_no_error (const GError *error,
                       const char *file,
                       int line)
{
  if (error != NULL)
    {
      g_error ("%s:%d:%s: code %u: %s",
          file, line, g_quark_to_string (error->domain),
          error->code, error->message);
    }
}
