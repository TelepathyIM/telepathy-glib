/*
 * TpLogger - a TpProxy for the logger
 *
 * Copyright (C) 2014 Collabora Ltd. <http://www.collabora.co.uk/>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __TP_LOGGER_H__
#define __TP_LOGGER_H__

#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

typedef struct _TpLogger TpLogger;
typedef struct _TpLoggerClass TpLoggerClass;
typedef struct _TpLoggerPriv TpLoggerPriv;

struct _TpLoggerClass
{
  /*<private>*/
  TpProxyClass parent_class;
  GCallback _padding[7];
};

struct _TpLogger
{
  /*<private>*/
  TpProxy parent;
  TpLoggerPriv *priv;
};

GType tp_logger_get_type (void);

/* TYPE MACROS */
#define TP_TYPE_LOGGER \
  (tp_logger_get_type ())
#define TP_LOGGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    TP_TYPE_LOGGER, \
    TpLogger))
#define TP_LOGGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    TP_TYPE_LOGGER, \
    TpLoggerClass))
#define TP_IS_LOGGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    TP_TYPE_LOGGER))
#define TP_IS_LOGGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    TP_TYPE_LOGGER))
#define TP_LOGGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    TP_TYPE_LOGGER, \
    TpLoggerClass))

TpLogger * tp_logger_dup (void);

G_END_DECLS

#endif /* #ifndef __TP_LOGGER_H__*/
