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
#include "entity.h"
#include "entity-internal.h"

#include <telepathy-glib/telepathy-glib.h>

#define DEBUG_FLAG TPL_DEBUG_ENTITY
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

/**
 * SECTION:entity
 * @title: TplEntity
 * @short_description: Representation of a contact or room
 *
 * An object representing a contact or room.
 */

/**
 * TplEntity:
 *
 * An object representing a contact or room.
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
  PROP_TYPE,
  PROP_IDENTIFIER,
  PROP_ALIAS,
  PROP_AVATAR_TOKEN
};

static const gchar * entity_types[] = {
    "unknown",
    "contact",
    "room",
    "self"
};


static void
tpl_entity_finalize (GObject *obj)
{
  TplEntity *self = TPL_ENTITY (obj);
  TplEntityPriv *priv = self->priv;

  tp_clear_pointer (&priv->alias, g_free);
  tp_clear_pointer (&priv->identifier, g_free);
  tp_clear_pointer (&priv->avatar_token, g_free);

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
      case PROP_TYPE:
        g_value_set_int (value, priv->type);
        break;
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
  TplEntityPriv *priv = TPL_ENTITY (object)->priv;

  switch (param_id)
    {
      case PROP_TYPE:
        priv->type = g_value_get_int (value);
        break;
      case PROP_IDENTIFIER:
        g_assert (priv->identifier == NULL);
        priv->identifier = g_value_dup_string (value);
        break;
      case PROP_ALIAS:
        g_assert (priv->alias == NULL);
        priv->alias = g_value_dup_string (value);
        break;
      case PROP_AVATAR_TOKEN:
        g_assert (priv->avatar_token == NULL);
        priv->avatar_token = g_value_dup_string (value);
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
   * TplEntity:type:
   *
   * The entity's type (see #TplEntityType).
   */
  param_spec = g_param_spec_int ("type",
      "Type",
      "The entity's type",
      TPL_ENTITY_UNKNOWN,
      TPL_ENTITY_SELF,
      TPL_ENTITY_UNKNOWN,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TYPE, param_spec);

  /**
   * TplEntity:identifier:
   *
   * The entity's identifier
   */
  param_spec = g_param_spec_string ("identifier",
      "Identifier",
      "The entity's identifier",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
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
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
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
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_AVATAR_TOKEN, param_spec);

  g_type_class_add_private (object_class, sizeof (TplEntityPriv));
}


static void
tpl_entity_init (TplEntity *self)
{
  TplEntityPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_ENTITY, TplEntityPriv);

  self->priv = priv;
}

TplEntity *
tpl_entity_new (const gchar *id,
    TplEntityType type,
    const gchar *alias,
    const gchar *avatar_token)
{
  TplEntity *ret;

  g_return_val_if_fail (!TPL_STR_EMPTY (id), NULL);

  ret = g_object_new (TPL_TYPE_ENTITY,
      "identifier", id,
      "type", type,
      "alias", alias == NULL ? id : alias,
      "avatar-token", avatar_token == NULL ? "" : avatar_token,
      NULL);

  switch (type)
    {
    case TPL_ENTITY_ROOM:
      DEBUG ("Room id: %s", id);
      break;
    case TPL_ENTITY_CONTACT:
      DEBUG ("Contact id: %s, tok: %s", id, avatar_token);
      break;
    case TPL_ENTITY_SELF:
      DEBUG ("Self id: %s, tok: %s", id, avatar_token);
      break;
    case TPL_ENTITY_UNKNOWN:
      DEBUG ("Unknown entity.");
      break;
    default:
      g_warning ("Unknown entity type %i", type);
      g_object_unref (ret);
      ret = NULL;
    }

  return ret;
}

/**
 * tpl_entity_new_from_room_id:
 * @room_id: the room id which will be the identifier for the entity
 *
 * Returns: a TplEntity instance with identifier, alias copied from
 * @room_id. It also sets %TPL_ENTITY_ROOM as type for
 * the #TplEntity returned.
 */
TplEntity *
tpl_entity_new_from_room_id (const gchar *room_id)
{
  return tpl_entity_new (room_id, TPL_ENTITY_ROOM, NULL, NULL);
}


/**
 * tpl_entity_new_from_tp_contact:
 * @contact: the TpContact instance to create the TplEntity from
 * @type: the #TplEntity type
 *
 * Returns: a TplEntity instance with identifier, alias and
 * avatar's token copied. Type parameter is useful to differentiate between
 * normal contact and self contact, thus only %TPL_ENTITY_CONTACT and
 * %TPL_ENTITY_SELF are accepted. If contact is %NULL, an entity of type
 * %TPL_ENTITY_UNKNOWN with id set to "unknown" is returned.
 */
TplEntity *
tpl_entity_new_from_tp_contact (TpContact *contact,
    TplEntityType type)
{
  g_return_val_if_fail (contact == NULL || TP_IS_CONTACT (contact), NULL);
  g_return_val_if_fail (type == TPL_ENTITY_CONTACT || type == TPL_ENTITY_SELF,
      NULL);

  if (contact != NULL)
    return tpl_entity_new (tp_contact_get_identifier (contact),
        type,
        tp_contact_get_alias (contact),
        tp_contact_get_avatar_token (contact));
  else
    return tpl_entity_new ("unknown", TPL_ENTITY_UNKNOWN, NULL, NULL);
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


/*
 * _tpl_entity_compare:
 * @a: a #TplEntity
 * @b: a #TplEntity
 *
 * Compares @a and @b.
 *
 * Returns: 0 if a == b, -1 if a < b, 1 otherwise.
 */
gint
_tpl_entity_compare (TplEntity *a,
    TplEntity *b)
{
  g_return_val_if_fail (TPL_IS_ENTITY (a), TPL_IS_ENTITY (b) ? -1 : 0);
  g_return_val_if_fail (TPL_IS_ENTITY (b), 1);

  if (tpl_entity_get_entity_type (a) == tpl_entity_get_entity_type (b))
    return g_strcmp0 (tpl_entity_get_identifier (a),
        tpl_entity_get_identifier (b));
  else if (tpl_entity_get_entity_type (a) < tpl_entity_get_entity_type (b))
    return -1;
  else
    return 1;
}


TplEntityType
_tpl_entity_type_from_str (const gchar *type_str)
{
  guint i;
  for (i = 0; i < G_N_ELEMENTS (entity_types); ++i)
    if (!tp_strdiff (type_str, entity_types[i]))
      return (TplEntityType) i;

  /* default case */
  return TPL_ENTITY_UNKNOWN;
}


const gchar *
_tpl_entity_type_to_str (TplEntityType type)
{
  g_return_val_if_fail (G_N_ELEMENTS (entity_types) >= type, "unknown");
  return entity_types[type];
}
