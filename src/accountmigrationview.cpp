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
#include "usernameregistrationbox.h"
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
    QMetaObject::Connection state_changed;
    guint timeout_tag;

    /* main_view */
    GtkWidget *main_view;
    GtkWidget *label_account_alias;
    GtkWidget *label_account_ringid;
    GtkWidget *image_avatar;
    GtkWidget *label_migration_error;
    GtkWidget *entry_password;
    GtkWidget *button_migrate_account;
    GtkWidget *spinner_migrating;
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
    g_warning(" &14& ");
    auto priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(object);

    // make sure to disconnect from all signals when disposing of view
    QObject::disconnect(priv->state_changed);

    if (priv->timeout_tag) {
        g_source_remove(priv->timeout_tag);
        priv->timeout_tag = 0;
    }
    G_OBJECT_CLASS(account_migration_view_parent_class)->dispose(object);
}

static void
account_migration_view_init(AccountMigrationView *view)
{
    g_warning(" &13& ");
    gtk_widget_init_template(GTK_WIDGET(view));
    
    //~ AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    //~ gtk_widget_hide(priv->spinner_migrating);
    
}

static void
account_migration_view_class_init(AccountMigrationViewClass *klass)
{
    g_warning(" &12& ");
    G_OBJECT_CLASS(klass)->dispose = account_migration_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountmigrationview.ui");
    g_warning(" &B1& ");
    /* main_view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, main_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, label_account_alias);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, label_account_ringid);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, image_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, label_migration_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, entry_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, button_migrate_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountMigrationView, spinner_migrating);

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

static gboolean
migration_timeout(AccountMigrationView *view)
{
    g_warning(" &11& ");
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    g_warning("timeout reached while migrating account %s", priv->account->id().constData());
    QObject::disconnect(priv->state_changed);
    priv->timeout_tag = 0;
    // XXX voir ce qui doit etre fait en cas de timeout
    return G_SOURCE_REMOVE;
}

/* XXX ancienement quand on cliquait sur le button, voir si la logique fonctionne...
static void
button_error_view_ok_clicked(G_GNUC_UNUSED GtkButton* button, AccountMigrationView *view)
{
    g_warning(" &10& ");
    g_signal_emit(G_OBJECT(view), account_migration_view_signals[ACCOUNT_MIGRATION_FAILED], 0);
}
*/

static void
migrate(AccountMigrationView *view)
{
    g_warning(" &8& ");
    g_warning("C : %d", view);
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    
    gtk_widget_set_sensitive(priv->button_migrate_account, FALSE);
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password));

    // Timeout in 30 seconds
    priv->timeout_tag = g_timeout_add_full(G_PRIORITY_DEFAULT, 30000, (GSourceFunc)migration_timeout, view, NULL);

    priv->state_changed = QObject::connect(priv->account, &Account::migrationEnded,
        [=] (const QString& accountId, const Account::MigrationEndedStatus state)
        {
            g_warning("message recu");
            switch(state)
            {
            case Account::MigrationEndedStatus::SUCCESS:
            //account_migration_completed(view);
            g_signal_emit(G_OBJECT(view), account_migration_view_signals[ACCOUNT_MIGRATION_COMPLETED], 0);
            break;
            case Account::MigrationEndedStatus::INVALID:
            g_warning("E R R E U R");
            g_signal_emit(G_OBJECT(view), account_migration_view_signals[ACCOUNT_MIGRATION_FAILED], 0);
            break;
            }
        });

    priv->account->setArchivePassword(password);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password), "");
    priv->account->performAction(Account::EditAction::SAVE);
    g_warning(" &FD& ");

}


static void
migrate_account_clicked(G_GNUC_UNUSED GtkButton* button, AccountMigrationView *view)
{
    g_warning("C : %d", view);
    migrate(view);
}

static void
password_entry_activate(G_GNUC_UNUSED GtkEntry* entry, AccountMigrationView *view)
{
    g_warning("C : %d", view);
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    gtk_widget_show(priv->spinner_migrating);
    migrate(view);
}

static void
password_entry_changed(G_GNUC_UNUSED GtkEntry* entry, AccountMigrationView *view)
{
    g_warning(" &5& ");
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);

    const gchar *password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password));

    if (strlen(password) > 0)
    {
        gtk_widget_set_sensitive(priv->button_migrate_account, TRUE);
    }
    else
    {
        gtk_widget_set_sensitive(priv->button_migrate_account, FALSE);
    }
}

static void
account_migration_completed(AccountMigrationView *view)
{
    g_warning(" &4& ");
    g_warning(" TOTO ");
    /* This check is needed because users may close the migration view while
     * the registration is still ongoing.
     */
    g_return_if_fail(IS_ACCOUNT_MIGRATION_VIEW(view));

    g_signal_emit(G_OBJECT(view), account_migration_view_signals[ACCOUNT_MIGRATION_COMPLETED], 0);
}

static void
button_register_username_later_clicked(G_GNUC_UNUSED GtkButton* button, AccountMigrationView *view)
{
    g_warning(" &3& ");
    account_migration_completed(view);
}

static void
build_migration_view(AccountMigrationView *view)
{
    g_warning(" &2& ");
    g_return_if_fail(IS_ACCOUNT_MIGRATION_VIEW(view));
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);

    g_signal_connect(priv->button_migrate_account, "clicked", G_CALLBACK(migrate_account_clicked), view);
    g_signal_connect(priv->entry_password, "changed", G_CALLBACK(password_entry_changed), view);
    g_signal_connect(priv->entry_password, "activate", G_CALLBACK(migrate_account_clicked), view);

    gtk_label_set_text(GTK_LABEL(priv->label_account_alias), priv->account->alias().toUtf8().constData());
    // display the ringID (without "ring:")
    gtk_label_set_text(GTK_LABEL(priv->label_account_ringid), URI(priv->account->username()).userinfo().toUtf8().constData());

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
    g_warning(" &1& ");
    gpointer view = g_object_new(ACCOUNT_MIGRATION_VIEW_TYPE, NULL);
    AccountMigrationViewPrivate *priv = ACCOUNT_MIGRATION_VIEW_GET_PRIVATE(view);
    priv->account = account;
    build_migration_view(ACCOUNT_MIGRATION_VIEW(view));
    return (GtkWidget *)view;
}
