#include <tpl_log_entry.h>
#include <tpl_utils.h>

G_DEFINE_TYPE (TplLogEntry, tpl_log_entry, G_TYPE_OBJECT)

static void tpl_log_entry_class_init(TplLogEntryClass* klass) {
	//GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
}

static void tpl_log_entry_init(TplLogEntry* self) {
	/* Init TplTextChannel's members to zero/NULL */
#define TPL_SET_NULL(x) tpl_log_entry_set_##x(self, NULL)
#undef TPL_SET_NULL
}

TplLogEntry *tpl_log_entry_new(void) {
	TplLogEntry *ret = g_object_new(TPL_TYPE_LOG_ENTRY, NULL);
	return ret;
}
