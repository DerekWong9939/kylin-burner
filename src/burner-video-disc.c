/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Burner
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 * Copyright (C) 2017,Tianjin KYLIN Information Technology Co., Ltd.
 * 
 *  Burner is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * burner is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with burner.  If not, write to:
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

#include <gdk/gdkkeysyms.h>

#include <gtk/gtk.h>

#include "eggtreemultidnd.h"

#include "burner-tags.h"
#include "burner-track-stream-cfg.h"

#include "burner-misc.h"
#include "burner-app.h"
#include "burner-disc.h"
#include "burner-io.h"
#include "burner-utils.h"
#include "burner-video-disc.h"
#include "burner-video-tree-model.h"
#include "burner-multi-song-props.h"
#include "burner-song-properties.h"
#include "burner-session-cfg.h"
#include "burner-track-stream.h"
#include "burner-video-options.h"
#include "burner-customize-title.h"

typedef struct _BurnerVideoDiscPrivate BurnerVideoDiscPrivate;
struct _BurnerVideoDiscPrivate
{
	GtkWidget *tree;

	GtkWidget *message;
	GtkUIManager *manager;
	GtkActionGroup *disc_group;

	BurnerIOJobBase *load_dir;

	guint reject_files:1;
	guint editing:1;
	guint loading:1;
};

#define BURNER_VIDEO_DISC_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BURNER_TYPE_VIDEO_DISC, BurnerVideoDiscPrivate))

static void
burner_video_disc_iface_disc_init (BurnerDiscIface *iface);

G_DEFINE_TYPE_WITH_CODE (BurnerVideoDisc,
			 burner_video_disc,
			 GTK_TYPE_BOX,
			 G_IMPLEMENT_INTERFACE (BURNER_TYPE_DISC,
					        burner_video_disc_iface_disc_init));

enum {
	PROP_NONE,
	PROP_REJECT_FILE,
};

static void
burner_video_disc_edit_information_cb (GtkAction *action,
					BurnerVideoDisc *disc);
static void
burner_video_disc_open_activated_cb (GtkAction *action,
				      BurnerVideoDisc *disc);
static void
burner_video_disc_delete_activated_cb (GtkAction *action,
					BurnerVideoDisc *disc);
static void
burner_video_disc_paste_activated_cb (GtkAction *action,
				       BurnerVideoDisc *disc);

static GtkActionEntry entries[] = {
	{"ContextualMenu", NULL, N_("Menu")},
	{"OpenVideo", GTK_STOCK_OPEN, NULL, NULL, N_("Open the selected video"),
	 G_CALLBACK (burner_video_disc_open_activated_cb)},
	{"EditVideo", GTK_STOCK_PROPERTIES, N_("_Edit Information…"), NULL, N_("Edit the video information (start, end, author, etc.)"),
	 G_CALLBACK (burner_video_disc_edit_information_cb)},
	{"DeleteVideo", GTK_STOCK_REMOVE, NULL, NULL, N_("Remove the selected videos from the project"),
	 G_CALLBACK (burner_video_disc_delete_activated_cb)},
	{"PasteVideo", NULL, N_("Paste files"), NULL, N_("Add the files stored in the clipboard"),
	 G_CALLBACK (burner_video_disc_paste_activated_cb)},
/*	{"Split", "transform-crop-and-resize", N_("_Split Track…"), NULL, N_("Split the selected track"),
	 G_CALLBACK (burner_video_disc_split_cb)} */
};

static const gchar *description = {
	"<ui>"
	"<menubar name='menubar' >"
		"<menu action='EditMenu'>"
/*		"<placeholder name='EditPlaceholder'>"
			"<menuitem action='Split'/>"
		"</placeholder>"
*/		"</menu>"
	"</menubar>"
	"<popup action='ContextMenu'>"
		"<menuitem action='OpenVideo'/>"
		"<menuitem action='DeleteVideo'/>"
		"<separator/>"
		"<menuitem action='PasteVideo'/>"
/*		"<separator/>"
		"<menuitem action='Split'/>"
*/		"<separator/>"
		"<menuitem action='EditVideo'/>"
	"</popup>"
/*	"<toolbar name='Toolbar'>"
		"<placeholder name='DiscButtonPlaceholder'>"
			"<separator/>"
			"<toolitem action='Split'/>"
		"</placeholder>"
	"</toolbar>"
*/	"</ui>"
};

enum {
	TREE_MODEL_ROW		= 150,
	TARGET_URIS_LIST,
};

static GtkTargetEntry ntables_cd [] = {
	{BURNER_DND_TARGET_SELF_FILE_NODES, GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
	{"text/uri-list", 0, TARGET_URIS_LIST}
};
static guint nb_targets_cd = sizeof (ntables_cd) / sizeof (ntables_cd[0]);

static GtkTargetEntry ntables_source [] = {
	{BURNER_DND_TARGET_SELF_FILE_NODES, GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
};

static guint nb_targets_source = sizeof (ntables_source) / sizeof (ntables_source[0]);


/**
 * Row name edition
 */

static void
burner_video_disc_name_editing_started_cb (GtkCellRenderer *renderer,
					    GtkCellEditable *editable,
					    gchar *path,
					    BurnerVideoDisc *disc)
{
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (disc);
	priv->editing = 1;
}

static void
burner_video_disc_name_editing_canceled_cb (GtkCellRenderer *renderer,
					     BurnerVideoDisc *disc)
{
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (disc);
	priv->editing = 0;
}

static void
burner_video_disc_name_edited_cb (GtkCellRendererText *cellrenderertext,
				   gchar *path_string,
				   gchar *text,
				   BurnerVideoDisc *self)
{
	BurnerVideoDiscPrivate *priv;
	BurnerTrack *track;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter row;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);

	priv->editing = 0;

	path = gtk_tree_path_new_from_string (path_string);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));

	/* see if this is still a valid path. It can happen a user removes it
	 * while the name of the row is being edited */
	if (!gtk_tree_model_get_iter (model, &row, path)) {
		gtk_tree_path_free (path);
		return;
	}

	track = burner_video_tree_model_path_to_track (BURNER_VIDEO_TREE_MODEL (model), path);
	gtk_tree_path_free (path);

	burner_track_tag_add_string (track, BURNER_TRACK_STREAM_TITLE_TAG, text);

	/* Advertize change to update view */
	burner_track_changed (track);
}

static BurnerDiscResult
burner_video_disc_add_uri_real (BurnerVideoDisc *self,
				 const gchar *uri,
				 gint pos,
				 gint64 start,
				 gint64 end,
				 GtkTreePath **path_return)
{
	GtkTreeModel *model;
	BurnerSessionCfg*session;
	BurnerTrack *sibling = NULL;
	BurnerTrackStreamCfg *track;
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);
	if (priv->reject_files)
		return BURNER_DISC_NOT_READY;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));

	/* create track */
	track = burner_track_stream_cfg_new ();
	burner_track_stream_set_source (BURNER_TRACK_STREAM (track), uri);
	burner_track_stream_set_boundaries (BURNER_TRACK_STREAM (track), start, end, 0);

	/* insert it in the session */
	session = burner_video_tree_model_get_session (BURNER_VIDEO_TREE_MODEL (model));
	if (pos > 0) {
		GSList *tracks;

		tracks = burner_burn_session_get_tracks (BURNER_BURN_SESSION (session));
		sibling = g_slist_nth_data (tracks, pos - 1);
	}
	burner_burn_session_add_track (BURNER_BURN_SESSION (session), BURNER_TRACK (track), sibling);

	if (path_return)
		*path_return = burner_video_tree_model_track_to_path (BURNER_VIDEO_TREE_MODEL (model),
								       BURNER_TRACK (track));

	return BURNER_DISC_OK;
}

static BurnerDiscResult
burner_video_disc_add_uri (BurnerDisc *self,
			    const gchar *uri)
{
	BurnerVideoDiscPrivate *priv;
	GtkTreePath *treepath = NULL;
	BurnerDiscResult result;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);
	result = burner_video_disc_add_uri_real (BURNER_VIDEO_DISC (self),
						  uri,
						  -1,
						  -1,
						  -1,
						  &treepath);

	if (treepath) {
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->tree),
					      treepath,
					      NULL,
					      TRUE,
					      0.5,
					      0.5);
		gtk_tree_path_free (treepath);
	}

	return result;
}

static void
burner_video_disc_add_directory_contents_result (GObject *obj,
						  GError *error,
						  const gchar *uri,
						  GFileInfo *info,
						  gpointer user_data)
{
	gint index;

	/* Check the return status for this file */
	if (error)
		return;

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR
	|| !g_file_info_get_attribute_boolean (info, BURNER_IO_HAS_VIDEO))
		return;

	index = GPOINTER_TO_INT (user_data);

	/* Add a video file and set all information */
	burner_video_disc_add_uri_real (BURNER_VIDEO_DISC (obj),
					 uri,
					 index,
					 -1,
					 -1,
					 NULL);
}

static void
burner_video_disc_add_directory_contents (BurnerVideoDisc *self,
					   const gchar *uri,
					   BurnerTrack *sibling)
{
	BurnerVideoDiscPrivate *priv;
	BurnerSessionCfg *session;
	GtkTreeModel *model;
	GSList *tracks;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);

	if (!priv->load_dir)
		priv->load_dir = burner_io_register (G_OBJECT (self),
						      burner_video_disc_add_directory_contents_result,
						      NULL,
						      NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	session = burner_video_tree_model_get_session (BURNER_VIDEO_TREE_MODEL (model));
	tracks = burner_burn_session_get_tracks (BURNER_BURN_SESSION (session));

	burner_io_load_directory (uri,
				   priv->load_dir,
				   BURNER_IO_INFO_MIME|
				   BURNER_IO_INFO_PERM|
				   BURNER_IO_INFO_METADATA|
				   BURNER_IO_INFO_METADATA_MISSING_CODEC|
				   BURNER_IO_INFO_RECURSIVE|
				   BURNER_IO_INFO_METADATA_THUMBNAIL,
				   GINT_TO_POINTER (g_slist_index (tracks, sibling)));
}

static gboolean
burner_video_disc_directory_dialog (BurnerVideoDisc *self)
{
	gint answer;
	GtkWidget *dialog;

	dialog = burner_app_dialog (burner_app_get_default (),
				     _("Do you want to search for video files inside the directory?"),
				     GTK_BUTTONS_NONE,
				     GTK_MESSAGE_WARNING);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
	                                          "%s.",
						  _("Directories cannot be added to video or audio discs"));

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("_Search Directory"), GTK_RESPONSE_OK,
				NULL);

	gtk_widget_show_all (dialog);
	burner_dialog_button_image(gtk_dialog_get_action_area(GTK_DIALOG (dialog)));
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer != GTK_RESPONSE_OK)
		return FALSE;

	return TRUE;
}

static void
burner_video_disc_unreadable_uri_dialog (BurnerVideoDisc *self,
					  const gchar *uri,
					  GError *error)
{
	gchar *primary;
	gchar *name;
	GFile *file;

	file = g_file_new_for_uri (uri);
	name = g_file_get_basename (file);
	g_object_unref (file);

	primary = g_strdup_printf (_("\"%s\" could not be opened."), name);
	burner_app_alert (burner_app_get_default (),
			   primary,
			   error->message,
			   GTK_MESSAGE_ERROR);
	g_free (primary);
	g_free (name);
}

static void
burner_video_disc_not_video_dialog (BurnerVideoDisc *self,
				     const gchar *uri)
{
	gchar *primary;
	gchar *name;

	BURNER_GET_BASENAME_FOR_DISPLAY (uri, name);
	primary = g_strdup_printf (_("\"%s\" does not have a suitable type for video projects."), name);
	burner_app_alert (burner_app_get_default (),
			   primary,
			   _("Please only add files with video content"),
			   GTK_MESSAGE_ERROR);
	g_free (primary);
	g_free (name);
}

static void
burner_video_disc_session_changed (BurnerSessionCfg *session,
				    BurnerVideoDisc *self)
{
	GSList *next;
	GSList *tracks;
	BurnerStatus *status;

	if (!gtk_widget_get_window (GTK_WIDGET (self)))
		return;

	/* make sure all tracks have video */
	status = burner_status_new ();
	tracks = burner_burn_session_get_tracks (BURNER_BURN_SESSION (session));
	for (; tracks; tracks = next) {
		BurnerStreamFormat format;
		BurnerTrackStream *track;
		BurnerBurnResult result;

		track = tracks->data;
		next = tracks->next;

		if (!BURNER_IS_TRACK_STREAM (track))
			continue;

		result = burner_track_get_status (BURNER_TRACK (track), status);
		if (result == BURNER_BURN_ERR) {
			GError *error;
			gboolean res;
			gchar *uri;

			uri = burner_track_stream_get_source (track, TRUE);

			/* Remove the track now otherwise on each session change we'll get the
			 * same message over and over again. */
			burner_burn_session_remove_track (BURNER_BURN_SESSION (session),
							   BURNER_TRACK (track));

			error = burner_status_get_error (status);
			if (!error)
				burner_video_disc_unreadable_uri_dialog (self, uri, error);
			else if (error->code == BURNER_BURN_ERROR_FILE_FOLDER) {
				res = burner_video_disc_directory_dialog (self);
				if (res)
					burner_video_disc_add_directory_contents (self, uri, BURNER_TRACK (track));
			}
			else if (error->code == BURNER_BURN_ERROR_FILE_NOT_FOUND) {
				/* It could be a file that was deleted */
				burner_video_disc_unreadable_uri_dialog (self, uri, error);
			}
			else
				burner_video_disc_unreadable_uri_dialog (self, uri, error);

			g_error_free (error);
			g_free (uri);
			continue;
		}

		if (result == BURNER_BURN_NOT_READY || result == BURNER_BURN_RUNNING)
			continue;

		if (result != BURNER_BURN_OK)
			continue;

		format = burner_track_stream_get_format (track);
		if (!BURNER_STREAM_FORMAT_HAS_VIDEO (format)) {
			gchar *uri;

			uri = burner_track_stream_get_source (track, TRUE);
			burner_video_disc_not_video_dialog (self, uri);
			g_free (uri);

			burner_burn_session_remove_track (BURNER_BURN_SESSION (session),
							   BURNER_TRACK (track));
		}
	}
	g_object_unref (status);
}

static void
burner_video_disc_delete_selected (BurnerDisc *self)
{
	BurnerVideoDiscPrivate *priv;
	GtkTreeSelection *selection;
	BurnerSessionCfg *session;
	GtkTreeModel *model;
	GList *selected;
	GList *iter;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));

	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	selected = g_list_reverse (selected);

	session = burner_video_tree_model_get_session (BURNER_VIDEO_TREE_MODEL (model));

	for (iter = selected; iter; iter = iter->next) {
		BurnerTrack *track;
		GtkTreePath *treepath;

		treepath = iter->data;

		track = burner_video_tree_model_path_to_track (BURNER_VIDEO_TREE_MODEL (model), treepath);
		gtk_tree_path_free (treepath);

		if (!track)
			continue;

		burner_burn_session_remove_track (BURNER_BURN_SESSION (session), track);
	}
	g_list_free (selected);
}

static gboolean
burner_video_disc_get_selected_uri (BurnerDisc *self,
				     gchar **uri)
{
	GList *selected;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	if (!selected)
		return FALSE;

	if (uri) {
		BurnerTrack *track;
		GtkTreePath *treepath;

		treepath = selected->data;
		track = burner_video_tree_model_path_to_track (BURNER_VIDEO_TREE_MODEL (model), treepath);
		if (track)
			*uri = burner_track_stream_get_source (BURNER_TRACK_STREAM (track), TRUE);
		else
			*uri = NULL;
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);

	return TRUE;
}

static void
burner_video_disc_selection_changed_cb (GtkTreeSelection *selection,
					 BurnerVideoDisc *self)
{
	burner_disc_selection_changed (BURNER_DISC (self));
}

static gboolean
burner_video_disc_selection_function (GtkTreeSelection *selection,
				       GtkTreeModel *model,
				       GtkTreePath *treepath,
				       gboolean path_currently_selected,
				       gpointer NULL_data)
{
/*	BurnerTrack *track;

	track = burner_video_tree_model_path_to_track (BURNER_VIDEO_TREE_MODEL (model), treepath);

	FIXME: add a tag??
	if (track)
		file->editable = !path_currently_selected;
*/
	return TRUE;
}


/**
 * Callback for menu
 */

static gboolean
burner_video_disc_rename_songs (GtkTreeModel *model,
				 GtkTreeIter *iter,
				 GtkTreePath *treepath,
				 const gchar *old_name,
				 const gchar *new_name)
{
	BurnerTrack *track;

	track = burner_video_tree_model_path_to_track (BURNER_VIDEO_TREE_MODEL (model), treepath);
	if (!track)
		return FALSE;

	burner_track_tag_add_string (track,
				      BURNER_TRACK_STREAM_TITLE_TAG,
				      new_name);

	/* Signal the change to have the view reflect it */
	burner_track_changed (track);
	return TRUE;
}

static void
burner_video_disc_edit_song_properties_list (BurnerVideoDisc *self,
					      GList *list)
{
	GList *item;
	GList *copy;
	GtkWidget *props;
	GtkWidget *toplevel;
	GtkTreeModel *model;
	gchar *artist = NULL;
	gchar *composer = NULL;
	gchar *isrc = NULL;
	GtkResponseType result;
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);

	if (!g_list_length (list))
		return;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	props = burner_multi_song_props_new ();
	burner_multi_song_props_set_show_gap (BURNER_MULTI_SONG_PROPS (props), FALSE);

	gtk_window_set_transient_for (GTK_WINDOW (props),
				      GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (props), TRUE);
	gtk_window_set_position (GTK_WINDOW (props),
				 GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show (GTK_WIDGET (props));
	burner_dialog_button_image(gtk_dialog_get_action_area(GTK_DIALOG (props)));
	result = gtk_dialog_run (GTK_DIALOG (props));
	gtk_widget_hide (GTK_WIDGET (props));
	if (result != GTK_RESPONSE_ACCEPT)
		goto end;

	burner_multi_song_props_set_rename_callback (BURNER_MULTI_SONG_PROPS (props),
						      gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree)),
						      BURNER_VIDEO_TREE_MODEL_NAME,
						      burner_video_disc_rename_songs);

	burner_multi_song_props_get_properties (BURNER_MULTI_SONG_PROPS (props),
						 &artist,
						 &composer,
						 &isrc,
						 NULL);

	copy = g_list_copy (list);
	copy = g_list_reverse (copy);

	for (item = copy; item; item = item->next) {
		GtkTreePath *treepath;
		BurnerTrack *track;

		treepath = item->data;
		track = burner_video_tree_model_path_to_track (BURNER_VIDEO_TREE_MODEL (model), treepath);
		if (!track)
			continue;

		burner_track_tag_add_string (track,
					      BURNER_TRACK_STREAM_ARTIST_TAG,
					      artist);

		burner_track_tag_add_string (track,
					      BURNER_TRACK_STREAM_COMPOSER_TAG,
					      composer);

		burner_track_tag_add_string (track,
					      BURNER_TRACK_STREAM_ISRC_TAG,
					      isrc);
	}

	g_list_free (copy);
	g_free (artist);
	g_free (composer);
	g_free (isrc);
end:

	gtk_widget_destroy (props);
}

static void
burner_video_disc_edit_song_properties_file (BurnerVideoDisc *self,
					      BurnerTrack *track)
{
	gint64 end;
	gint64 start;
	gchar *title;
	gchar *artist;
	gchar *composer;
	gchar *isrc;
	GtkWidget *props;
	guint64 length = 0;
	GtkWidget *toplevel;
	GtkResponseType result;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));

	burner_track_stream_get_length (BURNER_TRACK_STREAM (track), &length);

	props = burner_song_props_new ();
	burner_song_props_set_properties (BURNER_SONG_PROPS (props),
					   -1,
					   burner_track_tag_lookup_string (track, BURNER_TRACK_STREAM_ARTIST_TAG),
					   burner_track_tag_lookup_string (track, BURNER_TRACK_STREAM_TITLE_TAG),
					   burner_track_tag_lookup_string (track, BURNER_TRACK_STREAM_COMPOSER_TAG),
					   burner_track_tag_lookup_string (track, BURNER_TRACK_STREAM_ISRC_TAG),
					   length,
					   burner_track_stream_get_start (BURNER_TRACK_STREAM (track)),
					   burner_track_stream_get_end (BURNER_TRACK_STREAM (track)),
					   -1);

	gtk_window_set_transient_for (GTK_WINDOW (props),
				      GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (props), TRUE);
	gtk_window_set_position (GTK_WINDOW (props),
				 GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show (GTK_WIDGET (props));
	burner_dialog_button_image(gtk_dialog_get_action_area(GTK_DIALOG (props)));
	result = gtk_dialog_run (GTK_DIALOG (props));
	gtk_widget_hide (GTK_WIDGET (props));
	if (result != GTK_RESPONSE_ACCEPT)
		goto end;

	burner_song_props_get_properties (BURNER_SONG_PROPS (props),
					   &artist,
					   &title,
					   &composer,
					   &isrc,
					   &start,
					   &end,
					   NULL);

	if (title) {
		burner_track_tag_add_string (track,
					      BURNER_TRACK_STREAM_TITLE_TAG,
					      title);
		g_free (title);
	}

	if (artist) {
		burner_track_tag_add_string (track,
					      BURNER_TRACK_STREAM_ARTIST_TAG,
					      artist);
		g_free (artist);
	}

	if (composer) {
		burner_track_tag_add_string (track,
					      BURNER_TRACK_STREAM_COMPOSER_TAG,
					      composer);
		g_free (composer);
	}

	if (isrc) {
		burner_track_tag_add_string (track,
					      BURNER_TRACK_STREAM_ISRC_TAG,
					      isrc);
		g_free (isrc);
	}

	burner_track_stream_set_boundaries (BURNER_TRACK_STREAM (track),
					     start,
					     end,
					     0);
	/* Signal the change */
	burner_track_changed (track);

end:

	gtk_widget_destroy (props);
}

static void
burner_video_disc_edit_information_cb (GtkAction *action,
					BurnerVideoDisc *self)
{
	GList *list;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	if (!list)
		return;

	if (g_list_length (list) == 1) {
		BurnerTrack *track;
		GtkTreePath *treepath;

		treepath = list->data;

		track = burner_video_tree_model_path_to_track (BURNER_VIDEO_TREE_MODEL (model), treepath);
		if (track)
			burner_video_disc_edit_song_properties_file (self, track);
	}
	else
		burner_video_disc_edit_song_properties_list (self, list);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static void
burner_video_disc_open_file (BurnerVideoDisc *self)
{
	GList *item, *list;
	GSList *uris = NULL;
	GtkTreeModel *model;
	GtkTreePath *treepath;
	GtkTreeSelection *selection;
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	for (item = list; item; item = item->next) {
		BurnerTrack *track;
		gchar *uri;

		treepath = item->data;
		track = burner_video_tree_model_path_to_track (BURNER_VIDEO_TREE_MODEL (model), treepath);
		gtk_tree_path_free (treepath);

		if (!track)
			continue;

		uri = burner_track_stream_get_source (BURNER_TRACK_STREAM (track), TRUE);
		if (uri)
			uris = g_slist_prepend (uris, uri);
	}
	g_list_free (list);

	burner_utils_launch_app (GTK_WIDGET (self), uris);
	g_slist_free (uris);
}

static void
burner_video_disc_open_activated_cb (GtkAction *action,
				      BurnerVideoDisc *self)
{
	burner_video_disc_open_file (self);
}

static void
burner_video_disc_clipboard_text_cb (GtkClipboard *clipboard,
				      const gchar *text,
				      BurnerVideoDisc *self)
{
	gchar **array;
	gchar **item;

	if (!text)
		return;

	array = g_uri_list_extract_uris (text);
	item = array;
	while (*item) {
		if (**item != '\0') {
			GFile *file;
			gchar *uri;

			file = g_file_new_for_commandline_arg (*item);
			uri = g_file_get_uri (file);
			g_object_unref (file);

			burner_video_disc_add_uri_real (self,
							 uri,
							 -1,
							 -1,
							 -1,
							 NULL);
			g_free (uri);
		}

		item++;
	}
	g_strfreev (array);
}

static void
burner_video_disc_clipboard_targets_cb (GtkClipboard *clipboard,
					 GdkAtom *atoms,
					 gint n_atoms,
					 BurnerVideoDisc *self)
{
	if (burner_clipboard_selection_may_have_uri (atoms, n_atoms))
		gtk_clipboard_request_text (clipboard,
					    (GtkClipboardTextReceivedFunc) burner_video_disc_clipboard_text_cb,
					    self);
}

static void
burner_video_disc_paste_activated_cb (GtkAction *action,
				       BurnerVideoDisc *self)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_targets (clipboard,
				       (GtkClipboardTargetsReceivedFunc) burner_video_disc_clipboard_targets_cb,
				       self);
}

static void
burner_video_disc_delete_activated_cb (GtkAction *action,
					BurnerVideoDisc *self)
{
	burner_video_disc_delete_selected (BURNER_DISC (self));
}

static gboolean
burner_video_disc_button_pressed_cb (GtkTreeView *tree,
				      GdkEventButton *event,
				      BurnerVideoDisc *self)
{
	GtkWidgetClass *widget_class;
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);

	/* Avoid minding signals that happen out of the tree area (like in the 
	 * headers for example) */
	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree)))
		return FALSE;

	widget_class = GTK_WIDGET_GET_CLASS (tree);

	if (event->button == 3) {
		GtkTreeSelection *selection;
		GtkTreePath *path = NULL;
		GtkWidget *widget;

		gtk_tree_view_get_path_at_pos (tree,
					       event->x,
					       event->y,
					       &path,
					       NULL,
					       NULL,
					       NULL);

		selection = gtk_tree_view_get_selection (tree);

		/* Don't update the selection if the right click was on one of
		 * the already selected rows */
		if (!path || !gtk_tree_selection_path_is_selected (selection, path))
			widget_class->button_press_event (GTK_WIDGET (tree), event);

		widget = gtk_ui_manager_get_widget (priv->manager, "/ContextMenu/PasteAudio");
		if (widget) {
			if (gtk_clipboard_wait_is_text_available (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD)))
				gtk_widget_set_sensitive (widget, TRUE);
			else
				gtk_widget_set_sensitive (widget, FALSE);
		}

		widget = gtk_ui_manager_get_widget (priv->manager,"/ContextMenu");
		gtk_menu_popup (GTK_MENU (widget),
				NULL,
				NULL,
				NULL,
				NULL,
				event->button,
				event->time);
		return TRUE;
	}
	else if (event->button == 1) {
		gboolean result;
		GtkTreePath *treepath = NULL;

		result = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree),
							event->x,
							event->y,
							&treepath,
							NULL,
							NULL,
							NULL);

		/* we call the default handler for the treeview before everything else
		 * so it can update itself (paticularly its selection) before we have
		 * a look at it */
		widget_class->button_press_event (GTK_WIDGET (tree), event);
		
		if (!treepath) {
			GtkTreeSelection *selection;

			/* This is to deselect any row when selecting a 
			 * row that cannot be selected or in an empty
			 * part */
			selection = gtk_tree_view_get_selection (tree);
			gtk_tree_selection_unselect_all (selection);
			return FALSE;
		}
	
		if (!result)
			return FALSE;

		burner_disc_selection_changed (BURNER_DISC (self));
		if (event->type == GDK_2BUTTON_PRESS) {
			BurnerTrack *track;
			GtkTreeModel *model;

			model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
			track = burner_video_tree_model_path_to_track (BURNER_VIDEO_TREE_MODEL (model), treepath);
			if (track)
				burner_video_disc_edit_song_properties_file (self, track);
		}
	}

	return TRUE;
}

static guint
burner_video_disc_add_ui (BurnerDisc *disc,
			   GtkUIManager *manager,
			   GtkWidget *message)
{
	BurnerVideoDiscPrivate *priv;
	GError *error = NULL;
	guint merge_id;

	priv = BURNER_VIDEO_DISC_PRIVATE (disc);

	if (priv->message) {
		g_object_unref (priv->message);
		priv->message = NULL;
	}

	priv->message = message;
	g_object_ref (message);

	if (!priv->disc_group) {
		priv->disc_group = gtk_action_group_new (BURNER_DISC_ACTION "-video");
		gtk_action_group_set_translation_domain (priv->disc_group, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (priv->disc_group,
					      entries,
					      G_N_ELEMENTS (entries),
					      disc);
/*		gtk_action_group_add_toggle_actions (priv->disc_group,
						     toggle_entries,
						     G_N_ELEMENTS (toggle_entries),
						     disc);	*/
		gtk_ui_manager_insert_action_group (manager,
						    priv->disc_group,
						    0);
	}

	merge_id = gtk_ui_manager_add_ui_from_string (manager,
						      description,
						      -1,
						      &error);
	if (!merge_id) {
		g_error_free (error);
		return 0;
	}

	priv->manager = manager;
	g_object_ref (manager);
	return merge_id;
}

static void
burner_video_disc_rename_activated (BurnerVideoDisc *self)
{
	BurnerVideoDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GList *list;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	for (; list; list = g_list_remove (list, treepath)) {
		treepath = list->data;

		gtk_widget_grab_focus (priv->tree);
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->tree), 0);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->tree),
					      treepath,
					      NULL,
					      TRUE,
					      0.5,
					      0.5);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree),
					  treepath,
					  column,
					  TRUE);

		gtk_tree_path_free (treepath);
	}
}

static gboolean
burner_video_disc_key_released_cb (GtkTreeView *tree,
				    GdkEventKey *event,
				    BurnerVideoDisc *self)
{
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);
	if (priv->editing)
		return FALSE;

	if (event->keyval == GDK_KEY_KP_Delete || event->keyval == GDK_KEY_Delete) {
		burner_video_disc_delete_selected (BURNER_DISC (self));
	}
	else if (event->keyval == GDK_KEY_F2)
		burner_video_disc_rename_activated (self);

	return FALSE;
}

static void
burner_video_disc_init (BurnerVideoDisc *object)
{
	BurnerVideoDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *mainbox;
	GtkWidget *scroll;

	priv = BURNER_VIDEO_DISC_PRIVATE (object);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (object), GTK_ORIENTATION_VERTICAL);

	mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (mainbox);
	gtk_box_pack_start (GTK_BOX (object), mainbox, TRUE, TRUE, 0);

	priv->tree = gtk_tree_view_new ();
	egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (priv->tree));
	gtk_widget_show (priv->tree);

	g_signal_connect (priv->tree,
			  "button-press-event",
			  G_CALLBACK (burner_video_disc_button_pressed_cb),
			  object);
	g_signal_connect (priv->tree,
			  "key-release-event",
			  G_CALLBACK (burner_video_disc_key_released_cb),
			  object);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree), TRUE);
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (priv->tree), TRUE);

	/* columns */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_min_width (column, 200);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "pixbuf", BURNER_VIDEO_TREE_MODEL_THUMBNAIL);

	renderer = gtk_cell_renderer_text_new ();
	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (burner_video_disc_name_edited_cb), object);
	g_signal_connect (G_OBJECT (renderer), "editing-started",
			  G_CALLBACK (burner_video_disc_name_editing_started_cb), object);
	g_signal_connect (G_OBJECT (renderer), "editing-canceled",
			  G_CALLBACK (burner_video_disc_name_editing_canceled_cb), object);

	g_object_set (G_OBJECT (renderer),
		      "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
		      "ellipsize-set", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "markup", BURNER_VIDEO_TREE_MODEL_NAME);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "editable", BURNER_VIDEO_TREE_MODEL_EDITABLE);
	gtk_tree_view_column_set_title (column, _("Title"));
	g_object_set (G_OBJECT (column),
		      "expand", TRUE,
		      "spacing", 4,
		      NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree),
				     column);

	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (priv->tree),
					   column);


	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", BURNER_VIDEO_TREE_MODEL_SIZE);
	gtk_tree_view_column_set_title (column, _("Size"));

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, FALSE);

	/* selection */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (burner_video_disc_selection_changed_cb),
			  object);
	gtk_tree_selection_set_select_function (selection,
						burner_video_disc_selection_function,
						NULL,
						NULL);

	/* scroll */
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scroll);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), priv->tree);
	gtk_box_pack_start (GTK_BOX (mainbox), scroll, TRUE, TRUE, 0);

	/* dnd */
	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW
					      (priv->tree),
					      ntables_cd, nb_targets_cd,
					      GDK_ACTION_COPY |
					      GDK_ACTION_MOVE);

	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (priv->tree),
						GDK_BUTTON1_MASK,
						ntables_source,
						nb_targets_source,
						GDK_ACTION_MOVE);
}

static void
burner_video_disc_finalize (GObject *object)
{
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (object);

	if (priv->load_dir) {
		burner_io_cancel_by_base (priv->load_dir);
		burner_io_job_base_free (priv->load_dir);
		priv->load_dir = NULL;
	}	

	G_OBJECT_CLASS (burner_video_disc_parent_class)->finalize (object);
}

static void
burner_video_disc_get_property (GObject * object,
				 guint prop_id,
				 GValue * value,
				 GParamSpec * pspec)
{
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		g_value_set_boolean (value, priv->reject_files);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
burner_video_disc_set_property (GObject * object,
				 guint prop_id,
				 const GValue * value,
				 GParamSpec * pspec)
{
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		priv->reject_files = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static BurnerDiscResult
burner_video_disc_set_session_contents (BurnerDisc *self,
					 BurnerBurnSession *session)
{
	BurnerVideoTreeModel *model;
	GtkTreeModel *current_model;
	BurnerVideoDiscPrivate *priv;

	priv = BURNER_VIDEO_DISC_PRIVATE (self);

	if (priv->load_dir)
		burner_io_cancel_by_base (priv->load_dir);

	current_model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (current_model) {
		BurnerSessionCfg *current_session;

		current_session = burner_video_tree_model_get_session (BURNER_VIDEO_TREE_MODEL (current_model));
		if (current_session)
			g_signal_handlers_disconnect_by_func (current_session,
							      burner_video_disc_session_changed,
							      self);
	}

	if (!session) {
		gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree), NULL);
		return BURNER_DISC_OK;
	}

	model = burner_video_tree_model_new ();
	burner_video_tree_model_set_session (model, BURNER_SESSION_CFG (session));
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree),
				 GTK_TREE_MODEL (model));
	g_object_unref (model);

	g_signal_connect (session,
			  "is-valid",
			  G_CALLBACK (burner_video_disc_session_changed),
			  self);

	return BURNER_DISC_OK;
}

static gboolean
burner_video_disc_is_empty (BurnerDisc *disc)
{
	BurnerVideoDiscPrivate *priv;
	GtkTreeModel *model;

	priv = BURNER_VIDEO_DISC_PRIVATE (disc);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (!model)
		return FALSE;

	return gtk_tree_model_iter_n_children (model, NULL) != 0;
}

static void
burner_video_disc_iface_disc_init (BurnerDiscIface *iface)
{
	iface->add_uri = burner_video_disc_add_uri;
	iface->delete_selected = burner_video_disc_delete_selected;

	iface->is_empty = burner_video_disc_is_empty;
	iface->set_session_contents = burner_video_disc_set_session_contents;

	iface->get_selected_uri = burner_video_disc_get_selected_uri;
	iface->add_ui = burner_video_disc_add_ui;
}

static void
burner_video_disc_class_init (BurnerVideoDiscClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BurnerVideoDiscPrivate));

	object_class->finalize = burner_video_disc_finalize;
	object_class->set_property = burner_video_disc_set_property;
	object_class->get_property = burner_video_disc_get_property;

	g_object_class_install_property (object_class,
					 PROP_REJECT_FILE,
					 g_param_spec_boolean
					 ("reject-file",
					  "Whether it accepts files",
					  "Whether it accepts files",
					  FALSE,
					  G_PARAM_READWRITE));
}

GtkWidget *
burner_video_disc_new (void)
{
	return g_object_new (BURNER_TYPE_VIDEO_DISC, NULL);
}

