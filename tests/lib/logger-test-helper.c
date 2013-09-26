/*
 * logger-test-helper.c
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

#include <config.h>

#include "logger-test-helper.h"

#include "util.h"

void
tpl_test_create_and_prepare_account (TpDBusDaemon *dbus,
    TpSimpleClientFactory *factory,
    const gchar *path,
    TpAccount **account,
    TpTestsSimpleAccount **account_service)
{
  GError *error = NULL;
  GArray *features;
  GQuark zero = 0;

  *account_service = g_object_new (TP_TESTS_TYPE_SIMPLE_ACCOUNT,
      NULL);
  g_assert (*account_service != NULL);

  tp_dbus_daemon_register_object (dbus, path, *account_service);

  *account = tp_simple_client_factory_ensure_account (factory, path, NULL,
      &error);
  g_assert_no_error (error);
  g_assert (*account != NULL);

  features = tp_simple_client_factory_dup_account_features (factory, *account);
  g_array_append_val (features, zero);

  tp_tests_proxy_run_until_prepared (*account, (GQuark *) features->data);
  g_array_free (features, FALSE);
}

void
tpl_test_release_account (TpDBusDaemon *dbus,
    TpAccount *account,
    TpTestsSimpleAccount *account_service)
{
  tp_dbus_daemon_unregister_object (dbus, account_service);
  g_object_unref (account_service);
  g_object_unref (account);
}
