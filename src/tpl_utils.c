#include <tpl_utils.h>

void _unref_object_if_not_null(void* data) {
	if (data && G_IS_OBJECT(data)) { 
		g_object_unref(data);
	}
}

void _ref_object_if_not_null(void* data) {
	if (data && G_IS_OBJECT(data)) { 
		g_object_ref(data);
	}
}
