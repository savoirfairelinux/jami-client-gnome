/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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
 */

#include "drawing.h"

#include <gtk/gtk.h>
#include <math.h>

static constexpr const char* MSG_COUNT_FONT        = "Sans";
static constexpr int         MSG_COUNT_FONT_SIZE   = 12;
static constexpr GdkRGBA     MSG_COUNT_FONT_COLOUR = {1.0, 1.0, 1.0, 1.0}; // white
static constexpr GdkRGBA     MSG_COUNT_BACKGROUND  = {0.984, 0.282, 0.278, 0.9}; // red 251, 72, 71, 0.9

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

    cairo_pattern_destroy(linpat);

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
ring_frame_avatar(GdkPixbuf *avatar, bool presenceStatus) {
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

    double aspect = (double)w/(double)h;
    double corner_radius = 5;
    double radius = corner_radius/aspect;
    double degrees = M_PI / 180.0;

    // create the square path with ronded corners
    cairo_new_sub_path (cr);
    cairo_arc (cr, offset + w - radius, offset + radius, radius, -90 * degrees, 0 * degrees);
    cairo_arc (cr, offset + w - radius, offset + h - radius, radius, 0 * degrees, 90 * degrees);
    cairo_arc (cr, offset + radius, offset + h - radius, radius, 90 * degrees, 180 * degrees);
    cairo_arc (cr, offset + radius, offset + radius, radius, 180 * degrees, 270 * degrees);
    cairo_close_path (cr);

    // in case the image has alpha, we want to first set the background of the part inside the
    // blue frame to white; otherwise the resulting image will show whatever is in the background,
    // which can be weird in certain cases (eg: the image displayed over a video)
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    cairo_fill_preserve(cr);

    // now draw the image over this black square
    gdk_cairo_set_source_pixbuf(cr, avatar, offset, offset);
    cairo_fill_preserve(cr);

    // now draw the blue frame
    cairo_set_source_rgba (cr, 58.0/256.0, 191/256.0, 210/256.0, 1.0);

    // Present
    if (presenceStatus)
    {
        cairo_set_source_rgba (cr, 11.0/265.0, 208.0/256.0, 47.0/256.0, 1.0);
    }

    cairo_set_line_width (cr, 2.0);
    cairo_stroke (cr);

    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, w_surface, h_surface);

    /* free resources */
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return pixbuf;
}

static void
create_rounded_rectangle_path(cairo_t *cr, double corner_radius, double x, double y, double w, double h)
{
    double radius = corner_radius;
    double degrees = M_PI / 180.0;

    cairo_new_sub_path (cr);
    cairo_arc (cr, x + w - radius, y + radius, radius, -90 * degrees, 0 * degrees);
    cairo_arc (cr, x + w - radius, y + h - radius, radius, 0 * degrees, 90 * degrees);
    cairo_arc (cr, x + radius, y + h - radius, radius, 90 * degrees, 180 * degrees);
    cairo_arc (cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
    cairo_close_path (cr);
}

/**
 * Draws the unread message count in the bottom right corner of the given image.
 * In the case that the count is less than or equal to 0, nothing is drawn.
 */
GdkPixbuf *
ring_draw_unread_messages(const GdkPixbuf *avatar, int unread_count) {
    if (unread_count <= 0) {
        // simply return a copy of the original pixbuf
        return gdk_pixbuf_copy(avatar);
    }
    int w = gdk_pixbuf_get_width(avatar);
    int h = gdk_pixbuf_get_height(avatar);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(surface);
    cairo_surface_destroy(surface);

    /* draw original image */
    gdk_cairo_set_source_pixbuf(cr, avatar, 0, 0);
    cairo_paint(cr);

    /* make text */
    char *text = g_strdup_printf("%d", unread_count);
    cairo_text_extents_t extents;

    cairo_select_font_face (cr, MSG_COUNT_FONT,
    CAIRO_FONT_SLANT_NORMAL,
    CAIRO_FONT_WEIGHT_NORMAL);

    cairo_set_font_size (cr, MSG_COUNT_FONT_SIZE);
    cairo_text_extents (cr, text, &extents);

    /* draw rounded rectangle around the text, with 3 pixel border
     * ie: 6 pixels higher, 6 pixels wider */
    int border_width = 3;
    double rec_x = w - extents.width - border_width * 2;
    double rec_y = h - extents.height - border_width * 2;
    double rec_w = extents.width + border_width * 2;
    double rec_h = extents.height + border_width * 2;
    double corner_radius = rec_h/2.5;
    create_rounded_rectangle_path(cr, corner_radius, rec_x, rec_y, rec_w, rec_h);
    cairo_set_source_rgba(cr, MSG_COUNT_BACKGROUND.red, MSG_COUNT_BACKGROUND.blue, MSG_COUNT_BACKGROUND.green, MSG_COUNT_BACKGROUND.alpha);
    cairo_fill(cr);

    /* draw text */
    cairo_move_to (cr, w-extents.width-border_width, h-border_width);
    cairo_set_source_rgb(cr, MSG_COUNT_FONT_COLOUR.red, MSG_COUNT_FONT_COLOUR.blue, MSG_COUNT_FONT_COLOUR.green);
    cairo_show_text (cr, text);

    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, w, h);

    /* free resources */
    cairo_destroy(cr);
    g_free(text);

    return pixbuf;
}
