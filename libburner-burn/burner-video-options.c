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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "burner-misc.h"
#include "burner-tags.h"
#include "burner-session.h"
#include "burner-session-helper.h"
#include "burner-video-options.h"

typedef struct _BurnerVideoOptionsPrivate BurnerVideoOptionsPrivate;
struct _BurnerVideoOptionsPrivate
{
	BurnerBurnSession *session;

	GtkWidget *video_options;
	GtkWidget *vcd_label;
	GtkWidget *vcd_button;
	GtkWidget *svcd_button;

	GtkWidget *button_native;
	GtkWidget *button_ntsc;
	GtkWidget *button_pal;
	GtkWidget *button_4_3;
	GtkWidget *button_16_9;
};

#define BURNER_VIDEO_OPTIONS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BURNER_TYPE_VIDEO_OPTIONS, BurnerVideoOptionsPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (BurnerVideoOptions, burner_video_options, GTK_TYPE_ALIGNMENT);

static void
burner_video_options_update_from_tag (BurnerVideoOptions *options,
                                       const gchar *tag)
{
	BurnerVideoOptionsPrivate *priv;

	if (!tag)
		return;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (options);
	
	if (!strcmp (tag, BURNER_VCD_TYPE)) {
		BurnerMedia media;
		gint svcd_type;

		media = burner_burn_session_get_dest_media (priv->session);
		if (media & BURNER_MEDIUM_DVD) {
			/* Don't change anything in this case
			 * as the tag has no influence over 
			 * this type of image */
			return;
		}
		else if (media & BURNER_MEDIUM_FILE) {
			BurnerImageFormat format;

			format = burner_burn_session_get_output_format (priv->session);

			/* Same as above for the following case */
			if (format == BURNER_IMAGE_FORMAT_BIN)
				return;
		}

		svcd_type = burner_burn_session_tag_lookup_int (priv->session, tag);
		if (svcd_type == BURNER_SVCD) {
			if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->svcd_button)))
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->svcd_button), TRUE);

				gtk_widget_set_sensitive (priv->button_4_3, TRUE);
				gtk_widget_set_sensitive (priv->button_16_9, TRUE);
		}
		else {
			if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->vcd_button)))
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->vcd_button), TRUE);

			gtk_widget_set_sensitive (priv->button_4_3, FALSE);
			gtk_widget_set_sensitive (priv->button_16_9, FALSE);
		}
	}
	else if (!strcmp (tag, BURNER_VIDEO_OUTPUT_FRAMERATE)) {
		GValue *value = NULL;

		burner_burn_session_tag_lookup (priv->session,
						 tag,
						 &value);
		if (value) {
			if (g_value_get_int (value) == BURNER_VIDEO_FRAMERATE_NTSC) {
				if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_ntsc)))
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_ntsc), TRUE);
			}
			else {
				if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_pal)))
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_pal), TRUE);
			}
		}
		else if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_native)))
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_native), TRUE);
	}
	else if (!strcmp (tag, BURNER_VIDEO_OUTPUT_ASPECT)) {
		gint aspect_type = burner_burn_session_tag_lookup_int (priv->session, tag);

		if (aspect_type == BURNER_VIDEO_ASPECT_16_9) {
			if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_16_9)))
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_16_9), TRUE);
		}
		else {
			if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_4_3)))
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_4_3), TRUE);
		}
	}
}

static void
burner_video_options_update (BurnerVideoOptions *options)
{
	BurnerVideoOptionsPrivate *priv;
	BurnerMedia media;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (options);

	/* means we haven't initialized yet */
	if (!priv->vcd_label)
		return;

	media = burner_burn_session_get_dest_media (priv->session);
	if (media & BURNER_MEDIUM_DVD) {
		gtk_widget_hide (priv->vcd_label);
		gtk_widget_hide (priv->vcd_button);
		gtk_widget_hide (priv->svcd_button);

		gtk_widget_set_sensitive (priv->button_4_3, TRUE);
		gtk_widget_set_sensitive (priv->button_16_9, TRUE);
	}
	else if (media & BURNER_MEDIUM_CD) {
		gtk_widget_show (priv->vcd_label);
		gtk_widget_show (priv->vcd_button);
		gtk_widget_show (priv->svcd_button);

		burner_video_options_update_from_tag (options, BURNER_VCD_TYPE);
	}
	else if (media & BURNER_MEDIUM_FILE) {
		BurnerImageFormat format;

		/* Hide any options about (S)VCD type
		 * as this is handled in BurnerImageTypeChooser 
		 * object */
		gtk_widget_hide (priv->vcd_label);
		gtk_widget_hide (priv->vcd_button);
		gtk_widget_hide (priv->svcd_button);

		format = burner_burn_session_get_output_format (priv->session);
		if (format == BURNER_IMAGE_FORMAT_BIN) {
			gtk_widget_set_sensitive (priv->button_4_3, TRUE);
			gtk_widget_set_sensitive (priv->button_16_9, TRUE);
		}
		else if (format == BURNER_IMAGE_FORMAT_CUE)
			burner_video_options_update_from_tag (options, BURNER_VCD_TYPE);
	}
}

static void
burner_video_options_output_changed_cb (BurnerBurnSession *session,
                                         BurnerMedium *former_medium,
                                         BurnerVideoOptions *options)
{
	burner_video_options_update (options);
}

static void
burner_video_options_tag_changed_cb (BurnerBurnSession *session,
                                      const gchar *tag,
                                      BurnerVideoOptions *options)
{
	burner_video_options_update_from_tag (options, tag);
}

static void
burner_video_options_SVCD (GtkToggleButton *button,
			    BurnerVideoOptions *options)
{
	BurnerVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (options);
	burner_burn_session_tag_add_int (priv->session,
	                                  BURNER_VCD_TYPE,
	                                  BURNER_SVCD);

	/* NOTE: this is only possible when that's
	 * not an image */

	gtk_widget_set_sensitive (priv->button_4_3, TRUE);
	gtk_widget_set_sensitive (priv->button_16_9, TRUE);
}

static void
burner_video_options_VCD (GtkToggleButton *button,
			   BurnerVideoOptions *options)
{
	BurnerVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (options);
	burner_burn_session_tag_add_int (priv->session,
	                                  BURNER_VCD_TYPE,
	                                  BURNER_VCD_V2);

	/* NOTE: this is only possible when that's
	 * not an image */
	gtk_widget_set_sensitive (priv->button_4_3, FALSE);
	gtk_widget_set_sensitive (priv->button_16_9, FALSE);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_4_3), TRUE);
}

static void
burner_video_options_NTSC (GtkToggleButton *button,
			    BurnerVideoOptions *options)
{
	BurnerVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (options);
	burner_burn_session_tag_add_int (priv->session,
	                                  BURNER_VIDEO_OUTPUT_FRAMERATE,
	                                  BURNER_VIDEO_FRAMERATE_NTSC);
}

static void
burner_video_options_PAL_SECAM (GtkToggleButton *button,
				 BurnerVideoOptions *options)
{
	BurnerVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (options);
	burner_burn_session_tag_add_int (priv->session,
	                                  BURNER_VIDEO_OUTPUT_FRAMERATE,
	                                  BURNER_VIDEO_FRAMERATE_PAL_SECAM);
}

static void
burner_video_options_native_framerate (GtkToggleButton *button,
					BurnerVideoOptions *options)
{
	BurnerVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (options);
	burner_burn_session_tag_remove (priv->session, BURNER_VIDEO_OUTPUT_FRAMERATE);
}

static void
burner_video_options_16_9 (GtkToggleButton *button,
			    BurnerVideoOptions *options)
{
	BurnerVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (options);
	burner_burn_session_tag_add_int (priv->session,
	                                  BURNER_VIDEO_OUTPUT_ASPECT,
	                                  BURNER_VIDEO_ASPECT_16_9);
}

static void
burner_video_options_4_3 (GtkToggleButton *button,
			   BurnerVideoOptions *options)
{
	BurnerVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (options);
	burner_burn_session_tag_add_int (priv->session,
	                                  BURNER_VIDEO_OUTPUT_ASPECT,
	                                  BURNER_VIDEO_ASPECT_4_3);
}

void
burner_video_options_set_session (BurnerVideoOptions *options,
                                   BurnerBurnSession *session)
{
	BurnerVideoOptionsPrivate *priv;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (options);
	if (priv->session) {
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      burner_video_options_output_changed_cb,
		                                      options);
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      burner_video_options_tag_changed_cb,
		                                      options);
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (session) {
		priv->session = g_object_ref (session);
		burner_video_options_update (options);

		if (burner_burn_session_tag_lookup (session, BURNER_VIDEO_OUTPUT_FRAMERATE, NULL) == BURNER_BURN_OK)
			burner_video_options_update_from_tag (options, BURNER_VIDEO_OUTPUT_FRAMERATE);

		/* If session has tag update UI otherwise update _from_ UI */
		if (burner_burn_session_tag_lookup (session, BURNER_VIDEO_OUTPUT_ASPECT, NULL) == BURNER_BURN_OK)
			burner_video_options_update_from_tag (options, BURNER_VIDEO_OUTPUT_ASPECT);
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_4_3)))
			burner_burn_session_tag_add_int (priv->session,
			                                  BURNER_VIDEO_OUTPUT_ASPECT,
	        			                  BURNER_VIDEO_ASPECT_4_3);
		else
			burner_burn_session_tag_add_int (priv->session,
				                          BURNER_VIDEO_OUTPUT_ASPECT,
				                          BURNER_VIDEO_ASPECT_16_9);

		g_signal_connect (priv->session,
		                  "output-changed",
		                  G_CALLBACK (burner_video_options_output_changed_cb),
		                  options);
		g_signal_connect (priv->session,
		                  "tag-changed",
		                  G_CALLBACK (burner_video_options_tag_changed_cb),
		                  options);
	}
}

static void
burner_video_options_set_property (GObject *object,
				    guint prop_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	g_return_if_fail (BURNER_IS_VIDEO_OPTIONS (object));

	switch (prop_id)
	{
	case PROP_SESSION:
		burner_video_options_set_session (BURNER_VIDEO_OPTIONS (object),
		                                   g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
burner_video_options_get_property (GObject *object,
				    guint prop_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	BurnerVideoOptionsPrivate *priv;

	g_return_if_fail (BURNER_IS_VIDEO_OPTIONS (object));

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_SESSION:
		g_value_set_object (value, priv->session);
		g_object_ref (priv->session);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
burner_video_options_init (BurnerVideoOptions *object)
{
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *widget;
	GtkWidget *button1;
	GtkWidget *button2;
	GtkWidget *button3;
	BurnerVideoOptionsPrivate *priv;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (object);

	gtk_container_set_border_width (GTK_CONTAINER (object), 6);
	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	table = gtk_table_new (3, 4, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);

	label = gtk_label_new (_("Video format:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button1 = gtk_radio_button_new_with_mnemonic (NULL, _("_NTSC"));
	priv->button_ntsc = button1;
	gtk_widget_set_tooltip_text (button1, _("Format used mostly on the North American continent"));
	g_signal_connect (button1,
			  "toggled",
			  G_CALLBACK (burner_video_options_NTSC),
			  object);
	gtk_table_attach (GTK_TABLE (table),
			  button1,
			  3, 4,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button2 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1), _("_PAL/SECAM"));
	priv->button_pal = button2;
	gtk_widget_set_tooltip_text (button2, _("Format used mostly in Europe"));
	g_signal_connect (button2,
			  "toggled",
			  G_CALLBACK (burner_video_options_PAL_SECAM),
			  object);
	gtk_table_attach (GTK_TABLE (table),
			  button2,
			  2, 3,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button3 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1),
								  _("Native _format"));
	priv->button_native = button3;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button3), TRUE);
	g_signal_connect (button3,
			  "toggled",
			  G_CALLBACK (burner_video_options_native_framerate),
			  object);
	gtk_table_attach (GTK_TABLE (table),
			  button3,
			  1, 2,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	label = gtk_label_new (_("Aspect ratio:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button1 = gtk_radio_button_new_with_mnemonic (NULL, _("_4:3"));
	g_signal_connect (button1,
			  "toggled",
			  G_CALLBACK (burner_video_options_4_3),
			  object);
	gtk_table_attach (GTK_TABLE (table),
			  button1,
			  1, 2,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);
	priv->button_4_3 = button1;

	button2 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1),
								  _("_16:9"));
	g_signal_connect (button2,
			  "toggled",
			  G_CALLBACK (burner_video_options_16_9),
			  object);
	gtk_table_attach (GTK_TABLE (table),
			  button2,
			  2, 3,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);
	priv->button_16_9 = button2;

	/* Video options for (S)VCD */
	label = gtk_label_new (_("VCD type:"));
	priv->vcd_label = label;

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1,
			  2, 3,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button1 = gtk_radio_button_new_with_mnemonic_from_widget (NULL, _("Create an SVCD"));
	priv->svcd_button = button1;
	gtk_table_attach (GTK_TABLE (table),
			  button1,
			  1, 2,
			  2, 3,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	g_signal_connect (button1,
			  "clicked",
			  G_CALLBACK (burner_video_options_SVCD),
			  object);

	button2 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1), _("Create a VCD"));
	priv->vcd_button = button2;
	gtk_table_attach (GTK_TABLE (table),
			  button2,
			  2, 3,
			  2, 3,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	g_signal_connect (button2,
			  "clicked",
			  G_CALLBACK (burner_video_options_VCD),
			  object);

	gtk_box_pack_start (GTK_BOX (widget), table, FALSE, FALSE, 0);

	/* NOTE: audio options for DVDs were removed. For SVCD that is MP2 and
	 * for Video DVD even if we have a choice AC3 is the most widespread
	 * audio format. So use AC3 by default. */

	gtk_widget_show_all (widget);
	gtk_container_add (GTK_CONTAINER (object), widget);
}

static void
burner_video_options_finalize (GObject *object)
{
	BurnerVideoOptionsPrivate *priv;

	priv = BURNER_VIDEO_OPTIONS_PRIVATE (object);
	if (priv->session) {
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      burner_video_options_output_changed_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      burner_video_options_tag_changed_cb,
		                                      object);
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	G_OBJECT_CLASS (burner_video_options_parent_class)->finalize (object);
}

static void
burner_video_options_class_init (BurnerVideoOptionsClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BurnerVideoOptionsPrivate));

	object_class->finalize = burner_video_options_finalize;
	object_class->set_property = burner_video_options_set_property;
	object_class->get_property = burner_video_options_get_property;

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session",
							      "The session to work with",
							      BURNER_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE));
}

GtkWidget *
burner_video_options_new (BurnerBurnSession *session)
{
	return g_object_new (BURNER_TYPE_VIDEO_OPTIONS, "session", session, NULL);
}
