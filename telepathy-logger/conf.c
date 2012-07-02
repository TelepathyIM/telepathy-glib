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
#include <telepathy-glib/telepathy-glib.h>

#define DEBUG_FLAG TPL_DEBUG_CONF
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

#define GET_PRIV(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), TPL_TYPE_CONF, TplConfPriv))

#define GSETTINGS_SCHEMA "org.freedesktop.Telepathy.Logger"
#define KEY_ENABLED "enabled"

G_DEFINE_TYPE (TplConf, _tpl_conf, G_TYPE_OBJECT)

static TplConf *conf_singleton = NULL;

typedef struct
{
  gboolean test_mode;
  GSettings *gsettings;
} TplConfPriv;


enum /* properties */
{
  PROP_0,
  PROP_GLOBALLY_ENABLED
};


static void
_notify_globally_enable (GSettings *gsettings,
    const gchar *key,
    GObject *self)
{
  g_object_notify (self, "globally-enabled");
}


static void
tpl_conf_get_property (GObject *self,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  switch (prop_id)
    {
      case PROP_GLOBALLY_ENABLED:
        g_value_set_boolean (value,
            _tpl_conf_is_globally_enabled (TPL_CONF (self)));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
        break;
    }
}

static void
tpl_conf_set_property (GObject *self,
    guint prop_id,
    const GValue *value,
    GParamSpec *pspec)
{
  switch (prop_id)
    {
      case PROP_GLOBALLY_ENABLED:
        _tpl_conf_globally_enable (TPL_CONF (self),
            g_value_get_boolean (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
        break;
    }
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

  object_class->get_property = tpl_conf_get_property;
  object_class->set_property = tpl_conf_set_property;
  object_class->finalize = tpl_conf_finalize;
  object_class->constructor = tpl_conf_constructor;

  g_object_class_install_property (object_class, PROP_GLOBALLY_ENABLED,
      g_param_spec_boolean ("globally-enabled",
        "Globally Enabled",
        "TRUE if logging is enabled (may still be disabled for specific users)",
        TRUE,
        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (TplConfPriv));
}


static void
_tpl_conf_init (TplConf *self)
{
  TplConfPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_CONF, TplConfPriv);

  if (g_getenv ("TPL_TEST_MODE") != NULL)
    {
      priv->test_mode = TRUE;
    }
  else
    {
      priv->gsettings = g_settings_new (GSETTINGS_SCHEMA);

      g_signal_connect (priv->gsettings, "changed::" KEY_ENABLED,
          G_CALLBACK (_notify_globally_enable), self);
    }
}


/**
 * _tpl_conf_dup:
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
 * _tpl_conf_is_globally_enabled:
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

  if (GET_PRIV (self)->test_mode)
    return TRUE;
  else
    return g_settings_get_boolean (GET_PRIV (self)->gsettings, KEY_ENABLED);
}


/**
 * _tpl_conf_globally_enable:
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

  if (GET_PRIV (self)->test_mode)
    return;

  g_settings_set_boolean (GET_PRIV (self)->gsettings,
      KEY_ENABLED, enable);
}
