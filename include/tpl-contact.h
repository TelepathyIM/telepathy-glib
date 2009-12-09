#ifndef __TPL_CONTACT_H__
#define __TPL_CONTACT_H__

#include <glib-object.h>
#include <telepathy-glib/contact.h>

#include <tpl-channel.h>

G_BEGIN_DECLS

#define TPL_TYPE_CONTACT                  (tpl_contact_get_type ())
#define TPL_CONTACT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CONTACT, TplContact))
#define TPL_CONTACT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CONTACT, TplContactClass))
#define TPL_IS_CONTACT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CONTACT))
#define TPL_IS_CONTACT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CONTACT))
#define TPL_CONTACT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CONTACT, TplContactClass))


typedef enum {
	TPL_CONTACT_USER,
	TPL_CONTACT_GROUP
} TplContactType;

typedef struct {
	GObject parent;

	/* Private */
	TpContact	*contact; // maybe NULL
	TplContactType	contact_type;
	const gchar	*alias;
	const gchar	*identifier;
	const gchar	*presence_status;
	const gchar	*presence_message;

	TpAccount	*account;
} TplContact;


typedef struct {
	GObjectClass	parent_class;
} TplContactClass;


GType  tpl_contact_get_type (void);

TplContact *tpl_contact_from_tp_contact(TpContact *contact);
TplContact *tpl_contact_new(void);

#define ADD_GET(x,y)	y tpl_contact_get_##x(TplContact *self)
	ADD_GET(contact, TpContact *);
	ADD_GET(alias, const gchar *);
	ADD_GET(identifier, const gchar *);
	ADD_GET(presence_status, const gchar *);
	ADD_GET(presence_message, const gchar *);
	ADD_GET(contact_type, TplContactType);
	ADD_GET(account, TpAccount *);
#undef ADD_GET

#define ADD_SET(x,y)	void tpl_contact_set_##x(TplContact *self, y data)
	ADD_SET(contact, TpContact *);
	ADD_SET(alias, const gchar *);
	ADD_SET(identifier, const gchar *);
	ADD_SET(presence_status, const gchar *);
	ADD_SET(presence_message, const gchar *);
	ADD_SET(contact_type, TplContactType);
	ADD_SET(account, TpAccount *);
#undef ADD_SET

G_END_DECLS

#endif // __TPL_CONTACT_H__
