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

#include "config.h" /* autoheader generated */
#include "conf.h" /* conf.c module headers */

#include <glib.h>

#include <telepathy-logger/util.h>

#define DEBUG_FLAG TPL_DEBUG_CONF
#include <telepathy-logger/debug.h>

#define GET_PRIV(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), TPL_TYPE_CONF, TplConfPriv))

#define GCONF_KEY_BASE "/apps/telepathy-logger/"
#define GCONF_KEY_LOGGING_TURNED_ON GCONF_KEY_BASE "logging/turned_on"
#define GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST GCONF_KEY_BASE "logging/accounts/ignorelist"

G_DEFINE_TYPE (TplConf, tpl_conf, G_TYPE_OBJECT)

static TplConf *conf_singleton = NULL;

typedef struct {
    GConfClient *client;
} TplConfPriv;


static void
tpl_conf_finalize (GObject *obj)
{
  TplConfPriv *priv;

  priv = GET_PRIV (obj);

  if (priv->client != NULL)
    {
      g_object_unref (priv->client);
      priv->client = NULL;
    }

  G_OBJECT_CLASS (tpl_conf_parent_class)->finalize (obj);
}


static void
tpl_conf_dispose (GObject *obj)
{
  /* TplConf *self = TPL_CONF (obj); */

  G_OBJECT_CLASS (tpl_conf_parent_class)->dispose (obj);
}


static GObject *
tpl_conf_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *retval;

  if (conf_singleton)
    retval = g_object_ref (conf_singleton);
  else
    {
      retval = G_OBJECT_CLASS (tpl_conf_parent_class)->constructor (type,
          n_props, props);
      conf_singleton = TPL_CONF (retval);
      g_object_add_weak_pointer (retval, (gpointer *) &conf_singleton);
    }
  return retval;
}



static void
tpl_conf_class_init (TplConfClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = tpl_conf_finalize;
  object_class->dispose = tpl_conf_dispose;
  object_class->constructor = tpl_conf_constructor;

  g_type_class_add_private (object_class, sizeof (TplConfPriv));
}

static void
tpl_conf_init (TplConf *self)
{
  TplConfPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_CONF, TplConfPriv);

  priv->client = gconf_client_get_default ();
}

/**
 * tpl_conf_get_gconf_client
 * @self: TplConf instance
 *
 * You probably won't need to and anyway you shoudln't access directly the
 * GConf client.
 * In case you *really* need, remember to ref/unref properly.
 *
 * Returns: an GConfClient instance, owned by the TplConfInstance.
 */
GConfClient *
tpl_conf_get_gconf_client (TplConf *self)
{
  return GET_PRIV (self)->client;
}


/**
 * tpl_conf_dup
 *
 * Convenience function to obtain a TPL Configuration object, which is a
 * singleton.
 *
 * Returns: a TplConf signleton instance with its reference counter
 * incremented. Remember to unref the counter.
 */
TplConf *
tpl_conf_dup (void)
{
  return g_object_new (TPL_TYPE_CONF, NULL);
}


/**
 * tpl_conf_is_globally_enabled
 * @self: a TplConf instance
 * @error: memory adress where to store a GError, in case of error, or %NULL
 * to ignore error reporting.
 *
 * Wether TPL is globally enabled or not. If it's not globally enabled, no
 * signals will be logged at all.
 * To enable/disable a single account use tpl_conf_set_accounts_ignorelist()
 *
 * Returns: %TRUE if TPL logging is globally enable, otherwise returns %FALSE
 * and @error will be used.
 */
gboolean
tpl_conf_is_globally_enabled (TplConf *self,
    GError **error)
{
  GConfValue *value;
  gboolean ret;
  GError *loc_error = NULL;

  if (!TPL_IS_CONF (self))
    {
      g_set_error_literal (error, TPL_CONF_ERROR, TPL_CONF_ERROR_GCONF_KEY,
          "arg1 passed to tpl_conf_is_globally_enabled is not TplConf instance");
      return FALSE;
    }

  value = gconf_client_get (GET_PRIV (self)->client,
      GCONF_KEY_LOGGING_TURNED_ON, &loc_error);
  if (loc_error != NULL)
    {
      DEBUG ("Accessing " GCONF_KEY_LOGGING_TURNED_ON ": %s",
          loc_error->message);
      g_propagate_error (error, loc_error);
      g_error_free (loc_error);

      return TRUE;
    }

  if (value == NULL)
    {
      /* built in default is to log */
      DEBUG ("No value set or schema installed, defaulting to log");
      return TRUE;
    }

  ret = gconf_value_get_bool (value);
  gconf_value_free (value);

  return ret;
}


/**
 * tpl_conf_globally_enable
 * @self: a TplConf instance
 * @enable: wether to globally enable or globally disable logging.
 * @error: memory adress where to store a GError, in case of error, or %NULL
 * to ignore error reporting.
 *
 * Globally enables or disables logging for TPL. If it's globally disabled, no
 * signals will be logged at all.
 * Note that this will change the global TPL configuration, affecting all the
 * TPL instances, including the TPL logging process and all the clients using
 * libtelepathy-logger.
 */
void
tpl_conf_globally_enable (TplConf *self,
    gboolean enable,
    GError **error)
{
  GError *loc_error = NULL;

  g_return_if_fail (TPL_IS_CONF (self));

  gconf_client_set_bool (GET_PRIV (self)->client,
      GCONF_KEY_LOGGING_TURNED_ON, enable, &loc_error);

  /* According to GConf ref manual, an error is raised only if <key> is
   * actually holding a different type than gboolean. It means something wrong
   * is happening outside the library.
   *
   * TODO: is it better to return a GError as well? The above situation is not
   * a real run-time error and can occur only on bad updated systems. 99.9% of
   * times the schema+APIs will match and no error will be raised.
   */
  if (loc_error != NULL)
    {
      CRITICAL ("Probably the Telepathy-Logger GConf's schema has changed "
          "and you're using an out of date library\n");
      g_propagate_error (error, loc_error);
      g_error_free (loc_error);
      return;
    }
}


/**
 * tpl_conf_get_accounts_ignorelist
 * @self: a TplConf instance
 * @error: memory adress where to store a GError, in case of error, or %NULL
 * to ignore error reporting.
 *
 * The list of ignored accounts. If an account is ignored, no signals for this
 * account will be logged.
 *
 * Returns: a GList of (gchar *) contaning ignored accounts' object paths or
 * %NULL with @error set otherwise.
 */
GSList *
tpl_conf_get_accounts_ignorelist (TplConf *self,
    GError **error)
{
  GSList *ret;
  GError *loc_error = NULL;

  g_return_val_if_fail (TPL_IS_CONF (self), NULL);

  ret = gconf_client_get_list (GET_PRIV (self)->client,
      GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST, GCONF_VALUE_STRING,
      &loc_error);
  if (loc_error != NULL)
    {
      g_warning ("Accessing " GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST": %s",
          loc_error->message);
      g_propagate_error (error, loc_error);
      g_error_free (loc_error);
      return NULL;
    }

  return ret;
}


/**
 * tpl_conf_set_accounts_ignorelist
 * @self: a TplConf instance
 * @newlist: a new GList containing account's object paths (gchar *) to be
 * ignored
 * @error: memory adress where to store a GError, in case of error, or %NULL
 * to ignore error reporting.
 *
 * Globally disables logging for @newlist account's path. If an account is
 * disabled, no signals for such account will be logged.
 *
 * Note that this will change the global TPL configuration, affecting all the
 * TPL instances, including the TPL logging process and all the clients using
 * libtelepathy-logger.
 */
void
tpl_conf_set_accounts_ignorelist (TplConf *self,
    GSList *newlist,
    GError **error)
{
  GError *loc_error = NULL;

  g_return_if_fail (TPL_IS_CONF (self));

  gconf_client_set_list (GET_PRIV (self)->client,
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
      CRITICAL ("Probably the Telepathy-Logger GConf's schema has changed "
          "and you're using an out of date library\n");
      g_propagate_error (error, loc_error);
      g_error_free (loc_error);
      return;
    }
}


/**
 * tpl_conf_is_account_ignored
 * @self: a TplConf instance
 * @account_path: a TpAccount object-path
 * @error: memory adress where to store a GError, in case of error, or %NULL
 * to ignore error reporting.
 *
 * Wether @account_path is enabled or disable (aka ignored).
 *
 * Returns: %TRUE if @account_path is ignored, %FALSE if it's not or %FALSE
 * and @error set if an error occurs.
 */
gboolean
tpl_conf_is_account_ignored (TplConf *self,
    const gchar *account_path,
    GError **error)
{
  GError *loc_error = NULL;
  GSList *ignored_list;
  GSList *found_element;

  g_return_val_if_fail (TPL_IS_CONF (self), FALSE);
  g_return_val_if_fail (!TPL_STR_EMPTY (account_path), FALSE);

  ignored_list = gconf_client_get_list (GET_PRIV (self)->client,
      GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST, GCONF_VALUE_STRING,
      &loc_error);
  if (loc_error != NULL)
    {
      g_warning ("Accessing " GCONF_KEY_LOGGING_ACCOUNTS_IGNORELIST": %s",
          loc_error->message);
      g_propagate_error (error, loc_error);
      g_clear_error (&loc_error);
      g_error_free (loc_error);
      return FALSE;
    }

  found_element = g_slist_find_custom (ignored_list,
        account_path, (GCompareFunc) g_strcmp0);
  if (found_element != NULL)
    {
      return TRUE;
    }

  return FALSE;
}
