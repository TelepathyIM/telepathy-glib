/*
 * Copyright (C) 2011 Collabora Ltd.
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
 * Authors: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include "config.h"
#include "call-event.h"
#include "call-event-internal.h"

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

#include "entity.h"
#include "event.h"
#include "event-internal.h"
#include "util-internal.h"

#define DEBUG_FLAG TPL_DEBUG_LOG_EVENT
#include "debug-internal.h"

/**
 * SECTION:call-event
 * @title: TplCallEvent
 * @short_description: Representation of a call log event
 *
 * A subclass of #TplEvent representing a call log event.
 */

/**
 * TplCallEvent:
 *
 * An object representing a call log event.
 */

G_DEFINE_TYPE (TplCallEvent, tpl_call_event, TPL_TYPE_EVENT)

struct _TplCallEventPriv
{
  GTimeSpan duration;
  TplEntity *end_actor;
  TpCallStateChangeReason end_reason;
  gchar *detailed_end_reason;
};

enum
{
  PROP_DURATION = 1,
  PROP_END_ACTOR,
  PROP_END_REASON,
  PROP_DETAILED_END_REASON
};

static const gchar* end_reasons[] = {
    "unknown",
    "progress-made",
    "user-requested",
    "forwared",
    "rejected",
    "no-answer",
    "invalid-contact",
    "permission-denied",
    "busy",
    "internal-error",
    "service-error",
    "network-error",
    "media-error",
    "connectivity-error"
};


static void
tpl_call_event_dispose (GObject *object)
{
  TplCallEventPriv *priv = TPL_CALL_EVENT (object)->priv;

  tp_clear_object (&priv->end_actor);
  tp_clear_pointer (&priv->detailed_end_reason, g_free);

  G_OBJECT_CLASS (tpl_call_event_parent_class)->dispose (object);
}


static void
tpl_call_event_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplCallEventPriv *priv = TPL_CALL_EVENT (object)->priv;

  switch (param_id)
    {
      case PROP_DURATION:
        g_value_set_int64 (value, priv->duration);
        break;
      case PROP_END_ACTOR:
        g_value_set_object (value, priv->end_actor);
        break;
      case PROP_END_REASON:
        g_value_set_int (value, priv->end_reason);
        break;
      case PROP_DETAILED_END_REASON:
        g_value_set_string (value, priv->detailed_end_reason);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}


static void
tpl_call_event_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplCallEventPriv *priv = TPL_CALL_EVENT (object)->priv;

  switch (param_id)
    {
      case PROP_DURATION:
        priv->duration = g_value_get_int64 (value);
        break;
      case PROP_END_ACTOR:
        priv->end_actor = g_value_dup_object (value);
        break;
      case PROP_END_REASON:
        priv->end_reason = g_value_get_int (value);
        break;
      case PROP_DETAILED_END_REASON:
        priv->detailed_end_reason = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  }
}

static void tpl_call_event_class_init (TplCallEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->dispose = tpl_call_event_dispose;
  object_class->get_property = tpl_call_event_get_property;
  object_class->set_property = tpl_call_event_set_property;

  param_spec = g_param_spec_int64 ("duration",
      "Duration",
      "The call duration in seconds",
      -1, G_MAXINT64, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DURATION, param_spec);

  param_spec = g_param_spec_object ("end-actor",
      "End Actor",
      "Actor (a #TplEntity) that caused the call to end",
      TPL_TYPE_ENTITY,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_END_ACTOR, param_spec);

  param_spec = g_param_spec_int ("end-reason",
      "End Reason",
      "Reason for wich this call was ended",
      0, NUM_TP_CALL_STATE_CHANGE_REASONS, TP_CALL_STATE_CHANGE_REASON_UNKNOWN,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_END_REASON, param_spec);

  param_spec = g_param_spec_string ("detailed-end-reason",
      "Detailed End Reason",
      "A string representing a D-Bus error that gives more details about the end reason",
      "",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DETAILED_END_REASON, param_spec);

  g_type_class_add_private (object_class, sizeof (TplCallEventPriv));
}


static void
tpl_call_event_init (TplCallEvent *self)
{
  TplCallEventPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_CALL_EVENT, TplCallEventPriv);
  self->priv = priv;
}


/**
 * tpl_call_event_get_duration:
 * @self: a #TplCallEvent
 *
 * Returns: the same duration as the #TplCallEvent:duration property
 */
GTimeSpan
tpl_call_event_get_duration (TplCallEvent *self)
{
  g_return_val_if_fail (TPL_IS_CALL_EVENT (self), 0);

  return self->priv->duration;
}

/**
 * tpl_call_event_get_end_actor:
 * @self: a #TplCallEvent
 *
 * Returns: (transfer none): the same #TplEntity
 *          as #TplCallEvent:end-actor property
 */
TplEntity *
tpl_call_event_get_end_actor (TplCallEvent *self)
{
  g_return_val_if_fail (TPL_IS_CALL_EVENT (self), NULL);

  return self->priv->end_actor;
}


/**
 * tpl_call_event_get_end_reason:
 * @self: a #TplCallEvent
 *
 * Returns: the same #TpCallStateChangeReason as #TplCallEvent:end-reason property
 */
TpCallStateChangeReason
tpl_call_event_get_end_reason (TplCallEvent *self)
{
  g_return_val_if_fail (TPL_IS_CALL_EVENT (self),
      TP_CALL_STATE_CHANGE_REASON_UNKNOWN);

  return self->priv->end_reason;
}


/**
 * tpl_call_event_get_detailed_end_reason:
 * @self: a #TplCallEvent
 *
 * Returns: (transfer none): the same string as the
 *          #TplCallEvent:detailed-end-reason property
 */
const gchar *
tpl_call_event_get_detailed_end_reason (TplCallEvent *self)
{
  g_return_val_if_fail (TPL_IS_CALL_EVENT (self), "");

  return self->priv->detailed_end_reason;
}


const gchar *
_tpl_call_event_end_reason_to_str (TpCallStateChangeReason reason)
{
  g_return_val_if_fail (reason < G_N_ELEMENTS (end_reasons), end_reasons[0]);
  return end_reasons[reason];
}


TpCallStateChangeReason
_tpl_call_event_str_to_end_reason (const gchar *str)
{
  guint i;
  for (i = 0; i < G_N_ELEMENTS (end_reasons); i++)
    if (g_strcmp0 (str, end_reasons[i]) == 0)
      return i;

  return TP_CALL_STATE_CHANGE_REASON_UNKNOWN;
}
