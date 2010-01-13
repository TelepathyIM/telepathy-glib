/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
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
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "log-entry.h"

#include <glib.h>

#include <telepathy-logger/debug.h>

G_DEFINE_TYPE (TplLogEntry, tpl_log_entry, G_TYPE_OBJECT)
     static void tpl_log_entry_finalize (GObject * obj)
{
  G_OBJECT_CLASS (tpl_log_entry_parent_class)->finalize (obj);
}

static void
tpl_log_entry_dispose (GObject * obj)
{
  TplLogEntry *self = TPL_LOG_ENTRY (obj);

  DEBUG ("TplLogEntry: disposing\n");

  tpl_object_unref_if_not_null (tpl_log_entry_get_entry (self));
  self->entry.generic = NULL;

  G_OBJECT_CLASS (tpl_log_entry_parent_class)->dispose (obj);

  DEBUG ("TplLogEntry: disposed\n");
}


static void
tpl_log_entry_class_init (TplLogEntryClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = tpl_log_entry_finalize;
  object_class->dispose = tpl_log_entry_dispose;
}

static void
tpl_log_entry_init (TplLogEntry * self)
{
}


void
tpl_log_entry_set_entry (TplLogEntry * self, void *entry)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (self));
  g_return_if_fail (self->entry.generic == NULL);

  if (TPL_IS_LOG_ENTRY_TEXT (entry))
    {
      self->type = TPL_LOG_ENTRY_TEXT;
      self->entry.text = entry;
    }
  else
    {
      g_error ("TplLogEntry does handle only Text channels\n");
    }
}

TplLogEntryType
tpl_log_entry_get_entry_type (TplLogEntry * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), TPL_LOG_ENTRY_ERROR);

  return self->type;
}

void *
tpl_log_entry_get_entry (TplLogEntry * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), NULL);

  switch (self->type)
    {
    case TPL_LOG_ENTRY_TEXT:
      if (!TPL_IS_LOG_ENTRY_TEXT (self->entry.text))
	{
	  g_error ("TplLogEntry->entry->text not a TplLogEntryText instance");
	  return NULL;
	}
      return self->entry.text;
      break;
    default:
      g_warning ("TplLogEntry type not handled\n");
      return NULL;
      break;
    }
}

TplLogEntry *
tpl_log_entry_new (void)
{
  return g_object_new (TPL_TYPE_LOG_ENTRY, NULL);
}


time_t
tpl_log_entry_get_timestamp (TplLogEntry * self)
{
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (self), -1);
  return self->timestamp;
}

void
tpl_log_entry_set_timestamp (TplLogEntry * self, time_t data)
{
  g_return_if_fail (TPL_IS_LOG_ENTRY (self));

  self->timestamp = data;
}
