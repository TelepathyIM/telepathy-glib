/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Jonny Lamb <jonny.lamb@collabora.co.uk>
 */

#ifndef __EMPATHY_LOG_STORE_EMPATHY_H__
#define __EMPATHY_LOG_STORE_EMPATHY_H__

#include <glib.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_LOG_STORE_EMPATHY \
  (empathy_log_store_empathy_get_type ())
#define EMPATHY_LOG_STORE_EMPATHY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EMPATHY_TYPE_LOG_STORE_EMPATHY, \
                               EmpathyLogStoreEmpathy))
#define EMPATHY_LOG_STORE_EMPATHY_CLASS(vtable) \
  (G_TYPE_CHECK_CLASS_CAST ((vtable), EMPATHY_TYPE_LOG_STORE_EMPATHY, \
                            EmpathyLogStoreEmpathyClass))
#define EMPATHY_IS_LOG_STORE_EMPATHY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_LOG_STORE_EMPATHY))
#define EMPATHY_IS_LOG_STORE_EMPATHY_CLASS(vtable) \
  (G_TYPE_CHECK_CLASS_TYPE ((vtable), EMPATHY_TYPE_LOG_STORE_EMPATHY))
#define EMPATHY_LOG_STORE_EMPATHY_GET_CLASS(inst) \
  (G_TYPE_INSTANCE_GET_CLASS ((inst), EMPATHY_TYPE_LOG_STORE_EMPATHY, \
                              EmpathyLogStoreEmpathyClass))

typedef struct _EmpathyLogStoreEmpathy EmpathyLogStoreEmpathy;
typedef struct _EmpathyLogStoreEmpathyClass EmpathyLogStoreEmpathyClass;

struct _EmpathyLogStoreEmpathy
{
  GObject parent;
  gpointer priv;
};

struct _EmpathyLogStoreEmpathyClass
{
  GObjectClass parent;
};

GType empathy_log_store_empathy_get_type (void);

G_END_DECLS

#endif /* __EMPATHY_LOG_STORE_EMPATHY_H__ */
