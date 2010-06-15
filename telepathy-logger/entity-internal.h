/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *Copyright (C) 2009-2010 Collabora Ltd.
 *
 *This library is free software; you can redistribute it and/or
 *modify it under the terms of the GNU Lesser General Public
 *License as published by the Free Software Foundation; either
 *version 2.1 of the License, or (at your option) any later version.
 *
 *This library is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *Lesser General Public License for more details.
 *
 *You should have received a copy of the GNU Lesser General Public
 *License along with this library; if not, write to the Free Software
 *Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#ifndef __TPL_CONTACT_INTERNAL_H__
#define __TPL_CONTACT_INTERNAL_H__

#include <telepathy-logger/entity.h>

#include <glib-object.h>
#include <telepathy-glib/contact.h>

G_BEGIN_DECLS
#define TPL_CONTACT_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CONTACT, TplContactClass))
#define TPL_IS_CONTACT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CONTACT))
#define TPL_CONTACT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CONTACT, TplContactClass))

typedef struct
{
  GObjectClass parent_class;
} TplContactClass;

TplContact *_tpl_contact_from_tp_contact (TpContact *contact);
TplContact *_tpl_contact_new (const gchar *identifier);

void _tpl_contact_set_alias (TplContact *self,
    const gchar *data);

void _tpl_contact_set_identifier (TplContact *self,
    const gchar *data);

void _tpl_contact_set_contact_type (TplContact *self,
    TplContactType data);

void _tpl_contact_set_avatar_token (TplContact *self,
    const gchar *data);

TplContact * _tpl_contact_from_room_id (const gchar *chatroom_id);

G_END_DECLS
#endif // __TPL_CONTACT_INTERNAL_H__
