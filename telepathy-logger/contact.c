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

#include "config.h"
#include "contact.h"

#include <telepathy-logger/util.h>

#define DEBUG_FLAG TPL_DEBUG_CONTACT
#include <telepathy-logger/debug.h>

G_DEFINE_TYPE (TplContact, tpl_contact, G_TYPE_OBJECT)

#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplContact)
struct _TplContactPriv
{
  TpContact *contact;

  TplContactType contact_type;
  gchar *alias;
  gchar *identifier;
  gchar *avatar_token;
};

enum
{
  PROP0,
  PROP_IDENTIFIER,
  PROP_ALIAS,
  PROP_AVATAR_TOKEN
};

static void
tpl_contact_finalize (GObject *obj)
{
  TplContact *self = TPL_CONTACT (obj);
  TplContactPriv *priv = GET_PRIV (self);

  g_free (priv->alias);
  priv->alias = NULL;
  g_free (priv->identifier);
  priv->identifier = NULL;

  G_OBJECT_CLASS (tpl_contact_parent_class)->finalize (obj);
}


static void
tpl_contact_dispose (GObject *obj)
{
  TplContact *self = TPL_CONTACT (obj);
  TplContactPriv *priv = GET_PRIV (self);

  if (priv->contact != NULL)
    {
      g_object_unref (priv->contact);
      priv->contact = NULL;
    }

  G_OBJECT_CLASS (tpl_contact_parent_class)->dispose (obj);
}


static void
tpl_contact_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplContactPriv *priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_IDENTIFIER:
        g_value_set_string (value, priv->identifier);
        break;
      case PROP_ALIAS:
        g_value_set_string (value, priv->alias);
        break;
      case PROP_AVATAR_TOKEN:
        g_value_set_string (value, priv->avatar_token);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}


static void
tpl_contact_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplContact *self = TPL_CONTACT (object);

  switch (param_id) {
      case PROP_IDENTIFIER:
        tpl_contact_set_identifier (self, g_value_get_string (value));
        break;
      case PROP_ALIAS:
        tpl_contact_set_alias (self, g_value_get_string (value));
        break;
      case PROP_AVATAR_TOKEN:
        tpl_contact_set_avatar_token (self, g_value_get_string (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  };

}


static void tpl_contact_class_init (TplContactClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->finalize = tpl_contact_finalize;
  object_class->dispose = tpl_contact_dispose;
  object_class->get_property = tpl_contact_get_property;
  object_class->set_property = tpl_contact_set_property;

  /**
   * TplContact:identifier:
   *
   * The contact's identifier
   */
  param_spec = g_param_spec_string ("identifier",
      "Identifier",
      "The contact's identifier",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_IDENTIFIER, param_spec);

  /**
   * TplContact:alias:
   *
   * The contact's alias
   */
  param_spec = g_param_spec_string ("alias",
      "Alias",
      "The contact's alias",
      NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ALIAS, param_spec);

  /**
   * TplContact:avatar-token:
   *
   * The contact's avatar token
   */
  param_spec = g_param_spec_string ("avatar-token",
      "AvatarToken",
      "The contact's avatar's token",
      NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ALIAS, param_spec);

  g_type_class_add_private (object_class, sizeof (TplContactPriv));
}


static void
tpl_contact_init (TplContact *self)
{
  TplContactPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_CONTACT, TplContactPriv);

  self->priv = priv;
}


TplContact *
tpl_contact_from_tp_contact (TpContact *contact)
{
  TplContact *ret;

  g_return_val_if_fail (TP_IS_CONTACT (contact), NULL);

  ret = tpl_contact_new (tp_contact_get_identifier (contact));
  tpl_contact_set_contact (ret, contact);
  if (tp_contact_get_alias (contact) != NULL)
    tpl_contact_set_alias (ret, (gchar *) tp_contact_get_alias (contact));
  if (tp_contact_get_avatar_token (contact) != NULL)
    tpl_contact_set_avatar_token (ret, tp_contact_get_avatar_token (contact));

  DEBUG ("ID: %s, TOK: %s", tpl_contact_get_identifier (ret),
      tpl_contact_get_avatar_token (ret));
  return ret;
}


TplContact *
tpl_contact_new (const gchar *identifier)
{
  g_return_val_if_fail (!TPL_STR_EMPTY (identifier), NULL);

  return g_object_new (TPL_TYPE_CONTACT,
      "identifier", identifier, NULL);
}


TpContact *
tpl_contact_get_contact (TplContact *self)
{
  TplContactPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return priv->contact;
}


const gchar *
tpl_contact_get_alias (TplContact *self)
{
  TplContactPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return priv->alias;
}


const gchar *
tpl_contact_get_identifier (TplContact *self)
{
  TplContactPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return priv->identifier;
}


TplContactType
tpl_contact_get_contact_type (TplContact *self)
{
  TplContactPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_CONTACT (self), TPL_CONTACT_UNKNOWN);

  return priv->contact_type;
}


const gchar *
tpl_contact_get_avatar_token (TplContact *self)
{
  TplContactPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return priv->avatar_token;
}


void
tpl_contact_set_contact (TplContact *self,
    TpContact *data)
{
  TplContactPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_CONTACT (self));
  g_return_if_fail (TP_IS_CONTACT (data));
  g_return_if_fail (priv->contact == NULL);

  priv->contact = g_object_ref (data);
}


void
tpl_contact_set_alias (TplContact *self,
    const gchar *data)
{
  TplContactPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_CONTACT (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (priv->alias == NULL);

  priv->alias = g_strdup (data);
}


void
tpl_contact_set_identifier (TplContact *self,
    const gchar *data)
{
  TplContactPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_CONTACT (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (priv->identifier == NULL);

  priv->identifier = g_strdup (data);
}


void
tpl_contact_set_contact_type (TplContact *self,
    TplContactType data)
{
  TplContactPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_CONTACT (self));

  priv->contact_type = data;
}


void
tpl_contact_set_avatar_token (TplContact *self,
    const gchar *data)
{
  TplContactPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_CONTACT (self));
  g_return_if_fail (priv->avatar_token == NULL);
  /* data can be NULL, if no avatar_token is set */

  priv->avatar_token = g_strdup (data);
}
