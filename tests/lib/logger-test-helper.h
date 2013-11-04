/*
 * logger-test-helper.h
 *
 * Copyright (C) 2013 Collabora Ltd. <http://www.collabora.co.uk/>
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

#ifndef __LOGGER_TEST_HELPER_H__
#define __LOGGER_TEST_HELPER_H__

#include <telepathy-glib/telepathy-glib.h>

#include "simple-account.h"

void tpl_test_create_and_prepare_account (TpDBusDaemon *dbus,
    TpClientFactory *factory,
    const gchar *path,
    TpAccount **account,
    TpTestsSimpleAccount **account_service);

void tpl_test_release_account (TpDBusDaemon *dbus,
    TpAccount *account,
    TpTestsSimpleAccount *account_service);

void tp_tests_copy_dir (const gchar *from_dir, const gchar *to_dir);

#endif
