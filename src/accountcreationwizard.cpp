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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

// LRC
#include <account.h>
#include <codecmodel.h>
#include <profilemodel.h>
#include <profile.h>
#include <accountmodel.h>
#include <personmodel.h>

// Ring Client
#include "models/gtkqsortfiltertreemodel.h"
#include "utils/models.h"
#include "avatarmanipulation.h"
#include "accountcreationwizard.h"


struct _AccountCreationWizard
{
    GtkBox parent;
};

struct _AccountCreationWizardClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountCreationWizardPrivate AccountCreationWizardPrivate;

struct _AccountCreationWizardPrivate
{
    GtkWidget *stack_account_creation;
    QMetaObject::Connection state_changed;

    /* choose_account_type_vbox */
    GtkWidget *choose_account_type_vbox;
    GtkWidget *choose_account_type_ring_logo;
    GtkWidget *button_new_account;
    GtkWidget *button_existing_account;
    GtkWidget *button_wizard_cancel;

    /* existing account */
    GtkWidget *existing_account;
    GtkWidget *button_existing_account_next;
    GtkWidget *button_existing_account_previous;
    GtkWidget *entry_existing_account_pin;
    GtkWidget *entry_existing_account_password;
    GtkWidget *retrieving_account;

    /* account creation */
    GtkWidget *account_creation;
    GtkWidget *vbox_account_creation_entry;
    GtkWidget *entry_alias;
    GtkWidget *entry_password;
    GtkWidget *entry_password_confirm;
    GtkWidget *label_default_name;
    GtkWidget *label_paceholder;
    GtkWidget *button_account_creation_next;
    GtkWidget *button_account_creation_previous;
    GtkWidget *box_avatarselection;
    GtkWidget *avatar_manipulation;
    GtkWidget *label_password_error;

    /* generating_account_spinner */
    GtkWidget *vbox_generating_account_spinner;

    /* error_view */
    GtkWidget *error_view;
    GtkWidget *button_error_view_ok;

};

G_DEFINE_TYPE_WITH_PRIVATE(AccountCreationWizard, account_creation_wizard, GTK_TYPE_BOX);

#define ACCOUNT_CREATION_WIZARD_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_CREATION_WIZARD_TYPE, AccountCreationWizardPrivate))

/* signals */
enum {
    ACCOUNT_CREATION_CANCELED,
    ACCOUNT_CREATION_COMPLETED,
    LAST_SIGNAL
};

static guint account_creation_wizard_signals[LAST_SIGNAL] = { 0 };

static void
destroy_avatar_manipulation(AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    /* make sure the AvatarManipulation widget is destroyed so the VideoWidget inside of it is too;
     * NOTE: destorying its parent (box_avatarselection) first will cause a mystery 'BadDrawable'
     * crash due to X error */
    if (priv->avatar_manipulation)
    {
        gtk_container_remove(GTK_CONTAINER(priv->box_avatarselection), priv->avatar_manipulation);
        priv->avatar_manipulation = nullptr;
    }
}

static void
account_creation_wizard_dispose(GObject *object)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(object);
    destroy_avatar_manipulation(ACCOUNT_CREATION_WIZARD(object));
    QObject::disconnect(priv->state_changed);
    G_OBJECT_CLASS(account_creation_wizard_parent_class)->dispose(object);
}

static void
account_creation_wizard_init(AccountCreationWizard *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
account_creation_wizard_class_init(AccountCreationWizardClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_creation_wizard_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountcreationwizard.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, stack_account_creation);

    /* choose_account_type_vbox */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, choose_account_type_vbox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, choose_account_type_ring_logo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_new_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_existing_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_wizard_cancel);

    /* existing account */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, existing_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_existing_account_next);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_existing_account_previous);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_existing_account_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_existing_account_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_existing_account_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, retrieving_account);

    /* account creation */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, account_creation);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, vbox_account_creation_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_alias);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_password_confirm);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, label_default_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, label_paceholder);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_account_creation_next);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_account_creation_previous);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, box_avatarselection);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, label_password_error);

    /* generating_account_spinner */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, vbox_generating_account_spinner);

    /* error view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, error_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_error_view_ok);

    /* add signals */
    account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED] = g_signal_new("account-creation-completed",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

    account_creation_wizard_signals[ACCOUNT_CREATION_CANCELED] = g_signal_new("account-creation-canceled",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

static void
show_error_view(AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->error_view);
}

static gboolean
create_ring_account(AccountCreationWizard *view,
                    gchar *alias,
                    gchar *password,
                    gchar *pin)
{
    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(view), G_SOURCE_REMOVE);
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    g_object_ref(view); // ref so its not desroyed too early

    /* create account and set UPnP enabled, as its not by default in the daemon */
    Account *account = nullptr;

    /* get profile (if so) */
    auto profile = ProfileModel::instance().selectedProfile();

    if (alias && strlen(alias) > 0) {
        account = AccountModel::instance().add(alias, Account::Protocol::RING);
        if(profile && AccountModel::instance().size() == 1)
        {
            profile->person()->setFormattedName(alias);
        }
    } else {
        auto unknown_alias = C_("The default username / account alias, if none is set by the user", "Unknown");
        account = AccountModel::instance().add(unknown_alias, Account::Protocol::RING);
        if (profile && AccountModel::instance().size() == 1)
        {
            profile->person()->setFormattedName(unknown_alias);
        }
    }

    /* Set the archive password */
    account->setArchivePassword(password);

    /* Set the archive pin (existng accounts) */
    if(pin)
    {
        account->setArchivePin(pin);
    }

    account->setDisplayName(alias); // set the display name to the same as the alias

    account->setUpnpEnabled(TRUE);

    /* wait for hash to be generated to show the main view */
    priv->state_changed = QObject::connect(
        account,
        &Account::stateChanged,
        [=] (Account::RegistrationState state) {
            switch(state)
            {
                case Account::RegistrationState::READY:
                {
                    // Make sure that the account is ready to be displayed.
                    account << Account::EditAction::RELOAD;

                    g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED], 0);
                    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
                    QObject::disconnect(priv->state_changed); // only want to emit once
                    g_object_unref(view); // no longer needed
                    break;
                }
                case Account::RegistrationState::ERROR:
                {
                    //TODO: Possibly display diffrent error messages on ERROR_GENERIC
                    // and ERROR_NETWORK.
                    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
                    QObject::disconnect(priv->state_changed);
                    show_error_view(view);
                    g_object_unref(view); // decrement the ref
                    break;
                }
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

    account->performAction(Account::EditAction::SAVE);
    profile->save();

    return G_SOURCE_REMOVE;
}

static gboolean
create_new_ring_account(AccountCreationWizard *win)
{
    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(win), G_SOURCE_REMOVE);
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    gchar *alias = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_alias)));

    gchar *password = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_password)));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password), "");
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password_confirm), "");

    auto status = create_ring_account(win, alias, password, NULL);

    g_free(alias);
    g_free(password);

    return status;
}

static gboolean
create_existing_ring_account(AccountCreationWizard *win)
{
    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(win), G_SOURCE_REMOVE);
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    gchar *password = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_existing_account_password)));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_existing_account_password), "");

    gchar *pin = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_existing_account_pin)));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_existing_account_pin), "");

    auto status = create_ring_account(win, NULL, password, pin);

    g_free(password);
    g_free(pin);

    return status;
}


static void
alias_entry_changed(GtkEditable *entry, AccountCreationWizard *win)
{
    g_return_if_fail(IS_ACCOUNT_CREATION_WIZARD(win));
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    const gchar *alias = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!alias || strlen(alias) == 0) {
        gtk_widget_show(priv->label_default_name);
        gtk_widget_hide(priv->label_paceholder);
    } else {
        gtk_widget_hide(priv->label_default_name);
        gtk_widget_show(priv->label_paceholder);
    }
}

static void
show_generating_account_spinner(AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->vbox_generating_account_spinner);
}

static void
account_creation_next_clicked(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *win)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    /* Check for correct password */
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password));
    const gchar *password_confirm = gtk_entry_get_text(GTK_ENTRY(priv->entry_password_confirm));

    if (g_strcmp0(password, password_confirm) != 0)
    {
        gtk_widget_show(priv->label_password_error);
        return;
    }

    show_generating_account_spinner(win);

    /* now create account after a short timeout so that the the save doesn't
     * happen freeze the client before the widget changes happen;
     * the timeout function should then display the next step in account creation
     */
    g_timeout_add_full(G_PRIORITY_DEFAULT, 300, (GSourceFunc)create_new_ring_account, win, NULL);
}

static void
show_retrieving_account(AccountCreationWizard *win)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->retrieving_account);
}

static void
existing_account_next_clicked(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *win)
{
    show_retrieving_account(win);

    /* now create account after a short timeout so that the the save doesn't
     * happen freeze the client before the widget changes happen;
     * the timeout function should then display the next step in account creation
     */
    g_timeout_add_full(G_PRIORITY_DEFAULT, 300, (GSourceFunc)create_existing_ring_account, win, NULL);
}

static void
entry_alias_activated(GtkEntry *entry, AccountCreationWizard *win)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    const gchar *alias = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(alias) > 0)
        gtk_button_clicked(GTK_BUTTON(priv->button_account_creation_next));
}


static void
show_choose_account_type(AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->choose_account_type_vbox);
}

static void
show_existing_account(AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->existing_account);
}

static void
show_account_creation(AccountCreationWizard *win)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    /* avatar manipulation widget */
    if (!priv->avatar_manipulation)
    {
        priv->avatar_manipulation = avatar_manipulation_new_from_wizard();
        gtk_box_pack_start(GTK_BOX(priv->box_avatarselection), priv->avatar_manipulation, true, true, 0);
    }

    gtk_widget_hide(priv->label_password_error);

    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->account_creation);
}

static void
wizard_cancel_clicked(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *view)
{
    g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_CANCELED], 0);
}

static void
build_creation_wizard_view(AccountCreationWizard *view, bool show_cancel_button)
{
    g_return_if_fail(IS_ACCOUNT_CREATION_WIZARD(view));
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    /* set ring logo */
    GError *error = NULL;
    GdkPixbuf* logo_ring = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-logo-blue",
                                                                  -1, 50, TRUE, &error);
    if (logo_ring == NULL) {
        g_debug("Could not load logo: %s", error->message);
        g_clear_error(&error);
    } else
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->choose_account_type_ring_logo), logo_ring);


    /* use the real name / username of the logged in user as the default */
    const char* real_name = g_get_real_name();
    const char* user_name = g_get_user_name();
    g_debug("real_name = %s",real_name);
    g_debug("user_name = %s",user_name);

    /* check first if the real name was determined */
    if (g_strcmp0 (real_name,"Unknown") != 0)
        gtk_entry_set_text(GTK_ENTRY(priv->entry_alias), real_name);
    else
        gtk_entry_set_text(GTK_ENTRY(priv->entry_alias), user_name);

    /* cancel button */
    gtk_widget_set_visible(priv->button_wizard_cancel, show_cancel_button);

    /* choose_account_type signals */
    g_signal_connect_swapped(priv->button_new_account, "clicked", G_CALLBACK(show_account_creation), view);
    g_signal_connect_swapped(priv->button_existing_account, "clicked", G_CALLBACK(show_existing_account), view);
    g_signal_connect(priv->button_wizard_cancel, "clicked", G_CALLBACK(wizard_cancel_clicked), view);

    /* account_creation signals */
    g_signal_connect_swapped(priv->button_account_creation_previous, "clicked", G_CALLBACK(show_choose_account_type), view);
    g_signal_connect(priv->entry_alias, "changed", G_CALLBACK(alias_entry_changed), view);
    g_signal_connect(priv->button_account_creation_next, "clicked", G_CALLBACK(account_creation_next_clicked), view);
    g_signal_connect(priv->entry_alias, "activate", G_CALLBACK(entry_alias_activated), view);

    /* existing_account singals */
    g_signal_connect_swapped(priv->button_existing_account_previous, "clicked", G_CALLBACK(show_choose_account_type), view);
    g_signal_connect(priv->button_existing_account_next, "clicked", G_CALLBACK(existing_account_next_clicked), view);

    /* error_view signals */
    g_signal_connect_swapped(priv->button_error_view_ok, "clicked", G_CALLBACK(show_choose_account_type), view);

    show_choose_account_type(view);
}

GtkWidget *
account_creation_wizard_new(bool show_cancel_button)
{
    gpointer view = g_object_new(ACCOUNT_CREATION_WIZARD_TYPE, NULL);

    build_creation_wizard_view(ACCOUNT_CREATION_WIZARD(view), show_cancel_button);
    return (GtkWidget *)view;
}
