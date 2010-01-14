/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
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
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "conf.h"

#include <glib.h>

#include <telepathy-logger/utils.h>

//#define DEBUG(...)
#define GET_PRIV(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), TPL_TYPE_CONF, TplConfPriv))
//#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplConf)
#define GCONF_KEY_LOGGING_TURNED_ON "/apps/telepathy-logger/logging/turned_on"
#define GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST "/apps/telepathy-logger/logging/accounts/ignorelist"

G_DEFINE_TYPE (TplConf, tpl_conf, G_TYPE_OBJECT)

static TplConf *conf_singleton = NULL;

typedef struct {
    GConfClient *client;
} TplConfPriv;

static void tpl_conf_finalize (GObject * obj)
{
  TplConfPriv *priv;

  priv = GET_PRIV (TPL_CONF (obj));

  tpl_object_unref_if_not_null (priv->client);
  priv->client = NULL;

  G_OBJECT_CLASS (tpl_conf_parent_class)->finalize (obj);
}

static void
tpl_conf_dispose (GObject * obj)
{
  /* TplConf *self = TPL_CONF (obj); */

  G_OBJECT_CLASS (tpl_conf_parent_class)->dispose (obj);
}

/* 
 * - TplConf constructor -
 * Initialises GConfClient
 */

static GObject *
tpl_conf_constructor (GType type,
		guint n_props, GObjectConstructParam * props)
{
  GObject *retval;

  if (conf_singleton)
    {
      retval = g_object_ref (conf_singleton);
    }
  else
    {
      retval = G_OBJECT_CLASS (tpl_conf_parent_class)->constructor
        (type, n_props, props);
      conf_singleton = TPL_CONF (retval);
      g_object_add_weak_pointer (retval, (gpointer *) &conf_singleton);
    }
  return retval;
}



static void
tpl_conf_class_init (TplConfClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = tpl_conf_finalize;
  object_class->dispose = tpl_conf_dispose;
  object_class->constructor = tpl_conf_constructor;

  g_type_class_add_private (object_class, sizeof (TplConfPriv));
}

static void
tpl_conf_init (TplConf * self)
{
/*
  TplConfPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_CONF, TplConfPriv);
*/
  TplConfPriv *priv = GET_PRIV (self);
  priv->client = gconf_client_get_default ();

}

/* 
 * You probably won't need to access directly the GConf client.
 * In case you really need, remember to ref/unref properly.
 */
GConfClient *
tpl_conf_get_gconf_client(TplConf *self) {
  return GET_PRIV(self)->client;
}


TplConf *
tpl_conf_dup (void)
{
  return g_object_new (TPL_TYPE_CONF, NULL);
}

gboolean
tpl_conf_is_globally_enabled (TplConf * self, GError **error)
{
  gboolean ret;
  GError *loc_error = NULL;

  g_return_val_if_fail (TPL_IS_CONF (self), FALSE);

  ret = gconf_client_get_bool (GET_PRIV(self)->client, GCONF_KEY_LOGGING_TURNED_ON,
      &loc_error);
  if (loc_error != NULL)
  {
      g_warning("Accessing " GCONF_KEY_LOGGING_TURNED_ON": %s", loc_error->message);
      g_propagate_error(error, loc_error);
      g_clear_error(&loc_error);
      g_error_free(loc_error);
      return FALSE;
  }

  return ret;
}

void
tpl_conf_togle_globally_enable (TplConf *self, gboolean enable, GError **error)
{
  GError *loc_error = NULL;

  g_return_if_fail (TPL_IS_CONF (self));

  gconf_client_set_bool (GET_PRIV(self)->client,
      GCONF_KEY_LOGGING_TURNED_ON, enable, &loc_error);

  /* According to GConf ref manual, an error is raised only if <key> is
   * actually holding a differnt type than gboolean. It means something wrong
   * is happening outside the library.
   *
   * TODO: is it better to return a GError as well? The above situation is not
   * a real run-time error and can occur only on bad updated systems. 99.9% of
   * times the schema+APIs will match and no error will be raised.
   */
  if (loc_error != NULL)
  {
      g_error("Probably the Telepathy-Logger GConf's schema has changed "
          "and you're using an out of date library\n");
      g_propagate_error(error, loc_error);
      g_clear_error(&loc_error);
      g_error_free(loc_error);
      return;
  }

}

GSList *
tpl_conf_get_accounts_ignorelist (TplConf * self, GError **error)
{
  GSList *ret;
  GError *loc_error = NULL;

  g_return_val_if_fail (TPL_IS_CONF (self), NULL);

  ret = gconf_client_get_list (GET_PRIV(self)->client,
      GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST, GCONF_VALUE_STRING,
		  &loc_error);
  if (loc_error != NULL)
  {
      g_warning("Accessing " GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST": %s", loc_error->message);
      g_propagate_error(error, loc_error);
      g_clear_error(&loc_error);
      g_error_free(loc_error);
      return NULL;
  }

  return ret;
}

void
tpl_conf_set_accounts_ignorelist (TplConf *self, GSList *newlist,
    GError **error)
{
  GError *loc_error = NULL;

  g_return_if_fail (TPL_IS_CONF (self));

  gconf_client_set_list (GET_PRIV(self)->client,
      GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST, GCONF_VALUE_STRING,
      newlist, &loc_error);

  /* According to GConf ref manual, an error is raised only if <key> is
   * actually holding a differnt type than list. It means something wrong
   * is happening outside the library.
   *
   * The above situation can occur only on bad updated systems. 99.9% of times
   * the schema+APIs will match and no error will be raised.
   */
  if (loc_error != NULL)
  {
      g_error("Probably the Telepathy-Logger GConf's schema has changed "
          "and you're using an out of date library\n");
      g_propagate_error(error, loc_error);
      g_clear_error(&loc_error);
      g_error_free(loc_error);
      return;
  }
}


gboolean
tpl_conf_is_account_ignored (TplConf *self, const gchar *account_path,
    GError **error)
{
  GError *loc_error = NULL;
  GSList *ignored_list;
  GSList *found_element;

  g_return_val_if_fail (TPL_IS_CONF (self), FALSE);
  g_return_val_if_fail (!TPL_STR_EMPTY(account_path), FALSE);

  ignored_list = gconf_client_get_list (GET_PRIV(self)->client,
      GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST, GCONF_VALUE_STRING,
		  &loc_error);
  if (loc_error != NULL)
  {
      g_warning("Accessing " GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST": %s", loc_error->message);
      g_propagate_error(error, loc_error);
      g_clear_error(&loc_error);
      g_error_free(loc_error);
      return FALSE;
  }

  found_element = g_slist_find_custom(ignored_list,
        account_path, (GCompareFunc) g_strcmp0);
  if(found_element != NULL) {
      return TRUE;
  }

  return FALSE;
}
