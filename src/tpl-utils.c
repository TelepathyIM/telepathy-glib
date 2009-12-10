#include <tpl-utils.h>

void tpl_object_unref_if_not_null(void* data) {
	if (data && G_IS_OBJECT(data)) { 
		g_object_unref(data);
	}
}

void tpl_object_ref_if_not_null(void* data) {
	if (data && G_IS_OBJECT(data)) { 
		g_object_ref(data);
	}
}
