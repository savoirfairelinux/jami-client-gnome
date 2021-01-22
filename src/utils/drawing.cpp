/*
 *  Copyright (C) 2015-2021 Savoir-faire Linux Inc.
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
#include <algorithm>
#include <qrencode.h>

static constexpr const char* MSG_COUNT_FONT        = "Sans";
static constexpr int         MSG_COUNT_FONT_SIZE   = 12;
static constexpr GdkRGBA     MSG_COUNT_FONT_COLOUR = {1.0, 1.0, 1.0, 1.0}; // white
static constexpr GdkRGBA     MSG_COUNT_BACKGROUND  = {0.984, 0.282, 0.278, 1.0}; // red 251, 72, 71, 1.0
static constexpr GdkRGBA     GREEN_BACKGROUND = {0.298, 0.392, 0.85, 1.0}; // green 112, 217, 6, 0.9
static constexpr GdkRGBA     ORANGE_BACKGROUND = {1.0, 0.0, 0.5, 1.0}; // orange 255, 128, 0, 1.0
static constexpr GdkRGBA     RED_BACKGROUND = {1.0, 0.156, 0.231, 1.0}; // red 255, 59, 40, 0.9
// This is the color palette for default avatars
static constexpr GdkRGBA     COLOR_PALETTE[] = {{0.956862, 0.262745, 0.211764, 1.0}, // red 244, green 67, blue 54, 1 (red)
                                                {0.913725, 0.117647, 0.388235, 1.0}, // red 233, green 30, blue 99, 1 (pink)
                                                {0.611764, 0.152941, 0.690196, 1.0}, // red 156, green 39, blue 176, 1 (purple)
                                                {0.270588, 0.152941, 0.627450, 1.0}, // red 69, green 39, blue 160, 1 (deep purple)
                                                {0.403921, 0.227450, 0.717647, 1.0}, // red 103, green 58, blue 183, 1 (indigo)
                                                {0.247058, 0.317647, 0.211764, 1.0}, // red 63, green 81, blue 54, 1 (blue)
                                                {0, 0.737254, 0.831372, 1.0},        // red 0, green 188, blue 212, 1 (cyan)
                                                {0, 0.588235, 0.533333, 1.0},        // red 0, green 150, blue 136, 1 (teal)

                                                {0.298039, 0.682745, 0.313725, 1.0}, // red 76, green 175, blue 80, 1 (green)
                                                {0.545098, 0.764705, 0.290196, 1.0}, // red 138, green 194, blue 73, 1 (light green)
                                                {0.619607, 0.619607, 0.619607, 1.0}, // red 157, green 157, blue 157, 1 (grey)
                                                {0.803921, 0.862745, 0.223529, 1.0}, // red 204, green 219, blue 56, 1 (lime)
                                                {1, 0.756862, 0.027450, 1.0},        // red 255, green 192, blue 6, 1 (amber)
                                                {1, 0.341176, 0.133333, 1.0},        // red 255, green 86, blue 33, 1 (deep orange)
                                                {0.474509, 0.333333, 0.282352, 1.0}, // red 120, green 84, blue 71, 1 (brown)
                                                {0.376470, 0.490196, 0.545098, 1.0}};// red 95, green 124, blue 138, 1 (blue grey)

GdkPixbuf *
draw_fallback_avatar(int size, const std::string& letter, const char color) {
    cairo_surface_t *surface;
    cairo_t *cr;

    // Fill the background
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cr = cairo_create(surface);
    auto bg_color = COLOR_PALETTE[color % 16];
    cairo_set_source_rgb (cr, bg_color.red, bg_color.green, bg_color.blue);
    cairo_paint(cr);

    if (!letter.empty()) {
        // Draw a letter at the center of the avatar
        cairo_text_extents_t extents;
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, size / 2);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_text_extents(cr, letter.c_str(), &extents);
        auto x = size/2-(extents.width/2 + extents.x_bearing);
        auto y = size/2-(extents.height/2 + extents.y_bearing);
        cairo_move_to(cr, x, y);
        cairo_show_text(cr, letter.c_str());
    } else {
        // Compose from fallback svg if no letter found
        GError *error = nullptr;
        auto* finalAvatar = gdk_pixbuf_get_from_surface(cairo_get_target(cr), 0, 0, size, size);
        auto* fallbackavatar = gdk_pixbuf_new_from_resource_at_scale("/net/jami/JamiGnome/fallbackavatar", size, size, true, &error);
        gdk_pixbuf_composite (fallbackavatar, finalAvatar, 0, 0, size, size, 0, 0, 1, 1, GDK_INTERP_BILINEAR, 0xff);

        /* free resources */
        g_object_unref(fallbackavatar);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);

        return finalAvatar;
    }


    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(cairo_get_target(cr), 0, 0, size, size);

    /* free resources */
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return pixbuf;
}

GdkPixbuf *
draw_conference_avatar(int size) {
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
frame_avatar(GdkPixbuf *avatar) {

    auto w = gdk_pixbuf_get_width(avatar);
    auto h = gdk_pixbuf_get_height(avatar);
    auto crop_size = std::min(h, w);
    auto new_size = std::max(h, w);
    auto scale = (double)new_size/(double)crop_size;
    GdkPixbuf *crop_avatar = gdk_pixbuf_new (
                               gdk_pixbuf_get_colorspace (avatar),
                               gdk_pixbuf_get_has_alpha (avatar),
                               gdk_pixbuf_get_bits_per_sample (avatar),
                               new_size, new_size);
    gdk_pixbuf_scale (avatar, crop_avatar, 0, 0, new_size, new_size,
                      (w/2)-(new_size/2), (h/2)-(new_size/2), scale, scale,
                      GDK_INTERP_BILINEAR);
    auto extra_space = 10;
    auto offset = extra_space/2;
    auto s_surface = new_size + extra_space;
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, s_surface, s_surface);
    cairo_t *cr = cairo_create(surface);

    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, s_surface, s_surface);
    cairo_fill(cr);

    double radius = new_size/2;
    constexpr double degrees = M_PI / 180.0;

    // create the square path with ronded corners
    cairo_new_sub_path (cr);
    cairo_arc (cr, offset + new_size - radius, offset + radius, radius, -90 * degrees, 0 * degrees);
    cairo_arc (cr, offset + new_size - radius, offset + new_size - radius, radius, 0 * degrees, 90 * degrees);
    cairo_arc (cr, offset + radius, offset + new_size - radius, radius, 90 * degrees, 180 * degrees);
    cairo_arc (cr, offset + radius, offset + radius, radius, 180 * degrees, 270 * degrees);
    cairo_close_path (cr);

    // in case the image has alpha, we want to first set the background of the part inside the
    // blue frame to white; otherwise the resulting image will show whatever is in the background,
    // which can be weird in certain cases (eg: the image displayed over a video)
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    cairo_fill_preserve(cr);

    // now draw the image over this black square
    gdk_cairo_set_source_pixbuf(cr, crop_avatar, offset, offset);
    cairo_fill_preserve(cr);

    auto pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, s_surface, s_surface);

    /* free resources */
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(crop_avatar);

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

gboolean
draw_qrcode(cairo_t* cr, const std::string& to_encode, uint32_t size)
{
    auto rcode = QRcode_encodeString(to_encode.c_str(),
                                      0, //Let the version be decided by libqrencode
                                      QR_ECLEVEL_L, // Lowest level of error correction
                                      QR_MODE_8, // 8-bit data mode
                                      1);

    if (!rcode) { // no rcode, no draw
        g_warning("Failed to generate QR code");
        return FALSE;
    }

    auto margin = 5;
    int qrwidth = rcode->width + margin * 2;

    /* scaling */
    auto scale = size/qrwidth;
    cairo_scale(cr, scale, scale);

    /* fill the background in white */
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_rectangle(cr, 0, 0, qrwidth, qrwidth);
    cairo_fill (cr);

    unsigned char *p;
    p = rcode->data;
    cairo_set_source_rgb(cr, 0, 0, 0); // back in black
    for(int y = 0; y < rcode->width; y++) {
        unsigned char* row = (p + (y * rcode->width));
        for(int x = 0; x < rcode->width; x++) {
            if(*(row + x) & 0x1) {
                cairo_rectangle(cr, margin + x, margin + y, 1, 1);
                cairo_fill(cr);
            }
        }
    }

    QRcode_free(rcode);
    return TRUE;
}

/**
 * Draws the presence icon in the top right corner of the given image.
 */
GdkPixbuf *
draw_status(const GdkPixbuf *avatar, IconStatus status) {
    if (status == IconStatus::INVALID) {
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

    /* draw rounded rectangle, with 3 pixel border
     * ie: 6 pixels higher, 6 pixels wider */
    int border_width = 5;
    double rec_x = w - border_width * 3;
    double rec_y = h - border_width * 3;
    double rec_w = border_width * 2;
    double rec_h = border_width * 2;
    double corner_radius = rec_h/2.5;
    create_rounded_rectangle_path(cr, corner_radius, rec_x, rec_y, rec_w, rec_h);

    // For now we don't draw the absent background.
    auto background = RED_BACKGROUND;
    switch(status) {
        case IconStatus::TRYING:
            background = ORANGE_BACKGROUND;
            break;
        case IconStatus::PRESENT:
        case IconStatus::CONNECTED:
            background = GREEN_BACKGROUND;
            break;
        case IconStatus::ABSENT:
        case IconStatus::DISCONNECTED:
        case IconStatus::INVALID:
            background = RED_BACKGROUND;
            break;
    }
    cairo_set_source_rgba(
        cr,
        background.red,
        background.blue,
        background.green,
        background.alpha
    );
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, w, h);

    /* free resources */
    cairo_destroy(cr);

    return pixbuf;
}

/**
 * Draws the unread message count in the bottom right corner of the given image.
 * In the case that the count is less than or equal to 0, nothing is drawn.
 */
GdkPixbuf *
draw_unread_messages(const GdkPixbuf *avatar, int unread_count) {
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
    char *text = g_strdup_printf("%s", unread_count > 9 ? "9+" : std::to_string(unread_count).c_str());
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
    double rec_y = 0;
    double rec_w = extents.width + border_width * 2;
    double rec_h = extents.height + border_width * 2;
    double corner_radius = rec_h/2.5;
    create_rounded_rectangle_path(cr, corner_radius, rec_x, rec_y, rec_w, rec_h);
    cairo_set_source_rgba(cr,
                          MSG_COUNT_BACKGROUND.red,
                          MSG_COUNT_BACKGROUND.blue, MSG_COUNT_BACKGROUND.green, MSG_COUNT_BACKGROUND.alpha);
    cairo_fill(cr);

    /* draw text */
    cairo_move_to (cr, w - extents.width-border_width, extents.height + border_width );
    cairo_set_source_rgb(cr, MSG_COUNT_FONT_COLOUR.red, MSG_COUNT_FONT_COLOUR.blue, MSG_COUNT_FONT_COLOUR.green);
    cairo_show_text (cr, text);

    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, w, h);

    /* free resources */
    cairo_destroy(cr);
    g_free(text);

    return pixbuf;
}

GdkRGBA
get_ambient_color(GtkWidget* widget)
{
    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,4,4);
    auto cr = cairo_create(surface);
    if (widget) {
        auto context = gtk_widget_get_style_context(widget);
        gtk_render_background(context, cr, 0, 0, 1, 1);
        cairo_surface_flush(surface);
        auto content = cairo_image_surface_get_data(surface);
        if(content[3] == 255) {
            auto ret = GdkRGBA {
                content[2]/255.0f,
                content[1]/255.0f,
                content[0]/255.0f,
                content[3]/255.0f
            };
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
            return ret;
        }
        widget=gtk_widget_get_parent(GTK_WIDGET(widget));
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return GdkRGBA{1.0f, 1.0f, 1.0f, 1.0f};
}

gboolean
use_dark_theme(GdkRGBA color)
{
    return (color.red + color.green + color.blue) / 3 < .5;
}
