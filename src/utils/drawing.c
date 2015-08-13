/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "drawing.h"

#include <gtk/gtk.h>
#include <math.h>

GdkPixbuf *
ring_draw_fallback_avatar(int size) {
    cairo_surface_t *surface;
    cairo_t *cr;

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cr = cairo_create(surface);

    cairo_pattern_t *linpat = cairo_pattern_create_linear(0, 0, 0, size);
    cairo_pattern_add_color_stop_rgb(linpat, 0, 0.937, 0.937, 0.937);
    cairo_pattern_add_color_stop_rgb(linpat, 1, 0.969, 0.969, 0.969);

    cairo_set_source(cr, linpat);
    cairo_paint(cr);

    int avatar_size = size * 0.3;
    GtkIconInfo *icon_info = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(), "avatar-default-symbolic",
                                                        avatar_size, GTK_ICON_LOOKUP_GENERIC_FALLBACK);
    GdkPixbuf *pixbuf_icon = gtk_icon_info_load_icon(icon_info, NULL);
    g_object_unref(icon_info);

    if (pixbuf_icon != NULL) {
        gdk_cairo_set_source_pixbuf(cr, pixbuf_icon, (size - avatar_size) / 2, (size - avatar_size) / 2);
        g_object_unref(pixbuf_icon);
        cairo_rectangle(cr, (size - avatar_size) / 2, (size - avatar_size) / 2, avatar_size, avatar_size);
        cairo_fill(cr);
    }

    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, size, size);

    /* free resources */
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return pixbuf;
}

GdkPixbuf *
ring_draw_conference_avatar(int size) {
    cairo_surface_t *surface;
    cairo_t *cr;

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cr = cairo_create(surface);

    cairo_pattern_t *linpat = cairo_pattern_create_linear(0, 0, 0, size);
    cairo_pattern_add_color_stop_rgb(linpat, 0, 0.937, 0.937, 0.937);
    cairo_pattern_add_color_stop_rgb(linpat, 1, 0.969, 0.969, 0.969);

    cairo_set_source(cr, linpat);
    cairo_paint(cr);

    int avatar_size = size * 0.5;
    GtkIconInfo *icon_info = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(), "system-users-symbolic",
                                                        avatar_size, GTK_ICON_LOOKUP_GENERIC_FALLBACK);
    GdkPixbuf *pixbuf_icon = gtk_icon_info_load_icon(icon_info, NULL);
    g_object_unref(icon_info);

    if (pixbuf_icon != NULL) {
        gdk_cairo_set_source_pixbuf(cr, pixbuf_icon, (size - avatar_size) / 2, (size - avatar_size) / 2);
        g_object_unref(pixbuf_icon);
        cairo_rectangle(cr, (size - avatar_size) / 2, (size - avatar_size) / 2, avatar_size, avatar_size);
        cairo_fill(cr);
    }

    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, size, size);

    /* free resources */
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return pixbuf;
}

GdkPixbuf *
ring_frame_avatar(GdkPixbuf *avatar) {
    int extra_space = 10;
    int offset = extra_space/2;
    int w = gdk_pixbuf_get_width(avatar);
    int h = gdk_pixbuf_get_height(avatar);
    int w_surface = w + extra_space;
    int h_surface = h + extra_space;
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w_surface, h_surface);
    cairo_t *cr = cairo_create(surface);

    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, w_surface, h_surface);
    cairo_fill(cr);

    gdk_cairo_set_source_pixbuf(cr, avatar, offset, offset);

    double aspect = (double)w/(double)h;
    double corner_radius = 5;
    double radius = corner_radius/aspect;
    double degrees = M_PI / 180.0;

    cairo_new_sub_path (cr);
    cairo_arc (cr, offset + w - radius, offset + radius, radius, -90 * degrees, 0 * degrees);
    cairo_arc (cr, offset + w - radius, offset + h - radius, radius, 0 * degrees, 90 * degrees);
    cairo_arc (cr, offset + radius, offset + h - radius, radius, 90 * degrees, 180 * degrees);
    cairo_arc (cr, offset + radius, offset + radius, radius, 180 * degrees, 270 * degrees);
    cairo_close_path (cr);

    cairo_fill_preserve(cr);

    cairo_set_source_rgba (cr, 58.0/256.0, 191/256.0, 210/256.0, 1.0);
    cairo_set_line_width (cr, 2.0);
    cairo_stroke (cr);

    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, w_surface, h_surface);

    /* free resources */
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return pixbuf;
}
