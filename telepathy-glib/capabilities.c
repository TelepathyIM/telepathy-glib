/* Object representing the capabilities a Connection or a Contact supports.
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
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
 */

#include "telepathy-glib/capabilities.h"
#include "telepathy-glib/capabilities-internal.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CONNECTION
#include "telepathy-glib/debug-internal.h"


/**
 * SECTION:capabilities
 * @title: TpCapabilities
 * @short_description: object representing capabilities
 *
 * #TpCapabilities objects represent the capabilities a #TpConnection
 * or a #TpContact supports.
 *
 * Since: 0.11.UNRELEASED
 */

/**
 * TpCapabilities:
 *
 * An object representing capabilities a #TpConnection or #TpContact supports.
 *
 * Since: 0.11.UNRELEASED
 */

struct _TpCapabilitiesClass {
    /*<private>*/
    GObjectClass parent_class;
};

struct _TpCapabilities {
    /*<private>*/
    GObject parent;
    TpCapabilitiesPrivate *priv;
};

G_DEFINE_TYPE (TpCapabilities, tp_capabilities, G_TYPE_OBJECT);

enum {
    PROP_CHANNEL_CLASSES = 1,
    PROP_CONTACT_SPECIFIC,
    N_PROPS
};

struct _TpCapabilitiesPrivate {
    GPtrArray *classes;
    gboolean contact_specific;
};

/**
 * tp_capabilities_get_channel_classes:
 * @self: a #TpCapabilities object
 *
 * <!-- -->
 *
 * Returns: the same #GPtrArray as the #TpCapabilities:channel-classes property
 *
 * Since: 0.11.UNRELEASED
 */
GPtrArray *
tp_capabilities_get_channel_classes (TpCapabilities *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->priv->classes;
}

/**
 * tp_capabilities_is_specific_to_contact:
 * @self: a #TpCapabilities object
 *
 * <!-- -->
 *
 * Returns: the same #gboolean as the #TpCapabilities:contact-specific property
 *
 * Since: 0.11.UNRELEASED
 */
gboolean
tp_capabilities_is_specific_to_contact (TpCapabilities *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->priv->contact_specific;
}

static void
tp_capabilities_constructed (GObject *object)
{
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_capabilities_parent_class)->constructed;
  TpCapabilities *self = TP_CAPABILITIES (object);

  g_assert (self->priv->classes != NULL);

  if (chain_up != NULL)
    chain_up (object);
}

static void
tp_capabilities_dispose (GObject *object)
{
  TpCapabilities *self = TP_CAPABILITIES (object);

  if (self->priv->classes != NULL)
    {
      g_boxed_free (TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST,
          self->priv->classes);
      self->priv->classes = NULL;
    }

  ((GObjectClass *) tp_capabilities_parent_class)->dispose (object);
}

static void
tp_capabilities_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpCapabilities *self = TP_CAPABILITIES (object);

  switch (property_id)
    {
    case PROP_CHANNEL_CLASSES:
      g_value_set_boxed (value, self->priv->classes);
      break;

    case PROP_CONTACT_SPECIFIC:
      g_value_set_boolean (value, self->priv->contact_specific);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_capabilities_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpCapabilities *self = TP_CAPABILITIES (object);

  switch (property_id)
    {
    case PROP_CHANNEL_CLASSES:
      self->priv->classes = g_value_dup_boxed (value);
      break;

    case PROP_CONTACT_SPECIFIC:
      self->priv->contact_specific = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_capabilities_class_init (TpCapabilitiesClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpCapabilitiesPrivate));
  object_class->get_property = tp_capabilities_get_property;
  object_class->set_property = tp_capabilities_set_property;
  object_class->constructed = tp_capabilities_constructed;
  object_class->dispose = tp_capabilities_dispose;

  /**
   * TpCapabilities:channel-classes:
   *
   * The underlying data structure used by Telepathy to represent the
   * requests that can succeed.
   *
   * This can be used by advanced clients to determine whether an unusually
   * complex request would succeed. See the Telepathy D-Bus API Specification
   * for details of how to interpret the returned #GPtrArray of
   * #TP_STRUCT_TYPE_REQUESTABLE_CHANNEL_CLASS.
   *
   * The higher-level methods like
   * tp_capabilities_supports_text_chats() are likely to be more useful to
   * the majority of clients.
   */
  param_spec = g_param_spec_boxed ("channel-classes",
      "GPtrArray of TP_STRUCT_TYPE_REQUESTABLE_CHANNEL_CLASS",
      "The channel classes supported",
      TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_CLASSES,
      param_spec);

  /**
   * TpCapabilities:contact-specific:
   *
   * Whether this object accurately describes the capabilities of a particular
   * contact, or if it's only a guess based on the capabilities of the
   * underlying connection.
   */
  param_spec = g_param_spec_boolean ("contact-specific",
      "contact specific",
      "TRUE if this object describes the capabilities of a particular contact",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONTACT_SPECIFIC,
      param_spec);
}

static void
tp_capabilities_init (TpCapabilities *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CAPABILITIES,
      TpCapabilitiesPrivate);
}

TpCapabilities *
_tp_capabilities_new (GPtrArray *classes,
    gboolean contact_specific)
{
  return g_object_new (TP_TYPE_CAPABILITIES,
      "channel-classes", classes,
      "contact-specific", contact_specific,
      NULL);
}
