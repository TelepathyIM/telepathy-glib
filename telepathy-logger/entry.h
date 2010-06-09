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

#ifndef __TPL_ENTRY_H__
#define __TPL_ENTRY_H__

#include <glib-object.h>

#include <telepathy-logger/contact.h>

G_BEGIN_DECLS
#define TPL_TYPE_ENTRY (tpl_entry_get_type ())
#define TPL_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_ENTRY, TplEntry))
#define TPL_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_ENTRY, TplEntryClass))
#define TPL_IS_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_ENTRY))
#define TPL_IS_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_ENTRY))
#define TPL_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_ENTRY, TplEntryClass))

typedef struct _TplEntry TplEntry;
typedef struct _TplEntryClass TplEntryClass;
typedef struct _TplEntryPriv TplEntryPriv;

GType tpl_entry_get_type (void);

typedef enum
{
  TPL_ENTRY_DIRECTION_NONE = 0,

  TPL_ENTRY_DIRECTION_IN,
  TPL_ENTRY_DIRECTION_OUT
} TplEntryDirection;

gint64 tpl_entry_get_timestamp (TplEntry *self);

const gchar *tpl_entry_get_account_path (TplEntry *self);

TplContact * tpl_entry_get_sender (TplEntry *self);
TplContact * tpl_entry_get_receiver (TplEntry *self);

G_END_DECLS
#endif // __TPL_ENTRY_H__
