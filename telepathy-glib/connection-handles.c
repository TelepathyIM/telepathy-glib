/* Helper to hold Telepathy handles.
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
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

#include "telepathy-glib/connection-internal.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#define DEBUG_FLAG TP_DEBUG_HANDLES
#include "telepathy-glib/debug-internal.h"

/*
 * We allocate a global libdbus DBusConnection slot for this module. If
 * used, it contains a
 * GHashTable<gchar *Connection_object_path => Bucket>.
 *
 * A Bucket is basically just an array of GHashTable<handle => refcount>.
 *
 * This is external to the TpConnection because it has to be - if there
 * are two TpConnection instances for the same Connection (perhaps they're
 * of different subclasses), we need to share handle references between them.
 */

typedef struct {
    /* number of TpConnection objects sharing this bucket */
    gsize refcount;
    /* guint handle => gsize refcount */
    GHashTable *handle_refs[NUM_TP_HANDLE_TYPES];
} Bucket;

static inline void oom (void) G_GNUC_NORETURN;

static inline void
oom (void)
{
  g_error ("Out of memory in libdbus. Cannot have a bucket");
}

static void
bucket_free (gpointer p)
{
  Bucket *b = p;
  guint i;

  /* [0] is never allocated (handle type NONE), so start at [1] */
  g_assert (b->handle_refs[0] == NULL);

  for (i = 1; i < NUM_TP_HANDLE_TYPES; i++)
    {
      if (b->handle_refs[i] != NULL)
        g_hash_table_destroy (b->handle_refs[i]);
    }

  g_slice_free (Bucket, p);
}

static Bucket *
bucket_new (void)
{
  Bucket *b = g_slice_new0 (Bucket);
  b->refcount = 1;

  return b;
}

static dbus_int32_t connection_handle_refs_slot = -1;

static Bucket *
connection_get_bucket (TpConnection *connection)
{
  TpProxy *as_proxy = (TpProxy *) connection;
  DBusGConnection *g_connection = as_proxy->dbus_connection;
  const gchar *object_path = as_proxy->object_path;
  GHashTable *table;
  Bucket *bucket = NULL;

  /* _tp_connection_init_handle_refs should have already ensured that the
   * assertions in this function will succeed. */
  g_assert (connection_handle_refs_slot != -1);

  table = dbus_connection_get_data (dbus_g_connection_get_connection (
        g_connection), connection_handle_refs_slot);
  g_assert (table != NULL);

  bucket = g_hash_table_lookup (table, object_path);
  g_assert (bucket != NULL);

  return bucket;
}

static void
_tp_connection_ref_handles (TpConnection *connection,
                            TpHandleType handle_type,
                            const GArray *handles)
{
  TpProxy *as_proxy = (TpProxy *) connection;
  GHashTable *handle_refs;
  Bucket *bucket;
  guint i;

  g_assert (as_proxy->invalidated == NULL);
  g_assert (handle_type < NUM_TP_HANDLE_TYPES);

  DEBUG ("%p: %u handles of type %u", connection, handles->len, handle_type);

  bucket = connection_get_bucket (connection);

  if (bucket->handle_refs[handle_type] == NULL)
    bucket->handle_refs[handle_type] = g_hash_table_new (g_direct_hash,
        g_direct_equal);

  handle_refs = bucket->handle_refs[handle_type];

  for (i = 0; i < handles->len; i++)
    {
      gpointer handle = GUINT_TO_POINTER (g_array_index (handles, guint, i));
      gsize r = GPOINTER_TO_SIZE (g_hash_table_lookup (handle_refs, handle));

      g_hash_table_insert (handle_refs, handle, GSIZE_TO_POINTER (r + 1));
    }
}


static void
post_unref (TpConnection *connection,
            const GError *error,
            gpointer user_data,
            GObject *weak_object)
{
  GArray *arr = user_data;

  if (error == NULL)
    {
      DEBUG ("Released %u handles", arr->len);
    }
  else
    {
      guint i;

      DEBUG ("Failed to release %u handles: %s %u: %s",
          arr->len, g_quark_to_string (error->domain), error->code,
          error->message);

      for (i = 0; i < arr->len; i++)
        {
          DEBUG ("   %u", g_array_index (arr, guint, i));
        }
    }
}


static void
array_free_TRUE (gpointer p)
{
  g_array_free (p, TRUE);
}


void
_tp_connection_init_handle_refs (TpConnection *self)
{
  TpProxy *as_proxy = (TpProxy *) self;
  DBusGConnection *g_connection = as_proxy->dbus_connection;
  const gchar *object_path = as_proxy->object_path;
  GHashTable *table;
  Bucket *bucket = NULL;

  g_assert (as_proxy->invalidated == NULL);

  /* MT: libdbus protects us, if so configured */
  if (!dbus_connection_allocate_data_slot (&connection_handle_refs_slot))
    oom ();

  /* MT: if we become thread safe, the rest of this function needs a lock */
  table = dbus_connection_get_data (dbus_g_connection_get_connection (
        g_connection), connection_handle_refs_slot);

  if (table == NULL)
    {
      table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
          bucket_free);

      if (!dbus_connection_set_data (dbus_g_connection_get_connection (
              g_connection), connection_handle_refs_slot, table,
            (DBusFreeFunction) g_hash_table_destroy))
        oom ();
    }
  else
    {
      bucket = g_hash_table_lookup (table, object_path);
    }

  if (bucket == NULL)
    {
      bucket = bucket_new ();
      g_hash_table_insert (table, g_strdup (object_path), bucket);
    }
  else
    {
      bucket->refcount++;
    }
}


/* Clean up after the connection is invalidated */
void
_tp_connection_clean_up_handle_refs (TpConnection *self)
{
  TpProxy *as_proxy = (TpProxy *) self;
  DBusGConnection *g_connection = as_proxy->dbus_connection;
  const gchar *object_path = as_proxy->object_path;
  GHashTable *table;
  Bucket *bucket;

  DEBUG ("%p", self);

  g_assert (as_proxy->invalidated != NULL);

  if (connection_handle_refs_slot == -1)
    return;

  /* MT: if we become thread safe, the rest of this function needs a lock */
  table = dbus_connection_get_data (dbus_g_connection_get_connection (
        g_connection), connection_handle_refs_slot);

  if (table == NULL)
    return;

  bucket = g_hash_table_lookup (table, object_path);

  if ((--bucket->refcount) > 0)
    return;

  if (g_hash_table_remove (table, object_path) &&
      g_hash_table_size (table) == 0)
    {
      /* this calls the destructor, g_hash_table_destroy */
      dbus_connection_set_data (dbus_g_connection_get_connection (
            g_connection), connection_handle_refs_slot, NULL, NULL);
    }
}


/**
 * tp_connection_unref_handles:
 * @self: a connection
 * @handle_type: a handle type
 * @n_handles: the number of handles in @handles
 * @handles: an array of @n_handles handles
 *
 * Release the reference to the handles in @handles that was obtained by
 * calling tp_connection_hold_handles() or tp_connection_request_handles().
 *
 * This function might release any references held by calling
 * tp_cli_connection_call_request_handles(),
 * tp_cli_connection_run_request_handles(),
 * tp_cli_connection_call_hold_handles(),
 * tp_cli_connection_run_hold_handles(),
 * tp_cli_connection_interface_contacts_call_get_contact_attributes() or
 * tp_cli_connection_interface_contacts_run_get_contact_attributes() directly.
 * Those functions should be avoided in favour of using #TpContact,
 * tp_connection_hold_handles(), tp_connection_request_handles() and
 * tp_connection_get_contact_attributes(), which along with this function
 * perform client-side reference counting of handles.
 *
 * If @self has already become invalid, this function does nothing.
 */
void
tp_connection_unref_handles (TpConnection *self,
                             TpHandleType handle_type,
                             guint n_handles,
                             const TpHandle *handles)
{
  TpProxy *as_proxy = (TpProxy *) self;
  GHashTable *handle_refs;
  Bucket *bucket;
  guint i;
  GArray *unref;

  DEBUG ("%p: %u handles of type %u", self, n_handles, handle_type);

  g_return_if_fail (TP_IS_CONNECTION (self));
  g_return_if_fail (handle_type > TP_HANDLE_TYPE_NONE);
  g_return_if_fail (handle_type < NUM_TP_HANDLE_TYPES);

  if (as_proxy->invalidated != NULL)
    {
      return;
    }

  bucket = connection_get_bucket (self);

  /* if there's no hash table, then we can't have a ref to the handles -
   * user error */
  g_return_if_fail (((void)"no refs exist to any handle of that type",
        bucket->handle_refs[handle_type] != NULL));

  handle_refs = bucket->handle_refs[handle_type];

  unref = g_array_sized_new (FALSE, FALSE, sizeof (guint), n_handles);

  for (i = 0; i < n_handles; i++)
    {
      gpointer handle = GUINT_TO_POINTER (handles[i]);
      gsize r = GPOINTER_TO_SIZE (g_hash_table_lookup (handle_refs, handle));

      g_return_if_fail (handles[i] != 0);
      /* if we have no refs, it's user error */
      g_return_if_fail (((void)"no refs exist to one of the handles", r != 0));

      if (r == 1)
        {
          DEBUG ("releasing handle %u", handles[i]);
          g_array_append_val (unref, handles[i]);
          g_hash_table_remove (handle_refs, handle);
        }
      else
        {
          DEBUG ("decrementing handle %u to %" G_GSIZE_FORMAT, handles[i],
              r - 1);
          g_hash_table_insert (handle_refs, handle, GSIZE_TO_POINTER (r - 1));
        }
    }

  /* Fire off the unref call asynchronously, ignore error if any.
   * This can't be done idly (so we can combine unrefs) without additional
   * checks, since that would introduce a race between the idle handler
   * running, and someone else holding the handles again. */
  if (unref->len > 0)
    {
      DEBUG ("releasing %u handles", unref->len);

      tp_cli_connection_call_release_handles (self, -1,
          handle_type, unref, post_unref, unref, array_free_TRUE, NULL);
    }
  else
    {
      g_array_free (unref, TRUE);
    }
}


typedef struct {
    TpHandleType handle_type;
    GArray *handles;
    gpointer user_data;
    GDestroyNotify destroy;
    TpConnectionHoldHandlesCb callback;
} HoldHandlesContext;


static void
hold_handles_context_free (gpointer p)
{
  HoldHandlesContext *context = p;

  if (context->destroy != NULL)
    context->destroy (context->user_data);

  g_array_free (context->handles, TRUE);

  g_slice_free (HoldHandlesContext, context);
}

/**
 * TpConnectionHoldHandlesCb:
 * @connection: the connection
 * @handle_type: the handle type that was passed to
 *  tp_connection_hold_handles()
 * @n_handles: the number of handles that were passed to
 *  tp_connection_hold_handles() on success, or 0 on failure
 * @handles: a copy of the array of @n_handles handles that was passed to
 *  tp_connection_hold_handles() on success, or %NULL on failure
 * @error: %NULL on success, or an error on failure
 * @user_data: the same arbitrary pointer that was passed to
 *  tp_connection_hold_handles()
 * @weak_object: the same object that was passed to
 *  tp_connection_hold_handles()
 *
 * Signature of the callback called when tp_connection_hold_handles() succeeds
 * or fails.
 *
 * On success, the caller has one reference to each handle in @handles, which
 * may be released later with tp_connection_unref_handles(). If not
 * released, the handles will remain valid until @connection becomes invalid
 * (signalled by #TpProxy::invalidated).
 *
 * For convenience, the handle type and handles requested by the caller are
 * passed through to this callback on success, so the caller does not have to
 * include them in @user_data.
 */

static void
connection_held_handles (TpConnection *self,
                         const GError *error,
                         gpointer user_data,
                         GObject *weak_object)
{
  HoldHandlesContext *context = user_data;

  g_object_ref (self);

  if (error == NULL)
    {
      DEBUG ("%u handles of type %u", context->handles->len,
          context->handle_type);
      /* On the Telepathy side, we have held these handles (at least once).
       * On the GLib side, record that we have one reference */
      _tp_connection_ref_handles (self, context->handle_type,
          context->handles);

      context->callback (self, context->handle_type, context->handles->len,
          (const TpHandle *) context->handles->data, NULL,
          context->user_data, weak_object);
    }
  else
    {
      DEBUG ("%u handles of type %u failed: %s %u: %s",
          context->handles->len, context->handle_type,
          g_quark_to_string (error->domain), error->code, error->message);
      context->callback (self, context->handle_type, 0, NULL, error,
          context->user_data, weak_object);
    }

  g_object_unref (self);
}


/**
 * tp_connection_hold_handles:
 * @self: a connection
 * @timeout_ms: the timeout in milliseconds, or -1 to use the default
 * @handle_type: the handle type
 * @n_handles: the number of handles in @handles (must be at least 1)
 * @handles: an array of handles
 * @callback: called on success or failure (unless @weak_object has become
 *  unreferenced)
 * @user_data: arbitrary user-supplied data
 * @destroy: called to destroy @user_data after calling @callback, or when
 *  @weak_object becomes unreferenced (whichever occurs sooner)
 * @weak_object: if not %NULL, an object to be weakly referenced: if it is
 *  destroyed, @callback will not be called
 *
 * Hold (ensure a reference to) the given handles, if they are valid.
 *
 * If they are valid, the callback will later be called with the given
 * handles; if not all of them are valid, the callback will be called with
 * an error.
 *
 * This function, along with tp_connection_unref_handles(),
 * tp_connection_get_contact_attributes() and #TpContact, keeps a client-side
 * reference count of handles; you should not use the RequestHandles,
 * HoldHandles and GetContactAttributes D-Bus methods directly as well as these
 * functions.
 */
void
tp_connection_hold_handles (TpConnection *self,
                            gint timeout_ms,
                            TpHandleType handle_type,
                            guint n_handles,
                            const TpHandle *handles,
                            TpConnectionHoldHandlesCb callback,
                            gpointer user_data,
                            GDestroyNotify destroy,
                            GObject *weak_object)
{
  HoldHandlesContext *context;

  g_return_if_fail (TP_IS_CONNECTION (self));
  g_return_if_fail (handle_type > TP_HANDLE_TYPE_NONE);
  g_return_if_fail (handle_type < NUM_TP_HANDLE_TYPES);
  g_return_if_fail (n_handles >= 1);
  g_return_if_fail (callback != NULL);

  context = g_slice_new0 (HoldHandlesContext);
  context->handle_type = handle_type;
  context->user_data = user_data;
  context->destroy = destroy;
  context->handles = g_array_sized_new (FALSE, FALSE, sizeof (guint),
      n_handles);
  g_array_append_vals (context->handles, handles, n_handles);
  context->callback = callback;

  tp_cli_connection_call_hold_handles (self, timeout_ms, handle_type,
      context->handles, connection_held_handles,
      context, hold_handles_context_free, weak_object);
}


typedef struct {
    TpHandleType handle_type;
    guint n_ids;
    gchar **ids;
    gpointer user_data;
    GDestroyNotify destroy;
    TpConnectionRequestHandlesCb callback;
} RequestHandlesContext;


static void
request_handles_context_free (gpointer p)
{
  RequestHandlesContext *context = p;

  g_strfreev (context->ids);

  if (context->destroy != NULL)
    context->destroy (context->user_data);

  g_slice_free (RequestHandlesContext, context);
}


/**
 * TpConnectionRequestHandlesCb:
 * @connection: the connection
 * @handle_type: the handle type that was passed to
 *  tp_connection_request_handles()
 * @n_handles: the number of IDs that were passed to
 *  tp_connection_request_handles() on success, or 0 on failure
 * @handles: the @n_handles handles corresponding to @ids, in the same order,
 *  or %NULL on failure
 * @ids: a copy of the array of @n_handles IDs that was passed to
 *  tp_connection_request_handles() on success, or %NULL on failure
 * @error: %NULL on success, or an error on failure
 * @user_data: the same arbitrary pointer that was passed to
 *  tp_connection_request_handles()
 * @weak_object: the same object that was passed to
 *  tp_connection_request_handles()
 *
 * Signature of the callback called when tp_connection_request_handles()
 * succeeds or fails.
 *
 * On success, the caller has one reference to each handle in @handles, which
 * may be released later with tp_connection_unref_handles(). If not
 * released, the handles will remain valid until @connection becomes invalid
 * (signalled by TpProxy::invalidated).
 *
 * For convenience, the handle type and IDs requested by the caller are
 * passed through to this callback, so the caller does not have to include
 * them in @user_data.
 */


static void
connection_requested_handles (TpConnection *self,
                              const GArray *handles,
                              const GError *error,
                              gpointer user_data,
                              GObject *weak_object)
{
  RequestHandlesContext *context = user_data;

  g_object_ref (self);

  if (error == NULL)
    {
      if (G_UNLIKELY (g_strv_length (context->ids) != handles->len))
        {
          const gchar *cm = tp_proxy_get_bus_name ((TpProxy *) self);
          GError *e = g_error_new (TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
              "Connection manager %s is broken: we asked for %u "
              "handles but RequestHandles returned %u",
              cm, g_strv_length (context->ids), handles->len);

          /* This CM is bad and wrong. We can't trust it to get anything
           * right, so we'd probably better leak the handles, hence this
           * early-return comes before recording that we have a ref to them. */
          g_warning ("%s", e->message);

          context->callback (self, context->handle_type, 0, NULL, NULL,
              error, context->user_data, weak_object);
          return;
        }

      DEBUG ("%u handles of type %u", handles->len,
          context->handle_type);
      /* On the Telepathy side, we have held these handles (at least once).
       * On the GLib side, record that we have one reference */
      _tp_connection_ref_handles (self, context->handle_type,
          handles);

      context->callback (self, context->handle_type, handles->len,
          (const TpHandle *) handles->data,
          (const gchar * const *) context->ids,
          NULL, context->user_data, weak_object);
    }
  else
    {
      DEBUG ("%u handles of type %u failed: %s %u: %s",
          g_strv_length (context->ids), context->handle_type,
          g_quark_to_string (error->domain), error->code, error->message);
      context->callback (self, context->handle_type, 0, NULL, NULL, error,
          context->user_data, weak_object);
    }

  g_object_unref (self);
}


/**
 * tp_connection_request_handles:
 * @self: a connection
 * @timeout_ms: the timeout in milliseconds, or -1 to use the default
 * @handle_type: the handle type
 * @ids: an array of string identifiers for which handles are required,
 *  terminated by %NULL (must not be %NULL or empty)
 * @callback: called on success or failure (unless @weak_object has become
 *  unreferenced)
 * @user_data: arbitrary user-supplied data
 * @destroy: called to destroy @user_data after calling @callback, or when
 *  @weak_object becomes unreferenced (whichever occurs sooner)
 * @weak_object: if not %NULL, an object to be weakly referenced: if it is
 *  destroyed, @callback will not be called
 *
 * Request the handles corresponding to the given identifiers, and if they
 * are valid, hold (ensure a reference to) the corresponding handles.
 *
 * If they are valid, the callback will later be called with the given
 * handles; if not all of them are valid, the callback will be called with
 * an error.
 */
void
tp_connection_request_handles (TpConnection *self,
                               gint timeout_ms,
                               TpHandleType handle_type,
                               const gchar * const *ids,
                               TpConnectionRequestHandlesCb callback,
                               gpointer user_data,
                               GDestroyNotify destroy,
                               GObject *weak_object)
{
  RequestHandlesContext *context;

  g_return_if_fail (TP_IS_CONNECTION (self));
  g_return_if_fail (handle_type > TP_HANDLE_TYPE_NONE);
  g_return_if_fail (handle_type < NUM_TP_HANDLE_TYPES);
  g_return_if_fail (ids != NULL);
  g_return_if_fail (ids[0] != NULL);
  g_return_if_fail (callback != NULL);

  context = g_slice_new0 (RequestHandlesContext);
  context->handle_type = handle_type;
  context->ids = g_strdupv ((GStrv) ids);
  context->user_data = user_data;
  context->destroy = destroy;
  context->callback = callback;

  tp_cli_connection_call_request_handles (self, timeout_ms, handle_type,
      (const gchar **) context->ids, connection_requested_handles,
      context, request_handles_context_free, weak_object);
}


typedef struct {
    tp_cli_connection_interface_contacts_callback_for_get_contact_attributes
        callback;
    gpointer user_data;
    GDestroyNotify destroy;
    gboolean hold;
} GetContactAttributesContext;


static void
get_contact_attributes_context_free (gpointer p)
{
  GetContactAttributesContext *c = p;

  if (c->destroy != NULL)
    c->destroy (c->user_data);

  g_slice_free (GetContactAttributesContext, c);
}


static void
connection_got_contact_attributes (TpConnection *self,
                                   GHashTable *attributes,
                                   const GError *error,
                                   gpointer user_data,
                                   GObject *weak_object)
{
  GetContactAttributesContext *c = user_data;

  DEBUG ("%u handles, hold=%c", g_hash_table_size (attributes),
      c->hold ? 'T' : 'F');

  if (error == NULL && c->hold)
    {
      GHashTableIter iter;
      gpointer key, value;
      GArray *handles = g_array_sized_new (FALSE, FALSE, sizeof (guint),
          g_hash_table_size (attributes));

      g_hash_table_iter_init (&iter, attributes);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          TpHandle handle = GPOINTER_TO_UINT (key);

          DEBUG ("- %u", handle);

          g_array_append_val (handles, handle);
        }

      /* remember that we have a ref */
      _tp_connection_ref_handles (self, TP_HANDLE_TYPE_CONTACT, handles);
      g_array_free (handles, TRUE);
    }

  c->callback (self, attributes, error, c->user_data, weak_object);
}


/**
 * tp_connection_get_contact_attributes:
 * @self: a connection
 * @timeout_ms: the timeout in milliseconds, or -1 to use the default
 * @n_handles: the number of handles in @handles (must be at least 1)
 * @handles: an array of handles
 * @interfaces: a #GStrv of interfaces
 * @hold: if %TRUE, the callback will hold one reference to each valid handle
 * @callback: called on success or failure (unless @weak_object has become
 *  unreferenced)
 * @user_data: arbitrary user-supplied data
 * @destroy: called to destroy @user_data after calling @callback, or when
 *  @weak_object becomes unreferenced (whichever occurs sooner)
 * @weak_object: if not %NULL, an object to be weakly referenced: if it is
 *  destroyed, @callback will not be called
 *
 * Return (via a callback) any number of attributes of the given handles, and
 * if they are valid and @hold is TRUE, hold a reference to them.
 *
 * This is a thin wrapper around the GetContactAttributes D-Bus method, and
 * should be used in preference to
 * tp_cli_connection_interface_contacts_get_contact_attributes(); mixing this
 * function, tp_connection_hold_handles(), tp_connection_unref_handles(), and
 * #TpContact with direct use of the RequestHandles, HoldHandles and
 * GetContactAttributes D-Bus methods is unwise, as #TpConnection and
 * #TpContact perform client-side reference counting of handles.
 * The #TpContact API provides a higher-level abstraction which should
 * usually be used instead.
 *
 * @callback will later be called with the attributes of those of the given
 * handles that were valid. Invalid handles are simply omitted from the
 * parameter to the callback.
 *
 * If @hold is %TRUE, the @callback is given one reference to each handle
 * that appears as a key in the callback's @attributes parameter.
 */
void
tp_connection_get_contact_attributes (TpConnection *self,
    gint timeout_ms,
    guint n_handles,
    const TpHandle *handles,
    const gchar * const *interfaces,
    gboolean hold,
    tp_cli_connection_interface_contacts_callback_for_get_contact_attributes callback,
    gpointer user_data,
    GDestroyNotify destroy,
    GObject *weak_object)
{
  GetContactAttributesContext *c;
  GArray *a;
  guint i;

  DEBUG ("%u handles, hold=%c", n_handles, hold ? 'T' : 'F');

  for (i = 0; i < n_handles; i++)
    DEBUG ("- %u", handles[i]);

  g_return_if_fail (TP_IS_CONNECTION (self));
  g_return_if_fail (n_handles >= 1);
  g_return_if_fail (handles != NULL);
  g_return_if_fail (callback != NULL);

  c = g_slice_new0 (GetContactAttributesContext);
  a = g_array_sized_new (FALSE, FALSE, sizeof (guint), n_handles);

  g_array_append_vals (a, handles, n_handles);

  c->destroy = destroy;
  c->user_data = user_data;
  c->callback = callback;
  c->hold = hold;

  tp_cli_connection_interface_contacts_call_get_contact_attributes (self, -1,
      a, (const gchar **) interfaces, hold, connection_got_contact_attributes,
      c, get_contact_attributes_context_free, weak_object);
  g_array_free (a, TRUE);
}
