/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
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

// LRC
#include <codecmodel.h>
#include <account.h>
#include <accountmodel.h>
#include <profilemodel.h>
#include <profile.h>
#include <personmodel.h>
#include <globalinstances.h>

// Ring Client
#include "avatarmanipulation.h"
#include "accountmigrationview.h"
#include "models/gtkqsortfiltertreemodel.h"
#include "utils/models.h"
#include "native/pixbufmanipulator.h"
#include "video/video_widget.h"

/* size of avatar */
static constexpr int AVATAR_WIDTH  = 100; /* px */
static constexpr int AVATAR_HEIGHT = 100; /* px */

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
    Account   *account;
    GtkWidget *stack_account_migration;
    QMetaObject::Connection state_changed;

    /* generating_account_view */
    GtkWidget *migrating_account_view;

    /* main_view */
    GtkWidget *main_view;
    GtkWidget *label_account_alias;
    GtkWidget *label_account_id;
    GtkWidget *image_avatar;
    GtkWidget *label_password_error;
    GtkWidget *entry_password;
    GtkWidget *entry_password_confirm;
    GtkWidget *button_migrate_account;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountMigrationView, account_migration_view, GTK_TYPE_BOX);

#define ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_MIGRATION_VIEW_TYPE, AccountMigrationViewPrivate))

/* signals */
enum {
    ACCOUNT_MIGRATION_COMPLETED,
    LAST_SIGNAL
};

static guint account_migration_view_signals[LAST_SIGNAL] = { 0 };

static void
account_migration_view_dispose(GObject *object)
{
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
                                                "/cx/ring/RingGnome/accountmigrationview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, stack_account_migration);

    /* migrating_account_view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, migrating_account_view);

    /* main_view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, main_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, label_account_alias);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, label_account_id);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, image_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, label_password_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, entry_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, entry_password_confirm);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, button_migrate_account);

    /* add signals */
    account_migration_view_signals[ACCOUNT_MIGRATION_COMPLETED] = g_signal_new("account-migration-completed",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

static void
migrate_account_clicked(G_GNUC_UNUSED GtkButton* button, AccountMigrationView *view)
{
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    gtk_widget_hide(priv->label_password_error);

    /* Check for correct password */
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password));
    const gchar *password_confirm = gtk_entry_get_text(GTK_ENTRY(priv->entry_password_confirm));

    if (g_strcmp0(password, password_confirm) != 0)
    {
        gtk_widget_show(priv->label_password_error);
        return;
    }
    else
    {
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_migration), priv->migrating_account_view);

        priv->account->setArchivePassword(password);
        priv->account->performAction(Account::EditAction::SAVE);

        priv->state_changed = QObject::connect(
            priv->account,
            &Account::stateChanged,
            [=] (Account::RegistrationState state) {
                switch(state)
                {
                    case Account::RegistrationState::READY:
                    {
                        // Make sure that the account is ready to be displayed.
                        priv->account << Account::EditAction::RELOAD;

                        AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
                        QObject::disconnect(priv->state_changed); // only want to emit once
                        g_signal_emit(G_OBJECT(view), account_migration_view_signals[ACCOUNT_MIGRATION_COMPLETED], 0);
                        break;
                    }
                    case Account::RegistrationState::ERROR:
                    case Account::RegistrationState::UNREGISTERED:
                    case Account::RegistrationState::TRYING:
                    case Account::RegistrationState::COUNT__:
                    {
                        // Keep waiting...
                        break;
                    }
                }
            }
        );

    }
}

static void
build_migration_view(AccountMigrationView *view)
{
    g_return_if_fail(IS_ACCOUNT_MIGRATION_VIEW(view));
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);

    g_signal_connect(priv->button_migrate_account, "clicked", G_CALLBACK(migrate_account_clicked), view);

    gtk_label_set_text(GTK_LABEL(priv->label_account_alias), priv->account->alias().toUtf8().constData());
    gtk_label_set_text(GTK_LABEL(priv->label_account_id), priv->account->id().constData());

    /* set the avatar picture */
    auto photo = GlobalInstances::pixmapManipulator().contactPhoto(
                    ProfileModel::instance().selectedProfile()->person(),
                    QSize(AVATAR_WIDTH, AVATAR_HEIGHT),
                    false);
    std::shared_ptr<GdkPixbuf> pixbuf_photo = photo.value<std::shared_ptr<GdkPixbuf>>();

    if (photo.isValid()) {
        gtk_image_set_from_pixbuf (GTK_IMAGE(priv->image_avatar),  pixbuf_photo.get());
    } else {
        g_warning("invalid pixbuf");
    }
}

GtkWidget *
account_migration_view_new(Account* account)
{
    gpointer view = g_object_new(ACCOUNT_MIGRATION_VIEW_TYPE, NULL);
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    priv->account = account;
    build_migration_view(ACCOUNT_MIGRATION_VIEW(view));
    return (GtkWidget *)view;
}
