/*
 *  Copyright (C) 2015-2017 Savoir-faire Linux Inc.
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

// Qt
#include <QObject>
#include <QItemSelectionModel>

// LRC
#include <availableaccountmodel.h>

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
    GtkWidget *box_overlay;
    GtkWidget *label_ringid;
    GtkWidget *label_explanation;
    GtkWidget *button_qrcode;
    GtkWidget *revealer_qrcode;

    QMetaObject::Connection account_model_data_changed;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingWelcomeView, ring_welcome_view, GTK_TYPE_SCROLLED_WINDOW);

#define RING_WELCOME_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_WELCOME_VIEW_TYPE, RingWelcomeViewPrivate))

static gboolean   draw_qrcode(GtkWidget*,cairo_t*,gpointer);
static void       switch_qrcode(RingWelcomeView* self);

static void
update_view(RingWelcomeView *self) {
    auto priv = RING_WELCOME_VIEW_GET_PRIVATE(self);

    if (auto account = get_active_ring_account()) {
        gchar *ring_id = nullptr;
        if(!account->registeredName().isEmpty()){
            gtk_label_set_text(
                GTK_LABEL(priv->label_explanation),
                _("This is your Ring username.\nCopy and share it with your friends!")
            );
            ring_id = g_markup_printf_escaped("<span fgcolor=\"black\">ring:%s</span>",
                                              account->registeredName().toUtf8().constData());
        }
        else if (!account->username().isEmpty()) {
            gtk_label_set_text(
                GTK_LABEL(priv->label_explanation),
                C_("Do not translate \"RingID\"", "This is your RingID.\nCopy and share it with your friends!")
            );
            ring_id = g_markup_printf_escaped("<span fgcolor=\"black\">%s</span>",
                                              account->username().toUtf8().constData());
        } else {
            gtk_label_set_text(GTK_LABEL(priv->label_explanation), NULL);
            ring_id = g_markup_printf_escaped("<span fgcolor=\"gray\">%s</span>",
                                              _("fetching RingID..."));
        }
        gtk_label_set_markup(GTK_LABEL(priv->label_ringid), ring_id);
        gtk_widget_show(priv->label_explanation);
        gtk_widget_show(priv->label_ringid);
        gtk_widget_show(priv->button_qrcode);
        gtk_widget_show(priv->revealer_qrcode);

        g_free(ring_id);

    } else { // any other protocols
        gtk_widget_hide(priv->label_explanation);
        gtk_widget_hide(priv->label_ringid);
        gtk_widget_hide(priv->button_qrcode);
        gtk_widget_hide(priv->revealer_qrcode);
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->revealer_qrcode), FALSE);
        gtk_widget_set_opacity(priv->box_overlay, 1.0);
    }
}

static void
ring_welcome_view_init(RingWelcomeView *self)
{
    RingWelcomeViewPrivate *priv = RING_WELCOME_VIEW_GET_PRIVATE(self);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(self), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    auto box_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_add(GTK_CONTAINER(self), box_main);
    gtk_box_set_baseline_position(GTK_BOX(box_main), GTK_BASELINE_POSITION_CENTER);
    gtk_widget_set_vexpand(GTK_WIDGET(box_main), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(box_main), FALSE);
    gtk_widget_set_valign(GTK_WIDGET(box_main), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(box_main), GTK_ALIGN_CENTER);
    gtk_widget_set_visible(GTK_WIDGET(box_main), TRUE);

    /* overlay to show hide the qr code over the stuff in it */
    auto overlay_qrcode = gtk_overlay_new();
    gtk_box_pack_start(GTK_BOX(box_main), overlay_qrcode, FALSE, TRUE, 0);

    /* box which will be in the overaly so that we can display the QR code over the stuff in this box */
    priv->box_overlay = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_add(GTK_CONTAINER(overlay_qrcode), priv->box_overlay);

    /* get logo */
    GError *error = NULL;
    GdkPixbuf* logo = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-logo-blue",
                                                            350, -1, TRUE, &error);
    if (logo == NULL) {
        g_debug("Could not load logo: %s", error->message);
        g_clear_error(&error);
    } else {
        auto image_ring_logo = gtk_image_new_from_pixbuf(logo);
        gtk_box_pack_start(GTK_BOX(priv->box_overlay), image_ring_logo, FALSE, TRUE, 0);
        gtk_widget_set_visible(GTK_WIDGET(image_ring_logo), TRUE);
    }

    /* welcome text */
    auto label_welcome_text = gtk_label_new(_("Ring is free software for universal communication which respects the freedoms and privacy of its users."));
    gtk_label_set_justify(GTK_LABEL(label_welcome_text), GTK_JUSTIFY_CENTER);
    PangoAttrList *attrs_welcome_text = pango_attr_list_new();
    PangoAttribute *font_desc_welcome_text = pango_attr_font_desc_new(pango_font_description_from_string("12"));
    pango_attr_list_insert(attrs_welcome_text, font_desc_welcome_text);
    gtk_label_set_attributes(GTK_LABEL(label_welcome_text), attrs_welcome_text);
    pango_attr_list_unref(attrs_welcome_text);
    gtk_label_set_line_wrap(GTK_LABEL(label_welcome_text), TRUE);
    /* the max width chars is to limit how much the text expands */
    gtk_label_set_max_width_chars(GTK_LABEL(label_welcome_text), 50);
    gtk_label_set_selectable(GTK_LABEL(label_welcome_text), TRUE);
    gtk_box_pack_start(GTK_BOX(priv->box_overlay), label_welcome_text, FALSE, TRUE, 0);
    gtk_widget_set_visible(GTK_WIDGET(label_welcome_text), TRUE);

    /* RingID explanation */
    priv->label_explanation = gtk_label_new(NULL);
    auto context = gtk_widget_get_style_context(priv->label_explanation);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_DIM_LABEL);
    gtk_label_set_justify(GTK_LABEL(priv->label_explanation), GTK_JUSTIFY_CENTER);
    gtk_label_set_selectable(GTK_LABEL(priv->label_explanation), TRUE);
    gtk_widget_set_margin_top(priv->label_explanation, 20);
    gtk_widget_set_no_show_all(priv->label_explanation, TRUE);
    gtk_box_pack_start(GTK_BOX(priv->box_overlay), priv->label_explanation, FALSE, TRUE, 0);

    /* RingID label */
    priv->label_ringid = gtk_label_new(NULL);
    gtk_label_set_selectable(GTK_LABEL(priv->label_ringid), TRUE);
    PangoAttrList *attrs_ringid = pango_attr_list_new();
    PangoAttribute *font_desc_ringid = pango_attr_font_desc_new(pango_font_description_from_string("monospace 12"));
    pango_attr_list_insert(attrs_ringid, font_desc_ringid);
    gtk_label_set_attributes(GTK_LABEL(priv->label_ringid), attrs_ringid);
    pango_attr_list_unref(attrs_ringid);
    gtk_widget_set_no_show_all(priv->label_ringid, TRUE);
    gtk_box_pack_start(GTK_BOX(box_main), priv->label_ringid, FALSE, TRUE, 0);
    gtk_label_set_ellipsize(GTK_LABEL(priv->label_ringid), PANGO_ELLIPSIZE_END);

    /* QR drawing area */
    auto drawingarea_qrcode = gtk_drawing_area_new();
    auto qrsize = 200;
    gtk_widget_set_size_request(drawingarea_qrcode, qrsize, qrsize);
    g_signal_connect(drawingarea_qrcode, "draw", G_CALLBACK(draw_qrcode), NULL);
    gtk_widget_set_visible(drawingarea_qrcode, TRUE);

    /* revealer which will show the qr code */
    priv->revealer_qrcode = gtk_revealer_new();
    gtk_container_add(GTK_CONTAINER(priv->revealer_qrcode), drawingarea_qrcode);
    gtk_revealer_set_transition_type(GTK_REVEALER(priv->revealer_qrcode), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE); //GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_widget_set_halign(priv->revealer_qrcode, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(priv->revealer_qrcode, GTK_ALIGN_CENTER);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay_qrcode), priv->revealer_qrcode);
    gtk_widget_set_no_show_all(priv->revealer_qrcode, TRUE);
    gtk_widget_set_visible(priv->revealer_qrcode, FALSE);

    /* QR code button */
    priv->button_qrcode = gtk_button_new_with_label(C_("Do not translate \"Ring ID\"", "Ring ID QR code"));
    gtk_widget_set_hexpand(priv->button_qrcode, FALSE);
    gtk_widget_set_size_request(priv->button_qrcode,10,10);
    g_signal_connect_swapped(priv->button_qrcode, "clicked", G_CALLBACK(switch_qrcode), self);
    gtk_widget_set_no_show_all(priv->button_qrcode, TRUE);
    gtk_box_pack_start(GTK_BOX(box_main), priv->button_qrcode, TRUE, TRUE, 0);

    update_view(self);
    priv->account_model_data_changed = QObject::connect(
        AvailableAccountModel::instance().selectionModel(),
        &QItemSelectionModel::currentChanged,
        [self] ()
        {
            update_view(self);
        });

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
ring_welcome_view_dispose(GObject *object)
{
    RingWelcomeView *self = RING_WELCOME_VIEW(object);
    RingWelcomeViewPrivate *priv = RING_WELCOME_VIEW_GET_PRIVATE(self);

    QObject::disconnect(priv->account_model_data_changed);

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
    auto account = get_active_ring_account();
    if (!account)
        return TRUE;

    auto rcode = QRcode_encodeString(account->username().toStdString().c_str(),
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
switch_qrcode(RingWelcomeView* self)
{
    auto priv = RING_WELCOME_VIEW_GET_PRIVATE(self);

    auto to_reveal = !gtk_revealer_get_reveal_child(GTK_REVEALER(priv->revealer_qrcode));
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->revealer_qrcode), to_reveal);
    gtk_widget_set_opacity(priv->box_overlay, to_reveal ? 0.25 : 1.0);
}
