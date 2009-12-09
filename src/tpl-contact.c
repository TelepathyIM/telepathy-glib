#include <tpl-contact.h>
#include <tpl-utils.h>

G_DEFINE_TYPE (TplContact, tpl_contact, G_TYPE_OBJECT)

static void tpl_contact_class_init(TplContactClass* klass) {
	//GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
}

static void tpl_contact_init(TplContact* self) {
}

/* retrieved contact and set TplContact ready */

TplContact *tpl_contact_from_tp_contact(TpContact *contact)
{
	TplContact *ret;
	const gchar *id, *alias;
	const gchar *pres_msg, *pres_status;

	ret = tpl_contact_new();
	id = tp_contact_get_identifier(contact);
	alias = tp_contact_get_alias(contact);
	pres_status = tp_contact_get_presence_status(contact);
	pres_msg = tp_contact_get_presence_message (contact);

#define CONTACT_ENTRY_SET(x,y) tpl_contact_set_##x(ret,y)
	CONTACT_ENTRY_SET(contact, contact);
	CONTACT_ENTRY_SET(alias, alias);
	CONTACT_ENTRY_SET(identifier, id);
	CONTACT_ENTRY_SET(presence_status, pres_status);
	CONTACT_ENTRY_SET(presence_message, pres_msg );
#undef CONTACT_ENTRY_SET

	return ret;
}

TplContact *tpl_contact_new() {
	return g_object_new(TPL_TYPE_CONTACT, NULL);
}

#define ADD_GET(x,y)	y tpl_contact_get_##x(TplContact *self) { \
		return self->x; }
	ADD_GET(contact, TpContact *);
	ADD_GET(alias, const gchar *);
	ADD_GET(identifier, const gchar *);
	ADD_GET(presence_status, const gchar *);
	ADD_GET(presence_message, const gchar *);
	ADD_GET(contact_type, TplContactType);
	ADD_GET(account, TpAccount *);
#undef ADD_GET

#define ADD_SET_PTR(member,y)	void tpl_contact_set_##member(TplContact *self, y data) { \
		_unref_object_if_not_null(&(self->member)) ; \
		self->member = data; \
		_ref_object_if_not_null(data); }
	ADD_SET_PTR(contact, TpContact *);
	ADD_SET_PTR(account, TpAccount *);
#undef ADD_SET_PTR
#define ADD_SET_STR(member, Type)	\
		void tpl_contact_set_##member(TplContact *self, Type data) \
		{ g_free( (gchar*) self->member); self->member = g_strdup (data); }
	ADD_SET_STR(alias, const gchar *);
	ADD_SET_STR(identifier, const gchar *);
	ADD_SET_STR(presence_status, const gchar *);
	ADD_SET_STR(presence_message, const gchar *);
#undef ADD_SET_STR

void tpl_contact_set_contact_type(TplContact *self, TplContactType data)
{
		self->contact_type = data;
}
