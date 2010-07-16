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

typedef struct {
    GSettings *gsettings;
} TplConfPriv;


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


static void
tpl_conf_dispose (GObject *obj)
{
  /* TplConf *self = TPL_CONF (obj); */

  G_OBJECT_CLASS (_tpl_conf_parent_class)->dispose (obj);
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
  object_class->dispose = tpl_conf_dispose;
  object_class->constructor = tpl_conf_constructor;

  g_type_class_add_private (object_class, sizeof (TplConfPriv));
}


static void
_tpl_conf_init (TplConf *self)
{
  TplConfPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_CONF, TplConfPriv);

  priv->gsettings = g_settings_new (GSETTINGS_SCHEMA);
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
  GVariant *v, *child;
  GVariantIter iter;
  gboolean found = FALSE;

  g_return_val_if_fail (TPL_IS_CONF (self), FALSE);
  g_return_val_if_fail (!TPL_STR_EMPTY (account_path), FALSE);

  v = g_settings_get_value (GET_PRIV (self)->gsettings, KEY_IGNORE_ACCOUNTS);
  g_variant_iter_init (&iter, v);
  while (!found && (child = g_variant_iter_next_value (&iter)))
    {
      const gchar *o = g_variant_get_string (child, NULL);

      found = !tp_strdiff (o, account_path);

      g_variant_unref (child);
    }

  g_variant_unref (v);

  return found;
}
