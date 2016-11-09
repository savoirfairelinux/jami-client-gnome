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
#include "utils/models.h"
#include "avatarmanipulation.h"
#include "accountcreationwizard.h"
#include "usernameregistrationbox.h"


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
    QMetaObject::Connection account_state_changed;
    QMetaObject::Connection name_registration_ended;
    gboolean username_available;

    QString* password;
    QString* username;

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
    GtkWidget *entry_username;
    GtkWidget *entry_password;
    GtkWidget *entry_password_confirm;
    GtkWidget *button_account_creation_next;
    GtkWidget *button_account_creation_previous;
    GtkWidget *box_avatarselection;
    GtkWidget *avatar_manipulation;
    GtkWidget *label_password_error;
    GtkWidget *box_username_entry;
    GtkWidget *checkbutton_sign_up_blockchain;
    GtkWidget *username_registration_box;
    GtkWidget *entry_display_name;

    /* generating_account_spinner */
    GtkWidget *vbox_generating_account_spinner;

    /* registering_username_spinner */
    GtkWidget *vbox_registering_username_spinner;

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
    QObject::disconnect(priv->account_state_changed);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_password_confirm);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_account_creation_next);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_account_creation_previous);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, box_avatarselection);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, label_password_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, box_username_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, checkbutton_sign_up_blockchain);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_display_name);

    /* generating_account_spinner */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, vbox_generating_account_spinner);

    /* registering_username_spinner */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, vbox_registering_username_spinner);

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

static void
could_not_register_username_dialog(AccountCreationWizard *view)
{
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view))),
                    GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    _("Your account was created, but we could not register your username. Try again from the settings menu.")
    );
    gtk_dialog_run(GTK_DIALOG (dialog));
    gtk_widget_destroy(dialog);
}

static void
show_registering_on_blockchain_spinner(AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->vbox_registering_username_spinner);
}

static gboolean
create_ring_account(AccountCreationWizard *view,
                    gchar *display_name,
                    gchar *username,
                    gchar *password,
                    gchar *pin)
{

    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(view), G_SOURCE_REMOVE);
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    // Copy password and alias, which will be used in our callbacks
    priv->username = new QString(username);
    priv->password = new QString(password);

    g_object_ref(view); // ref so its not desroyed too early

    /* create account and set UPnP enabled, as its not by default in the daemon */
    Account *account = nullptr;

    /* get profile (if so) */
    auto profile = ProfileModel::instance().selectedProfile();

    if (display_name && strlen(display_name) > 0) {
        account = AccountModel::instance().add(display_name, Account::Protocol::RING);
        if(profile && AccountModel::instance().size() == 1)
        {
            profile->person()->setFormattedName(display_name);
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

    account->setDisplayName(display_name); // set the display name to the same as the alias

    account->setUpnpEnabled(TRUE);

    /* show error window if the account errors */
    priv->account_state_changed = QObject::connect(
        account,
        &Account::stateChanged,
        [=] (Account::RegistrationState state) {
            switch(state)
            {
                case Account::RegistrationState::ERROR:
                {
                    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
                    QObject::disconnect(priv->account_state_changed);
                    show_error_view(view);
                    g_object_unref(view);
                    break;
                }
                case Account::RegistrationState::READY:
                case Account::RegistrationState::TRYING:
                case Account::RegistrationState::UNREGISTERED:
                {
                    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
                    QObject::disconnect(priv->account_state_changed);

                    account << Account::EditAction::RELOAD;

                    // Now try to register the username
                    if (priv->username && !priv->username->isEmpty() && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->checkbutton_sign_up_blockchain)))
                    {
                        priv->name_registration_ended = QObject::connect(
                            account,
                            &Account::nameRegistrationEnded,
                            [=] (NameDirectory::RegisterNameStatus status, G_GNUC_UNUSED const QString& name) {
                                QObject::disconnect(priv->name_registration_ended);
                                switch(status)
                                {
                                    case NameDirectory::RegisterNameStatus::INVALID_NAME:
                                    case NameDirectory::RegisterNameStatus::WRONG_PASSWORD:
                                    case NameDirectory::RegisterNameStatus::ALREADY_TAKEN:
                                    case NameDirectory::RegisterNameStatus::NETWORK_ERROR:
                                    {
                                        could_not_register_username_dialog(view);
                                    }
                                    case NameDirectory::RegisterNameStatus::SUCCESS:
                                    {
                                        // Reload so that the username is displayed in the settings
                                        account << Account::EditAction::RELOAD;
                                        break;
                                    }
                                }
                                g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED], 0);
                                g_object_unref(view);
                            }
                        );

                        show_registering_on_blockchain_spinner(view);

                        bool register_name_result = account->registerName(*priv->password, *priv->username);
                        priv->username->clear();
                        priv->password->clear();

                        if (register_name_result == FALSE)
                        {
                            g_debug("Could not initialize registerName operation");
                            could_not_register_username_dialog(view);
                            QObject::disconnect(priv->name_registration_ended);
                            g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED], 0);
                            g_object_unref(view);
                        }
                    }
                    else
                    {
                        g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED], 0);
                        g_object_unref(view);
                    }
                    break;
                }
                case Account::RegistrationState::INITIALIZING:
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

    gchar *display_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_display_name)));
    gchar *username = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_username)));
    gchar *password = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_password)));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password), "");
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password_confirm), "");

    auto status = create_ring_account(win, display_name, username, password, NULL);

    g_free(display_name);
    g_free(password);
    g_free(username);

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

    auto status = create_ring_account(win, NULL, NULL, password, pin);

    g_free(password);
    g_free(pin);

    return status;
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
        gtk_label_set_text(GTK_LABEL(priv->label_password_error), _("Passwords don't match"));
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

    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->account_creation);
}

static void
wizard_cancel_clicked(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *view)
{
    g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_CANCELED], 0);
}

static void
entries_existing_account_changed(G_GNUC_UNUSED GtkEntry *entry, AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    const gchar *pin = gtk_entry_get_text(GTK_ENTRY(priv->entry_existing_account_pin));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(priv->entry_existing_account_password));

    if (strlen(pin) > 0 && strlen(password) > 0)
    {
        gtk_widget_set_sensitive(priv->button_existing_account_next, TRUE);
    }
    else
    {
        gtk_widget_set_sensitive(priv->button_existing_account_next, FALSE);
    }
}

static void
entries_new_account_changed(AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    const gchar *display_name = gtk_entry_get_text(GTK_ENTRY(priv->entry_display_name));
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(priv->entry_username));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password));
    const gchar *password_confirm = gtk_entry_get_text(GTK_ENTRY(priv->entry_password_confirm));
    const gboolean sign_up_blockchain = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->checkbutton_sign_up_blockchain));

    if (
            strlen(display_name) > 0 && // Display name is longer than 0
            strlen(password) > 0 && // Password is longer than 0
            strlen(password_confirm) > 0 && // Confirmation is also longer than 0
            (
                (sign_up_blockchain && strlen(username) > 0 && priv->username_available) || // we are signing up, username is set and avaialble
                !sign_up_blockchain // We are not signing up
            )
        )
    {
        gtk_widget_set_sensitive(priv->button_account_creation_next, TRUE);
    }
    else
    {
        gtk_widget_set_sensitive(priv->button_account_creation_next, FALSE);
    }
}

static void
checkbutton_sign_up_blockchain_toggled(GtkToggleButton *toggle_button, AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gboolean sign_up_blockchain = gtk_toggle_button_get_active(toggle_button);


    username_registration_box_set_use_blockchain(
        USERNAME_REGISTRATION_BOX(priv->username_registration_box),
        sign_up_blockchain
    );

    gtk_widget_set_sensitive(priv->entry_username, sign_up_blockchain);

    if (!sign_up_blockchain)
    {
        gtk_entry_set_text(GTK_ENTRY(priv->entry_username), "");
    }

    /* Unchecking blockchain signup when there is an empty username should
     * result in activating the next button.
    */
    entries_new_account_changed(view);
}

static void
username_availability_changed(AccountCreationWizard *view, gboolean username_available)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    priv->username_available = username_available;
    entries_new_account_changed(view);
}

static void
build_creation_wizard_view(AccountCreationWizard *view, gboolean show_cancel_button)
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

    /* create the username_registration_box */
    priv->username_registration_box = username_registration_box_new(nullptr, FALSE);
    gtk_container_add(GTK_CONTAINER(priv->box_username_entry), priv->username_registration_box);
    gtk_widget_show(priv->username_registration_box);
    priv->entry_username = GTK_WIDGET(
        username_registration_box_get_entry(
            USERNAME_REGISTRATION_BOX(priv->username_registration_box)
        )
    );

    /* use the real name / username of the logged in user as the default */
    const char* real_name = g_get_real_name();
    const char* user_name = g_get_user_name();
    g_debug("real_name = %s",real_name);
    g_debug("user_name = %s",user_name);

    /* check first if the real name was determined */
    if (g_strcmp0 (real_name,"Unknown") != 0)
        gtk_entry_set_text(GTK_ENTRY(priv->entry_display_name), real_name);
    else
        gtk_entry_set_text(GTK_ENTRY(priv->entry_display_name), user_name);

    /* cancel button */
    gtk_widget_set_visible(priv->button_wizard_cancel, show_cancel_button);

    /* choose_account_type signals */
    g_signal_connect_swapped(priv->button_new_account, "clicked", G_CALLBACK(show_account_creation), view);
    g_signal_connect_swapped(priv->button_existing_account, "clicked", G_CALLBACK(show_existing_account), view);
    g_signal_connect(priv->button_wizard_cancel, "clicked", G_CALLBACK(wizard_cancel_clicked), view);

    /* account_creation signals */
    g_signal_connect_swapped(priv->button_account_creation_previous, "clicked", G_CALLBACK(show_choose_account_type), view);
    g_signal_connect_swapped(priv->entry_username, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect(priv->button_account_creation_next, "clicked", G_CALLBACK(account_creation_next_clicked), view);
    g_signal_connect_swapped(priv->entry_password, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect_swapped(priv->entry_password_confirm, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect_swapped(priv->entry_username, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect_swapped(priv->entry_display_name, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect(priv->checkbutton_sign_up_blockchain, "toggled", G_CALLBACK(checkbutton_sign_up_blockchain_toggled), view);
    g_signal_connect_swapped(priv->username_registration_box, "username-availability-changed", G_CALLBACK(username_availability_changed), view);

    /* existing_account singals */
    g_signal_connect_swapped(priv->button_existing_account_previous, "clicked", G_CALLBACK(show_choose_account_type), view);
    g_signal_connect(priv->button_existing_account_next, "clicked", G_CALLBACK(existing_account_next_clicked), view);
    g_signal_connect(priv->entry_existing_account_pin, "changed", G_CALLBACK(entries_existing_account_changed), view);
    g_signal_connect(priv->entry_existing_account_password, "changed", G_CALLBACK(entries_existing_account_changed), view);

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
