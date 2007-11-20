/*
 * dbus.c - Source for some Telepathy D-Bus helper functions
 * Copyright (C) 2005 Collabora Ltd.
 * Copyright (C) 2005 Nokia Corporation
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

/**
 * SECTION:dbus
 * @title: D-Bus utilities
 * @short_description: some D-Bus utility functions
 *
 * D-Bus utility functions used in telepathy-glib.
 */

#include <telepathy-glib/dbus.h>

#include <stdlib.h>

#include <telepathy-glib/errors.h>
#include <telepathy-glib/util.h>

#include "telepathy-glib/proxy-internal.h"
#include "telepathy-glib/_gen/signals-marshal.h"

/**
 * tp_dbus_g_method_return_not_implemented:
 * @context: The D-Bus method invocation context
 *
 * Return the Telepathy error NotImplemented from the method invocation
 * given by @context.
 */
void
tp_dbus_g_method_return_not_implemented (DBusGMethodInvocation *context)
{
  GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED, "Not implemented" };

  dbus_g_method_return_error (context, &e);
}

/**
 * tp_get_bus:
 *
 * <!--Returns: says it all-->
 *
 * Returns: a connection to the starter or session D-Bus daemon.
 */
DBusGConnection *
tp_get_bus (void)
{
  static DBusGConnection *bus = NULL;

  if (bus == NULL)
    {
      GError *error = NULL;

      bus = dbus_g_bus_get (DBUS_BUS_STARTER, &error);

      if (bus == NULL)
        {
          g_warning ("Failed to connect to starter bus: %s", error->message);
          exit (1);
        }
    }

  return bus;
}

/**
 * tp_get_bus_proxy:
 *
 * <!--Returns: says it all-->
 *
 * Returns: a proxy for the bus daemon object on the starter or session bus.
 */
DBusGProxy *
tp_get_bus_proxy (void)
{
  static DBusGProxy *bus_proxy = NULL;

  if (bus_proxy == NULL)
    {
      DBusGConnection *bus = tp_get_bus ();

      bus_proxy = dbus_g_proxy_new_for_name (bus,
                                            "org.freedesktop.DBus",
                                            "/org/freedesktop/DBus",
                                            "org.freedesktop.DBus");

      if (bus_proxy == NULL)
        g_error ("Failed to get proxy object for bus.");
    }

  return bus_proxy;
}

struct _TpDBusDaemonClass
{
  TpProxyClass parent_class;
};

struct _TpDBusDaemon
{
  TpProxy parent;
  /* dup'd name => _NameOwnerWatch */
  GHashTable *name_owner_watches;
  /* hack hack hack - see _tp_dbus_daemon_name_owner_changed_cb */
  gchar *last_name;
  gchar *last_old_owner;
  gchar *last_new_owner;
};

G_DEFINE_TYPE (TpDBusDaemon, tp_dbus_daemon, TP_TYPE_PROXY);

TpDBusDaemon *
tp_dbus_daemon_new (DBusGConnection *connection)
{
  g_return_val_if_fail (connection != NULL, NULL);

  return TP_DBUS_DAEMON (g_object_new (TP_TYPE_DBUS_DAEMON,
        "dbus-connection", connection,
        "bus-name", DBUS_SERVICE_DBUS,
        "object-path", DBUS_PATH_DBUS,
        NULL));
}

typedef struct
{
  TpDBusDaemonNameOwnerChangedCb callback;
  gpointer user_data;
  GDestroyNotify destroy;
} _NameOwnerWatch;

static void
_tp_dbus_daemon_name_owner_changed_multiple (TpDBusDaemon *self,
                                             const gchar *name,
                                             const gchar *old_owner,
                                             const gchar *new_owner,
                                             gpointer user_data)
{
  GArray *array = user_data;
  guint i;

  for (i = 0; i < array->len; i++)
    {
      _NameOwnerWatch *watch = &g_array_index (array, _NameOwnerWatch, i);

      g_message ("... multiplexing to callback %p", watch->callback);
      watch->callback (self, name, old_owner, new_owner, watch->user_data);
    }
}

static void
_tp_dbus_daemon_name_owner_changed_multiple_free (gpointer data)
{
  GArray *array = data;
  guint i;

  for (i = 0; i < array->len; i++)
    {
      _NameOwnerWatch *watch = &g_array_index (array, _NameOwnerWatch, i);

      if (watch->destroy)
        watch->destroy (watch->user_data);
    }

  g_array_free (array, TRUE);
}

static void
_tp_dbus_daemon_name_owner_changed_cb (DBusGProxy *proxy,
                                       const gchar *name,
                                       const gchar *old_owner,
                                       const gchar *new_owner,
                                       TpProxySignalConnection *sig_conn)
{
  TpDBusDaemon *self = TP_DBUS_DAEMON (sig_conn->proxy);
  _NameOwnerWatch *watch = g_hash_table_lookup (self->name_owner_watches,
      name);

  /* XXX: due to oddness in dbus-gproxy.c (it believes that
   * org.freedesktop.DBus is both a well-known name and a unique name)
   * we get each signal twice. For the moment, just suppress the duplicates */
  if (!tp_strdiff (name, self->last_name) &&
      !tp_strdiff (old_owner, self->last_old_owner) &&
      !tp_strdiff (new_owner, self->last_new_owner))
    return;

  self->last_name = g_strdup (name);
  self->last_old_owner = g_strdup (old_owner);
  self->last_new_owner = g_strdup (new_owner);

  g_message ("%p NameOwnerChanged: %s %s -> %s", self, name, old_owner,
      new_owner);

  if (watch == NULL)
    {
      g_message ("%p ... but no callback", self);
      return;
    }

  g_message ("%p Calling callback %p", self, watch->callback);
  watch->callback (self, name, old_owner, new_owner, watch->user_data);
}

void
tp_dbus_daemon_watch_name_owner (TpDBusDaemon *self,
                                 const gchar *name,
                                 TpDBusDaemonNameOwnerChangedCb callback,
                                 gpointer user_data,
                                 GDestroyNotify destroy)
{
  _NameOwnerWatch *watch = g_hash_table_lookup (self->name_owner_watches,
      name);

  if (watch == NULL)
    {
      /* Allocate a single watch (common case) */
      watch = g_slice_new (_NameOwnerWatch);
      watch->callback = callback;
      watch->user_data = user_data;
      watch->destroy = destroy;

      g_hash_table_insert (self->name_owner_watches, g_strdup (name), watch);
    }
  else
    {
      _NameOwnerWatch tmp = { callback, user_data, destroy };

      if (watch->callback == _tp_dbus_daemon_name_owner_changed_multiple)
        {
          /* The watch is already a "multiplexer", just append to it */
          GArray *array = watch->user_data;

          g_array_append_val (array, tmp);
        }
      else
        {
          /* Replace the old contents of the watch with one that dispatches
           * the signal to more than one watcher */
          GArray *array = g_array_sized_new (FALSE, FALSE,
              sizeof (_NameOwnerWatch), 2);

          /* The new watcher */
          g_array_append_val (array, tmp);
          /* The old watcher */
          tmp.callback = watch->callback;
          tmp.user_data = watch->user_data;
          tmp.destroy = watch->destroy;
          g_array_prepend_val (array, tmp);

          watch->callback = _tp_dbus_daemon_name_owner_changed_multiple;
          watch->user_data = array;
          watch->destroy = _tp_dbus_daemon_name_owner_changed_multiple_free;
        }
    }
}

gboolean
tp_dbus_daemon_cancel_name_owner_watch (TpDBusDaemon *self,
                                        const gchar *name,
                                        TpDBusDaemonNameOwnerChangedCb callback,
                                        gpointer user_data)
{
  _NameOwnerWatch *watch = g_hash_table_lookup (self->name_owner_watches,
      name);

  if (watch == NULL)
    {
      /* No watch at all */
      return FALSE;
    }
  else if (watch->callback == callback && watch->user_data == user_data)
    {
      /* Simple case: there is one name-owner watch and it's what we wanted */
      if (watch->destroy)
        watch->destroy (watch->user_data);

      g_slice_free (_NameOwnerWatch, watch);
      g_hash_table_remove (self->name_owner_watches, name);
      return TRUE;
    }
  else if (watch->callback == _tp_dbus_daemon_name_owner_changed_multiple)
    {
      /* Complicated case: this watch is a "multiplexer", we need to check
       * its contents */
      GArray *array = watch->user_data;
      guint i;

      for (i = 0; i < array->len; i++)
        {
          _NameOwnerWatch *entry = &g_array_index (array, _NameOwnerWatch, i);

          if (entry->callback == callback && entry->user_data == user_data)
            {
              if (entry->destroy)
                entry->destroy (entry->user_data);

              g_array_remove_index_fast (array, i);

              if (array->len == 0)
                {
                  watch->destroy (watch->user_data);
                  g_slice_free (_NameOwnerWatch, watch);
                  g_hash_table_remove (self->name_owner_watches, name);
                }

              return TRUE;
            }
        }
    }

  /* We haven't found it */
  return FALSE;
}

static GObject *
tp_dbus_daemon_constructor (GType type,
                            guint n_params,
                            GObjectConstructParam *params)
{
  GObjectClass *object_class =
      (GObjectClass *) tp_dbus_daemon_parent_class;
  TpDBusDaemon *self = TP_DBUS_DAEMON (object_class->constructor (type,
        n_params, params));

  tp_cli_dbus_daemon_connect_to_name_owner_changed (self,
      _tp_dbus_daemon_name_owner_changed_cb, NULL, NULL);

  return (GObject *) self;
}

static void
tp_dbus_daemon_init (TpDBusDaemon *self)
{
  self->name_owner_watches = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);
}

static void
tp_dbus_daemon_dispose (GObject *object)
{
  TpDBusDaemon *self = TP_DBUS_DAEMON (object);

  if (self->name_owner_watches != NULL)
    {
      GHashTable *tmp = self->name_owner_watches;

      self->name_owner_watches = NULL;
      g_hash_table_destroy (tmp);
    }

  G_OBJECT_CLASS (tp_dbus_daemon_parent_class)->dispose (object);
}

static void
tp_dbus_daemon_finalize (GObject *object)
{
  TpDBusDaemon *self = TP_DBUS_DAEMON (object);

  g_free (self->last_name);
  g_free (self->last_old_owner);
  g_free (self->last_new_owner);

  G_OBJECT_CLASS (tp_dbus_daemon_parent_class)->finalize (object);
}

static void
tp_dbus_daemon_class_init (TpDBusDaemonClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->constructor = tp_dbus_daemon_constructor;
  object_class->dispose = tp_dbus_daemon_dispose;
  object_class->finalize = tp_dbus_daemon_finalize;

  proxy_class->interface = TP_IFACE_QUARK_DBUS_DAEMON;
  proxy_class->on_interface_added = g_slist_prepend
    (proxy_class->on_interface_added, tp_cli_dbus_daemon_add_signals);
}

/* Auto-generated implementation of _tp_register_dbus_glib_marshallers */
#include "_gen/register-dbus-glib-marshallers-body.h"
