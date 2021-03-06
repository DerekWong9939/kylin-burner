/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/***************************************************************************
 *            burner-layout.c
 *
 *  mer mai 24 15:14:42 2006
 *  Copyright  2006  Rouquier Philippe
 * Copyright (C) 2017,Tianjin KYLIN Information Technology Co., Ltd.
 *  burner-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Burner is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Burner is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "burner-setting.h"
#include "burner-layout.h"

#ifdef BUILD_PREVIEW
#include "burner-preview.h"
#endif

#include "burner-project.h"
#include "burner-uri-container.h"
#include "burner-layout-object.h"

G_DEFINE_TYPE (BurnerLayout, burner_layout, GTK_TYPE_BOX);

enum {
	TEXT_COL,
	ICON_COL,
	ITEM_COL,
	VISIBLE_COL,
	NB_COL
};

typedef struct {
	gchar *id;
	GtkWidget *widget;
	BurnerLayoutType types;

	guint is_active:1;
} BurnerLayoutItem;

typedef enum {
	BURNER_LAYOUT_RIGHT = 0,
	BURNER_LAYOUT_LEFT,
	BURNER_LAYOUT_TOP,
	BURNER_LAYOUT_BOTTOM,
} BurnerLayoutLocation;

struct BurnerLayoutPrivate {
	GtkActionGroup *action_group;

	gint accel;

	GtkUIManager *manager;

	BurnerLayoutLocation layout_type;
	GtkWidget *pane;

	GtkWidget *project;

	GtkWidget *combo;
	GtkWidget *top_box;

	BurnerLayoutType ctx_type;
	BurnerLayoutItem *active_item;

	GtkWidget *notebook;
	GtkWidget *main_box;
	GtkWidget *preview_pane;

	guint pane_size_allocated:1;
};

static GObjectClass *parent_class = NULL;

#define BURNER_LAYOUT_PREVIEW_ID	"Viewer"
#define BURNER_LAYOUT_PREVIEW_MENU	N_("P_review")
/* Translators: this is an image, a picture, not a "Disc Image" */
#define BURNER_LAYOUT_PREVIEW_TOOLTIP	N_("Display video, audio and image preview")
#define BURNER_LAYOUT_PREVIEW_ICON	NULL

#define BURNER_LAYOUT_NONE_ID		"EmptyView"
#define BURNER_LAYOUT_NONE_MENU	N_("_Show Side Panel")
#define BURNER_LAYOUT_NONE_TOOLTIP	N_("Show a side pane along the project")
#define BURNER_LAYOUT_NONE_ICON	NULL

static void
burner_layout_empty_toggled_cb (GtkToggleAction *action,
				 BurnerLayout *layout);

const GtkToggleActionEntry entries [] = {
	{ BURNER_LAYOUT_NONE_ID, BURNER_LAYOUT_NONE_ICON, N_(BURNER_LAYOUT_NONE_MENU),
	  "F7", N_(BURNER_LAYOUT_NONE_TOOLTIP), G_CALLBACK (burner_layout_empty_toggled_cb), 1 }
};

/** see #547687 **/
const GtkRadioActionEntry radio_entries [] = {
	{ "HView", NULL, N_("_Horizontal Layout"),
	  NULL, N_("Set a horizontal layout"), 1 },

	{ "VView", NULL, N_("_Vertical Layout"),
	  NULL, N_("Set a vertical layout"), 0 },
};

const gchar description [] =
	"<ui>"
	"<menubar name='menubar' >"
//		"<menu action='ViewMenu'>"
//			"<placeholder name='ViewPlaceholder'>"
//				"<menuitem action='"BURNER_LAYOUT_NONE_ID"'/>"
//				"<menuitem action='"BURNER_LAYOUT_PREVIEW_ID"'/>"
//				"<separator/>"
//				"<menuitem action='VView'/>"
//				"<menuitem action='HView'/>"
//				"<separator/>"
//			"</placeholder>"
//		"</menu>"
	"</menubar>"
	"</ui>";

/* signals */
typedef enum {
	SIDEPANE_SIGNAL,
	LAST_SIGNAL
} BurnerLayoutSignalType;

static guint burner_layout_signals [LAST_SIGNAL] = { 0 };

static void
burner_layout_pack_preview (BurnerLayout *layout)
{
	if (layout->priv->layout_type == BURNER_LAYOUT_LEFT) {
		gtk_box_pack_end (GTK_BOX (layout->priv->main_box),
				  layout->priv->preview_pane,
				  FALSE,
				  FALSE,
				  0);
	}
	else if (layout->priv->layout_type == BURNER_LAYOUT_TOP) {
		gtk_box_pack_end (GTK_BOX (layout->priv->main_box),
				  layout->priv->preview_pane,
				  FALSE,
				  FALSE,
				  0);
	}
	else if (layout->priv->layout_type == BURNER_LAYOUT_RIGHT) {
		gtk_box_pack_end (GTK_BOX (layout->priv->main_box),
				  layout->priv->preview_pane,
				  FALSE,
				  FALSE,
				  0);
	}
	else if (layout->priv->layout_type == BURNER_LAYOUT_BOTTOM) {
		gtk_box_pack_start (GTK_BOX (layout->priv->project),
				    layout->priv->preview_pane,
				    FALSE,
				    FALSE,
				    0);
	}
}

static void
burner_layout_show (GtkWidget *widget)
{
	BurnerLayout *layout;

	layout = BURNER_LAYOUT (widget);

	gtk_action_group_set_visible (layout->priv->action_group, TRUE);
	gtk_action_group_set_sensitive (layout->priv->action_group, TRUE);
	gtk_widget_set_sensitive (widget, TRUE);

	if (GTK_WIDGET_CLASS (parent_class)->show)
		GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void
burner_layout_hide (GtkWidget *widget)
{
	BurnerLayout *layout;

	layout = BURNER_LAYOUT (widget);

	gtk_action_group_set_visible (layout->priv->action_group, FALSE);
	gtk_action_group_set_sensitive (layout->priv->action_group, FALSE);
	gtk_widget_set_sensitive (widget, FALSE);

	if (GTK_WIDGET_CLASS (parent_class)->hide)
		GTK_WIDGET_CLASS (parent_class)->hide (widget);
}

static BurnerLayoutObject *
burner_layout_item_get_object (BurnerLayoutItem *item)
{
	BurnerLayoutObject *source;
	GList *children;
	GList *child;

	source = NULL;
	if (!item || !GTK_IS_CONTAINER (item->widget))
		return NULL;

	children = gtk_container_get_children (GTK_CONTAINER (item->widget));
	for (child = children; child; child = child->next) {
		if (BURNER_IS_LAYOUT_OBJECT (child->data)) {
			source = child->data;
			break;
		}
	}
	g_list_free (children);

	if (!source || !BURNER_IS_LAYOUT_OBJECT (source)) 
		return NULL;

	return source;
}

static void
burner_layout_size_reallocate (BurnerLayout *layout)
{
	gint pr_header, pr_center, pr_footer;
	gint header, center, footer;
	BurnerLayoutObject *source;
	GtkWidget *alignment;

	alignment = gtk_widget_get_parent (layout->priv->main_box);

	if (layout->priv->layout_type == BURNER_LAYOUT_TOP
	||  layout->priv->layout_type == BURNER_LAYOUT_BOTTOM) {
		gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
					   0.0,	
					   0.0,
					   0.0,
					   0.0);
		return;
	}

	burner_layout_object_get_proportion (BURNER_LAYOUT_OBJECT (layout->priv->project),
					      &pr_header,
					      &pr_center,
					      &pr_footer);

	source = burner_layout_item_get_object (layout->priv->active_item);
	if (!source)
		return;

	header = 0;
	center = 0;
	footer = 0;
	burner_layout_object_get_proportion (BURNER_LAYOUT_OBJECT (source),
					      &header,
					      &center,
					      &footer);

	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
				   0.0,	
				   pr_footer - footer,
				   0.0,
				   0.0);
}

static void
burner_layout_page_showed (GtkWidget *widget,
			    BurnerLayout *layout)
{
	burner_layout_size_reallocate (layout);
}

static void
burner_layout_project_size_allocated_cb (GtkWidget *widget,
					  GtkAllocation *allocation,
					  BurnerLayout *layout)
{
	burner_layout_size_reallocate (layout);
}

void
burner_layout_add_project (BurnerLayout *layout,
			    GtkWidget *project)
{
	GtkWidget *box;

	g_signal_connect (project,
			  "size-allocate",
			  G_CALLBACK (burner_layout_project_size_allocated_cb),
			  layout);

	if (layout->priv->layout_type == BURNER_LAYOUT_TOP
	||  layout->priv->layout_type == BURNER_LAYOUT_LEFT)
		box = gtk_paned_get_child1 (GTK_PANED (layout->priv->pane));
	else
		box = gtk_paned_get_child2 (GTK_PANED (layout->priv->pane));

	gtk_box_pack_end (GTK_BOX (box), project, TRUE, TRUE, 0);
	layout->priv->project = project;
}

#ifdef BUILD_PREVIEW
static void
burner_layout_preview_toggled_cb (GtkToggleAction *action, BurnerLayout *layout)
{
	gboolean active;

	active = gtk_toggle_action_get_active (action);
	if (active)
		gtk_widget_show (layout->priv->preview_pane);
	else
		gtk_widget_hide (layout->priv->preview_pane);

	burner_preview_set_enabled (BURNER_PREVIEW (layout->priv->preview_pane), active);
	burner_setting_set_value (burner_setting_get_default (),
	                           BURNER_SETTING_SHOW_PREVIEW,
	                           GINT_TO_POINTER (active));
}
#endif

void
burner_layout_add_preview (BurnerLayout *layout,
			    GtkWidget *preview)
{
#ifdef BUILD_PREVIEW
	gpointer value;
	gboolean active;
	gchar *accelerator;
	GtkToggleActionEntry entry;

	layout->priv->preview_pane = preview;
	burner_layout_pack_preview (layout);

	burner_setting_get_value (burner_setting_get_default (),
	                           BURNER_SETTING_SHOW_PREVIEW,
	                           &value);
	active = GPOINTER_TO_INT (value);

	/* add menu entry in display */
	accelerator = g_strdup ("F11");

	entry.name = BURNER_LAYOUT_PREVIEW_ID;
	entry.stock_id = BURNER_LAYOUT_PREVIEW_ICON;
	entry.label = BURNER_LAYOUT_PREVIEW_MENU;
	entry.accelerator = accelerator;
	entry.tooltip = BURNER_LAYOUT_PREVIEW_TOOLTIP;
	entry.is_active = active;
	entry.callback = G_CALLBACK (burner_layout_preview_toggled_cb);

	gtk_action_group_add_toggle_actions (layout->priv->action_group,
					     &entry,
					     1,
					     layout);
	g_free (accelerator);

	/* initializes the display */
	if (active)
		gtk_widget_show (layout->priv->preview_pane);
	else
		gtk_widget_hide (layout->priv->preview_pane);

	burner_preview_set_enabled (BURNER_PREVIEW (layout->priv->preview_pane), active);
#endif
}

/**************************** for the source panes *****************************/
static void
burner_layout_set_side_pane_visible (BurnerLayout *layout,
				      gboolean visible)
{
	BurnerLayoutObject *source;
	gboolean preview_in_project;
	GtkWidget *parent;
	GList *children;

	children = gtk_container_get_children (GTK_CONTAINER (layout->priv->main_box));
	preview_in_project = (g_list_find (children, layout->priv->preview_pane) == NULL);
	g_list_free (children);

	parent = gtk_widget_get_parent (layout->priv->main_box);
	source = burner_layout_item_get_object (layout->priv->active_item);

	if (!visible) {
		/* No side pane should be visible */
		if (!preview_in_project) {
			/* we need to unparent the preview widget
			 * and set it under the project */
			g_object_ref (layout->priv->preview_pane);
			gtk_container_remove (GTK_CONTAINER (layout->priv->main_box),
					      layout->priv->preview_pane);

			gtk_box_pack_start (GTK_BOX (layout->priv->project),
					    layout->priv->preview_pane,
					    FALSE,
					    FALSE,
					    0);
			g_object_unref (layout->priv->preview_pane);
		}

		burner_project_set_source (BURNER_PROJECT (layout->priv->project), NULL);
		gtk_widget_hide (parent);

		burner_uri_container_uri_selected (BURNER_URI_CONTAINER (source));
	}
	else {
		/* The side pane should be visible */
		if (preview_in_project) {
			/* we need to unparent the preview widget
			 * and set it back where it was */
			g_object_ref (layout->priv->preview_pane);
			gtk_container_remove (GTK_CONTAINER (layout->priv->project),
					      layout->priv->preview_pane);

			burner_layout_pack_preview (layout);
			g_object_unref (layout->priv->preview_pane);
		}

		/* Now tell the project which source it gets URIs from */
		if (!BURNER_IS_URI_CONTAINER (source)) {
			burner_project_set_source (BURNER_PROJECT (layout->priv->project),
						    NULL);
		}
		else {
			burner_project_set_source (BURNER_PROJECT (layout->priv->project),
						    BURNER_URI_CONTAINER (source));
			burner_uri_container_uri_selected (BURNER_URI_CONTAINER (source));
		}

		gtk_widget_show (parent);
	}

	g_signal_emit (layout,
		       burner_layout_signals [SIDEPANE_SIGNAL],
		       0,
		       visible);
}

static void
burner_layout_item_set_active (BurnerLayout *layout,
				BurnerLayoutItem *item)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (layout->priv->combo));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		BurnerLayoutItem *tree_item;

		gtk_tree_model_get (model, &iter,
				    ITEM_COL, &tree_item,
				    -1);

		if (tree_item == item) {
			BurnerLayoutObject *object;

			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (layout->priv->combo), &iter);
			gtk_widget_show (item->widget);
			layout->priv->active_item = item;

			/* tell the object what context we are in */
			object = burner_layout_item_get_object (item);
			burner_layout_object_set_context (object, layout->priv->ctx_type);
			return;
		}
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
burner_layout_save (BurnerLayout *layout,
		     const gchar *id)
{
	if (layout->priv->ctx_type == BURNER_LAYOUT_AUDIO)
		burner_setting_set_value (burner_setting_get_default (),
		                           BURNER_SETTING_DISPLAY_LAYOUT_AUDIO,
		                           id);
	else if (layout->priv->ctx_type == BURNER_LAYOUT_DATA)
		burner_setting_set_value (burner_setting_get_default (),
		                           BURNER_SETTING_DISPLAY_LAYOUT_DATA,
		                           id);
	else if (layout->priv->ctx_type == BURNER_LAYOUT_VIDEO)
		burner_setting_set_value (burner_setting_get_default (),
		                           BURNER_SETTING_DISPLAY_LAYOUT_VIDEO,
		                           id);
}

void
burner_layout_add_source (BurnerLayout *layout,
			   GtkWidget *source,
			   const gchar *id,
			   const gchar *subtitle,
			   const gchar *icon_name,
			   BurnerLayoutType types)
{
	GtkWidget *pane;
	GtkTreeIter iter;
	GtkTreeModel *model;
	BurnerLayoutItem *item;

	pane = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
	gtk_widget_hide (pane);
	gtk_box_pack_end (GTK_BOX (pane), source, TRUE, TRUE, 0);
	g_signal_connect (pane,
			  "show",
			  G_CALLBACK (burner_layout_page_showed),
			  layout);
	gtk_notebook_append_page (GTK_NOTEBOOK (layout->priv->notebook),
				  pane,
				  NULL);

	/* add it to the items list */
	item = g_new0 (BurnerLayoutItem, 1);
	item->id = g_strdup (id);
	item->widget = pane;
	item->types = types;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (layout->priv->combo));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    ICON_COL,icon_name,
			    TEXT_COL,subtitle,
			    ITEM_COL, item,
			    VISIBLE_COL, TRUE,
			    -1);

	if (gtk_tree_model_iter_n_children (model, NULL) > 1)
		gtk_widget_show (layout->priv->top_box);
}

/**************************** empty view callback ******************************/
static void
burner_layout_combo_changed_cb (GtkComboBox *combo,
				 BurnerLayout *layout)
{
	BurnerLayoutObject *source;
	BurnerLayoutItem *item;
	GtkTreeModel *model;
	gboolean is_visible;
	GtkAction *action;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (combo);
	if (!gtk_combo_box_get_active_iter (combo, &iter))
		return;

	gtk_tree_model_get (model, &iter,
			    ITEM_COL, &item,
			    -1);

	/* Make sure there is a displayed sidepane before setting the source for
	 * project. It can happen that when we're changing of project type this
	 * is called. */
	action = gtk_action_group_get_action (layout->priv->action_group, BURNER_LAYOUT_NONE_ID);
	is_visible = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	if (layout->priv->active_item)
		gtk_widget_hide (layout->priv->active_item->widget);

	layout->priv->active_item = item;
	gtk_widget_show (item->widget);

	/* tell the object what context we are in */
	source = burner_layout_item_get_object (item);
	burner_layout_object_set_context (source, layout->priv->ctx_type);

	if (!BURNER_IS_URI_CONTAINER (source)) {
		burner_project_set_source (BURNER_PROJECT (layout->priv->project), NULL);
	}
	else if (!is_visible) {
		burner_project_set_source (BURNER_PROJECT (layout->priv->project), NULL);
	}
	else {
		burner_project_set_source (BURNER_PROJECT (layout->priv->project),
					    BURNER_URI_CONTAINER (source));
		burner_uri_container_uri_selected (BURNER_URI_CONTAINER (source));
	}

	burner_layout_save (layout, item->id);
}

static void
burner_layout_item_set_visible (BurnerLayout *layout,
				 BurnerLayoutItem *item,
				 gboolean visible)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	guint num = 0;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (layout->priv->combo));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		BurnerLayoutItem *tree_item;

		gtk_tree_model_get (model, &iter,
				    ITEM_COL, &tree_item,
				    -1);

		if (tree_item == item) {
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    VISIBLE_COL, visible,
					    -1);
			break;
		}

	} while (gtk_tree_model_iter_next (model, &iter));

	if (visible)
		gtk_widget_show (item->widget);
	else
		gtk_widget_hide (item->widget);

	gtk_tree_model_get_iter_first (model, &iter);
	do {
		gboolean visible;

		gtk_tree_model_get (model, &iter,
				    VISIBLE_COL, &visible,
				    -1);

		num += visible;
	} while (gtk_tree_model_iter_next (model, &iter));


	if (num > 1)
		gtk_widget_show (layout->priv->top_box);
	else
		gtk_widget_hide (layout->priv->top_box);
}

void
burner_layout_load (BurnerLayout *layout,
		     BurnerLayoutType type)
{
	const gchar *layout_id = NULL;
	GtkTreeModel *model;
	gboolean sidepane;
	GtkAction *action;
	GtkTreeIter iter;
	gpointer value;

#ifdef BUILD_PREVIEW
	if (layout->priv->preview_pane)
		burner_preview_hide (BURNER_PREVIEW (layout->priv->preview_pane));
#endif

	if (type == BURNER_LAYOUT_NONE) {
		gtk_widget_hide (GTK_WIDGET (layout));
		return;
	}

	gtk_widget_show (GTK_WIDGET (layout));

	/* takes care of other panes */
	if (type == BURNER_LAYOUT_AUDIO) {
		burner_setting_get_value (burner_setting_get_default (),
		                           BURNER_SETTING_DISPLAY_LAYOUT_AUDIO,
		                           &value);
		layout_id = value;
	}
	else if (type == BURNER_LAYOUT_DATA) {
		burner_setting_get_value (burner_setting_get_default (),
		                           BURNER_SETTING_DISPLAY_LAYOUT_DATA,
		                           &value);
		layout_id = value;
	}
	else if (type == BURNER_LAYOUT_VIDEO) {
		burner_setting_get_value (burner_setting_get_default (),
		                           BURNER_SETTING_DISPLAY_LAYOUT_VIDEO,
		                           &value);
		layout_id = value;
	}

	/* even if we're not showing a side pane go through all items to make 
	 * sure they have the proper state in case the user wants to activate
	 * side pane again */
	if (layout->priv->active_item) {
		gtk_widget_hide (layout->priv->active_item->widget);
		layout->priv->active_item = NULL;
	}

	layout->priv->ctx_type = type;
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (layout->priv->combo));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			BurnerLayoutItem *item = NULL;

			gtk_tree_model_get (model, &iter,
					    ITEM_COL, &item,
					    -1);

			/* check if that pane should be displayed in such a context */
			if (!(item->types & type)) {
				burner_layout_item_set_visible (layout, item, FALSE);
				continue;
			}

			burner_layout_item_set_visible (layout, item, TRUE);

			if (layout_id && !strcmp (layout_id, item->id))
				burner_layout_item_set_active (layout, item);
			else
				gtk_widget_hide (item->widget);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	/* make sure there is a default for the pane */
	if (!layout->priv->active_item) {
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			BurnerLayoutItem *item = NULL;
			
			gtk_tree_model_get (model, &iter,
					    ITEM_COL, &item,
					    -1);
			
			burner_layout_item_set_active (layout, item);
		}
	}

	/* hide or show side pane */
	burner_setting_get_value (burner_setting_get_default (),
	                           BURNER_SETTING_SHOW_SIDEPANE,
	                           &value);
	sidepane = GPOINTER_TO_INT (value);

	action = gtk_action_group_get_action (layout->priv->action_group, BURNER_LAYOUT_NONE_ID);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), sidepane);
}

static void
burner_layout_pane_moved_cb (GtkWidget *paned,
			      GParamSpec *pspec,
			      BurnerLayout *layout)
{
	GtkAllocation allocation = {0, 0};
	gint position;
	gint percent;

	gtk_widget_get_allocation (GTK_WIDGET (paned), &allocation);
	position = gtk_paned_get_position (GTK_PANED (paned));

	percent = position * 10000;
	if (percent % allocation.width) {
		percent /= allocation.width;
		percent ++;
	}
	else
		percent /= allocation.width;

	burner_setting_set_value (burner_setting_get_default (),
	                           BURNER_SETTING_DISPLAY_PROPORTION,
	                           GINT_TO_POINTER (percent));
}

static void
burner_layout_main_pane_size_allocate (GtkWidget *widget,
					GtkAllocation *allocation,
					BurnerLayout *layout)
{
	if (!layout->priv->pane_size_allocated && gtk_widget_get_visible (widget)) {
		gint position;
		gint value_int;
		gpointer value = NULL;

		burner_setting_get_value (burner_setting_get_default (),
			                   BURNER_SETTING_DISPLAY_PROPORTION,
			                   &value);

		value_int = GPOINTER_TO_INT (value);
		if (value_int >= 0) {
			position = value_int * allocation->width / 10000;
			gtk_paned_set_position (GTK_PANED (layout->priv->pane), position);
		}

		g_signal_connect (layout->priv->pane,
				  "notify::position",
				  G_CALLBACK (burner_layout_pane_moved_cb),
				  layout);

		layout->priv->pane_size_allocated = TRUE;
	}
}

static void
burner_layout_change_type (BurnerLayout *layout,
			    BurnerLayoutLocation layout_type)
{
	GtkWidget *source_pane = NULL;
	GtkWidget *project_pane = NULL;

	if (layout->priv->pane) {
		g_object_ref (layout->priv->preview_pane);
		if (layout->priv->layout_type == BURNER_LAYOUT_BOTTOM)
			gtk_container_remove (GTK_CONTAINER (layout->priv->project), layout->priv->preview_pane);
		else
			gtk_container_remove (GTK_CONTAINER (layout->priv->main_box), layout->priv->preview_pane);

		if (layout->priv->layout_type == BURNER_LAYOUT_TOP
		||  layout->priv->layout_type == BURNER_LAYOUT_LEFT) {
			project_pane = gtk_paned_get_child1 (GTK_PANED (layout->priv->pane));
			source_pane = gtk_paned_get_child2 (GTK_PANED (layout->priv->pane));
		}
		else {
			source_pane = gtk_paned_get_child1 (GTK_PANED (layout->priv->pane));
			project_pane = gtk_paned_get_child2 (GTK_PANED (layout->priv->pane));
		}

		g_object_ref (source_pane);
		gtk_container_remove (GTK_CONTAINER (layout->priv->pane), source_pane);

		g_object_ref (project_pane);
		gtk_container_remove (GTK_CONTAINER (layout->priv->pane), project_pane);

		gtk_widget_destroy (layout->priv->pane);
		layout->priv->pane = NULL;
		layout->priv->pane_size_allocated = FALSE;
	}

	if (layout_type > BURNER_LAYOUT_BOTTOM
	||  layout_type < BURNER_LAYOUT_RIGHT)
		layout_type = BURNER_LAYOUT_RIGHT;

	switch (layout_type) {
		case BURNER_LAYOUT_TOP:
		case BURNER_LAYOUT_BOTTOM:
			layout->priv->pane = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
			break;

		case BURNER_LAYOUT_RIGHT:
		case BURNER_LAYOUT_LEFT:
			layout->priv->pane = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
			break;

		default:
			break;
	}

	gtk_widget_show (layout->priv->pane);
	gtk_box_pack_end (GTK_BOX (layout), layout->priv->pane, TRUE, TRUE, 0);

	/* This function will set its proportion */
	g_signal_connect (layout->priv->pane,
			  "size-allocate",
			  G_CALLBACK (burner_layout_main_pane_size_allocate),
			  layout);

	layout->priv->layout_type = layout_type;

	if (source_pane && project_pane) {
		switch (layout_type) {
			case BURNER_LAYOUT_TOP:
			case BURNER_LAYOUT_LEFT:
				gtk_paned_pack2 (GTK_PANED (layout->priv->pane), source_pane, TRUE, TRUE);
				gtk_paned_pack1 (GTK_PANED (layout->priv->pane), project_pane, TRUE, FALSE);
				break;

			case BURNER_LAYOUT_BOTTOM:
			case BURNER_LAYOUT_RIGHT:
				gtk_paned_pack1 (GTK_PANED (layout->priv->pane), source_pane, TRUE, TRUE);
				gtk_paned_pack2 (GTK_PANED (layout->priv->pane), project_pane, TRUE, FALSE);
				break;

			default:
				break;
		}

		g_object_unref (project_pane);
		g_object_unref (source_pane);
	}

	if (layout->priv->preview_pane) {
		burner_layout_pack_preview (layout);
		g_object_unref (layout->priv->preview_pane);
	}

	burner_layout_size_reallocate (layout);
}

static void
burner_layout_HV_radio_button_toggled_cb (GtkRadioAction *radio,
					   GtkRadioAction *current,
					   BurnerLayout *layout)
{
	guint layout_type;

	if (gtk_radio_action_get_current_value (current))
		layout_type = BURNER_LAYOUT_BOTTOM;
	else
		layout_type = BURNER_LAYOUT_RIGHT;

	burner_layout_change_type (layout, layout_type);
	burner_setting_set_value (burner_setting_get_default (),
	                           BURNER_SETTING_DISPLAY_LAYOUT,
	                           GINT_TO_POINTER (layout_type));
}

static void
burner_layout_close_button_clicked_cb (GtkWidget *button,
					BurnerLayout *layout)
{
	GtkAction *action;

	action = gtk_action_group_get_action (layout->priv->action_group,
					      BURNER_LAYOUT_NONE_ID);
	if (!action)
		return;

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
}

static void
burner_layout_empty_toggled_cb (GtkToggleAction *action,
				 BurnerLayout *layout)
{
	gboolean active;

	active = gtk_toggle_action_get_active (action);
	burner_layout_set_side_pane_visible (layout, active);

	burner_setting_set_value (burner_setting_get_default (),
	                           BURNER_SETTING_SHOW_SIDEPANE,
	                           GINT_TO_POINTER (active));
}

void
burner_layout_register_ui (BurnerLayout *layout,
			    GtkUIManager *manager)
{
	GtkWidget *toolbar;
	GError *error = NULL;

	/* should be called only once */
	gtk_ui_manager_insert_action_group (manager,
					    layout->priv->action_group,
					    0);

	if (!gtk_ui_manager_add_ui_from_string (manager, description, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	/* get the toolbar */
	toolbar = gtk_ui_manager_get_widget (manager, "/Toolbar");
	if (toolbar)
		gtk_box_pack_start (GTK_BOX (layout), toolbar, FALSE, FALSE, 0);

	layout->priv->manager = manager;
}

static gboolean
burner_layout_foreach_item_cb (GtkTreeModel *model,
				GtkTreePath *path,
				GtkTreeIter *iter,
				gpointer NULL_data)
{
	BurnerLayoutItem *item;

	gtk_tree_model_get (model, iter,
			    ITEM_COL, &item,
			    -1);
	g_free (item->id);
	g_free (item);

	return FALSE;
}

static void
burner_layout_combo_destroy_cb (GtkWidget *object,
                                 gpointer NULL_data)
{
	GtkTreeModel *model;

	/* empty tree */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (object));
	if (model)
		model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

	if (model)
		gtk_tree_model_foreach (model,
					burner_layout_foreach_item_cb,
					NULL);
}

static void
burner_layout_finalize (GObject *object)
{
	BurnerLayout *cobj;

	cobj = BURNER_LAYOUT(object);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
burner_layout_class_init (BurnerLayoutClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *gtk_widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = burner_layout_finalize;

	gtk_widget_class->hide = burner_layout_hide;
	gtk_widget_class->show = burner_layout_show;

	burner_layout_signals[SIDEPANE_SIGNAL] =
	    g_signal_new ("sidepane", G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_ACTION | G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__BOOLEAN,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_BOOLEAN);
}

static void
burner_layout_init (BurnerLayout *obj)
{
	GtkCellRenderer *renderer;
	GtkWidget *alignment;
	GtkListStore *store;
	GtkTreeModel *model;
	GtkWidget *button;
	GtkWidget *box;
	gpointer value;

	obj->priv = g_new0 (BurnerLayoutPrivate, 1);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (obj), GTK_ORIENTATION_VERTICAL);

	/* menu */
	obj->priv->action_group = gtk_action_group_new ("BurnerLayoutActions");
	gtk_action_group_set_translation_domain (obj->priv->action_group, 
						 GETTEXT_PACKAGE);
	gtk_action_group_add_toggle_actions (obj->priv->action_group,
					     entries,
					     1,
					     obj);

	/* get our layout */
	burner_setting_get_value (burner_setting_get_default (),
	                           BURNER_SETTING_DISPLAY_LAYOUT,
	                           &value);

	obj->priv->layout_type = GPOINTER_TO_INT (value);

	if (obj->priv->layout_type > BURNER_LAYOUT_BOTTOM
	||  obj->priv->layout_type < BURNER_LAYOUT_RIGHT)
		obj->priv->layout_type = BURNER_LAYOUT_RIGHT;

	switch (obj->priv->layout_type) {
		case BURNER_LAYOUT_TOP:
		case BURNER_LAYOUT_BOTTOM:
			obj->priv->pane = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
			break;

		case BURNER_LAYOUT_RIGHT:
		case BURNER_LAYOUT_LEFT:
			obj->priv->pane = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
			break;

		default:
			break;
	}

	gtk_box_pack_end (GTK_BOX (obj), obj->priv->pane, TRUE, TRUE, 0);
	gtk_widget_show (obj->priv->pane);

	/* This function will set its proportion */
	g_signal_connect (obj->priv->pane,
			  "size-allocate",
			  G_CALLBACK (burner_layout_main_pane_size_allocate),
			  obj);

	/* reflect that layout in the menus */
	gtk_action_group_add_radio_actions (obj->priv->action_group,
					    radio_entries,
					    sizeof (radio_entries) / sizeof (GtkRadioActionEntry),
					    GTK_IS_PANED (obj->priv->pane) && 
					    (gtk_orientable_get_orientation (GTK_ORIENTABLE (obj->priv->pane)) == GTK_ORIENTATION_VERTICAL),
					    G_CALLBACK (burner_layout_HV_radio_button_toggled_cb),
					    obj);

	/* set up pane for project */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (box);

	if (obj->priv->layout_type == BURNER_LAYOUT_TOP
	||  obj->priv->layout_type == BURNER_LAYOUT_LEFT)
		gtk_paned_pack1 (GTK_PANED (obj->priv->pane), box, TRUE, FALSE);
	else
		gtk_paned_pack2 (GTK_PANED (obj->priv->pane), box, TRUE, FALSE);

	/* set up containers */
	alignment = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_widget_show (alignment);

	if (obj->priv->layout_type == BURNER_LAYOUT_TOP
	||  obj->priv->layout_type == BURNER_LAYOUT_LEFT)
		gtk_paned_pack2 (GTK_PANED (obj->priv->pane), alignment, TRUE, TRUE);
	else
		gtk_paned_pack1 (GTK_PANED (obj->priv->pane), alignment, TRUE, TRUE);

	obj->priv->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (alignment), obj->priv->main_box);
	gtk_widget_show (obj->priv->main_box);

	/* close button and combo. Don't show it now. */
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	obj->priv->top_box = box;
	gtk_box_pack_start (GTK_BOX (obj->priv->main_box),
			    box,
			    FALSE,
			    FALSE,
			    3);

	store = gtk_list_store_new (NB_COL,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_POINTER,
				    G_TYPE_BOOLEAN);
	model = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
	gtk_tree_model_filter_set_visible_column (GTK_TREE_MODEL_FILTER (model), VISIBLE_COL);
	g_object_unref (G_OBJECT (store));

	obj->priv->combo = gtk_combo_box_new_with_model (model);
	g_object_set (obj->priv->combo,
		      "has-frame", FALSE,
		      NULL);
	g_signal_connect (obj->priv->combo,
			  "changed",
			  G_CALLBACK (burner_layout_combo_changed_cb),
			  obj);
	g_signal_connect (obj->priv->combo,
			  "destroy",
			  G_CALLBACK (burner_layout_combo_destroy_cb),
			  obj);
	gtk_widget_show (obj->priv->combo);
	g_object_unref (G_OBJECT (model));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (obj->priv->combo), renderer,
				    FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (obj->priv->combo),
				       renderer, "icon-name",
				       ICON_COL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (obj->priv->combo), renderer,
				    FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (obj->priv->combo),
				       renderer, "markup",
				       TEXT_COL);

	gtk_box_pack_start (GTK_BOX (box), obj->priv->combo, TRUE, TRUE, 0);


	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_BUTTON));
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (button, _("Click to close the side pane"));
	gtk_widget_show (button);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (burner_layout_close_button_clicked_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	obj->priv->notebook = gtk_notebook_new ();
	gtk_widget_show (obj->priv->notebook);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (obj->priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (obj->priv->notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (obj->priv->main_box),
			    obj->priv->notebook,
			    TRUE,
			    TRUE,
			    0);
}

GtkWidget *
burner_layout_new ()
{
	BurnerLayout *obj;
	
	obj = BURNER_LAYOUT (g_object_new (BURNER_TYPE_LAYOUT, NULL));
	return GTK_WIDGET (obj);
}
