/*
 * Copyright ©2013 Collabora Ltd.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * This is a subclass of TplLogStoreXml to read logs from the directory Empathy
 * used to store them it. It disables writing to that legacy location.
 */

#include "config.h"
#include "log-store-empathy-internal.h"

#include "telepathy-logger/log-store-internal.h"

static void log_store_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (TplLogStoreEmpathy, _tpl_log_store_empathy,
    TPL_TYPE_LOG_STORE_XML,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_LOG_STORE, log_store_iface_init))

static void
_tpl_log_store_empathy_init (TplLogStoreEmpathy *self)
{
}

static void
_tpl_log_store_empathy_class_init (TplLogStoreEmpathyClass *klass)
{
}


static const gchar *
log_store_empathy_get_name (TplLogStore *store)
{
  TplLogStoreXml *self = (TplLogStoreXml *) store;

  g_return_val_if_fail (TPL_IS_LOG_STORE_EMPATHY (self), NULL);

  return "Empathy";
}

static void
log_store_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  TplLogStoreInterface *iface = (TplLogStoreInterface *) g_iface;

  iface->get_name = log_store_empathy_get_name;

  /* We don't want to store new logs in Empathy's directory, just read the old
   * ones. */
  iface->add_event = NULL;
}
