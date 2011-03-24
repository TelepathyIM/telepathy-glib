/*
 * Copyright (C) 2008-2011 Collabora Ltd.
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
 * Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>,
 *          Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "config.h"

#include <telepathy-logger/log-store-internal.h>

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include <telepathy-logger/debug-internal.h>

/**
 * SECTION:log-store
 * @title: TplLogStore
 * @short_description: LogStore interface can register into #TplLogManager as
 * #TplLogStore:writable or #TplLogStore:readable log stores.
 * @see_also: #text-event:#TplTextEvent and other subclasses when they'll exist
 *
 * The #TplLogStore defines all the public methods that a TPL Log Store has to
 * implement in order to be used into a #TplLogManager.
 */

static void _tpl_log_store_init (gpointer g_iface);

GType
_tpl_log_store_get_type (void)
{
  static GType type = 0;
  if (type == 0)
    {
      static const GTypeInfo info = {
          sizeof (TplLogStoreInterface),
          NULL, /* base_init */
          NULL, /* base_finalize */
          (GClassInitFunc) _tpl_log_store_init, /* class_init */
          NULL, /* class_finalize */
          NULL, /* class_data */
          0,
          0,    /* n_preallocs */
          NULL  /* instance_init */
      };
      type = g_type_register_static (G_TYPE_INTERFACE, "TplLogStore",
          &info, 0);
    }
  return type;
}

static void
_tpl_log_store_init (gpointer g_iface)
{
  g_object_interface_install_property (g_iface,
      g_param_spec_string ("name",
        "Name",
        "The TplLogStore implementation's name",
        NULL,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * TplLogStore:writable:
   *
   * Defines wether the object is writable for a #TplLogManager.
   *
   * If an TplLogStore implementation is writable, the #TplLogManager will call
   * it's tpl_log_store_add_event() method every time a loggable even occurs,
   * i.e., everytime _tpl_log_manager_add_event() is called.
   */
  g_object_interface_install_property (g_iface,
      g_param_spec_boolean ("readable",
        "Readable",
        "Whether this log store is readable",
        TRUE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * TplLogStore:readable:
   *
   * Defines wether the object is readable for a #TplLogManager.
   *
   * If an TplLogStore implementation is readable, the #TplLogManager will
   * use the query methods against the instance (i.e. tpl_log_store_get_dates())
   * every time a #TplLogManager instance is queried (i.e.,
   * tpl_log_manager_get_date()).
   */
  g_object_interface_install_property (g_iface,
      g_param_spec_boolean ("writable",
        "Writable",
        "Whether this log store is writable",
        TRUE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

const gchar *
_tpl_log_store_get_name (TplLogStore *self)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->get_name)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_name (self);
}


gboolean
_tpl_log_store_exists (TplLogStore *self,
    TpAccount *account,
    TplEntity *target,
    gint type_mask)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE (self), FALSE);
  if (!TPL_LOG_STORE_GET_INTERFACE (self)->exists)
    return FALSE;

  return TPL_LOG_STORE_GET_INTERFACE (self)->exists (self, account, target,
      type_mask);
}


/**
 * _tpl_log_store_add_event:
 * @self: a TplLogStore
 * @event: an instance of a subclass of TplEvent (ie TplTextEvent)
 * @error: memory location used if an error occurs
 *
 * Sends @event to the LogStore @self, in order to be stored.
 *
 * Returns: %TRUE if succeeds, %FALSE with @error set otherwise
 */
gboolean
_tpl_log_store_add_event (TplLogStore *self,
    TplEvent *event,
    GError **error)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  if (TPL_LOG_STORE_GET_INTERFACE (self)->add_event == NULL)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "%s: add_event not implemented, but writable set to TRUE : %s",
          G_STRFUNC, G_OBJECT_CLASS_NAME (self));
      return FALSE;
    }

  return TPL_LOG_STORE_GET_INTERFACE (self)->add_event (self, event,
      error);
}


/**
 * _tpl_log_store_get_dates:
 * @self: a TplLogStore
 * @account: a TpAccount
 * @target: a #TplEntity
 * @type_mask: event type mask see #TplEventTypeMask
 *
 * Retrieves a list of #GDate, corresponding to each day
 * at least an event was sent to or received from @id.
 *
 * Returns: a GList of (GDate *), to be freed using something like
 * g_list_foreach (lst, g_date_free, NULL);
 * g_list_free (lst);
 */
GList *
_tpl_log_store_get_dates (TplLogStore *self,
    TpAccount *account,
    TplEntity *target,
    gint type_mask)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  if (TPL_LOG_STORE_GET_INTERFACE (self)->get_dates == NULL)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_dates (self, account,
      target, type_mask);
}


/**
 * _tpl_log_store_get_events_for_date:
 * @self: a TplLogStore
 * @account: a TpAccount
 * @target: a #TplEntity
 * @type_mask: event type mask see #TplEventTypeMask
 * @date: a #GDate
 *
 * Retrieves a list of events, with timestamp matching @date.
 *
 * Returns: a GList of TplTextEvent, to be freed using something like
 * g_list_foreach (lst, g_object_unref, NULL);
 * g_list_free (lst);
 */
GList *
_tpl_log_store_get_events_for_date (TplLogStore *self,
    TpAccount *account,
    TplEntity *target,
    gint type_mask,
    const GDate *date)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  if (TPL_LOG_STORE_GET_INTERFACE (self)->get_events_for_date == NULL)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_events_for_date (self,
      account, target, type_mask, date);
}


GList *
_tpl_log_store_get_recent_events (TplLogStore *self,
    TpAccount *account,
    TplEntity *target,
    gint type_mask)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  if (TPL_LOG_STORE_GET_INTERFACE (self)->get_recent_events == NULL)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_recent_events (self, account,
      target, type_mask);
}


/**
 * _tpl_log_store_get_entities:
 * @self: a TplLogStore
 * @account: a TpAccount
 *
 * Retrieves a list of #TplEntity, corresponding to each buddy/chatroom id
 * the user exchanged at least a event with inside @account.
 *
 * Returns: a GList of #TplEntity, to be freed using something like
 * g_list_foreach (lst, g_object_unref, NULL);
 * g_list_free (lst);
 */
GList *
_tpl_log_store_get_entities (TplLogStore *self,
    TpAccount *account)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  if (TPL_LOG_STORE_GET_INTERFACE (self)->get_entities == NULL)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_entities (self, account);
}


/**
 * _tpl_log_store_search_new:
 * @self: a TplLogStore
 * @text: a text to be searched among text messages
 * @type_mask: event type mask see #TplEventTypeMask
 *
 * Searches all textual log entries matching @text.
 *
 * Returns: a GList of (TplLogSearchHit *), to be freed using something like
 * g_list_foreach (lst, tpl_log_manager_search_free, NULL);
 * g_list_free (lst);
 */
GList *
_tpl_log_store_search_new (TplLogStore *self,
    const gchar *text,
    gint type_mask)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  if (TPL_LOG_STORE_GET_INTERFACE (self)->search_new == NULL)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->search_new (self, text,
      type_mask);
}


/**
 * _tpl_log_store_get_filtered_events:
 * @self: a TplLogStore
 * @account: a TpAccount
 * @target: a #TplEntity
 * @type_mask: event type mask see #TplEventTypeMask
 * @num_events: max number of events to return
 * @filter: filter function
 * @user_data: data be passed to @filter, may be NULL
 *
 * Filters all events related to @id, using the boolean function
 * @filter.
 * It will return at most the last (ie most recent) @num_events events.
 * Pass G_MAXUINT if all the events are needed.
 *
 * Returns: a GList of TplTextEvent, to be freed using something like
 * g_list_foreach (lst, g_object_unref, NULL);
 * g_list_free (lst);
 */
GList *
_tpl_log_store_get_filtered_events (TplLogStore *self,
    TpAccount *account,
    TplEntity *target,
    gint type_mask,
    guint num_events,
    TplLogEventFilter filter,
    gpointer user_data)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  if (TPL_LOG_STORE_GET_INTERFACE (self)->get_filtered_events == NULL)
    return NULL;

  return TPL_LOG_STORE_GET_INTERFACE (self)->get_filtered_events (self,
      account, target, type_mask, num_events, filter, user_data);
}


void
_tpl_log_store_clear (TplLogStore *self)
{
  g_return_if_fail (TPL_IS_LOG_STORE (self));
  if (TPL_LOG_STORE_GET_INTERFACE (self)->clear == NULL)
    return;

  TPL_LOG_STORE_GET_INTERFACE (self)->clear (self);
}


void
_tpl_log_store_clear_account (TplLogStore *self, TpAccount *account)
{
  g_return_if_fail (TPL_IS_LOG_STORE (self));
  if (TPL_LOG_STORE_GET_INTERFACE (self)->clear_account == NULL)
    return;

  TPL_LOG_STORE_GET_INTERFACE (self)->clear_account (self, account);
}


void
_tpl_log_store_clear_entity (TplLogStore *self,
    TpAccount *account,
    TplEntity *entity)
{
  g_return_if_fail (TPL_IS_LOG_STORE (self));
  if (TPL_LOG_STORE_GET_INTERFACE (self)->clear_entity == NULL)
    return;

  TPL_LOG_STORE_GET_INTERFACE (self)->clear_entity (self, account, entity);
}


gboolean
_tpl_log_store_is_writable (TplLogStore *self)
{
  gboolean writable;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), FALSE);

  g_object_get (self,
      "writable", &writable,
      NULL);

  return writable;
}


gboolean
_tpl_log_store_is_readable (TplLogStore *self)
{
  gboolean readable;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), FALSE);

  g_object_get (self,
      "readable", &readable,
      NULL);

  return readable;
}
