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
#include "entity-internal.h"

#define DEBUG_FLAG TPL_DEBUG_ENTITY
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

/**
 * SECTION:entity
 * @title: TplEntity
 * @short_description: Representation of a contact or chatroom
 *
 * An object representing a contact or chatroom.
 */

/**
 * TplEntity:
 *
 * An object representing a contact or chatroom.
 */

G_DEFINE_TYPE (TplEntity, tpl_entity, G_TYPE_OBJECT)

struct _TplEntityPriv
{
  TplEntityType type;
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
tpl_entity_finalize (GObject *obj)
{
  TplEntity *self = TPL_ENTITY (obj);
  TplEntityPriv *priv = self->priv;

  g_free (priv->alias);
  priv->alias = NULL;
  g_free (priv->identifier);
  priv->identifier = NULL;

  G_OBJECT_CLASS (tpl_entity_parent_class)->finalize (obj);
}


static void
tpl_entity_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplEntityPriv *priv = TPL_ENTITY (object)->priv;

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
tpl_entity_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplEntity *self = TPL_ENTITY (object);

  switch (param_id) {
      case PROP_IDENTIFIER:
        _tpl_entity_set_identifier (self, g_value_get_string (value));
        break;
      case PROP_ALIAS:
        _tpl_entity_set_alias (self, g_value_get_string (value));
        break;
      case PROP_AVATAR_TOKEN:
        _tpl_entity_set_avatar_token (self, g_value_get_string (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  };

}


static void tpl_entity_class_init (TplEntityClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->finalize = tpl_entity_finalize;
  object_class->get_property = tpl_entity_get_property;
  object_class->set_property = tpl_entity_set_property;

  /**
   * TplEntity:identifier:
   *
   * The entity's identifier
   */
  param_spec = g_param_spec_string ("identifier",
      "Identifier",
      "The entity's identifier",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_IDENTIFIER, param_spec);

  /**
   * TplEntity:alias:
   *
   * The entity's alias
   */
  param_spec = g_param_spec_string ("alias",
      "Alias",
      "The entity's alias",
      NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ALIAS, param_spec);

  /**
   * TplEntity:avatar-token:
   *
   * The entity's avatar token
   */
  param_spec = g_param_spec_string ("avatar-token",
      "AvatarToken",
      "The entity's avatar's token",
      NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ALIAS, param_spec);

  g_type_class_add_private (object_class, sizeof (TplEntityPriv));
}


static void
tpl_entity_init (TplEntity *self)
{
  TplEntityPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_ENTITY, TplEntityPriv);

  self->priv = priv;
}


/* _tpl_entity_from_room_id:
 * @chatroom_id: the chatroom id which will be the identifier for the entity
 *
 * Return a TplEntity instance with identifier, alias copied from
 * @chatroom_id. It also sets %TPL_ENTITY_GROUP as type for
 * the #TplEntity returned.
 */
TplEntity *
_tpl_entity_from_room_id (const gchar *chatroom_id)
{
  TplEntity *ret;

  g_return_val_if_fail (chatroom_id != NULL, NULL);

  ret = _tpl_entity_new (chatroom_id);
  _tpl_entity_set_alias (ret, chatroom_id);
  _tpl_entity_set_entity_type (ret, TPL_ENTITY_GROUP);

  DEBUG ("Chatroom id: %s", chatroom_id);
  return ret;
}


/* _tpl_entity_from_tp_contact:
 * @contact: the TpContact instance to create the TplEntity from
 *
 * Return a TplEntity instance with identifier, alias and
 * avatar's token copied. It also sets %TPL_ENTITY_CONTACT as contact type for
 * the #TplEntity returned. The client needs to set it to %TPL_ENTITY_SELF
 * in case the contact is the account's onwer.
 *
 * @see #tpl_entity_set_contact_type() and %TPL_ENTITY_SELF description.
 */
TplEntity *
_tpl_entity_from_tp_contact (TpContact *contact)
{
  TplEntity *ret;

  g_return_val_if_fail (TP_IS_CONTACT (contact), NULL);

  ret = _tpl_entity_new (tp_contact_get_identifier (contact));

  if (tp_contact_get_alias (contact) != NULL)
    _tpl_entity_set_alias (ret, (gchar *) tp_contact_get_alias (contact));
  if (tp_contact_get_avatar_token (contact) != NULL)
    _tpl_entity_set_avatar_token (ret, tp_contact_get_avatar_token (contact));

  /* set contact type to TPL_ENTITY_CONTACT by default, the client need to set
   * it to TPL_ENTITY_SELF in case the contact is actually the account's
   * owner */
  _tpl_entity_set_entity_type (ret, TPL_ENTITY_CONTACT);

  DEBUG ("ID: %s, TOK: %s", tpl_entity_get_identifier (ret),
      tpl_entity_get_avatar_token (ret));
  return ret;
}


TplEntity *
_tpl_entity_new (const gchar *identifier)
{
  g_return_val_if_fail (!TPL_STR_EMPTY (identifier), NULL);

  return g_object_new (TPL_TYPE_ENTITY,
      "identifier", identifier, NULL);
}

/**
 * tpl_entity_get_alias:
 * @self: a #TplEntity
 *
 * Returns: the alias of the entity, or %NULL
 */
const gchar *
tpl_entity_get_alias (TplEntity *self)
{
  g_return_val_if_fail (TPL_IS_ENTITY (self), NULL);

  return self->priv->alias;
}

/**
 * tpl_entity_get_identifier:
 * @self: a #TplEntity
 *
 * Returns: the identifier of the entity
 */
const gchar *
tpl_entity_get_identifier (TplEntity *self)
{
  g_return_val_if_fail (TPL_IS_ENTITY (self), NULL);

  return self->priv->identifier;
}

/**
 * tpl_entity_get_entity_type:
 * @self: a #TplEntity
 *
 * Returns: the type of the entity
 */
TplEntityType
tpl_entity_get_entity_type (TplEntity *self)
{
  g_return_val_if_fail (TPL_IS_ENTITY (self), TPL_ENTITY_UNKNOWN);

  return self->priv->type;
}


/**
 * tpl_entity_get_avatar_token:
 * @self: a #TplEntity
 *
 * Returns: a token representing the avatar of the token, or %NULL
 */
const gchar *
tpl_entity_get_avatar_token (TplEntity *self)
{
  g_return_val_if_fail (TPL_IS_ENTITY (self), NULL);

  return self->priv->avatar_token;
}


void
_tpl_entity_set_alias (TplEntity *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_ENTITY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->alias == NULL);

  self->priv->alias = g_strdup (data);
}


void
_tpl_entity_set_identifier (TplEntity *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_ENTITY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->identifier == NULL);

  self->priv->identifier = g_strdup (data);
}


/* _tpl_entity_set_entity_type:
 * @self: a TplEntity instance
 * @data: the contact type for @self
 *
 * Set a entity type for @self.
 *
 * Note: %TPL_ENTITY_CONTACT and %TPL_ENTITY_GROUP are automatically set after
 * _tpl_entity_from_tp_contact() and #tpl_entity_from_chatroom_id(),
 * respectively. Though, the client will need to set %TPL_ENTITY_SELF after
 * those function calls when @self represents the owner of the account.
 *
 * @see #TplEntityType
 */
void
_tpl_entity_set_entity_type (TplEntity *self,
    TplEntityType data)
{
  g_return_if_fail (TPL_IS_ENTITY (self));

  self->priv->type = data;
}


void
_tpl_entity_set_avatar_token (TplEntity *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_ENTITY (self));
  g_return_if_fail (self->priv->avatar_token == NULL);
  /* data can be NULL, if no avatar_token is set */

  self->priv->avatar_token = g_strdup (data);
}
