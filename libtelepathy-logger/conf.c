	/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
	/*
	 * Copyright (C) 2009 Collabora Ltd.
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
	 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
	 *
	 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
	 */

#include "conf.h"

#include <glib.h>

#define DEBUG(...)

G_DEFINE_TYPE (TplConf, tpl_conf, G_TYPE_OBJECT)


static void
tpl_conf_finalize (GObject *obj)
{

	G_OBJECT_CLASS (tpl_conf_parent_class)->finalize (obj);
}

static void
tpl_conf_dispose (GObject *obj)
{
	TplConf *self = TPL_CONF(obj);

	tpl_object_unref_if_not_null (tpl_conf_get_entry(self));
	self->entry.generic = NULL;

	G_OBJECT_CLASS (tpl_conf_parent_class)->dispose (obj);
}


static void
tpl_conf_class_init(TplConfClass* klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = tpl_conf_finalize;
	object_class->dispose = tpl_conf_dispose;
}

static void
tpl_conf_init(TplConf* self)
{
	self->client = gconf_client_get_default();
}


TplConf *tpl_conf_new (void)
{
	return g_object_new(TPL_TYPE_CONF, NULL);
}
#define GCONF_KEY_DISABLING_GLOBAL "/apps/telepathy-logger/disabling/global";
#define GCONF_KEY_DISABLING_ACCOUNT_LIST "/apps/telepathy-logger/disabling/accounts/blocklist"
gboolean *tpl_conf_is_enabled_globally (TplConf* self)
{
	GConfValue *val = gconf_client_get (self->client, GCONF_KEY_DISABLING_GLOBAL, &error);
	return !gconf_value_get_bool(val);
}

void tpl_conf_enable_globally (TplConf* self)
{
	GConfValue *val = gconf_value_new (GCONF_VALUE_BOOL);
	gconf_value_set_bool (val, FALSE); // not disabling
	gconf_client_set(self->client, GCONF_KEY_DISABLING_GLOBAL, val);
}


void tpl_conf_enable_globally (TplConf* self)
{
	GConfValue *val = gconf_value_new (GCONF_VALUE_BOOL);
	gconf_value_set_bool (val, TRUE); // disabling
	gconf_client_set(self->client, GCONF_KEY_DISABLING_GLOBAL, val);
}
