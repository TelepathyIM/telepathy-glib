/*
 * Copyright (C) 2012 Red Hat, Inc.
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
 * Author: Debarshi Ray <debarshir@freedesktop.org>
 */

#include "config.h"
#include "log-iter-pidgin-internal.h"


struct _TplLogIterPidginPriv
{
  GList *dates;
  GList *events;
  GList *next_date;
  GList *next_event;
  TpAccount *account;
  TplEntity *target;
  TplLogStore *store;
  gint type_mask;
};

enum
{
  PROP_ACCOUNT = 1,
  PROP_STORE,
  PROP_TARGET,
  PROP_TYPE_MASK
};


G_DEFINE_TYPE (TplLogIterPidgin, tpl_log_iter_pidgin, TPL_TYPE_LOG_ITER);


static GList *
tpl_log_iter_pidgin_get_events (TplLogIter *iter,
    guint num_events,
    GError **error)
{
  TplLogIterPidginPriv *priv;
  GList *events;
  guint i;

  priv = TPL_LOG_ITER_PIDGIN (iter)->priv;
  events = NULL;

  if (priv->dates == NULL)
    {
      priv->dates = _tpl_log_store_get_dates (priv->store, priv->account,
          priv->target, priv->type_mask);
      priv->next_date = g_list_last (priv->dates);
    }

  i = 0;
  while (i < num_events)
    {
      TplEvent *event;

      if (priv->next_event == NULL)
        {
          if (priv->next_date == NULL)
            break;

          g_list_free_full (priv->events, g_object_unref);
          priv->events = _tpl_log_store_get_events_for_date (priv->store,
              priv->account, priv->target, priv->type_mask,
              (GDate *) priv->next_date->data);

          priv->next_event = g_list_last (priv->events);
          priv->next_date = g_list_previous (priv->next_date);
        }

      event = TPL_EVENT (priv->next_event->data);
      events = g_list_prepend (events, g_object_ref (event));
      i++;

      priv->next_event = g_list_previous (priv->next_event);
    }

  return events;
}


static void
tpl_log_iter_pidgin_rewind (TplLogIter *iter,
    guint num_events,
    GError **error)
{
  GList *e;
  TplLogIterPidginPriv *priv;
  guint i;

  priv = TPL_LOG_ITER_PIDGIN (iter)->priv;
  e = NULL;

  /* Set e to the last event that was returned */
  if (priv->next_event == NULL)
    e = priv->events;
  else
    e = g_list_next (priv->next_event);

  i = 0;
  while (i < num_events)
    {
      if (e == NULL)
        {
          GList *d;

          if (priv->next_date == NULL)
            d = priv->dates;
          else
            d = g_list_next (priv->next_date);

          /* This can happen if get_events was never called or called
           * with num_events == 0
           */
          if (d == NULL)
            break;

          g_list_free_full (priv->events, g_object_unref);
          priv->events = NULL;
          priv->next_event = NULL;

          /* Rollback the priv->next_date */
          priv->next_date = d;

          /* Rollback the current date (ie. d) */
          d = g_list_next (d);
          if (d == NULL)
            break;

          priv->events = _tpl_log_store_get_events_for_date (priv->store,
              priv->account, priv->target, priv->type_mask,
              (GDate *) d->data);
          e = priv->events;
        }

      priv->next_event = e;
      e = g_list_next (e);
      i++;
    }
}


static void
tpl_log_iter_pidgin_dispose (GObject *object)
{
  TplLogIterPidginPriv *priv;

  priv = TPL_LOG_ITER_PIDGIN (object)->priv;

  g_list_free_full (priv->dates, (GDestroyNotify) g_date_free);
  priv->dates = NULL;

  g_list_free_full (priv->events, g_object_unref);
  priv->events = NULL;

  g_clear_object (&priv->account);
  g_clear_object (&priv->store);
  g_clear_object (&priv->target);

  G_OBJECT_CLASS (tpl_log_iter_pidgin_parent_class)->dispose (object);
}


static void
tpl_log_iter_pidgin_finalize (GObject *object)
{
  G_OBJECT_CLASS (tpl_log_iter_pidgin_parent_class)->finalize (object);
}


static void
tpl_log_iter_pidgin_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplLogIterPidginPriv *priv;

  priv = TPL_LOG_ITER_PIDGIN (object)->priv;

  switch (param_id)
    {
    case PROP_ACCOUNT:
      g_value_set_object (value, priv->account);
      break;

    case PROP_STORE:
      g_value_set_object (value, priv->store);
      break;

    case PROP_TARGET:
      g_value_set_object (value, priv->target);
      break;

    case PROP_TYPE_MASK:
      g_value_set_int (value, priv->type_mask);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
tpl_log_iter_pidgin_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplLogIterPidginPriv *priv;

  priv = TPL_LOG_ITER_PIDGIN (object)->priv;

  switch (param_id)
    {
    case PROP_ACCOUNT:
      priv->account = g_value_dup_object (value);
      break;

    case PROP_STORE:
      priv->store = g_value_dup_object (value);
      break;

    case PROP_TARGET:
      priv->target = g_value_dup_object (value);
      break;

    case PROP_TYPE_MASK:
      priv->type_mask = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
tpl_log_iter_pidgin_init (TplLogIterPidgin *iter)
{
  iter->priv = G_TYPE_INSTANCE_GET_PRIVATE (iter, TPL_TYPE_LOG_ITER_PIDGIN,
      TplLogIterPidginPriv);
}


static void
tpl_log_iter_pidgin_class_init (TplLogIterPidginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TplLogIterClass *log_iter_class = TPL_LOG_ITER_CLASS (klass);
  GParamSpec *param_spec;

  object_class->dispose = tpl_log_iter_pidgin_dispose;
  object_class->finalize = tpl_log_iter_pidgin_finalize;
  object_class->get_property = tpl_log_iter_pidgin_get_property;
  object_class->set_property = tpl_log_iter_pidgin_set_property;
  log_iter_class->get_events = tpl_log_iter_pidgin_get_events;
  log_iter_class->rewind = tpl_log_iter_pidgin_rewind;

  param_spec = g_param_spec_object ("account",
      "Account",
      "The account whose logs are to be traversed",
      TP_TYPE_ACCOUNT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT, param_spec);

  param_spec = g_param_spec_object ("store",
      "Store",
      "The storage backend from which the logs are to be retrieved",
      TPL_TYPE_LOG_STORE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STORE, param_spec);

  param_spec = g_param_spec_object ("target",
      "Target",
      "The target entity with which the account interacted",
      TPL_TYPE_ENTITY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TARGET, param_spec);

  param_spec = g_param_spec_int ("type-mask",
      "Type Mask",
      "A bitmask to filter the events to be retrieved",
      1,
      0xffff,
      TPL_EVENT_MASK_ANY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TYPE_MASK, param_spec);

  g_type_class_add_private (klass, sizeof (TplLogIterPidginPriv));
}


TplLogIter *
tpl_log_iter_pidgin_new (TplLogStore *store,
    TpAccount *account,
    TplEntity *target,
    gint type_mask)
{
  return g_object_new (TPL_TYPE_LOG_ITER_PIDGIN,
      "store", store,
      "account", account,
      "target", target,
      "type-mask", type_mask,
      NULL);
}
