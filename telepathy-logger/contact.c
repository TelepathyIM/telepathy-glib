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
 */

#include "config.h"
#include "contact-internal.h"

#define DEBUG_FLAG TPL_DEBUG_CONTACT
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

G_DEFINE_TYPE (TplContact, tpl_contact, G_TYPE_OBJECT)

struct _TplContactPriv
{
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
  TplContactPriv *priv = self->priv;

  g_free (priv->alias);
  priv->alias = NULL;
  g_free (priv->identifier);
  priv->identifier = NULL;

  G_OBJECT_CLASS (tpl_contact_parent_class)->finalize (obj);
}


static void
tpl_contact_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplContactPriv *priv = TPL_CONTACT (object)->priv;

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


/* tpl_contact_from_room_id:
 * @chatroom_id: the chatroom id which will be the identifier for the contact
 *
 * Return a TplContact instance with identifier, alias copied from
 * @chatroom_id. It also sets %TPL_CONTACT_GROUP as contact type for
 * the #TplContact returned.
 */
TplContact *
tpl_contact_from_room_id (const gchar *chatroom_id)
{
  TplContact *ret;

  g_return_val_if_fail (chatroom_id != NULL, NULL);

  ret = _tpl_contact_new (chatroom_id);
  tpl_contact_set_alias (ret, chatroom_id);
  tpl_contact_set_contact_type (ret, TPL_CONTACT_GROUP);

  DEBUG ("Chatroom id: %s", chatroom_id);
  return ret;
}


/* tpl_contact_from_tp_contact:
 * @contact: the TpContact instance to create the TplContact from
 *
 * Return a TplContact instance with identifier, alias and
 * avatar's token copied. It also sets %TPL_CONTACT_USER as contact type for
 * the #TplContact returned. The client needs to set it to %TPL_CONTACT_SELF
 * in case the contact is the account's onwer.
 *
 * @see #tpl_contact_set_contact_type() and %TPL_CONTACT_SELF description.
 */
TplContact *
_tpl_contact_from_tp_contact (TpContact *contact)
{
  TplContact *ret;

  g_return_val_if_fail (TP_IS_CONTACT (contact), NULL);

  ret = _tpl_contact_new (tp_contact_get_identifier (contact));

  if (tp_contact_get_alias (contact) != NULL)
    tpl_contact_set_alias (ret, (gchar *) tp_contact_get_alias (contact));
  if (tp_contact_get_avatar_token (contact) != NULL)
    tpl_contact_set_avatar_token (ret, tp_contact_get_avatar_token (contact));

  /* set contact type to TPL_CONTACT_USER by default, the client need to set
   * it to TPL_CONTACT_SELF in case the contact is actually the account's
   * owner */
  tpl_contact_set_contact_type (ret, TPL_CONTACT_USER);

  DEBUG ("ID: %s, TOK: %s", tpl_contact_get_identifier (ret),
      tpl_contact_get_avatar_token (ret));
  return ret;
}


TplContact *
_tpl_contact_new (const gchar *identifier)
{
  g_return_val_if_fail (!TPL_STR_EMPTY (identifier), NULL);

  return g_object_new (TPL_TYPE_CONTACT,
      "identifier", identifier, NULL);
}


const gchar *
tpl_contact_get_alias (TplContact *self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return self->priv->alias;
}


const gchar *
tpl_contact_get_identifier (TplContact *self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return self->priv->identifier;
}


TplContactType
tpl_contact_get_contact_type (TplContact *self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), TPL_CONTACT_UNKNOWN);

  return self->priv->contact_type;
}


const gchar *
tpl_contact_get_avatar_token (TplContact *self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return self->priv->avatar_token;
}


void
tpl_contact_set_alias (TplContact *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->alias == NULL);

  self->priv->alias = g_strdup (data);
}


void
tpl_contact_set_identifier (TplContact *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->identifier == NULL);

  self->priv->identifier = g_strdup (data);
}


/* tpl_contact_set_contact_type:
 * @self: a TplContact instance
 * @data: the contact type for @self
 *
 * Set a contact type for @self.
 *
 * Note: %TPL_CONTACT_USER and %TPL_CONTACT_GROUP are automatically set after
 * #tpl_contact_from_tp_contact() and #tpl_contact_from_chatroom_id(),
 * respectively. Though, the client will need to set %TPL_CONTACT_SELF after
 * those function calls when @self represents the owner of the account.
 *
 * @see #TplContactType
 */
void
tpl_contact_set_contact_type (TplContact *self,
    TplContactType data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));

  self->priv->contact_type = data;
}


void
tpl_contact_set_avatar_token (TplContact *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));
  g_return_if_fail (self->priv->avatar_token == NULL);
  /* data can be NULL, if no avatar_token is set */

  self->priv->avatar_token = g_strdup (data);
}
