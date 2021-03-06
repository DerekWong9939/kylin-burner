/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libburner-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libburner-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libburner-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libburner-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libburner-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libburner-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <glib/gi18n.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burner-track.h"
#include "burner-plugin.h"
#include "burner-plugin-private.h"
#include "burner-plugin-information.h"
#include "burn-plugin-manager.h"

static BurnerPluginManager *default_manager = NULL;

#define BURNER_PLUGIN_MANAGER_NOT_SUPPORTED_LOG(caps, error)			\
{										\
	BURNER_BURN_LOG ("Unsupported operation");				\
	g_set_error (error,							\
		     BURNER_BURN_ERROR,					\
		     BURNER_BURN_ERROR_GENERAL,				\
		     _("An internal error occurred"),				\
		     G_STRLOC);							\
	return BURNER_BURN_NOT_SUPPORTED;					\
}

typedef struct _BurnerPluginManagerPrivate BurnerPluginManagerPrivate;
struct _BurnerPluginManagerPrivate {
	GSList *plugins;
	GSettings *settings;
};

#define BURNER_PLUGIN_MANAGER_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BURNER_TYPE_PLUGIN_MANAGER, BurnerPluginManagerPrivate))

G_DEFINE_TYPE (BurnerPluginManager, burner_plugin_manager, G_TYPE_OBJECT);

enum
{
	CAPS_CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint caps_signals [LAST_SIGNAL] = { 0 };

#define BURNER_SCHEMA_CONFIG		       "org.gnome.burner.config"
#define BURNER_PROPS_PLUGINS_KEY	       "plugins"

static GObjectClass* parent_class = NULL;

GSList *
burner_plugin_manager_get_plugins_list (BurnerPluginManager *self)
{
	BurnerPluginManagerPrivate *priv;
	GSList *retval = NULL;
	GSList *iter;

	priv = BURNER_PLUGIN_MANAGER_PRIVATE (self);

	for (iter = priv->plugins; iter; iter = iter->next) {
		BurnerPlugin *plugin;

		plugin = iter->data;
		g_object_ref (plugin);
		retval = g_slist_prepend (retval, plugin);
	}

	return retval;
}

static void
burner_plugin_manager_plugin_state_changed (BurnerPlugin *plugin,
					     gboolean active,
					     BurnerPluginManager *self)
{
	BurnerPluginManagerPrivate *priv;
	GPtrArray *array;
	GSList *iter;

	priv = BURNER_PLUGIN_MANAGER_PRIVATE (self);

	/* build a list of all active plugins */
	array = g_ptr_array_new ();
	for (iter = priv->plugins; iter; iter = iter->next) {
		BurnerPlugin *plugin;
		const gchar *name;

		plugin = iter->data;

		if (burner_plugin_get_gtype (plugin) == G_TYPE_NONE)
			continue;

		if (!burner_plugin_get_active (plugin, 0))
			continue;

		if (burner_plugin_can_burn (plugin) == BURNER_BURN_OK
		||  burner_plugin_can_convert (plugin) == BURNER_BURN_OK
		||  burner_plugin_can_image (plugin) == BURNER_BURN_OK)
			continue;

		name = burner_plugin_get_name (plugin);
		if (name)
			g_ptr_array_add (array, (gchar *) name);
	}

	if (array->len) {
		g_ptr_array_add (array, NULL);
		g_settings_set_strv (priv->settings,
		                     BURNER_PROPS_PLUGINS_KEY,
		                     (const gchar * const *) array->pdata);
	}
	else {
		gchar *none = "none";

		g_ptr_array_add (array, none);
		g_ptr_array_add (array, NULL);
		g_settings_set_strv (priv->settings,
		                     BURNER_PROPS_PLUGINS_KEY,
		                     (const gchar * const *) array->pdata);
	}
	g_ptr_array_free (array, TRUE);

	/* tell the rest of the world */
	g_signal_emit (self,
		       caps_signals [CAPS_CHANGED_SIGNAL],
		       0);
}

static void
burner_plugin_manager_set_plugins_state (BurnerPluginManager *self)
{
	GSList *iter;
	int name_num;
	gchar **names = NULL;
	BurnerPluginManagerPrivate *priv;

	priv = BURNER_PLUGIN_MANAGER_PRIVATE (self);

	/* get the list of user requested plugins. while at it we add a watch
	 * on the key so as to be warned whenever the user changes prefs. */
	BURNER_BURN_LOG ("Getting list of plugins to be loaded");
	names = g_settings_get_strv (priv->settings, BURNER_PROPS_PLUGINS_KEY);
	name_num = g_strv_length (names);

	for (iter = priv->plugins; iter; iter = iter->next) {
		gboolean active;
		BurnerPlugin *plugin;

		plugin = iter->data;

		if (burner_plugin_get_compulsory (plugin)) {
			g_signal_handlers_block_matched (plugin,
							 G_SIGNAL_MATCH_FUNC,
							 0,
							 0,
							 0,
							 burner_plugin_manager_plugin_state_changed,
							 NULL);
			burner_plugin_set_active (plugin, TRUE);
			g_signal_handlers_unblock_matched (plugin,
							   G_SIGNAL_MATCH_FUNC,
							   0,
							   0,
							   0,
							   burner_plugin_manager_plugin_state_changed,
							   NULL);
			BURNER_BURN_LOG ("Plugin set to active. %s is %s",
					  burner_plugin_get_name (plugin),
					  burner_plugin_get_active (plugin, 0)? "active":"inactive");
			continue;
		}

		/* See if this plugin is in the names list. If not, de-activate it. */
		if (name_num) {
			int i;

			active = FALSE;
			for (i = 0; i < name_num; i++) {
				/* This allows one to be able to support the old way
				 * burner had to save which plugins should be
				 * used */
				if (!g_strcmp0 (burner_plugin_get_name (plugin), names [i])
				||  !g_strcmp0 (burner_plugin_get_display_name (plugin), names [i])) {
					active = TRUE;
					break;
				}
			}
		}
		else
			active = TRUE;

		/* we don't want to receive a signal from this plugin if its 
		 * active state changes */
		g_signal_handlers_block_matched (plugin,
						 G_SIGNAL_MATCH_FUNC,
						 0,
						 0,
						 0,
						 burner_plugin_manager_plugin_state_changed,
						 NULL);
		burner_plugin_set_active (plugin, active);
		g_signal_handlers_unblock_matched (plugin,
						   G_SIGNAL_MATCH_FUNC,
						   0,
						   0,
						   0,
						   burner_plugin_manager_plugin_state_changed,
						   NULL);

		BURNER_BURN_LOG ("Setting plugin %s %s",
				  burner_plugin_get_name (plugin),
				  burner_plugin_get_active (plugin, 0)? "active":"inactive");
	}
	g_strfreev (names);
}

static void
burner_plugin_manager_plugin_list_changed_cb (GSettings *settings,
                                               const gchar *key,
                                               gpointer user_data)
{
	burner_plugin_manager_set_plugins_state (BURNER_PLUGIN_MANAGER (user_data));
}

#if 0

/**
 * This function is only for debugging purpose. It allows one to load plugins in a
 * particular order which is useful since sometimes it triggers some new bugs.
 */

static void
burner_plugin_manager_init (BurnerPluginManager *self)
{
	guint i = 0;
	const gchar *name [] = {
				"libburner-transcode.so",
				"libburner-checksum.so",
				"libburner-dvdcss.so",
				"libburner-checksum-file.so",
				"libburner-local-track.so",
				"libburner-toc2cue.so",
				"libburner-wodim.so",
				"libburner-readom.so",
				"libburner-dvdrwformat.so",
				"libburner-genisoimage.so",
				"libburner-mkisofs.so",
				//"libburner-normalize.so",
				"libburner-cdrdao.so",
				//"libburner-readcd.so",
				//"libburner-cdrecord.so",
				"libburner-growisofs.so",
				//"libburner-libburn.so",
				//"libburner-libisofs.so",
				//"libburner-vcdimager.so",
				//"libburner-dvdauthor.so",
				//"libburner-vob.so"
				NULL};
	BurnerPluginManagerPrivate *priv;

	priv = BURNER_PLUGIN_MANAGER_PRIVATE (self);

	/* open the plugin directory */
	BURNER_BURN_LOG ("opening plugin directory %s", BURNER_PLUGIN_DIRECTORY);

	/* load all plugins from directory */
	for (i = 0; name [i] != NULL; i++) {
		BurnerPluginRegisterType function;
		BurnerPlugin *plugin;
		GModule *handle;
		gchar *path;

		/* the name must end with *.so */
		if (!g_str_has_suffix (name [i], G_MODULE_SUFFIX))
			continue;

		path = g_module_build_path (BURNER_PLUGIN_DIRECTORY, name [i]);
		BURNER_BURN_LOG ("loading %s", path);

		handle = g_module_open (path, 0);
		if (!handle) {
			g_free (path);
			BURNER_BURN_LOG ("Module can't be loaded: g_module_open failed (%s)",
					  g_module_error ());
			continue;
		}

		if (!g_module_symbol (handle, "burner_plugin_register", (gpointer) &function)) {
			g_free (path);
			g_module_close (handle);
			BURNER_BURN_LOG ("Module can't be loaded: no register function");
			continue;
		}

		/* now we can create the plugin */
		plugin = burner_plugin_new (path);
		g_module_close (handle);
		g_free (path);

		if (!plugin) {
			BURNER_BURN_LOG ("Load failure");
			continue;
		}

		if (burner_plugin_get_gtype (plugin) == G_TYPE_NONE) {
			BURNER_BURN_LOG ("Load failure, no GType was returned %s",
					  burner_plugin_get_error (plugin));
		}
		else
			g_signal_connect (plugin,
					  "activated",
					  G_CALLBACK (burner_plugin_manager_plugin_state_changed),
					  self);

		priv->plugins = g_slist_prepend (priv->plugins, plugin);
	}

	burner_plugin_manager_set_plugins_state (self);
}

#endif

static void
burner_plugin_manager_init (BurnerPluginManager *self)
{
	GDir *directory;
	const gchar *name;
	GError *error = NULL;
	BurnerPluginManagerPrivate *priv;

	priv = BURNER_PLUGIN_MANAGER_PRIVATE (self);

	priv->settings = g_settings_new (BURNER_SCHEMA_CONFIG);
	g_signal_connect (priv->settings,
	                  "changed",
	                  G_CALLBACK (burner_plugin_manager_plugin_list_changed_cb),
	                  self);

	/* open the plugin directory */
	BURNER_BURN_LOG ("opening plugin directory %s", BURNER_PLUGIN_DIRECTORY);
	directory = g_dir_open (BURNER_PLUGIN_DIRECTORY, 0, &error);
	if (!directory) {
		if (error) {
			BURNER_BURN_LOG ("Error opening plugin directory %s", error->message);
			g_error_free (error);
			return;
		}
	}

	/* load all plugins from directory */
	while ((name = g_dir_read_name (directory))) {
		BurnerPluginRegisterType function;
		BurnerPlugin *plugin;
		GModule *handle;
		gchar *path;

		/* the name must end with *.so */
		if (!g_str_has_suffix (name, G_MODULE_SUFFIX))
			continue;

		path = g_module_build_path (BURNER_PLUGIN_DIRECTORY, name);
		BURNER_BURN_LOG ("loading %s", path);

		handle = g_module_open (path, 0);
		if (!handle) {
			g_free (path);
			BURNER_BURN_LOG ("Module can't be loaded: g_module_open failed (%s)",
					  g_module_error ());
			continue;
		}

		if (!g_module_symbol (handle, "burner_plugin_register", (gpointer) &function)) {
			g_free (path);
			g_module_close (handle);
			BURNER_BURN_LOG ("Module can't be loaded: no register function");
			continue;
		}

		/* now we can create the plugin */
		plugin = burner_plugin_new (path);
		g_module_close (handle);
		g_free (path);

		if (!plugin) {
			BURNER_BURN_LOG ("Load failure");
			continue;
		}

		if (burner_plugin_get_gtype (plugin) == G_TYPE_NONE) {
			gchar *error_string;

			error_string = burner_plugin_get_error_string (plugin);
			BURNER_BURN_LOG ("Load failure, no GType was returned %s", error_string);
			g_free (error_string);
		}

		g_signal_connect (plugin,
		                  "activated",
		                  G_CALLBACK (burner_plugin_manager_plugin_state_changed),
		                  self);

		g_assert (burner_plugin_get_name (plugin));
		priv->plugins = g_slist_prepend (priv->plugins, plugin);
	}
	g_dir_close (directory);

	burner_plugin_manager_set_plugins_state (self);
}

static void
burner_plugin_manager_finalize (GObject *object)
{
	BurnerPluginManagerPrivate *priv;

	priv = BURNER_PLUGIN_MANAGER_PRIVATE (object);

	if (priv->settings) {
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	if (priv->plugins) {
		g_slist_free (priv->plugins);
		priv->plugins = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
	default_manager = NULL;
}

static void
burner_plugin_manager_class_init (BurnerPluginManagerClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BurnerPluginManagerPrivate));

	object_class->finalize = burner_plugin_manager_finalize;

	caps_signals [CAPS_CHANGED_SIGNAL] =
		g_signal_new ("caps_changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
}

BurnerPluginManager *
burner_plugin_manager_get_default (void)
{
	if (!default_manager)
		default_manager = BURNER_PLUGIN_MANAGER (g_object_new (BURNER_TYPE_PLUGIN_MANAGER, NULL));

	return default_manager;
}
