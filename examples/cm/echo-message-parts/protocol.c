/*
 * protocol.c - an example Protocol
 *
 * Copyright © 2007-2010 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "protocol.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

#include "conn.h"
#include "im-manager.h"

struct _ExampleEcho2ProtocolPrivate
{
  GPtrArray *params;
};

static void addressing_iface_init (TpProtocolAddressingInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ExampleEcho2Protocol, example_echo_2_protocol,
    TP_TYPE_BASE_PROTOCOL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_PROTOCOL_ADDRESSING, addressing_iface_init))

const gchar * const supported_avatar_mime_types[] = {
  "image/png",
  "image/jpeg",
  "image/gif",
  NULL };

const gchar * const addressing_vcard_fields[] = {
  "x-jabber",
  "tel",
  NULL };

const gchar * const addressing_uri_schemes[] = {
  "xmpp",
  "tel",
  NULL };

static void
example_echo_2_protocol_init (
    ExampleEcho2Protocol *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EXAMPLE_TYPE_ECHO_2_PROTOCOL,
      ExampleEcho2ProtocolPrivate);
}

static GPtrArray *
dup_parameters (TpBaseProtocol *base)
{
  ExampleEcho2Protocol *self = (ExampleEcho2Protocol *) base;

  if (self->priv->params == NULL)
    {
      self->priv->params = g_ptr_array_new_full (1,
          (GDestroyNotify) tp_cm_param_spec_unref);

      g_ptr_array_add (self->priv->params,
          tp_cm_param_spec_new ("account",
              TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER,
              g_variant_new_string (""),
              tp_cm_param_filter_string_nonempty, NULL, NULL));
    }

  return g_ptr_array_ref (self->priv->params);
}

static TpBaseConnection *
new_connection (TpBaseProtocol *protocol,
    GHashTable *asv,
    GError **error)
{
  ExampleEcho2Connection *conn;
  const gchar *account;

  account = tp_asv_get_string (asv, "account");

  if (account == NULL || account[0] == '\0')
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "The 'account' parameter is required");
      return NULL;
    }

  conn = EXAMPLE_ECHO_2_CONNECTION (
      g_object_new (EXAMPLE_TYPE_ECHO_2_CONNECTION,
        "account", account,
        "protocol", tp_base_protocol_get_name (protocol),
        NULL));

  return (TpBaseConnection *) conn;
}

gchar *
example_echo_2_protocol_normalize_contact (const gchar *id, GError **error)
{
  if (id[0] == '\0')
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_HANDLE,
          "ID must not be empty");
      return NULL;
    }

  return g_utf8_strdown (id, -1);
}

static gchar *
normalize_contact (TpBaseProtocol *self G_GNUC_UNUSED,
    const gchar *contact,
    GError **error)
{
  return example_echo_2_protocol_normalize_contact (contact, error);
}

static gchar *
identify_account (TpBaseProtocol *self G_GNUC_UNUSED,
    GHashTable *asv,
    GError **error)
{
  const gchar *account = tp_asv_get_string (asv, "account");

  if (account != NULL)
    return g_utf8_strdown (account, -1);

  g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
      "'account' parameter not given");
  return NULL;
}

static void
get_connection_details (TpBaseProtocol *self G_GNUC_UNUSED,
    GStrv *connection_interfaces,
    GType **channel_managers,
    gchar **icon_name,
    gchar **english_name,
    gchar **vcard_field)
{
  if (connection_interfaces != NULL)
    {
      *connection_interfaces = g_strdupv (
          (GStrv) example_echo_2_connection_get_possible_interfaces ());
    }

  if (channel_managers != NULL)
    {
      GType types[] = { EXAMPLE_TYPE_ECHO_2_IM_MANAGER, G_TYPE_INVALID };

      *channel_managers = g_memdup (types, sizeof (types));
    }

  if (icon_name != NULL)
    {
      /* a real protocol would use its own icon name - for this example we
       * borrow the one from ICQ */
      *icon_name = g_strdup ("im-icq");
    }

  if (english_name != NULL)
    {
      /* in a real protocol this would be "ICQ" or
       * "Windows Live Messenger (MSN)" or something */
      *english_name = g_strdup ("Echo II example");
    }

  if (vcard_field != NULL)
    {
      /* in a real protocol this would be "tel" or "x-jabber" or something */
      *vcard_field = g_strdup ("x-telepathy-example");
    }
}

static gboolean
get_avatar_details (TpBaseProtocol *self,
    GStrv *supported_mime_types,
    guint *min_height,
    guint *min_width,
    guint *recommended_height,
    guint *recommended_width,
    guint *max_height,
    guint *max_width,
    guint *max_bytes)
{
  if (supported_mime_types != NULL)
    *supported_mime_types = g_strdupv ((GStrv) supported_avatar_mime_types);

  if (min_height != NULL)
    *min_height = 32;

  if (min_width != NULL)
    *min_width = 32;

  if (recommended_height != NULL)
    *recommended_height = 64;

  if (recommended_width != NULL)
    *recommended_width = 64;

  if (max_height != NULL)
    *max_height = 96;

  if (max_width != NULL)
    *max_width = 96;

  if (max_bytes != NULL)
    *max_bytes = 37748736;

  return TRUE;
}

static GStrv
dup_supported_uri_schemes (TpBaseProtocol *self)
{
  return g_strdupv ((GStrv) addressing_uri_schemes);
}

static GStrv
dup_supported_vcard_fields (TpBaseProtocol *self)
{
  return g_strdupv ((GStrv) addressing_vcard_fields);
}

static gchar *
normalize_vcard_address (TpBaseProtocol *self,
    const gchar *vcard_field,
    const gchar *vcard_address,
    GError **error)
{
  if (g_ascii_strcasecmp (vcard_field, "x-jabber") == 0)
    {
      /* This is not really how you normalize a JID but it's good enough
       * for an example. In real life you'd do syntax-checking beyond
       * "is it empty?", stringprep, and so on. Here, we just assume
       * non-empty means valid, and lower-case means normalized. */

      if (tp_str_empty (vcard_address))
        {
          g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
              "The empty string is not a valid JID");
          return NULL;
        }

      return g_utf8_strdown (vcard_address, -1);
    }
  else
    {
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "Don't know how to normalize vCard field: %s", vcard_field);
      return NULL;
    }
}

static gchar *
normalize_contact_uri (TpBaseProtocol *self,
    const gchar *uri,
    GError **error)
{
  gchar *scheme = g_uri_parse_scheme (uri);

  if (g_ascii_strcasecmp (scheme, "xmpp") == 0)
    {
      gchar *ret = NULL;
      gchar *id;

      id = normalize_vcard_address (self, "x-jabber", uri + 5, error);

      if (id != NULL)
        ret = g_strdup_printf ("%s:%s", scheme, id);

      g_free (scheme);
      g_free (id);
      return ret;
    }
  else if (scheme == NULL)
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "Not a valid URI: %s", uri);
      return NULL;
    }
  else
    {
      g_set_error (error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
          "Don't know how to normalize URIs of that scheme: %s", scheme);
      g_free (scheme);
      return NULL;
    }
}

static void
finalize (GObject *object)
{
  ExampleEcho2Protocol *self = (ExampleEcho2Protocol *) object;

  g_clear_pointer (&self->priv->params, g_ptr_array_unref);

  G_OBJECT_CLASS (example_echo_2_protocol_parent_class)->finalize (object);
}

static void
example_echo_2_protocol_class_init (
    ExampleEcho2ProtocolClass *klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;
  TpBaseProtocolClass *base_class = (TpBaseProtocolClass *) klass;

  g_type_class_add_private (klass, sizeof (ExampleEcho2ProtocolPrivate));

  oclass->finalize = finalize;

  base_class->dup_parameters = dup_parameters;
  base_class->new_connection = new_connection;

  base_class->normalize_contact = normalize_contact;
  base_class->identify_account = identify_account;
  base_class->get_connection_details = get_connection_details;
  base_class->get_avatar_details = get_avatar_details;
}

static void
addressing_iface_init (TpProtocolAddressingInterface *iface)
{
  iface->dup_supported_vcard_fields = dup_supported_vcard_fields;
  iface->dup_supported_uri_schemes = dup_supported_uri_schemes;
  iface->normalize_vcard_address = normalize_vcard_address;
  iface->normalize_contact_uri = normalize_contact_uri;
}
