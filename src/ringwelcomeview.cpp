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

#include "ringwelcomeview.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "utils/accounts.h"
#include <account.h>
#include <accountmodel.h>
#include <qrencode.h>

struct _RingWelcomeView
{
    GtkScrolledWindow parent;
};

struct _RingWelcomeViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _RingWelcomeViewPrivate RingWelcomeViewPrivate;

struct _RingWelcomeViewPrivate
{
    QMetaObject::Connection ringaccount_updated;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingWelcomeView, ring_welcome_view, GTK_TYPE_SCROLLED_WINDOW);

#define RING_WELCOME_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_WELCOME_VIEW_TYPE, RingWelcomeViewPrivate))

static gboolean   draw_qrcode(GtkWidget*,cairo_t*,gpointer);
static void       switch_qrcode(GtkWidget* qrcode);

static void
show_ring_id(GtkLabel *label, Account *account) {
    g_return_if_fail(label);

    gchar *text = nullptr;
    if (account) {
        if (!account->username().isEmpty()) {
            text = g_markup_printf_escaped("<span fgcolor=\"black\">%s</span>",
                                           account->username().toUtf8().constData());
        } else {
            g_warning("got ring account, but Ring id is empty");
            text = g_markup_printf_escaped("<span fgcolor=\"gray\">%s</span>",
                                           _("fetching RingID..."));
        }
    }
    gtk_label_set_markup(label, text);
    g_free(text);
}

static void
ring_welcome_view_init(RingWelcomeView *self)
{
    RingWelcomeViewPrivate *priv = RING_WELCOME_VIEW_GET_PRIVATE(self);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(self), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    auto box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_add(GTK_CONTAINER(self), box);
    gtk_box_set_baseline_position(GTK_BOX(box), GTK_BASELINE_POSITION_CENTER);
    gtk_widget_set_vexpand(GTK_WIDGET(box), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(box), FALSE);
    gtk_widget_set_valign(GTK_WIDGET(box), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(box), GTK_ALIGN_CENTER);
    gtk_widget_set_visible(GTK_WIDGET(box), TRUE);

    /* get logo */
    GError *error = NULL;
    GdkPixbuf* logo = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-logo-blue",
                                                            350, -1, TRUE, &error);
    if (logo == NULL) {
        g_debug("Could not load logo: %s", error->message);
        g_clear_error(&error);
    } else {
        auto image_ring_logo = gtk_image_new_from_pixbuf(logo);
        gtk_box_pack_start(GTK_BOX(box), image_ring_logo, FALSE, TRUE, 0);
        gtk_widget_set_visible(GTK_WIDGET(image_ring_logo), TRUE);
    }

    /* welcome text */
    auto label_welcome_text = gtk_label_new(_("Ring is a secure and distributed voice, video, and chat communication platform that requires no centralized server and leaves the power of privacy in the hands of the user."));
    gtk_label_set_justify(GTK_LABEL(label_welcome_text), GTK_JUSTIFY_CENTER);
    gtk_widget_override_font(label_welcome_text, pango_font_description_from_string("12"));
    gtk_label_set_line_wrap(GTK_LABEL(label_welcome_text), TRUE);
    /* the max width chars is to limit how much the text expands */
    gtk_label_set_max_width_chars(GTK_LABEL(label_welcome_text), 50);
    gtk_label_set_selectable(GTK_LABEL(label_welcome_text), TRUE);
    gtk_box_pack_start(GTK_BOX(box), label_welcome_text, FALSE, TRUE, 0);
    gtk_widget_set_visible(GTK_WIDGET(label_welcome_text), TRUE);

    /* RingID explanation */
    auto label_explanation = gtk_label_new(C_("Do not translate \"RingID\"", "This is your RingID.\nCopy and share it with your friends!"));
    auto context = gtk_widget_get_style_context(label_explanation);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_DIM_LABEL);
    gtk_label_set_justify(GTK_LABEL(label_explanation), GTK_JUSTIFY_CENTER);
    gtk_label_set_selectable(GTK_LABEL(label_explanation), TRUE);
    gtk_widget_set_margin_top(label_explanation, 20);
    /* we migth need to hide the label if a RING account doesn't exist */
    gtk_widget_set_no_show_all(label_explanation, TRUE);
    gtk_box_pack_start(GTK_BOX(box), label_explanation, FALSE, TRUE, 0);

    /* RingID label */
    auto label_ringid = gtk_label_new(NULL);
    gtk_label_set_selectable(GTK_LABEL(label_ringid), TRUE);
    gtk_widget_override_font(label_ringid, pango_font_description_from_string("monospace 12"));
    show_ring_id(GTK_LABEL(label_ringid), get_active_ring_account());
    gtk_widget_set_no_show_all(label_ringid, TRUE);
    gtk_box_pack_start(GTK_BOX(box), label_ringid, FALSE, TRUE, 0);
    gtk_label_set_ellipsize(GTK_LABEL(label_ringid), PANGO_ELLIPSIZE_END);

    /* QR drawing area */
    auto qr_ringid = gtk_drawing_area_new();
    auto qrsize = 200;
    gtk_widget_set_size_request (qr_ringid, qrsize, qrsize);
    gtk_widget_set_halign(qr_ringid, GTK_ALIGN_CENTER);
    g_signal_connect(qr_ringid, "draw", G_CALLBACK(draw_qrcode), NULL);
    gtk_widget_set_visible(qr_ringid, FALSE);

    /* QR code button */
    auto rcode_button = gtk_button_new_with_label("QR code");
    gtk_widget_set_hexpand(rcode_button, FALSE);
    gtk_widget_set_size_request(rcode_button,10,10);
    g_signal_connect_swapped(rcode_button, "clicked", G_CALLBACK(switch_qrcode), qr_ringid);
    gtk_widget_set_visible(rcode_button, TRUE);

    gtk_box_pack_start(GTK_BOX(box), rcode_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), qr_ringid, FALSE, TRUE, 0);

    if (get_active_ring_account()) {
        gtk_widget_show(label_explanation);
        gtk_widget_show(label_ringid);
    }

    priv-> ringaccount_updated = QObject::connect(
        &AccountModel::instance(),
        &AccountModel::dataChanged,
        [label_explanation, label_ringid] () {
            /* check if the active ring account has changed,
             * eg: if it was deleted */
            auto account = get_active_ring_account();
            show_ring_id(GTK_LABEL(label_ringid), get_active_ring_account());
            if (account) {
                gtk_widget_show(label_explanation);
                gtk_widget_show(label_ringid);

            } else {
                gtk_widget_hide(label_explanation);
                gtk_widget_hide(label_ringid);
            }
        }
    );

    gtk_widget_show(GTK_WIDGET(self));
}

static void
ring_welcome_view_dispose(GObject *object)
{
    RingWelcomeView *self = RING_WELCOME_VIEW(object);
    RingWelcomeViewPrivate *priv = RING_WELCOME_VIEW_GET_PRIVATE(self);

    QObject::disconnect(priv->ringaccount_updated);

    G_OBJECT_CLASS(ring_welcome_view_parent_class)->dispose(object);
}

static void
ring_welcome_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(ring_welcome_view_parent_class)->finalize(object);
}

static void
ring_welcome_view_class_init(RingWelcomeViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = ring_welcome_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = ring_welcome_view_dispose;
}

GtkWidget *
ring_welcome_view_new()
{
    gpointer self = g_object_new(RING_WELCOME_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}

static gboolean
draw_qrcode(G_GNUC_UNUSED GtkWidget* diese,
            cairo_t*   cr,
            G_GNUC_UNUSED gpointer   data)
{
    auto rcode = QRcode_encodeString(get_active_ring_account()->username().toStdString().c_str(),
                                      0, //Let the version be decided by libqrencode
                                      QR_ECLEVEL_L, // Lowest level of error correction
                                      QR_MODE_8, // 8-bit data mode
                                      1);

    if (!rcode) { // no rcode, no draw
        g_warning("Failed to generate QR code");
        return TRUE;
    }

    auto margin = 5;
    auto qrsize = 200;
    int qrwidth = rcode->width + margin * 2;

    /* scaling */
    auto scale = qrsize/qrwidth;
    cairo_scale(cr, scale, scale);

    /* fill the background in white */
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_rectangle(cr, 0, 0, qrwidth, qrwidth);
    cairo_fill (cr);

    unsigned char *row, *p;
    p = rcode->data;
    cairo_set_source_rgb(cr, 0, 0, 0); // back in black
    for(int y = 0; y < rcode->width; y++) {
        row = (p + (y * rcode->width));
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

static void
switch_qrcode(GtkWidget* qrcode)
{
    gtk_widget_set_visible(qrcode, !gtk_widget_get_visible(qrcode));
}
