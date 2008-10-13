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
test_connection_run_until_dbus_queue_processed (TpConnection *connection)
{
  tp_cli_connection_run_get_protocol (connection, -1, NULL, NULL, NULL);
}
