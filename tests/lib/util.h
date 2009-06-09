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

#include <telepathy-glib/proxy.h>
#include <telepathy-glib/connection.h>

void test_proxy_run_until_dbus_queue_processed (gpointer proxy);

void test_connection_run_until_dbus_queue_processed (TpConnection *connection);

#define test_assert_no_error(e) _test_assert_no_error (e, __FILE__, __LINE__)

void _test_assert_no_error (const GError *error, const char *file, int line);

#endif
