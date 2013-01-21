/*
 * Copyright © 2013 Collabora Ltd.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef TPL_LOG_STORE_EMPATHY_H
#define TPL_LOG_STORE_EMPATHY_H

#include "log-store-xml-internal.h"

typedef struct _TplLogStoreEmpathy TplLogStoreEmpathy;
typedef struct _TplLogStoreEmpathyClass TplLogStoreEmpathyClass;

struct _TplLogStoreEmpathyClass {
    /*< private >*/
    TplLogStoreXmlClass parent_class;
};

struct _TplLogStoreEmpathy {
    TplLogStoreXml parent;
};

GType _tpl_log_store_empathy_get_type (void);

/* TYPE MACROS */
#define TPL_TYPE_LOG_STORE_EMPATHY \
  (_tpl_log_store_empathy_get_type ())
#define TPL_LOG_STORE_EMPATHY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TPL_TYPE_LOG_STORE_EMPATHY, TplLogStoreEmpathy))
#define TPL_LOG_STORE_EMPATHY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TPL_TYPE_LOG_STORE_EMPATHY,\
                           TplLogStoreEmpathyClass))
#define TPL_IS_LOG_STORE_EMPATHY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TPL_TYPE_LOG_STORE_EMPATHY))
#define TPL_IS_LOG_STORE_EMPATHY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TPL_TYPE_LOG_STORE_EMPATHY))
#define TPL_LOG_STORE_EMPATHY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_LOG_STORE_EMPATHY, \
                              TplLogStoreEmpathyClass))

#endif /* TPL_LOG_STORE_EMPATHY_H */
