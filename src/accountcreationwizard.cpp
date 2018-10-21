/*
 *  Copyright (C) 2016-2018 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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
#include <profilemodel.h>
#include <profile.h>
#include <accountmodel.h>
#include <personmodel.h>
#include "api/newaccountmodel.h"

// Ring Client
#include "utils/models.h"
#include "avatarmanipulation.h"
#include "accountcreationwizard.h"
#include "usernameregistrationbox.h"

struct _AccountCreationWizard
{
    GtkScrolledWindow parent;
};

struct _AccountCreationWizardClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _AccountCreationWizardPrivate AccountCreationWizardPrivate;

struct _AccountCreationWizardPrivate
{
    AccountInfoPointer const *accountInfo_ = nullptr;
    gchar* accountId;
    gchar* username;
    gchar* password;
    gchar* avatar;

    GtkWidget *stack_account_creation;
    gboolean username_available;

    /* choose_account_type_vbox */
    GtkWidget *choose_account_type_vbox;
    GtkWidget *choose_account_type_ring_logo;
    GtkWidget *button_new_account;
    GtkWidget *button_existing_account;
    GtkWidget *button_wizard_cancel;
    GtkWidget *button_show_advanced;
    GtkWidget *button_new_sip_account;

    /* existing account */
    GtkWidget *existing_account;
    GtkWidget *button_existing_account_next;
    GtkWidget *button_existing_account_previous;
    GtkWidget *entry_existing_account_pin;
    GtkWidget *entry_existing_account_archive;
    GtkWidget *entry_existing_account_password;
    GtkWidget *retrieving_account;

    /* account creation */
    GtkWidget *account_creation;
    GtkWidget *entry_username;
    GtkWidget *entry_password;
    GtkWidget *entry_password_confirm;
    GtkWidget *label_password_error;
    GtkWidget *button_account_creation_next;
    GtkWidget *button_account_creation_previous;
    GtkWidget *box_avatarselection;
    GtkWidget *avatar_manipulation;
    GtkWidget *box_username_entry;
    GtkWidget *switch_register;
    GtkWidget *username_registration_box;
    GtkWidget *entry_display_name;

    /* generating_account_spinner */
    GtkWidget *vbox_generating_account_spinner;

    /* registering_username_spinner */
    GtkWidget *vbox_registering_username_spinner;

    /* error_view */
    GtkWidget *error_view;
    GtkWidget *button_error_view_ok;

    lrc::api::AVModel* avModel_;

};

G_DEFINE_TYPE_WITH_PRIVATE(AccountCreationWizard, account_creation_wizard, GTK_TYPE_SCROLLED_WINDOW);

#define ACCOUNT_CREATION_WIZARD_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_CREATION_WIZARD_TYPE, AccountCreationWizardPrivate))

/* signals */
enum {
    ACCOUNT_CREATION_CANCELED,
    ACCOUNT_CREATION_COMPLETED,
    LAST_SIGNAL
};

static guint account_creation_wizard_signals[LAST_SIGNAL] = { 0 };

static void
account_creation_wizard_dispose(GObject *object)
{
    // make sure preview is stopped and destroyed
    account_creation_wizard_show_preview(ACCOUNT_CREATION_WIZARD(object), FALSE);

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
                                                "/cx/jami/JamiGnome/accountcreationwizard.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, stack_account_creation);

    /* choose_account_type_vbox */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, choose_account_type_vbox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, choose_account_type_ring_logo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_new_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_existing_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_wizard_cancel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_show_advanced);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_new_sip_account);

    /* existing account */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, existing_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_existing_account_next);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_existing_account_previous);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_existing_account_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_existing_account_archive);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, switch_register);
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

void
account_creation_show_error_view(AccountCreationWizard *view, const std::string& id)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    if (priv->accountId && id == priv->accountId)
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->error_view);
}

static gboolean
create_ring_account(AccountCreationWizard *view,
                    gchar *display_name,
                    gchar *username,
                    gchar *password,
                    gchar *pin,
                    gchar *archivePath)
{
    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(view), G_SOURCE_REMOVE);
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    priv->username = g_strdup(username);
    priv->password = g_strdup(password);
    std::string accountId = lrc::api::NewAccountModel::createNewAccount(
                                   lrc::api::profile::Type::RING,
                                   display_name? display_name : "",
                                   archivePath? archivePath : "",
                                   password? password : "",
                                   pin? pin : "");
    priv->accountId = g_strdup(accountId.c_str());
    priv->avatar = g_strdup(avatar_manipulation_get_temporary(AVATAR_MANIPULATION(priv->avatar_manipulation)));
    // NOTE: NewAccountModel::accountAdded will be triggered here and will call account_creation_wizard_account_added

    g_object_ref(view);  // ref so its not destroyed too early

    return G_SOURCE_REMOVE;
}

void
account_creation_wizard_account_added(AccountCreationWizard *view, AccountInfoPointer const & accountInfo)
{
    g_return_if_fail(IS_ACCOUNT_CREATION_WIZARD(view));
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    priv->accountInfo_ = &accountInfo;
    if ((*priv->accountInfo_)->id != priv->accountId) {
        // Not for this account. Abort
        return;
    }
    // Register username
    if (priv->username) {
        (*priv->accountInfo_)->accountModel->registerName(priv->accountId, priv->password, priv->username);
    }
    // Set avatar if any.
    if (priv->avatar) {
        try {
            (*priv->accountInfo_)->accountModel->setAvatar(priv->accountId, priv->avatar);
        } catch (std::out_of_range&) {
            g_warning("Can't set avatar for unknown account");
        }
    }

    g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED], 0);
    g_object_unref(view);
}

static gboolean
create_new_ring_account(AccountCreationWizard *win)
{
    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(win), G_SOURCE_REMOVE);
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    /* Tuleap: #1441
     * if the user did not validate the avatar area selection, we still take that as the image
     * for their avatar; otherwise many users end up with no avatar by default
     * TODO: improve avatar creation process to not need this fix
     */
    avatar_manipulation_wizard_completed(AVATAR_MANIPULATION(priv->avatar_manipulation));

    gchar *display_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_display_name)));
    gchar *username = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_username)));
    gchar *password = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_password)));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password), "");
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password_confirm), "");

    auto status = create_ring_account(win, display_name, username, password, nullptr, nullptr);

    // Now that we've use the preview to generate the avatar, we can safely close it. Don't
    // assume owner will do it for us, this might not always be the case
    account_creation_wizard_show_preview(win, FALSE);

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
    if (pin and strlen(pin) > 0) {
        gtk_entry_set_text(GTK_ENTRY(priv->entry_existing_account_pin), "");
    }

    gchar *archive = g_strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(priv->entry_existing_account_archive)));
    if (archive) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->entry_existing_account_archive), nullptr);
    }

    auto status = create_ring_account(win, NULL, NULL, password, pin, archive);

    g_free(archive);
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
existing_account_next_clicked(AccountCreationWizard *win)
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

    /* Make sure other stack elements are hidden, otherwise their width will prevent
       the window to be correctly resized at this stage. */
    gtk_widget_hide(priv->account_creation);
    gtk_widget_hide(priv->existing_account);

    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->choose_account_type_vbox);
}

static void
account_creation_previous_clicked(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *view)
{
    // make sure to stop preview before going back to choose account type
    account_creation_wizard_show_preview(view, FALSE);
    show_choose_account_type(view);
}

static void
show_existing_account(AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    gtk_widget_show(priv->existing_account);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->existing_account);
}

static void
wizard_cancel_clicked(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *view)
{
    account_creation_wizard_cancel(view);
}

static void
show_advanced(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gtk_widget_set_visible(GTK_WIDGET(priv->button_new_sip_account), !gtk_widget_is_visible(GTK_WIDGET(priv->button_new_sip_account)));
}

static void
create_new_sip_account(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *view)
{
    lrc::api::NewAccountModel::createNewAccount(lrc::api::profile::Type::SIP, "SIP");
    g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED], 0);
    g_object_unref(view);
}

static void
entries_existing_account_changed(G_GNUC_UNUSED GtkEntry *entry, AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    const gchar *pin = gtk_entry_get_text(GTK_ENTRY(priv->entry_existing_account_pin));
    gchar *archive_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(priv->entry_existing_account_archive));

    bool hasPin = pin and strlen(pin) > 0;
    bool hasArchive = archive_path and strlen(archive_path) > 0;

    gtk_widget_set_sensitive(
        priv->entry_existing_account_pin,
        (not hasArchive)
    );
    gtk_widget_set_sensitive(
        priv->entry_existing_account_archive,
        (not hasPin)
    );
    gtk_widget_set_sensitive(
        priv->button_existing_account_next,
        (hasArchive || hasPin)
    );

    g_free(archive_path);
}

static void
entries_new_account_changed(AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    const gchar *display_name = gtk_entry_get_text(GTK_ENTRY(priv->entry_display_name));
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(priv->entry_username));
    const gboolean sign_up_blockchain = gtk_switch_get_active(GTK_SWITCH(priv->switch_register));

    if (
            (sign_up_blockchain && strlen(username) > 0 && priv->username_available) || // we are signing up, username is set and avaialble
            !sign_up_blockchain // We are not signing up
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
sign_up_blockchain_switched(GtkSwitch* switch_btn, GParamSpec*, AccountCreationWizard *view)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gboolean sign_up_blockchain = gtk_switch_get_active(switch_btn);

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
    GdkPixbuf* logo_ring = gdk_pixbuf_new_from_resource_at_scale("/cx/jami/JamiGnome/jami-logo-blue",
                                                                  -1, 50, TRUE, &error);
    if (logo_ring == NULL) {
        g_debug("Could not load logo: %s", error->message);
        g_clear_error(&error);
    } else
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->choose_account_type_ring_logo), logo_ring);

    /* create the username_registration_box */
    priv->username_registration_box = username_registration_box_new_empty(false);
    gtk_box_pack_end(GTK_BOX(priv->box_username_entry), GTK_WIDGET(priv->username_registration_box), false, false, 0);
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
    g_signal_connect_swapped(priv->button_new_account, "clicked", G_CALLBACK(account_creation_wizard_show_preview), view);
    g_signal_connect_swapped(priv->button_existing_account, "clicked", G_CALLBACK(show_existing_account), view);
    g_signal_connect(priv->button_wizard_cancel, "clicked", G_CALLBACK(wizard_cancel_clicked), view);
    g_signal_connect(priv->button_show_advanced, "clicked", G_CALLBACK(show_advanced), view);
    g_signal_connect(priv->button_new_sip_account, "clicked", G_CALLBACK(create_new_sip_account), view);

    /* account_creation signals */
    g_signal_connect_swapped(priv->entry_username, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect(priv->button_account_creation_next, "clicked", G_CALLBACK(account_creation_next_clicked), view);
    g_signal_connect(priv->button_account_creation_previous, "clicked", G_CALLBACK(account_creation_previous_clicked), view);
    g_signal_connect_swapped(priv->entry_password, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect_swapped(priv->entry_password_confirm, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect_swapped(priv->entry_display_name, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect(priv->switch_register, "notify::active", G_CALLBACK(sign_up_blockchain_switched), view);
    g_signal_connect_swapped(priv->username_registration_box, "username-availability-changed", G_CALLBACK(username_availability_changed), view);

    /* existing_account signals */
    g_signal_connect_swapped(priv->button_existing_account_previous, "clicked", G_CALLBACK(show_choose_account_type), view);
    g_signal_connect_swapped(priv->button_existing_account_next, "clicked", G_CALLBACK(existing_account_next_clicked), view);
    g_signal_connect(priv->entry_existing_account_pin, "changed", G_CALLBACK(entries_existing_account_changed), view);
    g_signal_connect(priv->entry_existing_account_archive, "file-set", G_CALLBACK(entries_existing_account_changed), view);
    g_signal_connect(priv->entry_existing_account_password, "changed", G_CALLBACK(entries_existing_account_changed), view);

    /* error_view signals */
    g_signal_connect_swapped(priv->button_error_view_ok, "clicked", G_CALLBACK(show_choose_account_type), view);

    show_choose_account_type(view);

    gtk_button_set_relief(GTK_BUTTON(priv->button_show_advanced), GTK_RELIEF_NONE);

    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".black { color: grey; font-size: 0.8em; }\
        .transparent-button { margin-left: 10px; border: 0; background-color: rgba(0,0,0,0); margin-right: 0; padding-right: 0;}",
        -1, nullptr
    );
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

GtkWidget *
account_creation_wizard_new(bool show_cancel_button, lrc::api::AVModel& avModel)
{
    gpointer view = g_object_new(ACCOUNT_CREATION_WIZARD_TYPE, NULL);

    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    priv->avModel_ = &avModel;

    build_creation_wizard_view(ACCOUNT_CREATION_WIZARD(view), show_cancel_button);
    return (GtkWidget *)view;
}

void
account_creation_wizard_show_preview(AccountCreationWizard *win, gboolean show_preview)
{
    AccountCreationWizardPrivate *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    if (priv->account_creation) {
        if (show_preview)
            gtk_widget_show(priv->account_creation);
        else
            gtk_widget_hide(priv->account_creation);
    }

    /* Similarily to general settings view, we construct and destroy the avatar manipulation widget
       each time the profile is made visible / hidden. While not the most elegant solution, this
       allows us to run the preview if and only if it is displayed, and always stop it when hidden. */
    if (show_preview && !priv->avatar_manipulation) {
        priv->avatar_manipulation = avatar_manipulation_new_from_wizard(priv->avModel_);
        gtk_box_pack_start(GTK_BOX(priv->box_avatarselection), priv->avatar_manipulation, true, true, 0);
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->account_creation);
    } else if (priv->avatar_manipulation) {
        /* make sure the AvatarManipulation widget is destroyed so the VideoWidget inside of it is too;
         * NOTE: destorying its parent (box_avatarselection) first will cause a mystery 'BadDrawable'
         * crash due to X error */
        gtk_container_remove(GTK_CONTAINER(priv->box_avatarselection), priv->avatar_manipulation);
        priv->avatar_manipulation = nullptr;
    }
}

void
account_creation_wizard_cancel(AccountCreationWizard *win)
{
    // make sure to stop preview before doing something else
    account_creation_wizard_show_preview(win, FALSE);
    g_signal_emit(G_OBJECT(win), account_creation_wizard_signals[ACCOUNT_CREATION_CANCELED], 0);
}
