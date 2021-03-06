/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libburner-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libburner-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libburner-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libburner-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libburner-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libburner-misc is distributed in the hope that it will be useful,
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

#ifndef _BURNER_JACKET_FONT_H_
#define _BURNER_JACKET_FONT_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BURNER_TYPE_JACKET_FONT             (burner_jacket_font_get_type ())
#define BURNER_JACKET_FONT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BURNER_TYPE_JACKET_FONT, BurnerJacketFont))
#define BURNER_JACKET_FONT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BURNER_TYPE_JACKET_FONT, BurnerJacketFontClass))
#define BURNER_IS_JACKET_FONT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BURNER_TYPE_JACKET_FONT))
#define BURNER_IS_JACKET_FONT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BURNER_TYPE_JACKET_FONT))
#define BURNER_JACKET_FONT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BURNER_TYPE_JACKET_FONT, BurnerJacketFontClass))

typedef struct _BurnerJacketFontClass BurnerJacketFontClass;
typedef struct _BurnerJacketFont BurnerJacketFont;

struct _BurnerJacketFontClass
{
	GtkBoxClass parent_class;
};

struct _BurnerJacketFont
{
	GtkBox parent_instance;
};

GType burner_jacket_font_get_type (void) G_GNUC_CONST;

GtkWidget *
burner_jacket_font_new (void);

void
burner_jacket_font_set_name (BurnerJacketFont *font,
			      const gchar *name);

gchar *
burner_jacket_font_get_family (BurnerJacketFont *font);

guint
burner_jacket_font_get_size (BurnerJacketFont *font);

G_END_DECLS

#endif /* _BURNER_JACKET_FONT_H_ */
