#ifndef __TPL_UTILS_H__
#define __TPL_UTILS_H__

#include <glib-object.h>

void _unref_object_if_not_null(void* data);
void _ref_object_if_not_null(void* data);

#endif // __TPL_UTILS_H__
