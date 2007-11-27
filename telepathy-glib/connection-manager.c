/*
 * connection-manager.c - proxy for a Telepathy connection manager
 *
 * Copyright (C) 2007 Collabora Ltd.
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

#include "telepathy-glib/connection-manager.h"

#include <string.h>

#include "telepathy-glib/defs.h"
#include "telepathy-glib/enums.h"
#include "telepathy-glib/errors.h"
#include "telepathy-glib/gtypes.h"
#include <telepathy-glib/interfaces.h>
#include "telepathy-glib/util.h"

#define DEBUG_FLAG TP_DEBUG_MANAGER
#include "telepathy-glib/debug-internal.h"

#include "telepathy-glib/_gen/tp-cli-connection-manager-body.h"

/**
 * SECTION:connection-manager
 * @title: TpConnectionManager
 * @short_description: proxy object for a Telepathy connection manager
 * @see_also: #TpConnection
 *
 * #TpConnectionManager objects represent Telepathy connection managers. They
 * can be used to open connections.
 */

/**
 * TpConnectionManagerListCb:
 * @cms: %NULL-terminated array of #TpConnectionManager, or %NULL on error
 * @error: %NULL on success, or an error that occurred
 * @user_data: user-supplied data
 *
 * Signature of the callback supplied to tp_list_connection_managers().
 */

/**
 * TpConnectionManagerClass:
 *
 * The class of a #TpConnectionManager.
 */
struct _TpConnectionManagerClass {
    TpProxyClass parent_class;
    /*<private>*/
};

enum
{
  SIGNAL_ACTIVATED,
  SIGNAL_GOT_INFO,
  SIGNAL_EXITED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

enum
{
  PROP_INFO_SOURCE = 1,
  PROP_MANAGER_FILE,
  PROP_ALWAYS_INTROSPECT,
  PROP_CONNECTION_MANAGER,
  N_PROPS
};

/**
 * TpConnectionManager:
 *
 * A proxy object for a Telepathy connection manager.
 *
 * This might represent a connection manager which is currently running
 * (in which case it can be introspected) or not (in which case its
 * capabilities can be read from .manager files in the filesystem).
 * Accordingly, this object never emits #TpProxy::destroyed unless all
 * references to it are discarded.
 *
 * On initialization, we find and read the .manager file, and emit
 * got-info(FILE) on success, got-info(NONE) if no file
 * or if reading the file failed.
 *
 * When the CM runs, we automatically introspect it. On success we emit
 * got-info(LIVE). On failure, re-emit got-info(NONE) or got-info(FILE) as
 * appropriate.
 *
 * If we're asked to activate the CM, it'll implicitly be introspected.
 *
 * If the CM exits, we still consider it to have been "introspected". If it's
 * re-run, we introspect it again.
 */
struct _TpConnectionManager {
    TpProxy parent;
    /*<private>*/

    /* absolute path to .manager file */
    gchar *manager_file;

    /* TRUE if we have introspect info from a file and/or from the CM */
    TpCMInfoSource info_source:2;

    /* If TRUE, we opportunistically introspect the CM when it comes online,
     * even if we have its info from the .manager file */
    gboolean always_introspect:1;

    /* TRUE if the CM is currently running */
    gboolean running:1;
    /* TRUE if we're waiting for ListProtocols */
    gboolean listing_protocols:1;

    /* GPtrArray of TpConnectionManagerProtocol *
     *
     * NULL if file_info and live_info are both FALSE
     * Protocols from file, if file_info is TRUE but live_info is FALSE
     * Protocols from last time introspecting the CM succeeded, if live_info
     * is TRUE */
    GPtrArray *protocols;

    /* If we're waiting for a GetParameters, then GPtrArray of g_strdup'd
     * gchar * representing protocols we haven't yet introspected.
     * Otherwise NULL */
    GPtrArray *pending_protocols;
    /* If we're waiting for a GetParameters, then GPtrArray of
     * TpConnectionManagerProtocol * for the introspection that is in
     * progress (will replace ->protocols when finished).
     * Otherwise NULL */
    GPtrArray *found_protocols;
};

G_DEFINE_TYPE (TpConnectionManager,
    tp_connection_manager,
    TP_TYPE_PROXY);

static void tp_connection_manager_continue_introspection
    (TpConnectionManager *self);

static void
tp_connection_manager_got_parameters (TpProxy *proxy,
                                      const GPtrArray *parameters,
                                      const GError *error,
                                      gpointer user_data)
{
  TpConnectionManager *self = TP_CONNECTION_MANAGER (proxy);
  gchar *protocol = user_data;
  GArray *output;
  guint i;
  TpConnectionManagerProtocol *proto_struct;

  DEBUG ("Protocol name: %s", protocol);

  if (error != NULL)
    {
      DEBUG ("Error getting params for %s, skipping it", protocol);
      tp_connection_manager_continue_introspection (self);
    }

   output = g_array_sized_new (TRUE, TRUE,
      sizeof (TpConnectionManagerParam), parameters->len);

  for (i = 0; i < parameters->len; i++)
    {
      GValue structure = { 0 };
      GValue *tmp;
      /* Points to the zeroed entry just after the end of the array
       * - but we're about to extend the array to make it valid */
      TpConnectionManagerParam *param = &g_array_index (output,
          TpConnectionManagerParam, output->len);

      g_value_init (&structure, TP_STRUCT_TYPE_PARAM_SPEC);
      g_value_set_static_boxed (&structure, g_ptr_array_index (parameters, i));

      g_array_set_size (output, output->len + 1);

      if (!dbus_g_type_struct_get (&structure,
            0, &param->name,
            1, &param->flags,
            2, &param->dbus_signature,
            3, &tmp,
            G_MAXUINT))
        {
          DEBUG ("Unparseable parameter #%d for %s, ignoring", i, protocol);
          /* *shrug* that one didn't work, let's skip it */
          g_array_set_size (output, output->len - 1);
          continue;
        }

      g_value_init (&param->default_value,
          G_VALUE_TYPE (tmp));
      g_value_copy (tmp, &param->default_value);
      g_value_unset (tmp);
      g_free (tmp);

      param->priv = NULL;

      DEBUG ("\tParam name: %s", param->name);
      DEBUG ("\tParam flags: 0x%x", param->flags);
      DEBUG ("\tParam sig: %s", param->dbus_signature);

#ifdef ENABLE_DEBUG
        {
          gchar *repr = g_strdup_value_contents (&(param->default_value));

          DEBUG ("\tParam default value: %s of type %s", repr,
              G_VALUE_TYPE_NAME (&(param->default_value)));
          g_free (repr);
        }
#endif
    }

  proto_struct = g_slice_new (TpConnectionManagerProtocol);
  proto_struct->name = g_strdup (protocol);
  proto_struct->params =
      (TpConnectionManagerParam *) g_array_free (output, FALSE);
  g_ptr_array_add (self->found_protocols, proto_struct);

  tp_connection_manager_continue_introspection (self);
}

static void
tp_connection_manager_free_protocols (GPtrArray *protocols)
{
  guint i;

  for (i = 0; i < protocols->len; i++)
    {
      TpConnectionManagerProtocol *proto = g_ptr_array_index (protocols, i);
      TpConnectionManagerParam *param;

      if (proto == NULL)
        continue;

      g_free (proto->name);

      for (param = proto->params; param->name != NULL; param++)
        {
          g_free (param->name);
          g_free (param->dbus_signature);
          g_value_unset (&(param->default_value));
        }

      g_free (proto->params);

      g_slice_free (TpConnectionManagerProtocol, proto);
    }

  g_ptr_array_free (protocols, TRUE);
}

static void
tp_connection_manager_end_introspection (TpConnectionManager *self)
{
  gboolean emit = self->listing_protocols;

  self->listing_protocols = FALSE;

  if (self->found_protocols != NULL)
    {
      tp_connection_manager_free_protocols (self->found_protocols);
      self->found_protocols = NULL;
    }

  if (self->pending_protocols != NULL)
    {
      emit = TRUE;
      if (self->pending_protocols->len > 0)
        g_strfreev ((gchar **) g_ptr_array_free (self->pending_protocols,
              FALSE));
      self->pending_protocols = NULL;
    }

  if (emit)
    g_signal_emit (self, signals[SIGNAL_GOT_INFO], 0, self->info_source);
}

static void
tp_connection_manager_continue_introspection (TpConnectionManager *self)
{
  gchar *next_protocol;

  g_assert (self->pending_protocols != NULL);

  if (self->pending_protocols->len == 0)
    {
      GPtrArray *tmp;
      g_ptr_array_add (self->found_protocols, NULL);

      /* swap found_protocols and protocols, so we'll free the old protocols
       * as part of end_introspection */
      tmp = self->protocols;
      self->protocols = self->found_protocols;
      self->found_protocols = tmp;

      self->info_source = TP_CM_INFO_SOURCE_LIVE;
      tp_connection_manager_end_introspection (self);

      return;
    }

  next_protocol = g_ptr_array_remove_index_fast (self->pending_protocols, 0);
  tp_cli_connection_manager_call_get_parameters (self, -1, next_protocol,
      tp_connection_manager_got_parameters, next_protocol, g_free,
      NULL);
}

static void
tp_connection_manager_got_protocols (TpProxy *proxy,
                                     const gchar **protocols,
                                     const GError *error,
                                     gpointer user_data)
{
  TpConnectionManager *self = TP_CONNECTION_MANAGER (proxy);
  guint i = 0;
  const gchar **iter;

  self->listing_protocols = FALSE;

  if (error != NULL)
    {
      if (!self->running)
        {
          /* ListProtocols failed to start it - we assume this is because
           * activation failed */
          g_signal_emit (self, signals[SIGNAL_EXITED], 0);
        }

      tp_connection_manager_end_introspection (self);
      return;
    }

  for (iter = protocols; *iter != NULL; iter++)
    i++;

  g_assert (self->found_protocols == NULL);
  /* Allocate one more pointer - we're going to append NULL afterwards */
  self->found_protocols = g_ptr_array_sized_new (i + 1);

  g_assert (self->pending_protocols == NULL);
  self->pending_protocols = g_ptr_array_sized_new (i);

  for (iter = protocols; *iter != NULL; iter++)
    {
      g_ptr_array_add (self->pending_protocols, g_strdup (*iter));
    }

  tp_connection_manager_continue_introspection (self);
}

static void
tp_connection_manager_name_owner_changed_cb (TpDBusDaemon *bus,
                                             const gchar *name,
                                             const gchar *new_owner,
                                             gpointer user_data)
{
  TpConnectionManager *self = user_data;

  if (new_owner[0] == '\0')
    {
      self->running = FALSE;

      /* cancel pending introspection, if any */
      tp_connection_manager_end_introspection (self);

      g_signal_emit (self, signals[SIGNAL_EXITED], 0);
    }
  else
    {
      /* represent an atomic change of ownership as if it was an exit and
       * restart */
      if (self->running)
        tp_connection_manager_name_owner_changed_cb (bus, name, "", self);

      self->running = TRUE;
      g_signal_emit (self, signals[SIGNAL_ACTIVATED], 0);

      /* Start introspecting if we want to and we're not already */
      if (!self->listing_protocols &&
          (self->always_introspect ||
           self->info_source == TP_CM_INFO_SOURCE_NONE))
        {
          self->listing_protocols = TRUE;

          tp_cli_connection_manager_call_list_protocols (self, -1,
              tp_connection_manager_got_protocols, NULL, NULL,
              NULL);
        }
    }
}

static gboolean
init_gvalue_from_dbus_sig (const gchar *sig,
                           GValue *value)
{
  switch (sig[0])
    {
    case 'b':
      g_value_init (value, G_TYPE_BOOLEAN);
      return TRUE;

    case 's':
      g_value_init (value, G_TYPE_STRING);
      return TRUE;

    case 'q':
    case 'u':
      g_value_init (value, G_TYPE_UINT);
      return TRUE;

    case 'y':
      g_value_init (value, G_TYPE_UCHAR);
      return TRUE;

    case 'n':
    case 'i':
      g_value_init (value, G_TYPE_INT);
      return TRUE;

    case 'x':
      g_value_init (value, G_TYPE_INT64);
      return TRUE;

    case 't':
      g_value_init (value, G_TYPE_UINT64);
      return TRUE;

    case 'o':
      g_value_init (value, DBUS_TYPE_G_OBJECT_PATH);
      g_value_set_static_string (value, "/");
      return TRUE;

    case 'd':
      g_value_init (value, G_TYPE_DOUBLE);
      return TRUE;

    case 'v':
      g_value_init (value, G_TYPE_VALUE);
      return TRUE;

    case 'a':
      switch (sig[1])
        {
        case 's':
          g_value_init (value, G_TYPE_STRV);
          return TRUE;

        case 'y':
          g_value_init (value, DBUS_TYPE_G_UCHAR_ARRAY);
          return TRUE;
        }
    }

  g_value_unset (value);
  return FALSE;
}

static gboolean
parse_default_value (GValue *value,
                     const gchar *sig,
                     gchar *string)
{
  gchar *p;
  switch (sig[0])
    {
    case 'b':
      for (p = string; *p != '\0'; p++)
        *p = g_ascii_tolower (*p);
      if (!tp_strdiff (string, "1") || !tp_strdiff (string, "true"))
        g_value_set_boolean (value, TRUE);
      else if (!tp_strdiff (string, "0") || !tp_strdiff (string, "false"))
        g_value_set_boolean (value, TRUE);
      else
        return FALSE;
      return TRUE;

    case 's':
      g_value_set_string (value, string);
      return TRUE;

    case 'q':
    case 'u':
    case 't':
      if (string[0] == '\0')
        {
          return FALSE;
        }
      else
        {
          gchar *end;
          guint64 v = g_ascii_strtoull (string, &end, 10);

          if (*end != '\0')
            return FALSE;

          if (sig[0] == 't')
            {
              g_value_set_uint64 (value, v);
              return TRUE;
            }

          if (v > G_MAXUINT32 || (sig[0] == 'q' && v > G_MAXUINT16))
            return FALSE;

          g_value_set_uint (value, v);
          return TRUE;
        }

    case 'n':
    case 'i':
    case 'x':
      if (string[0] == '\0')
        {
          return FALSE;
        }
      else
        {
          gchar *end;
          gint64 v = g_ascii_strtoll (string, &end, 10);

          if (*end != '\0')
            return FALSE;

          if (sig[0] == 'x')
            {
              g_value_set_int64 (value, v);
              return TRUE;
            }

          if (v > G_MAXINT32 || (sig[0] == 'q' && v > G_MAXINT16))
            return FALSE;

          if (v < G_MININT32 || (sig[0] == 'n' && v < G_MININT16))
            return FALSE;

          g_value_set_int (value, v);
          return TRUE;
        }

    case 'o':
      if (string[0] != '/')
        return FALSE;

      g_value_set_boxed (value, string);
      return TRUE;

    case 'd':
        {
          gchar *end;
          gdouble v = g_ascii_strtod (string, &end);

          if (*end != '\0')
            return FALSE;

          g_value_set_double (value, v);
          return TRUE;
        }
    }

  g_value_unset (value);
  return FALSE;
}

static void
tp_connection_manager_read_file (TpConnectionManager *self,
                                 const gchar *filename)
{
  GKeyFile *file;
  GError *error = NULL;
  gchar **groups, **group;
  guint i;
  TpConnectionManagerProtocol *proto_struct;
  GPtrArray *protocols;

  file = g_key_file_new ();

  if (!g_key_file_load_from_file (file, filename, G_KEY_FILE_NONE, &error))
    {
      DEBUG ("Failed to read %s: %s", filename, error->message);
      g_signal_emit (self, signals[SIGNAL_GOT_INFO], 0, self->info_source);
    }

  groups = g_key_file_get_groups (file, NULL);

  i = 0;
  for (group = groups; *group != NULL; group++)
    {
      if (g_str_has_prefix (*group, "Protocol "))
        i++;
    }

  /* We're going to add a NULL at the end, so +1 */
  protocols = g_ptr_array_sized_new (i + 1);

  for (group = groups; *group != NULL; group++)
    {
      gchar **keys, **key;
      GArray *output;

      if (!g_str_has_prefix (*group, "Protocol "))
        continue;

      proto_struct = g_slice_new (TpConnectionManagerProtocol);

      keys = g_strsplit (*group, " ", 2);
      proto_struct->name = g_strdup (keys[1]);
      g_strfreev (keys);

      keys = g_key_file_get_keys (file, *group, NULL, &error);

      i = 0;
      for (key = keys; *key != NULL; key++)
        {
          if (g_str_has_prefix (*key, "param-"))
            i++;
        }

      output = g_array_sized_new (TRUE, TRUE,
          sizeof (TpConnectionManagerParam), i);

      for (key = keys; *key != NULL; key++)
        {
          if (g_str_has_prefix (*key, "param-"))
            {
              gchar **strv, **iter;
              gchar *value, *def;
              /* Points to the zeroed entry just after the end of the array
               * - but we're about to extend the array to make it valid */
              TpConnectionManagerParam *param = &g_array_index (output,
                  TpConnectionManagerParam, output->len);

              value = g_key_file_get_string (file, *group, *key, NULL);
              if (value == NULL)
                continue;

              /* zero_terminated=TRUE and clear_=TRUE */
              g_assert (param->name == NULL);

              g_array_set_size (output, output->len + 1);

              /* strlen ("param-") == 6 */
              param->name = g_strdup (*key + 6);

              strv = g_strsplit (value, " ", 0);
              g_free (value);

              param->dbus_signature = g_strdup (strv[0]);

              for (iter = strv + 1; *iter != NULL; iter++)
                {
                  if (!tp_strdiff (*iter, "required"))
                    param->flags |= TP_CONN_MGR_PARAM_FLAG_REQUIRED;
                  if (!tp_strdiff (*iter, "register"))
                    param->flags |= TP_CONN_MGR_PARAM_FLAG_REGISTER;
                }

              g_strfreev (strv);

              def = g_strdup_printf ("default-%s", param->name);
              value = g_key_file_get_string (file, *group, def, NULL);
              g_free (def);

              init_gvalue_from_dbus_sig (param->dbus_signature,
                  &param->default_value);

              if (value != NULL && parse_default_value (&param->default_value,
                    param->dbus_signature, value))
                param->flags |= TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT;

              g_free (value);

              DEBUG ("\tParam name: %s", param->name);
              DEBUG ("\tParam flags: 0x%x", param->flags);
              DEBUG ("\tParam sig: %s", param->dbus_signature);

#ifdef ENABLE_DEBUG
                {
                  gchar *repr = g_strdup_value_contents
                      (&(param->default_value));

                  DEBUG ("\tParam default value: %s of type %s", repr,
                      G_VALUE_TYPE_NAME (&(param->default_value)));
                  g_free (repr);
                }
#endif
            }
        }

      g_strfreev (keys);

      proto_struct->params =
          (TpConnectionManagerParam *) g_array_free (output, FALSE);

      g_ptr_array_add (protocols, proto_struct);
    }

  g_ptr_array_add (protocols, NULL);

  g_assert (self->protocols == NULL);
  self->protocols = protocols;
  self->info_source = TP_CM_INFO_SOURCE_FILE;

  g_strfreev (groups);
  g_key_file_free (file);
}

static gboolean
tp_connection_manager_idle_read_manager_file (gpointer data)
{
  TpConnectionManager *self = TP_CONNECTION_MANAGER (data);

  if (self->protocols == NULL && self->manager_file != NULL
      && self->manager_file[0] != '\0')
    tp_connection_manager_read_file (self, self->manager_file);

  g_signal_emit (self, signals[SIGNAL_GOT_INFO], 0, self->info_source);

  return FALSE;
}

static gchar *
tp_connection_manager_find_manager_file (const gchar *name)
{
  gchar *filename = g_strdup_printf ("%s/telepathy/managers/%s.manager",
      g_get_user_data_dir (), name);
  const gchar * const * data_dirs;

  DEBUG ("in XDG_DATA_HOME: trying %s", filename);

  if (g_file_test (filename, G_FILE_TEST_EXISTS))
    return filename;

  g_free (filename);

  for (data_dirs = g_get_system_data_dirs ();
       *data_dirs != NULL;
       data_dirs++)
    {
      filename = g_strdup_printf ("%s/telepathy/managers/%s.manager",
          *data_dirs, name);

      DEBUG ("in XDG_DATA_DIRS: trying %s", filename);

      if (g_file_test (filename, G_FILE_TEST_EXISTS))
        return filename;
    }

  return NULL;
}

static GObject *
tp_connection_manager_constructor (GType type,
                                   guint n_params,
                                   GObjectConstructParam *params)
{
  GObjectClass *object_class =
      (GObjectClass *) tp_connection_manager_parent_class;
  TpConnectionManager *self =
      TP_CONNECTION_MANAGER (object_class->constructor (type, n_params,
            params));
  TpProxy *as_proxy = (TpProxy *) self;

  /* Watch my D-Bus name */
  tp_dbus_daemon_watch_name_owner (TP_DBUS_DAEMON (as_proxy->dbus_daemon),
      as_proxy->bus_name, tp_connection_manager_name_owner_changed_cb, self,
      NULL);

  return (GObject *) self;
}

static void
tp_connection_manager_init (TpConnectionManager *self)
{
}

static void
tp_connection_manager_dispose (GObject *object)
{
  TpProxy *as_proxy = TP_PROXY (object);

  tp_dbus_daemon_cancel_name_owner_watch (
      TP_DBUS_DAEMON (as_proxy->dbus_daemon), as_proxy->bus_name,
      tp_connection_manager_name_owner_changed_cb, object);

  G_OBJECT_CLASS (tp_connection_manager_parent_class)->dispose (object);
}

static void
tp_connection_manager_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
  TpConnectionManager *self = TP_CONNECTION_MANAGER (object);

  switch (property_id)
    {
    case PROP_CONNECTION_MANAGER:
        {
          const gchar *name = strrchr (((TpProxy *) self)->object_path, '/');

          name++; /* avoid the '/' */
          g_value_set_string (value, name);
        }
      break;

    case PROP_INFO_SOURCE:
      g_value_set_uint (value, self->info_source);
      break;

    case PROP_MANAGER_FILE:
      g_value_set_string (value, self->manager_file);
      break;

    case PROP_ALWAYS_INTROSPECT:
      g_value_set_boolean (value, self->always_introspect);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_connection_manager_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
  TpConnectionManager *self = TP_CONNECTION_MANAGER (object);
  const gchar *tmp;

  switch (property_id)
    {
    case PROP_MANAGER_FILE:
      g_free (self->manager_file);

      tmp = g_value_get_string (value);
      if (tmp == NULL)
        {
          const gchar *name = strrchr (((TpProxy *) self)->object_path, '/');

          name++; /* avoid the '/' */

          self->manager_file =
              tp_connection_manager_find_manager_file (name);
        }
      else if (tmp[0] == '\0')
        {
          self->manager_file = g_strdup (tmp);
        }

      g_idle_add (tp_connection_manager_idle_read_manager_file, self);

      break;

    case PROP_ALWAYS_INTROSPECT:
      self->always_introspect = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_connection_manager_class_init (TpConnectionManagerClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  object_class->constructor = tp_connection_manager_constructor;
  object_class->get_property = tp_connection_manager_get_property;
  object_class->set_property = tp_connection_manager_set_property;
  object_class->dispose = tp_connection_manager_dispose;

  proxy_class->interface = TP_IFACE_QUARK_CONNECTION_MANAGER;
  tp_proxy_class_hook_on_interface_add (proxy_class,
      tp_cli_connection_manager_add_signals);

  /**
   * TpConnectionManager::info-source:
   *
   * Where we got the current information on supported protocols
   * (a #TpCMInfoSource).
   */
  param_spec = g_param_spec_uint ("info-source", "CM info source",
      "Where we got the current information on supported protocols",
      TP_CM_INFO_SOURCE_NONE, TP_CM_INFO_SOURCE_LIVE, TP_CM_INFO_SOURCE_NONE,
      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_INFO_SOURCE,
      param_spec);

  /**
   * TpConnectionManager::connection-manager:
   *
   * The name of the connection manager, e.g. "gabble" (read-only).
   */
  param_spec = g_param_spec_string ("connection-manager", "CM name",
      "The name of the connection manager, e.g. \"gabble\" (read-only)",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_CONNECTION_MANAGER,
      param_spec);

  /**
   * TpConnectionManager::manager-file:
   *
   * The absolute path of the .manager file. If set to %NULL (the default),
   * the XDG data directories will be searched for a .manager file of the
   * correct name.
   *
   * If set to the empty string, no .manager file will be read.
   */
  param_spec = g_param_spec_string ("manager-file", ".manager filename",
      "The .manager filename",
      NULL,
      G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_MANAGER_FILE,
      param_spec);

  /**
   * TpConnectionManager::always-introspect:
   *
   * If %TRUE, always introspect the connection manager as it comes online,
   * even if we already have its info from a .manager file. Default %FALSE.
   */
  param_spec = g_param_spec_boolean ("always-introspect", "Always introspect?",
      "Opportunistically introspect the CM when it's run", FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_ALWAYS_INTROSPECT,
      param_spec);

  /**
   * TpConnectionManager::activated:
   * @self: the connection manager proxy
   *
   * Emitted when the connection manager's well-known name appears on the bus.
   */
  signals[SIGNAL_ACTIVATED] = g_signal_new ("activated",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  /**
   * TpConnectionManager::exited:
   * @self: the connection manager proxy
   *
   * Emitted when the connection manager's well-known name disappears from
   * the bus or when activation fails.
   */
  signals[SIGNAL_EXITED] = g_signal_new ("exited",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  /**
   * TpConnectionManager::got-info:
   * @self: the connection manager proxy
   * @source: a #TpCMInfoSource
   *
   * Emitted when the connection manager's capabilities have been discovered.
   */
  signals[SIGNAL_GOT_INFO] = g_signal_new ("got-info",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__UINT,
      G_TYPE_NONE, 1, G_TYPE_UINT);
}

/**
 * tp_connection_manager_new:
 * @dbus: Proxy for the D-Bus daemon
 * @name: The connection manager name
 * @manager_filename: The #TpConnectionManager::manager-file property
 *
 * Convenience function to create a new connection manager proxy.
 *
 * Returns: a new reference to a connection manager proxy
 */
TpConnectionManager *
tp_connection_manager_new (TpDBusDaemon *dbus,
                           const gchar *name,
                           const gchar *manager_filename)
{
  TpConnectionManager *cm;
  gchar *object_path, *bus_name;
  const gchar *name_char;

  g_return_val_if_fail (dbus != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  /* FIXME: return a GError rather than using assertions? */
  g_return_val_if_fail (g_ascii_isalpha (name[0]), NULL);

  for (name_char = name; *name_char != '\0'; name_char++)
    {
      g_return_val_if_fail (g_ascii_isalnum (*name_char) || *name_char == '_',
          NULL);
    }

  object_path = g_strdup_printf ("%s%s", TP_CM_OBJECT_PATH_BASE, name);
  bus_name = g_strdup_printf ("%s%s", TP_CM_BUS_NAME_BASE, name);

  cm = TP_CONNECTION_MANAGER (g_object_new (TP_TYPE_CONNECTION_MANAGER,
        "dbus-daemon", dbus,
        "dbus-connection", ((TpProxy *) dbus)->dbus_connection,
        "bus-name", bus_name,
        "object-path", object_path,
        "manager-file", manager_filename,
        NULL));

  g_free (object_path);
  g_free (bus_name);

  return cm;
}

/**
 * tp_connection_manager_activate:
 * @self: a connection manager proxy
 *
 * Attempt to run and introspect the connection manager, asynchronously.
 *
 * If the CM was already running, do nothing and return %FALSE.
 *
 * On success, emit #TpConnectionManager::activated when the CM appears
 * on the bus, and #TpConnectionManager::got-info when its capabilities
 * have been (re-)discovered.
 *
 * On failure, emit #TpConnectionManager::exited without first emitting
 * activated.
 *
 * Returns: %TRUE if activation was needed and is now in progress, %FALSE
 *  if the connection manager was already running and no additional signals
 *  will be emitted.
 */
gboolean
tp_connection_manager_activate (TpConnectionManager *self)
{
  if (self->running)
    return FALSE;

  self->listing_protocols = TRUE;
  tp_cli_connection_manager_call_list_protocols (self, -1,
      tp_connection_manager_got_protocols, NULL, NULL, NULL);

  return TRUE;
}

static gboolean
steal_into_ptr_array (gpointer key,
                      gpointer value,
                      gpointer user_data)
{
  if (value != NULL)
    g_ptr_array_add (user_data, value);

  g_free (key);

  return TRUE;
}

typedef struct
{
  GHashTable *table;
  TpConnectionManagerListCb callback;
  gpointer user_data;
  GDestroyNotify destroy;
  TpProxyPendingCall *pending_call;
  GObject *weak_object;
  size_t base_len;
  gboolean getting_names:1;
  guint refcount:2;
} _ListContext;

static void
list_context_unref (_ListContext *list_context)
{
  if (--list_context->refcount > 0)
    return;

  if (list_context->destroy != NULL)
    list_context->destroy (list_context->user_data);

  g_hash_table_destroy (list_context->table);
  g_slice_free (_ListContext, list_context);
}

void
tp_list_connection_managers_got_names (TpProxy *proxy,
                                       const gchar **names,
                                       const GError *error,
                                       gpointer user_data)
{
  _ListContext *list_context = user_data;
  TpDBusDaemon *bus_daemon = (TpDBusDaemon *) proxy;
  const gchar **iter;

  if (error != NULL)
    {
      list_context->callback (NULL, error, list_context->user_data,
          list_context->weak_object);
      return;
    }

  for (iter = names; iter != NULL && *iter != NULL; iter++)
    {
      const gchar *name;
      TpConnectionManager *cm;

      if (strncmp (TP_CM_BUS_NAME_BASE, *iter, list_context->base_len) != 0)
        continue;

      name = *iter + list_context->base_len;

      if (g_hash_table_lookup (list_context->table, name) == NULL)
        {
          cm = tp_connection_manager_new (bus_daemon, name, NULL);
          g_hash_table_insert (list_context->table, g_strdup (name), cm);
        }
    }

  if (list_context->getting_names)
    {
      /* actually call the callback */
      GPtrArray *arr = g_ptr_array_sized_new (g_hash_table_size
              (list_context->table));

      g_hash_table_foreach_steal (list_context->table, steal_into_ptr_array,
          arr);
      g_ptr_array_add (arr, NULL);

      list_context->callback ((TpConnectionManager **) g_ptr_array_free (arr,
            FALSE),
          NULL, list_context->user_data, list_context->weak_object);
      list_context->callback = NULL;
    }
  else
    {
      list_context->getting_names = TRUE;
      list_context->refcount++;
      tp_cli_dbus_daemon_call_list_names (bus_daemon, 2000,
          tp_list_connection_managers_got_names, list_context,
          (GDestroyNotify) list_context_unref, list_context->weak_object);
    }
}

/**
 * tp_list_connection_managers:
 * @bus_daemon: proxy for the D-Bus daemon
 * @callback: callback to be called when listing the CMs succeeds or fails;
 *   not called if the D-Bus connection fails completely or if the
 *   @weak_object goes away
 * @user_data: user-supplied data for the callback
 * @destroy: callback to destroy the user-supplied data, called after
 *   @callback, but also if the D-Bus connection fails or if the @weak_object
 *   goes away
 * @weak_object: if not %NULL, will be weakly referenced; the callback will
 *   not be called, and the call will be cancelled, if the object has vanished
 *
 * List the available (running or installed) connection managers. Call the
 * callback when done.
 */
void
tp_list_connection_managers (TpDBusDaemon *bus_daemon,
                             TpConnectionManagerListCb callback,
                             gpointer user_data,
                             GDestroyNotify destroy,
                             GObject *weak_object)
{
  _ListContext *list_context = g_slice_new0 (_ListContext);

  list_context->base_len = strlen (TP_CM_BUS_NAME_BASE);
  list_context->callback = callback;
  list_context->user_data = user_data;
  list_context->destroy = destroy;
  list_context->weak_object = weak_object;

  list_context->getting_names = FALSE;
  list_context->refcount = 1;
  list_context->table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      g_object_unref);

  tp_cli_dbus_daemon_call_list_activatable_names (bus_daemon, 2000,
      tp_list_connection_managers_got_names, list_context,
      (GDestroyNotify) list_context_unref, weak_object);
}
