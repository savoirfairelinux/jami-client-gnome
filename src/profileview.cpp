/*
 *  Copyright(C) 2015-2021 Savoir-faire Linux Inc.
 *  Author: Sebastien Blin <sebastien.blin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
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

#include "profileview.h"

#include "native/pixbufmanipulator.h"
#include "utils/drawing.h"

#include <QSize>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <api/account.h>
#include <api/contact.h>
#include <api/contactmodel.h>
#include <api/conversationmodel.h>
#include <globalinstances.h>

namespace { namespace details {
class CppImpl;
}}

struct _ProfileView
{
  GtkDialog parent;
};

typedef struct _ProfileViewPrivate ProfileViewPrivate;

struct _ProfileViewPrivate
{
    AccountInfoPointer const *accountInfo_ = nullptr;
    details::CppImpl* cpp {nullptr};

    GtkWidget* avatar;

    GtkWidget* grid_infos;
        GtkWidget* best_name_label;
        GtkWidget* username_label;
        GtkWidget* id_label;
        GtkWidget* qr_image;
        GtkWidget* is_swarm_label;
};

G_DEFINE_TYPE_WITH_PRIVATE(ProfileView, profile_view, GTK_TYPE_DIALOG)
#define PROFILE_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), PROFILE_VIEW_TYPE, _ProfileViewPrivate))

namespace { namespace details {
class CppImpl
{
public:
    explicit CppImpl();
    QString uid_;
};
CppImpl::CppImpl()
    : uid_("")
{}
}}

static void
profile_view_init(ProfileView *prefs)
{
    gtk_widget_init_template(GTK_WIDGET(prefs));
}

static void
profile_view_dispose(GObject *object)
{
    auto* priv = PROFILE_VIEW_GET_PRIVATE(object);
    delete priv->cpp;
    priv->cpp = nullptr;
    G_OBJECT_CLASS(profile_view_parent_class)->dispose(object);
}

static void
profile_view_class_init(ProfileViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = profile_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/net/jami/JamiGnome/profile.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ProfileView, avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ProfileView, grid_infos);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ProfileView, best_name_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ProfileView, username_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ProfileView, id_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ProfileView, qr_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), ProfileView, is_swarm_label);
}

static bool
build_view(ProfileView* view)
{
    g_return_val_if_fail(IS_PROFILE_VIEW(view), false);
    auto* priv = PROFILE_VIEW_GET_PRIVATE(view);
    g_return_val_if_fail(priv || !(*priv->accountInfo_), false);

    try
    {
        const auto convOpt = (*priv->accountInfo_)->conversationModel->getConversationForUid(priv->cpp->uid_);
        auto contacts = (*priv->accountInfo_)->conversationModel->peersForConversation(priv->cpp->uid_);
        if (!convOpt || contacts.empty()) return false;
        const auto& contact = (*priv->accountInfo_)->contactModel->getContact(contacts.front());

        auto alias = contact.profileInfo.alias;
        alias.remove('\r');
        alias.remove('\n');
        if (alias.isEmpty()) alias = contact.registeredName;
        if (alias.isEmpty()) alias = contact.profileInfo.uri;
        gtk_label_set_text(GTK_LABEL(priv->best_name_label), qUtf8Printable(alias));
        GtkStyleContext* context;
        context = gtk_widget_get_style_context(GTK_WIDGET(priv->best_name_label));
        gtk_style_context_add_class(context, "bestname");

        if (contact.registeredName.isEmpty()) {
            gtk_label_set_text(GTK_LABEL(priv->username_label), _("(None)"));
            context = gtk_widget_get_style_context(GTK_WIDGET(priv->username_label));
            gtk_style_context_add_class(context, "empty");
            gtk_label_set_selectable(GTK_LABEL(priv->username_label), false);
        } else {
            gtk_label_set_text(GTK_LABEL(priv->username_label), qUtf8Printable(contact.registeredName));
        }
        gtk_label_set_text(GTK_LABEL(priv->id_label), qUtf8Printable(contact.profileInfo.uri));

        uint32_t img_size = 128;
        std::shared_ptr<GdkPixbuf> image;
        auto var_photo = GlobalInstances::pixmapManipulator().conversationPhoto(
            *convOpt,
            **(priv->accountInfo_),
            QSize(img_size, img_size),
            false
        );
        image = var_photo.value<std::shared_ptr<GdkPixbuf>>();
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->avatar), image.get());

        auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, img_size, img_size);
        auto* cr = cairo_create(surface);
        if (draw_qrcode(cr, contact.profileInfo.uri.toStdString(), img_size)) {
            GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(cairo_get_target(cr), 0, 0, img_size, img_size);
            gtk_image_set_from_pixbuf(GTK_IMAGE(priv->qr_image), pixbuf);
        }

        g_free(surface);
        g_free(cr);

        gtk_label_set_text(GTK_LABEL(priv->is_swarm_label), convOpt->get().isSwarm() ? _("Yes") : _("No"));

        gtk_window_set_title(GTK_WINDOW(view), std::string("Profile - " + alias.toStdString()).c_str());
        gtk_window_set_modal(GTK_WINDOW(view), false);
    }
    catch (...)
    {
        g_warning("Can't get conversation %s", priv->cpp? priv->cpp->uid_.toStdString().c_str() : "");
        return false;
    }
    return true;
}

GtkWidget*
profile_view_new(AccountInfoPointer const & accountInfo, const QString& uid)
{
    gpointer view = g_object_new(PROFILE_VIEW_TYPE, NULL);
    auto* priv = PROFILE_VIEW_GET_PRIVATE(view);
    priv->accountInfo_ = &accountInfo;
    priv->cpp = new details::CppImpl();
    priv->cpp->uid_ = uid;
    if (!build_view(PROFILE_VIEW(view))) return nullptr;

    auto provider = gtk_css_provider_new();
    std::string css = ".bestname { font-size: 3em; font-weight: 100; }";
    css += ".section_title { font-size: 1.2em; font-weight: bold; }";
    css += ".sub_section_title { font-size: 1.2em; opacity: 0.7; }";
    css += ".value { font-size: 1.2em; }";
    css += ".empty { font-size: 1.2em; font-style: italic; opacity: 0.7; }";
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    return static_cast<GtkWidget*>(view);
}
