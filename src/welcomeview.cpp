/*
 *  Copyright (C) 2015-2020 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "welcomeview.h"
#include "utils/drawing.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <api/newaccountmodel.h>

// Qt
#include <QObject>
#include <QItemSelectionModel>

struct _WelcomeView
{
    GtkScrolledWindow parent;
};

struct _WelcomeViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _WelcomeViewPrivate WelcomeViewPrivate;

struct _WelcomeViewPrivate
{
    GtkWidget *box_overlay;
    GtkWidget *label_explanation;
    GtkWidget *hbox_idlayout;
    GtkWidget *label_ringid;
    GtkWidget *image_logo;
    GtkWidget *button_qrcode;
    GtkWidget *revealer_qrcode;

    bool useDarkTheme {false};

    AccountInfoPointer const *accountInfo_;
    QMetaObject::Connection nameRegistrationEnded_;

};

G_DEFINE_TYPE_WITH_PRIVATE(WelcomeView, welcome_view, GTK_TYPE_SCROLLED_WINDOW);

#define WELCOME_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), WELCOME_VIEW_TYPE, WelcomeViewPrivate))

static gboolean   draw_qr_event(GtkWidget*,cairo_t*,WelcomeView*);
static void       switch_qrcode(WelcomeView* self);

void
welcome_update_view(WelcomeView* self) {
    auto priv = WELCOME_VIEW_GET_PRIVATE(self);

    // Only draw a basic view for SIP accounts
    if (not *priv->accountInfo_ || (*priv->accountInfo_)->profileInfo.type == lrc::api::profile::Type::SIP) {
        gtk_widget_hide(priv->button_qrcode);
        gtk_widget_hide(priv->hbox_idlayout);
        gtk_widget_hide(priv->label_ringid);
        gtk_widget_hide(priv->label_explanation);
        gtk_widget_hide(priv->revealer_qrcode);
        gtk_widget_set_opacity(priv->box_overlay, 1.0);
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->revealer_qrcode), FALSE);
        return;
    }

    auto color = priv->useDarkTheme? "white" : "black";

    // Get registeredName, else the ID
    gchar *id = nullptr;
    if(! (*priv->accountInfo_)->registeredName.isEmpty()){
        gtk_label_set_text(
            GTK_LABEL(priv->label_explanation),
            _("This is your Jami username.\nCopy and share it with your friends!")
        );
        id = g_markup_printf_escaped("<span fgcolor=\"%s\">%s</span>", color,
                                          qUtf8Printable((*priv->accountInfo_)->registeredName));
    }
    else if (!(*priv->accountInfo_)->profileInfo.uri.isEmpty()) {
        gtk_label_set_text(
            GTK_LABEL(priv->label_explanation),
            _("This is your ID.\nCopy and share it with your friends!")
        );
        id = g_markup_printf_escaped("<span fgcolor=\"%s\">%s</span>", color,
                                          qUtf8Printable((*priv->accountInfo_)->profileInfo.uri));
    } else {
        gtk_label_set_text(GTK_LABEL(priv->label_explanation), NULL);
        id = g_strdup("");
    }

    gtk_label_set_markup(GTK_LABEL(priv->label_ringid), id);

    gtk_widget_show(priv->label_explanation);
    gtk_widget_show(priv->hbox_idlayout);
    gtk_widget_show(priv->label_ringid);
    gtk_widget_show(priv->button_qrcode);
    gtk_widget_show(priv->revealer_qrcode);

    g_free(id);


    GError *error = NULL;

    GdkPixbuf* logo = gdk_pixbuf_new_from_resource_at_scale(
        priv->useDarkTheme
            ? "/net/jami/JamiGnome/jami-logo-white"
            : "/net/jami/JamiGnome/jami-logo-blue",
        350, -1, TRUE, &error);
    if (logo == NULL) {
        g_debug("Could not load logo: %s", error->message);
        g_clear_error(&error);
    } else {
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_logo), logo);
    }

    GdkPixbuf *image_qr = gdk_pixbuf_new_from_resource_at_scale(priv->useDarkTheme? "/net/jami/JamiGnome/qrcode-white" : "/net/jami/JamiGnome/qrcode",
                                                                  -1, 16, TRUE, &error);
    if (!image_qr) {
        g_warning("Could not load icon: %s", error->message);
        g_clear_error(&error);
    } else {
        auto image = gtk_image_new_from_pixbuf(image_qr);
        gtk_button_set_image(GTK_BUTTON(priv->button_qrcode), image);
    }

    priv->nameRegistrationEnded_ = QObject::connect(
        (*priv->accountInfo_)->accountModel,
        &lrc::api::NewAccountModel::nameRegistrationEnded,
        [=] (const QString& accountId, lrc::api::account::RegisterNameStatus status, const QString& name) {
            if (not *priv->accountInfo_) return;
            if (accountId == (*priv->accountInfo_)->id
                && status == lrc::api::account::RegisterNameStatus::SUCCESS)
                {
                    gchar *markup = g_markup_printf_escaped("<span fgcolor=\"%s\">%s</span>", color,
                        qUtf8Printable(name));
                    gtk_label_set_markup(GTK_LABEL(priv->label_ringid), markup);
                    g_free(markup);
                }
        });
}

void
welcome_set_theme(WelcomeView* self, bool useDarkTheme)
{
    g_return_if_fail(IS_WELCOME_VIEW(self));
    auto* priv = WELCOME_VIEW_GET_PRIVATE(self);
    priv->useDarkTheme = useDarkTheme;
}


static void
welcome_view_init(WelcomeView *self)
{
    g_return_if_fail(IS_WELCOME_VIEW(self));
    auto* priv = WELCOME_VIEW_GET_PRIVATE(self);

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
    priv->image_logo = gtk_image_new();
    GError *error = NULL;
    GdkPixbuf* logo = gdk_pixbuf_new_from_resource_at_scale(
        priv->useDarkTheme
            ? "/net/jami/JamiGnome/jami-logo-white"
            : "/net/jami/JamiGnome/jami-logo-blue",
        350, -1, TRUE, &error);
    if (logo == NULL) {
        g_debug("Could not load logo: %s", error->message);
        g_clear_error(&error);
    } else {
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_logo), logo);
        gtk_box_pack_start(GTK_BOX(priv->box_overlay), GTK_WIDGET(priv->image_logo), FALSE, TRUE, 0);
        gtk_widget_set_visible(GTK_WIDGET(priv->image_logo), TRUE);
    }

    /* welcome text */
    auto label_welcome_text = gtk_label_new(_("Jami is free software for universal communication which respects the freedoms and privacy of its users."));
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

    /* ID explanation */
    priv->label_explanation = gtk_label_new(NULL);
    auto context = gtk_widget_get_style_context(priv->label_explanation);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_DIM_LABEL);
    gtk_label_set_justify(GTK_LABEL(priv->label_explanation), GTK_JUSTIFY_CENTER);
    gtk_label_set_selectable(GTK_LABEL(priv->label_explanation), TRUE);
    gtk_widget_set_margin_top(priv->label_explanation, 20);
    gtk_widget_set_no_show_all(priv->label_explanation, TRUE);
    gtk_box_pack_start(GTK_BOX(priv->box_overlay), priv->label_explanation, FALSE, TRUE, 0);

    priv->hbox_idlayout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_container_add(GTK_CONTAINER(self), priv->hbox_idlayout);
    gtk_box_set_baseline_position(GTK_BOX(priv->hbox_idlayout), GTK_BASELINE_POSITION_CENTER);
    gtk_widget_set_vexpand(GTK_WIDGET(priv->hbox_idlayout), FALSE);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->hbox_idlayout), FALSE);
    gtk_widget_set_valign(GTK_WIDGET(priv->hbox_idlayout), GTK_ALIGN_CENTER);
    gtk_widget_set_halign(GTK_WIDGET(priv->hbox_idlayout), GTK_ALIGN_CENTER);
    gtk_widget_set_visible(GTK_WIDGET(priv->hbox_idlayout), TRUE);

    /* ID label */
    priv->label_ringid = gtk_label_new(NULL);
    gtk_label_set_selectable(GTK_LABEL(priv->label_ringid), TRUE);
    PangoAttrList *attrs_ringid = pango_attr_list_new();
    PangoAttribute *font_desc_ringid = pango_attr_font_desc_new(pango_font_description_from_string("monospace 12"));
    pango_attr_list_insert(attrs_ringid, font_desc_ringid);
    gtk_label_set_attributes(GTK_LABEL(priv->label_ringid), attrs_ringid);
    pango_attr_list_unref(attrs_ringid);
    gtk_widget_set_no_show_all(priv->label_ringid, TRUE);
    gtk_box_pack_start(GTK_BOX(priv->hbox_idlayout), priv->label_ringid, FALSE, TRUE, 0);

    /* QR drawing area */
    auto drawingarea_qrcode = gtk_drawing_area_new();
    auto qrsize = 200;
    gtk_widget_set_size_request(drawingarea_qrcode, qrsize, qrsize);
    g_signal_connect(drawingarea_qrcode, "draw", G_CALLBACK(draw_qr_event), self);
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
    priv->button_qrcode = gtk_button_new();
    GdkPixbuf *image_qr = gdk_pixbuf_new_from_resource_at_scale(priv->useDarkTheme? "/net/jami/JamiGnome/qrcode-white" : "/net/jami/JamiGnome/qrcode",
                                                                  -1, 16, TRUE, &error);
    if (!image_qr) {
        g_warning("Could not load icon: %s", error->message);
        g_clear_error(&error);
    } else {
        auto image = gtk_image_new_from_pixbuf(image_qr);
        gtk_button_set_image(GTK_BUTTON(priv->button_qrcode), image);
    }
    gtk_widget_set_hexpand(priv->button_qrcode, FALSE);
    gtk_widget_set_size_request(priv->button_qrcode,10,10);
    g_signal_connect_swapped(priv->button_qrcode, "clicked", G_CALLBACK(switch_qrcode), self);
    gtk_widget_set_no_show_all(priv->button_qrcode, TRUE);
    gtk_box_pack_start(GTK_BOX(priv->hbox_idlayout), priv->button_qrcode, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(box_main), priv->hbox_idlayout, FALSE, TRUE, 0);
    gtk_label_set_ellipsize(GTK_LABEL(priv->label_ringid), PANGO_ELLIPSIZE_END);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
welcome_view_dispose(GObject *object)
{
    auto* priv = WELCOME_VIEW_GET_PRIVATE(object);
    QObject::disconnect(priv->nameRegistrationEnded_);
    G_OBJECT_CLASS(welcome_view_parent_class)->dispose(object);
}

static void
welcome_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(welcome_view_parent_class)->finalize(object);
}

static void
welcome_view_class_init(WelcomeViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = welcome_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = welcome_view_dispose;
}

GtkWidget *
welcome_view_new(AccountInfoPointer const & accountInfo)
{
    gpointer self = g_object_new(WELCOME_VIEW_TYPE, NULL);
    auto priv = WELCOME_VIEW_GET_PRIVATE(self);
    priv->accountInfo_ = &accountInfo;
    welcome_update_view(WELCOME_VIEW(self));

    return (GtkWidget *)self;
}


static gboolean
draw_qr_event(G_GNUC_UNUSED GtkWidget* diese,
            cairo_t*   cr,
            WelcomeView* self)
{
    auto priv = WELCOME_VIEW_GET_PRIVATE(self);
    g_return_val_if_fail(priv, false);
    return draw_qrcode(cr, qUtf8Printable((*priv->accountInfo_)->profileInfo.uri), 200);
}

static void
switch_qrcode(WelcomeView* self)
{
    auto priv = WELCOME_VIEW_GET_PRIVATE(self);

    auto to_reveal = !gtk_revealer_get_reveal_child(GTK_REVEALER(priv->revealer_qrcode));
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->revealer_qrcode), to_reveal);
    gtk_widget_set_opacity(priv->box_overlay, to_reveal ? 0.25 : 1.0);
}
