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

#ifndef __TPL_UTILS_H__
#define __TPL_UTILS_H__

#include <glib-object.h>
#include <gio/gio.h>

#define TPL_GET_PRIV(obj,type) ((type##Priv *) ((type *) obj)->priv)
#define TPL_STR_EMPTY(x) ((x) == NULL || (x)[0] == '\0')
#define tpl_call_with_err_if_fail(guard, obj, PREFIX, POSTFIX, msg, func, user_data) \
  if (!(guard)) \
    { \
      if (func != NULL) \
        { \
          GSimpleAsyncResult *result=NULL; \
          g_simple_async_result_set_error (result, PREFIX ## _ERROR, \
              PREFIX ## _ERROR_ ## POSTFIX, \
              msg); \
          return func (G_OBJECT (obj), G_ASYNC_RESULT (result), user_data); \
        } \
      return; \
    }

typedef struct {
    GQueue *chain;
    GSimpleAsyncResult *simple;
} TplActionChain;

void tpl_object_unref_if_not_null (void *data);
void tpl_object_ref_if_not_null (void *data);

gboolean tpl_strequal (const gchar *left, const gchar *right);

TplActionChain *tpl_actionchain_new (GObject *obj, GAsyncReadyCallback cb,
    gpointer user_data);
void tpl_actionchain_free (TplActionChain *self);
typedef void (*TplPendingAction) (TplActionChain *ctx);
void tpl_actionchain_append (TplActionChain *self, TplPendingAction func);
void tpl_actionchain_prepend (TplActionChain *self, TplPendingAction func);
void tpl_actionchain_continue (TplActionChain *self);
void tpl_actionchain_terminate (TplActionChain *self);
gpointer tpl_actionchain_get_object (TplActionChain *self);
gboolean tpl_actionchain_finish (GAsyncResult *result);

#endif // __TPL_UTILS_H__
