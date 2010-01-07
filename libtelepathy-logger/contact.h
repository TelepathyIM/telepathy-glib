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

#ifndef __TPL_CONTACT_H__
#define __TPL_CONTACT_H__

#include <glib-object.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/account.h>

G_BEGIN_DECLS

#define TPL_TYPE_CONTACT		(tpl_contact_get_type ())
#define TPL_CONTACT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CONTACT, TplContact))
#define TPL_CONTACT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CONTACT, TplContactClass))
#define TPL_IS_CONTACT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CONTACT))
#define TPL_IS_CONTACT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CONTACT))
#define TPL_CONTACT_GET_CLASS(obj) 	(G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CONTACT, TplContactClass))


typedef enum {
	TPL_CONTACT_UNKNOWN,
	TPL_CONTACT_USER,
	TPL_CONTACT_GROUP
} TplContactType;

typedef struct {
	GObject parent;

	/* Private */
	TpContact	*contact;
	TplContactType	contact_type;
	gchar	*alias;
	gchar	*identifier;
	gchar	*presence_status;
	gchar	*presence_message;

	TpAccount	*account;
} TplContact;


typedef struct {
	GObjectClass	parent_class;
} TplContactClass;


GType  tpl_contact_get_type (void);

TplContact *tpl_contact_from_tp_contact(TpContact *contact);

TplContact *tpl_contact_new(void);

TpContact *tpl_contact_get_contact(TplContact *self);

gchar *tpl_contact_get_alias(TplContact *self);

gchar *tpl_contact_get_identifier(TplContact *self);

gchar *tpl_contact_get_presence_status(TplContact *self);

gchar *tpl_contact_get_presence_message(TplContact *self);

TplContactType tpl_contact_get_contact_type(TplContact *self);

TpAccount *tpl_contact_get_account(TplContact *self);

void tpl_contact_set_contact(TplContact *self, TpContact *data);

void tpl_contact_set_account(TplContact *self, TpAccount *data);

void tpl_contact_set_alias(TplContact *self, gchar *data); 

void tpl_contact_set_identifier(TplContact *self, gchar *data); 

void tpl_contact_set_presence_status(TplContact *self, gchar *data); 

void tpl_contact_set_presence_message(TplContact *self, gchar *data); 

void tpl_contact_set_contact_type(TplContact *self, TplContactType data);

G_END_DECLS

#endif // __TPL_CONTACT_H__
