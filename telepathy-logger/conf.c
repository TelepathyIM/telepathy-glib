/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009-2010 Collabora Ltd.
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
 *          Danielle Madeley <danielle.madeley@collabora.co.uk>
 */

#include "config.h"
#include "conf-internal.h"
#include "tpl-marshal.h"

#include <glib.h>
#include <gio/gio.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TPL_DEBUG_CONF
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

#define GET_PRIV(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), TPL_TYPE_CONF, TplConfPriv))

#define GSETTINGS_SCHEMA "org.freedesktop.Telepathy.Logger"
#define KEY_ENABLED "enabled"
#define KEY_IGNORE_ACCOUNTS "ignore-accounts"

G_DEFINE_TYPE (TplConf, _tpl_conf, G_TYPE_OBJECT)

static TplConf *conf_singleton = NULL;

enum /* signals */
{
  IGNORE_ACCOUNTS_CHANGED,
  LAST_SIGNAL
};

static guint _signals[LAST_SIGNAL] = { 0, };

typedef struct {
    GSettings *gsettings;

    GHashTable *ignore_accounts; /* char * -> NULL */
} TplConfPriv;


static void
_ignore_accounts_changed (GSettings *gsettings,
    gchar *key,
    TplConf *self)
{
  TplConfPriv *priv = GET_PRIV (self);
  GVariant *v, *child;
  GVariantIter iter;
  GList *added = NULL, *removed;
  GHashTable *new_ignore_accounts, *old_ignore_accounts;

  new_ignore_accounts = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);

  /* walk the new ignore list, work out what's been added */
  v = g_settings_get_value (priv->gsettings, KEY_IGNORE_ACCOUNTS);
  g_variant_iter_init (&iter, v);
  while ((child = g_variant_iter_next_value (&iter)))
    {
      const gchar *o = g_variant_get_string (child, NULL);

      if (!g_hash_table_remove (priv->ignore_accounts, o))
        {
          /* account is not in list */
          added = g_list_prepend (added, (gpointer) o);
        }

      g_hash_table_insert (new_ignore_accounts, g_strdup (o),
          GUINT_TO_POINTER (TRUE));

      g_variant_unref (child);
    }

  /* get the remaining keys */
  removed = g_hash_table_get_keys (priv->ignore_accounts);

  /* swap priv->ignore_accounts over before emitting the signal */
  old_ignore_accounts = priv->ignore_accounts;
  priv->ignore_accounts = new_ignore_accounts;

  g_signal_emit (self, _signals[IGNORE_ACCOUNTS_CHANGED], 0, added, removed);

  g_variant_unref (v);
  g_list_free (added);
  g_list_free (removed);
  g_hash_table_destroy (old_ignore_accounts);
}


static void
tpl_conf_finalize (GObject *obj)
{
  TplConfPriv *priv;

  priv = GET_PRIV (obj);

  if (priv->gsettings != NULL)
    {
      g_object_unref (priv->gsettings);
      priv->gsettings = NULL;
    }

  G_OBJECT_CLASS (_tpl_conf_parent_class)->finalize (obj);
}


static GObject *
tpl_conf_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *retval;

  if (conf_singleton != NULL)
    {
      retval = g_object_ref (conf_singleton);
    }
  else
    {
      retval = G_OBJECT_CLASS (_tpl_conf_parent_class)->constructor (type,
          n_props, props);
      conf_singleton = TPL_CONF (retval);
      g_object_add_weak_pointer (retval, (gpointer *) &conf_singleton);
    }

  return retval;
}


static void
_tpl_conf_class_init (TplConfClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = tpl_conf_finalize;
  object_class->constructor = tpl_conf_constructor;

  _signals[IGNORE_ACCOUNTS_CHANGED] = g_signal_new ("ignore-accounts-changed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      tpl_marshal_VOID__POINTER_POINTER,
      G_TYPE_NONE,
      2, G_TYPE_POINTER, G_TYPE_POINTER);

  g_type_class_add_private (object_class, sizeof (TplConfPriv));
}


static void
_tpl_conf_init (TplConf *self)
{
  TplConfPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_CONF, TplConfPriv);

  priv->gsettings = g_settings_new (GSETTINGS_SCHEMA);

  priv->ignore_accounts = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);

  g_signal_connect (priv->gsettings, "changed::" KEY_IGNORE_ACCOUNTS,
      G_CALLBACK (_ignore_accounts_changed), self);
}


/**
 * _tpl_conf_dup
 *
 * Convenience function to obtain a TPL Configuration object, which is a
 * singleton.
 *
 * Returns: a TplConf signleton instance with its reference counter
 * incremented. Remember to unref the counter.
 */
TplConf *
_tpl_conf_dup (void)
{
  return g_object_new (TPL_TYPE_CONF, NULL);
}


/**
 * _tpl_conf_is_globally_enabled
 * @self: a TplConf instance
 *
 * Whether TPL is globally enabled or not. If it's not globally enabled, no
 * signals will be logged at all.
 * To enable/disable a single account use _tpl_conf_set_accounts_ignorelist()
 *
 * Returns: %TRUE if TPL logging is globally enabled, otherwise returns
 * %FALSE.
 */
gboolean
_tpl_conf_is_globally_enabled (TplConf *self)
{
  g_return_val_if_fail (TPL_IS_CONF (self), FALSE);

  return g_settings_get_boolean (GET_PRIV (self)->gsettings, KEY_ENABLED);
}


/**
 * _tpl_conf_globally_enable
 * @self: a TplConf instance
 * @enable: wether to globally enable or globally disable logging.
 *
 * Globally enables or disables logging for TPL. If it's globally disabled, no
 * signals will be logged at all.
 * Note that this will change the global TPL configuration, affecting all the
 * TPL instances, including the TPL logging process and all the clients using
 * libtelepathy-logger.
 */
void
_tpl_conf_globally_enable (TplConf *self,
    gboolean enable)
{
  g_return_if_fail (TPL_IS_CONF (self));

  g_settings_set_boolean (GET_PRIV (self)->gsettings,
      KEY_ENABLED, enable);
}


#if 0
/**
 * _tpl_conf_get_accounts_ignorelist
 * @self: a TplConf instance
 *
 * The list of ignored accounts. If an account is ignored, no signals for this
 * account will be logged.
 *
 * Returns: a GList of (gchar *) contaning ignored accounts' object paths
 */
GSList *
_tpl_conf_get_accounts_ignorelist (TplConf *self)
{
  GSList *ret = NULL;
  GVariant *v;

  g_return_val_if_fail (TPL_IS_CONF (self), NULL);

  v = g_settings_get_value (GET_PRIV (self)->gsettings, KEY_IGNORE_ACCOUNTS);

  return ret;
}


/**
 * _tpl_conf_set_accounts_ignorelist
 * @self: a TplConf instance
 * @newlist: a new GList containing account's object paths (gchar *) to be
 * ignored
 *
 * Globally disables logging for @newlist account's path. If an account is
 * disabled, no signals for such account will be logged.
 *
 * Note that this will change the global TPL configuration, affecting all the
 * TPL instances, including the TPL logging process and all the clients using
 * libtelepathy-logger.
 */
void
_tpl_conf_set_accounts_ignorelist (TplConf *self,
    GSList *newlist)
{
  g_return_if_fail (TPL_IS_CONF (self));
}
#endif


/**
 * _tpl_conf_is_account_ignored
 * @self: a TplConf instance
 * @account_path: a TpAccount object-path
 *
 * Whether @account_path is enabled or disabled (aka ignored).
 *
 * Returns: %TRUE if @account_path is ignored, %FALSE otherwise
 */
gboolean
_tpl_conf_is_account_ignored (TplConf *self,
    const gchar *account_path)
{
  g_return_val_if_fail (TPL_IS_CONF (self), FALSE);
  g_return_val_if_fail (!TPL_STR_EMPTY (account_path), FALSE);

  return g_hash_table_lookup (GET_PRIV (self)->ignore_accounts, account_path)
    != NULL;
}
