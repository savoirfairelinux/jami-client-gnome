/*
 *  Copyright (C) 2016-2021 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
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


// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>

// Qt
#include <QSize>

// std
#include <memory> // for std::shared_ptr

// LRC
#include <api/newaccountmodel.h>

// Jami  Client
#include "accountmigrationview.h"
#include "native/pixbufmanipulator.h"

/* size of avatar */
static constexpr int AVATAR_WIDTH  = 150; /* px */
static constexpr int AVATAR_HEIGHT = 150; /* px */

struct _AccountMigrationView
{
    GtkBox parent;
};

struct _AccountMigrationViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountMigrationViewPrivate AccountMigrationViewPrivate;

struct _AccountMigrationViewPrivate
{
    AccountInfoPointer const *accountInfo_;
    QMetaObject::Connection state_changed;

    /* main_view */
    GtkWidget *main_view;
    GtkWidget *label_account_alias;
    GtkWidget *label_account_username;
    GtkWidget *label_account_manager;
    GtkWidget *image_avatar;
    GtkWidget *label_migration_error;
    GtkWidget *entry_password;
    GtkWidget *button_migrate_account;
    GtkWidget *username_row;
    GtkWidget *manager_row;
    GtkWidget *button_delete_account;

    GtkWidget *hbox_migrating_account_spinner;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountMigrationView, account_migration_view, GTK_TYPE_BOX);

#define ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_MIGRATION_VIEW_TYPE, AccountMigrationViewPrivate))

/* signals */
enum {
    ACCOUNT_MIGRATION_COMPLETED,
    ACCOUNT_MIGRATION_FAILED,
    LAST_SIGNAL
};

static guint account_migration_view_signals[LAST_SIGNAL] = { 0 };

static void
account_migration_view_dispose(GObject *object)
{
    auto priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(object);

    // make sure to disconnect from all signals when disposing of view
    QObject::disconnect(priv->state_changed);

    G_OBJECT_CLASS(account_migration_view_parent_class)->dispose(object);
}

static void
account_migration_view_init(AccountMigrationView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
account_migration_view_class_init(AccountMigrationViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_migration_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/net/jami/JamiGnome/accountmigrationview.ui");
    /* main_view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, main_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, label_account_alias);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, label_account_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, label_account_manager);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, image_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, label_migration_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, entry_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, button_migrate_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, username_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, manager_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, button_delete_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, hbox_migrating_account_spinner);

    /* add signals */
    account_migration_view_signals[ACCOUNT_MIGRATION_COMPLETED] = g_signal_new("account-migration-completed",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

    account_migration_view_signals[ACCOUNT_MIGRATION_FAILED] = g_signal_new("account-migration-failed",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

static void
migrate(AccountMigrationView *view)
{
    g_return_if_fail(IS_ACCOUNT_MIGRATION_VIEW(view));
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password));
    priv->state_changed = QObject::connect((*priv->accountInfo_)->accountModel,
                                           &lrc::api::NewAccountModel::migrationEnded,
        [=] (const QString&, bool ok)
        {
            gtk_widget_hide(priv->hbox_migrating_account_spinner);
            gtk_widget_set_sensitive(GTK_WIDGET(priv->button_delete_account), true);
            gtk_widget_set_sensitive(GTK_WIDGET(priv->button_migrate_account), true);
            if (ok) {
                g_signal_emit(G_OBJECT(view), account_migration_view_signals[ACCOUNT_MIGRATION_COMPLETED], 0);
            } else {
                gtk_widget_show(priv->label_migration_error);
            }
        });

    auto currentProp = (*priv->accountInfo_)->accountModel->getAccountConfig((*priv->accountInfo_)->id);
    currentProp.archivePassword = password;
    (*priv->accountInfo_)->accountModel->setAccountConfig((*priv->accountInfo_)->id, currentProp);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password), "");

    gtk_widget_show_all(priv->hbox_migrating_account_spinner);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->button_delete_account), false);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->button_migrate_account), false);
}


static void
delete_account(AccountMigrationView *view)
{
    g_return_if_fail(IS_ACCOUNT_MIGRATION_VIEW(view));
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    auto* top_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
    auto* password_dialog = gtk_message_dialog_new(top_window,
        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
        _("Warning! This action will remove this account on this device!\nNote: this action cannot be undone. Also, your registered name can be lost!"));
    gtk_window_set_title(GTK_WINDOW(password_dialog), _("Delete account"));
    gtk_dialog_set_default_response(GTK_DIALOG(password_dialog), GTK_RESPONSE_OK);

    auto res = gtk_dialog_run(GTK_DIALOG(password_dialog));
    if (res == GTK_RESPONSE_OK) {
        (*priv->accountInfo_)->accountModel->removeAccount((*priv->accountInfo_)->id);
        g_signal_emit(G_OBJECT(view), account_migration_view_signals[ACCOUNT_MIGRATION_COMPLETED], 0);
    }

    gtk_widget_destroy(password_dialog);

}

static void
password_entry_changed(G_GNUC_UNUSED GtkEntry* entry, AccountMigrationView *view)
{
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    gtk_widget_hide(priv->label_migration_error);
}

static void
build_migration_view(AccountMigrationView *view)
{
    g_return_if_fail(IS_ACCOUNT_MIGRATION_VIEW(view));
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);

    g_signal_connect(priv->entry_password, "changed", G_CALLBACK(password_entry_changed), view);
    g_signal_connect_swapped(priv->button_migrate_account, "clicked", G_CALLBACK(migrate), view);
    g_signal_connect_swapped(priv->button_delete_account, "clicked", G_CALLBACK(delete_account), view);
    g_signal_connect_swapped(priv->entry_password, "activate", G_CALLBACK(migrate), view);

    gtk_label_set_text(GTK_LABEL(priv->label_account_alias), qUtf8Printable((*priv->accountInfo_)->profileInfo.alias));

    // display the ringID (without "ring:")
    g_debug("MIGRATE FOR %s", qUtf8Printable((*priv->accountInfo_)->id));
    auto username = (*priv->accountInfo_)->registeredName;
    try {
        auto conf = (*priv->accountInfo_)->accountModel->getAccountConfig((*priv->accountInfo_)->id);
        if (username.isEmpty() && !conf.managerUsername.isEmpty()) {
            username = conf.managerUsername;
        }
        gtk_label_set_text(GTK_LABEL(priv->label_account_username), qUtf8Printable(username));
        if (username.isEmpty()) {
            gtk_widget_hide(priv->username_row);
        }
        auto manager = conf.managerUri;
        gtk_label_set_text(GTK_LABEL(priv->label_account_manager), qUtf8Printable(manager));
        if (manager.isEmpty()) {
            gtk_widget_hide(priv->manager_row);
        }
    } catch (...) {
        gtk_widget_hide(priv->username_row);
        gtk_widget_hide(priv->manager_row);
    }

    /* get the current or default profile avatar */
    auto default_avatar = Interfaces::PixbufManipulator().generateAvatar("", "");
    auto default_scaled = Interfaces::PixbufManipulator().scaleAndFrame(default_avatar.get(), QSize(AVATAR_WIDTH, AVATAR_HEIGHT));
    auto photo = default_scaled;
    auto photostr = (*priv->accountInfo_)->profileInfo.avatar;
    if (!photostr.isEmpty()) {
        QByteArray byteArray = photostr.toUtf8();
        QVariant avatar = Interfaces::PixbufManipulator().personPhoto(byteArray);
        auto pixbuf_photo = Interfaces::PixbufManipulator().scaleAndFrame(avatar.value<std::shared_ptr<GdkPixbuf>>().get(), QSize(AVATAR_WIDTH, AVATAR_HEIGHT));
        if (avatar.isValid()) {
            photo = pixbuf_photo;
        }
    }
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_avatar), photo.get());

    // CSS styles
    auto provider = gtk_css_provider_new();
    std::string css = ".button_red { color: white; background: #dc3a37; border: 0; }";
    css += ".button_red:hover { background: #dc2719; }";
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    GtkStyleContext* context;
    context = gtk_widget_get_style_context(GTK_WIDGET(priv->button_delete_account));
    gtk_style_context_add_class(context, "button_red");

    gtk_widget_hide(priv->hbox_migrating_account_spinner);
}

GtkWidget *
account_migration_view_new(AccountInfoPointer const & accountInfo)
{
    gpointer view = g_object_new(ACCOUNT_MIGRATION_VIEW_TYPE, NULL);
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    priv->accountInfo_ = &accountInfo;
    build_migration_view(ACCOUNT_MIGRATION_VIEW(view));
    return (GtkWidget *)view;
}
