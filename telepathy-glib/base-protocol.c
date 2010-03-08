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
 * @parent_class: the parent class
 * @is_stub: if %TRUE, this protocol will not be advertised on D-Bus (for
 *  internal use by #TpBaseConnection)
 * @get_parameters: a callback used to implement
 *  tp_base_protocol_get_parameters(), which all subclasses must provide;
 *  see the documentation of that method for details
 * @new_connection: a callback used to implement
 *  tp_base_protocol_new_connection(), which all subclasses must provide;
 *  see the documentation of that method for details
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

/**
 * tp_base_protocol_get_parameters:
 * @self: a Protocol object
 *
 * Returns the parameters supported by this protocol, as an array of structs
 * which must remain valid at least as long as @self exists (it will typically
 * be a global static array).
 *
 * Returns: (transfer none) (array zero-terminated=1): a description of the
 *  parameters supported by this protocol
 */
const TpCMParamSpec *
tp_base_protocol_get_parameters (TpBaseProtocol *self)
{
  TpBaseProtocolClass *cls = TP_BASE_PROTOCOL_GET_CLASS (self);

  g_return_val_if_fail (cls != NULL, NULL);
  g_return_val_if_fail (cls->get_parameters != NULL, NULL);

  return cls->get_parameters (self);
}

static gboolean
_tp_cm_param_spec_check_all_allowed (const TpCMParamSpec *parameters,
    GHashTable *asv,
    GError **error)
{
  GHashTable *tmp = g_hash_table_new (g_str_hash, g_str_equal);
  const TpCMParamSpec *iter;

  tp_g_hash_table_update (tmp, asv, NULL, NULL);

  for (iter = parameters; iter->name != NULL; iter++)
    {
      g_hash_table_remove (tmp, iter->name);
    }

  if (g_hash_table_size (tmp) != 0)
    {
      gchar *error_txt;
      GString *error_str = g_string_new ("unknown parameters provided:");
      GHashTableIter h_iter;
      gpointer k;

      g_hash_table_iter_init (&h_iter, tmp);

      while (g_hash_table_iter_next (&h_iter, &k, NULL))
        {
          g_string_append_c (error_str, ' ');
          g_string_append (error_str, k);
        }

      error_txt = g_string_free (error_str, FALSE);

      DEBUG ("%s", error_txt);
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "%s", error_txt);
      g_free (error_txt);
      return FALSE;
    }

  return TRUE;
}

static GValue *
_tp_cm_param_spec_coerce (const TpCMParamSpec *param_spec,
    GHashTable *asv,
    GError **error)
{
  const gchar *name = param_spec->name;
  const GValue *value = tp_asv_lookup (asv, name);

  if (tp_asv_lookup (asv, name) == NULL)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "%s not found in parameters", name);
      return NULL;
    }

  switch (param_spec->dtype[0])
    {
    case DBUS_TYPE_BOOLEAN:
    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_STRING:
    case DBUS_TYPE_ARRAY:
        {
          /* These types only accept an exactly-matching GType. */

          if (G_VALUE_TYPE (value) != param_spec->gtype)
            {
              g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                  "%s has type %s, but %s was expected",
                  name, G_VALUE_TYPE_NAME (value),
                  g_type_name (param_spec->gtype));
              return NULL;
            }

          return tp_g_value_slice_dup (value);
        }

    case DBUS_TYPE_INT16:
    case DBUS_TYPE_INT32:
        {
          /* Coerce any sensible integer to G_TYPE_INT */
          gboolean valid;
          gint i;

          i = tp_asv_get_int32 (asv, name, &valid);

          if (!valid)
            {
              g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                  "%s has a non-integer type or is out of range (type=%s)",
                  name, G_VALUE_TYPE_NAME (value));
              return NULL;
            }

          if (param_spec->dtype[0] == DBUS_TYPE_INT16 &&
              (i < -0x8000 || i > 0x7fff))
            {
              g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                  "%s is out of range for a 16-bit signed integer", name);
              return NULL;
            }

          return tp_g_value_slice_new_int (i);
        }

    case DBUS_TYPE_BYTE:
    case DBUS_TYPE_UINT16:
    case DBUS_TYPE_UINT32:
        {
          /* Coerce any sensible integer to G_TYPE_UINT */
          gboolean valid;
          guint i;

          i = tp_asv_get_uint32 (asv, name, &valid);

          if (!valid)
            {
              g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                  "%s has a non-integer type or is out of range (type=%s)",
                  name, G_VALUE_TYPE_NAME (value));
              return NULL;
            }

          if (param_spec->dtype[0] == DBUS_TYPE_BYTE && i > 0xff)
            {
              g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                  "%s is out of range for a byte", name);
              return NULL;
            }

          if (param_spec->dtype[0] == DBUS_TYPE_UINT16 && i > 0xffff)
            {
              g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                  "%s is out of range for a 16-bit unsigned integer", name);
              return NULL;
            }

          if (param_spec->dtype[0] == DBUS_TYPE_BYTE)
            return tp_g_value_slice_new_byte (i);
          else
            return tp_g_value_slice_new_uint (i);
        }

    case DBUS_TYPE_INT64:
        {
          /* Coerce any sensible integer to G_TYPE_INT64 */
          gboolean valid;
          gint64 i;

          i = tp_asv_get_int64 (asv, name, &valid);

          if (!valid)
            {
              g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                  "%s is not a valid 64-bit signed integer (type=%s)", name,
                  G_VALUE_TYPE_NAME (value));
              return NULL;
            }

          return tp_g_value_slice_new_int64 (i);
        }

    case DBUS_TYPE_UINT64:
        {
          /* Coerce any sensible integer to G_TYPE_UINT64 */
          gboolean valid;
          guint64 i;

          i = tp_asv_get_uint64 (asv, name, &valid);

          if (!valid)
            {
              g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                  "%s is not a valid 64-bit unsigned integer (type=%s)", name,
                  G_VALUE_TYPE_NAME (value));
              return NULL;
            }

          return tp_g_value_slice_new_uint64 (i);
        }

    case DBUS_TYPE_DOUBLE:
        {
          /* Coerce any sensible number to G_TYPE_DOUBLE */
          gboolean valid;
          gdouble d;

          d = tp_asv_get_double (asv, name, &valid);

          if (!valid)
            {
              g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                  "%s is not a valid double (type=%s)", name,
                  G_VALUE_TYPE_NAME (value));
              return NULL;
            }

          return tp_g_value_slice_new_double (d);
        }

    default:
        {
          g_error ("%s: encountered unhandled D-Bus type %s on argument %s",
              G_STRFUNC, param_spec->dtype, param_spec->name);
        }
    }

  g_assert_not_reached ();
}

/**
 * tp_base_protocol_new_connection:
 * @self: a Protocol object
 * @asv: (transfer none) (element-type utf8 GObject.Value): the parameters
 *  provided via D-Bus
 * @error: used to return an error if %NULL is returned
 *
 * Create a new connection using the #TpBaseProtocolClass.get_parameters and
 * #TpBaseProtocolClass.new_connection implementations provided by a subclass.
 * This is used to implement the RequestConnection() D-Bus method.
 *
 * If the parameters in @asv do not fit the result of @get_parameters (unknown
 * parameters are given, types are inappropriate, required parameters are
 * not given, or a #TpCMParamSpec.filter fails), then this method raises an
 * error and @new_connection is not called.
 *
 * Otherwise, @new_connection is called. Its @asv argument is a copy of the
 * @asv given to this method, with default values for missing parameters
 * filled in where available, and parameters' types converted to the #GType
 * specified by #TpCMParamSpec.gtype.
 *
 * Returns: a new connection, or %NULL on error
 */
TpBaseConnection *
tp_base_protocol_new_connection (TpBaseProtocol *self,
    GHashTable *asv,
    GError **error)
{
  TpBaseProtocolClass *cls = TP_BASE_PROTOCOL_GET_CLASS (self);
  GHashTable *combined;
  const TpCMParamSpec *parameters;
  guint i;
  TpBaseConnection *conn = NULL;
  guint mandatory_flag;

  g_return_val_if_fail (cls != NULL, NULL);
  g_return_val_if_fail (cls->new_connection != NULL, NULL);

  parameters = tp_base_protocol_get_parameters (self);

  combined = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) tp_g_value_slice_free);

  if (!_tp_cm_param_spec_check_all_allowed (parameters, asv, error))
    goto finally;

  if (tp_asv_get_boolean (asv, "register", NULL))
    {
      mandatory_flag = TP_CONN_MGR_PARAM_FLAG_REGISTER;
    }
  else
    {
      mandatory_flag = TP_CONN_MGR_PARAM_FLAG_REQUIRED;
    }

  for (i = 0; parameters[i].name != NULL; i++)
    {
      const gchar *name = parameters[i].name;

      if (tp_asv_lookup (asv, name) != NULL)
        {
          /* coerce to the expected type */
          GValue *coerced = _tp_cm_param_spec_coerce (parameters + i, asv,
              error);

          if (coerced == NULL)
            goto finally;

          if (G_UNLIKELY (G_VALUE_TYPE (coerced) != parameters[i].gtype))
            {
              g_error ("parameter %s should have been coerced to %s, got %s",
                  name, g_type_name (parameters[i].gtype),
                    G_VALUE_TYPE_NAME (coerced));
            }

          if (parameters[i].filter != NULL)
            {
              if (!(parameters[i].filter (parameters + i, coerced, error)))
                {
                  DEBUG ("parameter %s rejected by filter function", name);
                  tp_g_value_slice_free (coerced);
                  goto finally;
                }
            }

          if (G_UNLIKELY (G_VALUE_TYPE (coerced) != parameters[i].gtype))
            {
              g_error ("parameter %s filter changed its type from %s to %s",
                  name, g_type_name (parameters[i].gtype),
                    G_VALUE_TYPE_NAME (coerced));
            }

          DEBUG ("using specified value for %s", name);
          g_hash_table_insert (combined, g_strdup (name), coerced);
        }
      else if ((parameters[i].flags & mandatory_flag) != 0)
        {
          DEBUG ("missing mandatory account parameter %s", name);
          g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
              "missing mandatory account parameter %s",
              name);
          goto finally;
        }
      else if ((parameters[i].flags & TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT) != 0)
        {
          GValue *value = param_default_value (parameters + i);

          DEBUG ("using default value for %s", name);
          g_hash_table_insert (combined, g_strdup (name), value);
        }
      else
        {
          DEBUG ("no default value for %s", name);
        }
    }

  conn = cls->new_connection (self, combined, error);

finally:
  if (combined != NULL)
    g_hash_table_unref (combined);

  return conn;
}
