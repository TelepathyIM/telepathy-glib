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

#ifndef __TPL_LOG_ENTRY_H__
#define __TPL_LOG_ENTRY_H__

#include <glib-object.h>

#include <log-entry-text.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_ENTRY                  (tpl_log_entry_get_type ())
#define TPL_LOG_ENTRY(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_LOG_ENTRY, TplLogEntry))
#define TPL_LOG_ENTRY_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_LOG_ENTRY, TplLogEntryClass))
#define TPL_IS_LOG_ENTRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_LOG_ENTRY))
#define TPL_IS_LOG_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_LOG_ENTRY))
#define TPL_LOG_ENTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_LOG_ENTRY, TplLogEntryClass))


typedef enum { 
	TPL_LOG_ENTRY_ERROR,
	TPL_LOG_ENTRY_TEXT
} TplLogEntryType;

typedef struct {
	GObject parent;

	/* Private */
	TplLogEntryType type;
	union {
		TplLogEntryText *text;
		void*	generic;
	} entry;
	time_t		timestamp;
} TplLogEntry;

typedef struct {
	GObjectClass	parent_class;
} TplLogEntryClass;

GType tpl_log_entry_get_type (void);

TplLogEntry *tpl_log_entry_new (void);

TplLogEntryType
tpl_log_entry_get_entry_type(TplLogEntry *data);
void *
tpl_log_entry_get_entry(TplLogEntry *data);
time_t
tpl_log_entry_get_timestamp (TplLogEntry *self);

// sets entry type and its object
void
tpl_log_entry_set_entry(TplLogEntry *self, void* entry);
void
tpl_log_entry_set_timestamp (TplLogEntry *self, time_t data);

G_END_DECLS

#endif // __TPL_LOG_ENTRY_H__
