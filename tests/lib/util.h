/* Simple utility code used by the regression tests.
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef TEST_LIB_UTIL_H
#define TEST_LIB_UTIL_H

#include <telepathy-glib/connection.h>

void test_connection_run_until_dbus_queue_processed (TpConnection *connection);

#endif
