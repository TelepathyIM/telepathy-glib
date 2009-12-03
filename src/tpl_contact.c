#include <tpl_contact.h>
#include <tpl_utils.h>

G_DEFINE_TYPE (TplContact, tpl_contact, G_TYPE_OBJECT)

static void tpl_contact_class_init(TplContactClass* klass) {
	//GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
}

static void tpl_contact_init(TplContact* self) {
}

/* retrieved contat and set TplContact ready */

TplContact *tpl_contact_new() {
	return g_object_new(TPL_TYPE_CONTACT,NULL);
}

#define ADD_GET(x,y)	y tpl_contact_get_##x(TplContact *self) { \
		return self->x; }
	ADD_GET(contact, TpContact *);
	ADD_GET(alias, const gchar *);
	ADD_GET(identifier, const gchar *);
	ADD_GET(presence_status, const gchar *);
	ADD_GET(presence_message, const gchar *);
#undef ADD_GET

#define ADD_SET(member,y)	void tpl_contact_set_##member(TplContact *self, y data) { \
		_unref_object_if_not_null(&(self->member)) ; \
		self->member = data; \
		_ref_object_if_not_null(data); }
	ADD_SET(contact, TpContact *);
#undef ADD_SET
#define ADD_SET_SIMPLE(member,y)	void tpl_contact_set_##member(TplContact *self, y data) { \
		self->member = data;}
	ADD_SET_SIMPLE(alias, const gchar *);
	ADD_SET_SIMPLE(identifier, const gchar *);
	ADD_SET_SIMPLE(presence_status, const gchar *);
	ADD_SET_SIMPLE(presence_message, const gchar *);
#undef ADD_SET_SIMPLE
