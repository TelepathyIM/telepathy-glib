#ifndef __TPL_UTILS_H__
#define __TPL_UTILS_H__

#include <glib-object.h>

#define TPL_GET_PRIV(obj,type) ((type##Priv *) ((type *) obj)->priv)
#define TPL_STR_EMPTY(x) ((x) == NULL || (x)[0] == '\0')

void tpl_object_unref_if_not_null(void* data);
void tpl_object_ref_if_not_null(void* data);

#endif // __TPL_UTILS_H__
