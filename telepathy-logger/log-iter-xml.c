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
#include "log-iter-xml-internal.h"


struct _TplLogIterXmlPriv
{
  TpAccount *account;
  TplEntity *target;
  TplLogEventFilter filter;
  TplLogStore *store;
  gint type_mask;
  gpointer filter_data;
};

enum
{
  PROP_ACCOUNT = 1,
  PROP_FILTER,
  PROP_FILTER_DATA,
  PROP_STORE,
  PROP_TARGET,
  PROP_TYPE_MASK
};


G_DEFINE_TYPE (TplLogIterXml, tpl_log_iter_xml, TPL_TYPE_LOG_ITER);


static void
tpl_log_iter_xml_dispose (GObject *object)
{
  TplLogIterXmlPriv *priv;

  priv = TPL_LOG_ITER_XML (object)->priv;

  g_clear_object (&priv->account);
  g_clear_object (&priv->store);
  g_clear_object (&priv->target);

  G_OBJECT_CLASS (tpl_log_iter_xml_parent_class)->dispose (object);
}


static void
tpl_log_iter_xml_finalize (GObject *object)
{
  G_OBJECT_CLASS (tpl_log_iter_xml_parent_class)->finalize (object);
}


static void
tpl_log_iter_xml_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplLogIterXmlPriv *priv;

  priv = TPL_LOG_ITER_XML (object)->priv;

  switch (param_id)
    {
    case PROP_ACCOUNT:
      g_value_set_object (value, priv->account);
      break;

    case PROP_FILTER:
      g_value_set_pointer (value, priv->filter);
      break;

    case PROP_FILTER_DATA:
      g_value_set_pointer (value, priv->filter_data);
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
tpl_log_iter_xml_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplLogIterXmlPriv *priv;

  priv = TPL_LOG_ITER_XML (object)->priv;

  switch (param_id)
    {
    case PROP_ACCOUNT:
      priv->account = g_value_dup_object (value);
      break;

    case PROP_FILTER:
      priv->filter = g_value_get_pointer (value);
      break;

    case PROP_FILTER_DATA:
      priv->filter_data = g_value_get_pointer (value);
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
tpl_log_iter_xml_init (TplLogIterXml *iter)
{
  iter->priv = G_TYPE_INSTANCE_GET_PRIVATE (iter, TPL_TYPE_LOG_ITER_XML,
      TplLogIterXmlPriv);
}


static void
tpl_log_iter_xml_class_init (TplLogIterXmlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->dispose = tpl_log_iter_xml_dispose;
  object_class->finalize = tpl_log_iter_xml_finalize;
  object_class->get_property = tpl_log_iter_xml_get_property;
  object_class->set_property = tpl_log_iter_xml_set_property;

  param_spec = g_param_spec_object ("account",
      "Account",
      "The account whose logs are to be traversed",
      TP_TYPE_ACCOUNT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT, param_spec);

  param_spec = g_param_spec_pointer ("filter",
      "Filter",
      "An optional filter function",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_FILTER, param_spec);

  param_spec = g_param_spec_pointer ("filter-data",
      "Filter Data",
      "User data to pass to the filter function",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_FILTER_DATA, param_spec);

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

  g_type_class_add_private (klass, sizeof (TplLogIterXmlPriv));
}


TplLogIter *
tpl_log_iter_xml_new (TplLogStore *store,
    TpAccount *account,
    TplEntity *target,
    gint type_mask,
    TplLogEventFilter filter,
    gpointer filter_data)
{
  return g_object_new (TPL_TYPE_LOG_ITER_XML,
      "store", store,
      "account", account,
      "target", target,
      "type-mask", type_mask,
      "filter", filter,
      "filter-data", filter_data,
      NULL);
}
