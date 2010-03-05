/* TpBaseProtocol
 *
 * Copyright © 2007-2010 Collabora Ltd.
 * Copyright © 2007-2009 Nokia Corporation
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

#include <telepathy-glib/base-protocol.h>
#include <telepathy-glib/base-protocol-internal.h>

#include <dbus/dbus-protocol.h>
#include <telepathy-glib/telepathy-glib.h>

#define DEBUG_FLAG TP_DEBUG_PARAMS
#include "telepathy-glib/debug-internal.h"

/**
 * TpCMParamSpec:
 * @name: Name as passed over D-Bus
 * @dtype: D-Bus type signature. We currently support 16- and 32-bit integers
 *         (@gtype is INT), 16- and 32-bit unsigned integers (gtype is UINT),
 *         strings (gtype is STRING) and booleans (gtype is BOOLEAN).
 * @gtype: GLib type, derived from @dtype as above
 * @flags: Some combination of TP_CONN_MGR_PARAM_FLAG_foo
 * @def: Default value, as a (const gchar *) for string parameters, or
         using #GINT_TO_POINTER or #GUINT_TO_POINTER for integer parameters
 * @offset: Offset of the parameter in the opaque data structure, if
 *          appropriate. The member at that offset is expected to be a gint,
 *          guint, (gchar *) or gboolean, depending on @gtype. The default
 *          parameter setter, #tp_cm_param_setter_offset, uses this field.
 * @filter: A callback which is used to validate or normalize the user-provided
 *          value before it is written into the opaque data structure
 * @filter_data: Arbitrary opaque data intended for use by the filter function
 * @setter_data: Arbitrary opaque data intended for use by the setter function
 *               instead of or in addition to @offset.
 *
 * Structure representing a connection manager parameter, as accepted by
 * RequestConnection.
 *
 * In addition to the fields documented here, there is one gpointer field
 * which must currently be %NULL. A meaning may be defined for it in a
 * future version of telepathy-glib.
 */

/**
 * TpCMParamFilter:
 * @paramspec: The parameter specification. The filter is likely to use
 *  name (for the error message if the value is invalid) and filter_data.
 * @value: The value for that parameter provided by the user.
 *  May be changed to contain a different value of the same type, if
 *  some sort of normalization is required
 * @error: Used to raise %TP_ERROR_INVALID_ARGUMENT if the given value is
 *  rejected
 *
 * Signature of a callback used to validate and/or normalize user-provided
 * CM parameter values.
 *
 * Returns: %TRUE to accept, %FALSE (with @error set) to reject
 */

/**
 * TpCMParamSetter:
 * @paramspec: The parameter specification.  The setter is likely to use
 *  some combination of the name, offset and setter_data fields.
 * @value: The value for that parameter provided by the user.
 * @params: An opaque data structure, created by
 *  #TpCMProtocolSpec.params_new.
 *
 * The signature of a callback used to set a parameter within the opaque
 * data structure used for a protocol.
 *
 * Since: 0.7.0
 */

static GValue *
param_default_value (const TpCMParamSpec *param)
{
  GValue *value;

  value = tp_g_value_slice_new (param->gtype);

  /* If HAS_DEFAULT is false, we don't really care what the value is, so we'll
   * just use whatever's in the user-supplied param spec. As long as we're
   * careful to accept NULL, that should be fine. */

  switch (param->dtype[0])
    {
      case DBUS_TYPE_STRING:
        g_assert (param->gtype == G_TYPE_STRING);
        if (param->def == NULL)
          g_value_set_static_string (value, "");
        else
          g_value_set_static_string (value, param->def);
        break;

      case DBUS_TYPE_INT16:
      case DBUS_TYPE_INT32:
        g_assert (param->gtype == G_TYPE_INT);
        g_value_set_int (value, GPOINTER_TO_INT (param->def));
        break;

      case DBUS_TYPE_UINT16:
      case DBUS_TYPE_UINT32:
        g_assert (param->gtype == G_TYPE_UINT);
        g_value_set_uint (value, GPOINTER_TO_UINT (param->def));
        break;

      case DBUS_TYPE_UINT64:
        g_assert (param->gtype == G_TYPE_UINT64);
        g_value_set_uint64 (value, param->def == NULL ? 0
            : *(const guint64 *) param->def);
        break;

      case DBUS_TYPE_INT64:
        g_assert (param->gtype == G_TYPE_INT64);
        g_value_set_int64 (value, param->def == NULL ? 0
            : *(const gint64 *) param->def);
        break;

      case DBUS_TYPE_DOUBLE:
        g_assert (param->gtype == G_TYPE_DOUBLE);
        g_value_set_double (value, param->def == NULL ? 0.0
            : *(const double *) param->def);
        break;

      case DBUS_TYPE_OBJECT_PATH:
        g_assert (param->gtype == DBUS_TYPE_G_OBJECT_PATH);
        g_value_set_static_boxed (value, param->def == NULL ? "/"
            : param->def);
        break;

      case DBUS_TYPE_ARRAY:
        switch (param->dtype[1])
          {
          case DBUS_TYPE_STRING:
            g_assert (param->gtype == G_TYPE_STRV);
            g_value_set_static_boxed (value, param->def);
            break;

          case DBUS_TYPE_BYTE:
            g_assert (param->gtype == DBUS_TYPE_G_UCHAR_ARRAY);
            if (param->def == NULL)
              {
                GArray *array = g_array_new (FALSE, FALSE, sizeof (guint8));
                g_value_take_boxed (value, array);
              }
            else
              {
                g_value_set_static_boxed (value, param->def);
              }
            break;

          default:
            ERROR ("encountered unknown type %s on argument %s",
                param->dtype, param->name);
          }
        break;

      case DBUS_TYPE_BOOLEAN:
        g_assert (param->gtype == G_TYPE_BOOLEAN);
        g_value_set_boolean (value, GPOINTER_TO_INT (param->def));
        break;

      default:
        ERROR ("encountered unknown type %s on argument %s",
            param->dtype, param->name);
    }

  return value;
}

void
_tp_cm_param_spec_set_default (const TpCMParamSpec *paramspec,
    const TpCMParamSetter set_param,
    gpointer params)
{
  GValue *value = param_default_value (paramspec);

  set_param (paramspec, value, params);
  tp_g_value_slice_free (value);
}

GValueArray *
_tp_cm_param_spec_to_dbus (const TpCMParamSpec *paramspec)
{
  GValueArray *susv;
  GValue *value = param_default_value (paramspec);

  susv = tp_value_array_build (4,
      G_TYPE_STRING, paramspec->name,
      G_TYPE_UINT, paramspec->flags,
      G_TYPE_STRING, paramspec->dtype,
      G_TYPE_VALUE, value,
      G_TYPE_INVALID);

  tp_g_value_slice_free (value);

  return susv;
}

/**
 * tp_cm_param_filter_uint_nonzero:
 * @paramspec: The parameter specification for a guint parameter
 * @value: A GValue containing a guint, which will not be altered
 * @error: Used to return an error if the guint is 0
 *
 * A #TpCMParamFilter which rejects zero, useful for server port numbers.
 *
 * Returns: %TRUE to accept, %FALSE (with @error set) to reject
 */
gboolean
tp_cm_param_filter_uint_nonzero (const TpCMParamSpec *paramspec,
                                 GValue *value,
                                 GError **error)
{
  if (g_value_get_uint (value) == 0)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Account parameter '%s' may not be set to zero",
          paramspec->name);
      return FALSE;
    }
  return TRUE;
}

/**
 * tp_cm_param_filter_string_nonempty:
 * @paramspec: The parameter specification for a string parameter
 * @value: A GValue containing a string, which will not be altered
 * @error: Used to return an error if the string is empty
 *
 * A #TpCMParamFilter which rejects empty strings.
 *
 * Returns: %TRUE to accept, %FALSE (with @error set) to reject
 */
gboolean
tp_cm_param_filter_string_nonempty (const TpCMParamSpec *paramspec,
                                    GValue *value,
                                    GError **error)
{
  const gchar *str = g_value_get_string (value);

  if (str == NULL || str[0] == '\0')
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Account parameter '%s' may not be set to an empty string",
          paramspec->name);
      return FALSE;
    }
  return TRUE;
}

/**
 * SECTION:base-protocol
 * @title: TpBaseProtocol
 * @short_description: base class for #TpSvcProtocol implementations
 * @see_also: #TpBaseConnectionManager, #TpSvcProtocol
 *
 * Base class for Telepathy Protocol objects.
 *
 * Since: 0.11.UNRELEASED
 */

/**
 * TpBaseProtocol:
 *
 * An object providing static details of the implementation of one real-time
 * communications protocol.
 *
 * Since: 0.11.UNRELEASED
 */

/**
 * TpBaseProtocolClass:
 *
 * The class of a #TpBaseProtocol.
 *
 * Since: 0.11.UNRELEASED
 */

G_DEFINE_ABSTRACT_TYPE(TpBaseProtocol, tp_base_protocol, G_TYPE_OBJECT);

struct _TpBaseProtocolPrivate
{
  gchar *name;
};

enum
{
    PROP_NAME = 1,
    N_PROPS
};

static void
tp_base_protocol_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->priv->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_base_protocol_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;

  switch (property_id)
    {
    case PROP_NAME:
      g_assert (self->priv->name == NULL);    /* construct-only */
      self->priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_base_protocol_finalize (GObject *object)
{
  TpBaseProtocol *self = (TpBaseProtocol *) object;
  GObjectFinalizeFunc finalize =
    ((GObjectClass *) tp_base_protocol_parent_class)->finalize;

  g_free (self->priv->name);

  if (finalize != NULL)
    finalize (object);
}

static void
tp_base_protocol_class_init (TpBaseProtocolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpBaseProtocolPrivate));

  object_class->get_property = tp_base_protocol_get_property;
  object_class->set_property = tp_base_protocol_set_property;
  object_class->finalize = tp_base_protocol_finalize;

  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name",
        "Name of this protocol",
        "The Protocol from telepathy-spec, such as 'jabber' or 'local-xmpp'",
        NULL,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
tp_base_protocol_init (TpBaseProtocol *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_BASE_PROTOCOL,
      TpBaseProtocolPrivate);
}
