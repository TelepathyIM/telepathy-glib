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

#include "contact.h"

#include <telepathy-glib/account.h>

#include <telepathy-logger/utils.h>

G_DEFINE_TYPE (TplContact, tpl_contact, G_TYPE_OBJECT)

static void
tpl_contact_finalize (GObject * obj)
{
  TplContact *self = TPL_CONTACT (obj);

  g_free (self->alias);
  self->alias = NULL;
  g_free (self->identifier);
  self->identifier = NULL;
  g_free (self->presence_status);
  self->presence_status = NULL;
  g_free (self->presence_message);
  self->presence_message = NULL;

  G_OBJECT_CLASS (tpl_contact_parent_class)->finalize (obj);
}

static void
tpl_contact_dispose (GObject * obj)
{
  TplContact *self = TPL_CONTACT (obj);

  tpl_object_unref_if_not_null (self->contact);
  self->contact = NULL;

  G_OBJECT_CLASS (tpl_contact_parent_class)->dispose (obj);
}



static void tpl_contact_class_init (TplContactClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = tpl_contact_finalize;
  object_class->dispose = tpl_contact_dispose;
}

static void
tpl_contact_init (TplContact * self)
{
}


TplContact *
tpl_contact_from_tp_contact (TpContact * contact)
{
  TplContact *ret;

  ret = tpl_contact_new ();
  tpl_contact_set_contact (ret, contact);
  tpl_contact_set_identifier (ret,
			      tp_contact_get_identifier (contact));
  tpl_contact_set_alias (ret, (gchar *) tp_contact_get_alias (contact));
  tpl_contact_set_presence_status (ret,
           tp_contact_get_presence_status (contact));
  tpl_contact_set_presence_message (ret,
            tp_contact_get_presence_message (contact));

  return ret;
}

TplContact *
tpl_contact_new (void)
{
  return g_object_new (TPL_TYPE_CONTACT, NULL);
}

TpContact *
tpl_contact_get_contact (TplContact * self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return self->contact;
}

const gchar *
tpl_contact_get_alias (TplContact * self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return self->alias;
}

const gchar *
tpl_contact_get_identifier (TplContact * self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return self->identifier;
}

const gchar *
tpl_contact_get_presence_status (TplContact * self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return self->presence_status;
}

const gchar *
tpl_contact_get_presence_message (TplContact * self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return self->presence_message;
}

TplContactType
tpl_contact_get_contact_type (TplContact * self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), TPL_CONTACT_UNKNOWN);

  return self->contact_type;
}

const gchar *
tpl_contact_get_avatar_token (TplContact * self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return self->avatar_token;
}

TpAccount *
tpl_contact_get_account (TplContact * self)
{
  g_return_val_if_fail (TPL_IS_CONTACT (self), NULL);

  return self->account;
}


void
tpl_contact_set_contact (TplContact * self, TpContact * data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));
  g_return_if_fail (TP_IS_CONTACT (data) || data == NULL);

  tpl_object_unref_if_not_null (self->contact);
  self->contact = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_contact_set_account (TplContact * self, TpAccount * data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));
  g_return_if_fail (TP_IS_ACCOUNT (data) || data == NULL);

  tpl_object_unref_if_not_null (self->account);
  self->account = data;
  tpl_object_ref_if_not_null (data);
}

void
tpl_contact_set_alias (TplContact * self, const gchar * data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));

  g_free (self->alias);
  self->alias = g_strdup (data);
}

void
tpl_contact_set_identifier (TplContact * self, const gchar * data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));

  g_free (self->identifier);
  self->identifier = g_strdup (data);
}

void
tpl_contact_set_presence_status (TplContact * self, const gchar * data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));

  g_free (self->presence_status);
  self->presence_status = g_strdup (data);
}

void
tpl_contact_set_presence_message (TplContact * self, const gchar * data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));

  g_free (self->presence_message);
  self->presence_message = g_strdup (data);
}


void
tpl_contact_set_contact_type (TplContact * self, TplContactType data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));

  self->contact_type = data;
}

void
tpl_contact_set_avatar_token (TplContact * self, const gchar *data)
{
  g_return_if_fail (TPL_IS_CONTACT (self));

  g_free (self->avatar_token);
  self->avatar_token = g_strdup (data);
}
