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

#ifndef __TPL_ACTION_CHAIN_H__
#define __TPL_ACTION_CHAIN_H__

#include <glib-object.h>
#include <gio/gio.h>

typedef struct {
    GQueue *chain;
    GSimpleAsyncResult *simple;
} TplActionChain;

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

#endif // __TPL_ACTION_CHAIN_H__
