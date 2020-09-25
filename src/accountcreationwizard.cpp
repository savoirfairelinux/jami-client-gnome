/*
 *  Copyright (C) 2016-2020 Savoir-faire Linux Inc.
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
#include "api/newaccountmodel.h"

// Jami Client
#include "utils/models.h"
#include "avatarmanipulation.h"
#include "accountcreationwizard.h"
#include "usernameregistrationbox.h"
#include "utils/files.h"

struct _AccountCreationWizard
{
    GtkScrolledWindow parent;
};

struct _AccountCreationWizardClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _AccountCreationWizardPrivate AccountCreationWizardPrivate;

enum class Mode {
    ADD_LOCAL,
    IMPORT_FROM_DEVICE,
    IMPORT_FROM_BACKUP,
    CONNECT_TO_MANAGER,
    ADD_RENDEZVOUS
};

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
    GtkWidget *button_new_rendezvous;
    GtkWidget *button_import_from_device;
    GtkWidget *button_import_from_backup;
    GtkWidget *button_show_advanced;
    GtkWidget *button_connect_account_manager;
    GtkWidget *button_new_sip_account;

    /* existing account */
    GtkWidget *existing_account;
    GtkWidget *existing_account_label;
    GtkWidget *row_archive;
    GtkWidget *row_pin;
    GtkWidget *button_pin_info;
    GtkWidget *row_pin_label;
    GtkWidget *button_import_from_do;
    GtkWidget *button_import_from_back;
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
    GtkWidget *button_account_creation_back;
    GtkWidget *box_avatarselection;
    GtkWidget *avatar_manipulation;
    GtkWidget *label_register;
    GtkWidget *label_username;
    GtkWidget *box_username_entry;
    GtkWidget *switch_register;
    GtkWidget *username_registration_box;
    GtkWidget *entry_display_name;

    /* generating_account_spinner */
    GtkWidget *vbox_generating_account_spinner;
    GtkWidget *generating_label;

    /* registering_username_spinner */
    GtkWidget *vbox_registering_username_spinner;

    /* backup */
    GtkWidget *info_backup;
    GtkWidget *button_never_show_again;
    GtkWidget *button_export_account;
    GtkWidget *button_skip_backup;
    GtkWidget *button_backup_info;
    GtkWidget *row_backup_label;

    /* error_view */
    GtkWidget *error_view;
    GtkWidget *label_error_view;
    GtkWidget *button_error_view_ok;

    /* connect to account_manager */
    GtkWidget *connect_to_account_manager;
    GtkWidget *entry_account_manager_username;
    GtkWidget *entry_account_manager_password;
    GtkWidget *entry_account_manager_uri;
    GtkWidget *button_account_manager_connect_back;
    GtkWidget *button_account_manager_connect_connect;

    lrc::api::AVModel* avModel_;
    lrc::api::NewAccountModel* accountModel_;

    Mode mode;

    GSettings *settings;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountCreationWizard, account_creation_wizard, GTK_TYPE_SCROLLED_WINDOW);

#define ACCOUNT_CREATION_WIZARD_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_CREATION_WIZARD_TYPE, AccountCreationWizardPrivate))

/* signals */
enum {
    ACCOUNT_CREATION_LOCK,
    ACCOUNT_CREATION_UNLOCK,
    ACCOUNT_CREATION_COMPLETED,
    LAST_SIGNAL
};

static guint account_creation_wizard_signals[LAST_SIGNAL] = { 0 };

static void
account_creation_wizard_dispose(GObject *object)
{
    // make sure preview is stopped and destroyed
    account_creation_wizard_show_preview(ACCOUNT_CREATION_WIZARD(object), FALSE);

    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(object);
    g_clear_object(&priv->settings);

    G_OBJECT_CLASS(account_creation_wizard_parent_class)->dispose(object);
}

static void
account_creation_wizard_init(AccountCreationWizard *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    auto *priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    priv->settings = g_settings_new_full(get_settings_schema(), NULL, NULL);


    g_settings_bind(priv->settings, "never-show-backup-again",
                    priv->button_never_show_again, "active",
                    G_SETTINGS_BIND_DEFAULT);
}

static void
account_creation_wizard_class_init(AccountCreationWizardClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_creation_wizard_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/net/jami/JamiGnome/accountcreationwizard.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, stack_account_creation);

    /* choose_account_type_vbox */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, choose_account_type_vbox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, choose_account_type_ring_logo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_new_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_new_rendezvous);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_import_from_device);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_import_from_backup);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_show_advanced);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_connect_account_manager);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_new_sip_account);

    /* existing account */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, existing_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, existing_account_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, row_archive);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, row_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_pin_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, row_pin_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_import_from_do);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_import_from_back);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_existing_account_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_existing_account_archive);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_existing_account_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, retrieving_account);

    /* account creation */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, account_creation);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_password_confirm);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_account_creation_next);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_account_creation_back);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, box_avatarselection);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, label_password_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, label_register);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, label_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, box_username_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, switch_register);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_display_name);

    /* generating_account_spinner */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, vbox_generating_account_spinner);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, generating_label);

    /* registering_username_spinner */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, vbox_registering_username_spinner);

    /* info backup */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, info_backup);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_never_show_again);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_export_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, row_backup_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_backup_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_skip_backup);

    /* error view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, error_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, label_error_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_error_view_ok);

    /* connect to account_manager */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, connect_to_account_manager);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_account_manager_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_account_manager_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, entry_account_manager_uri);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_account_manager_connect_back);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCreationWizard, button_account_manager_connect_connect);

    /* add signals */
    account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED] = g_signal_new("account-creation-completed",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

    account_creation_wizard_signals[ACCOUNT_CREATION_LOCK] = g_signal_new("account-creation-lock",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

    account_creation_wizard_signals[ACCOUNT_CREATION_UNLOCK] = g_signal_new("account-creation-unlock",
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
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    if (priv->accountId && id == priv->accountId) {
        switch (priv->mode) {
            case Mode::ADD_LOCAL:
                gtk_label_set_text(GTK_LABEL(priv->label_error_view),
                    _("An error occured during the account creation."));
                break;
            case Mode::ADD_RENDEZVOUS:
                gtk_label_set_text(GTK_LABEL(priv->label_error_view),
                    _("An error occured during the rendez-vous creation."));
                break;
            case Mode::IMPORT_FROM_DEVICE:
                gtk_label_set_text(GTK_LABEL(priv->label_error_view),
                    _("Cannot retrieve any account, please verify your PIN and your password."));
                break;
            case Mode::IMPORT_FROM_BACKUP:
                gtk_label_set_text(GTK_LABEL(priv->label_error_view),
                    _("Cannot retrieve any account, please verify your archive and your password."));
                break;
            case Mode::CONNECT_TO_MANAGER:
                gtk_label_set_text(GTK_LABEL(priv->label_error_view),
                    _("Cannot connect to provided account manager. Please check your credentials."));
                break;
        }
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->error_view);
    }
}

static gboolean
connect_to_manager(AccountCreationWizard *view, gchar* username, gchar* password, gchar* managerUri)
{
    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(view), G_SOURCE_REMOVE);
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_LOCK], 0);

    auto accountId = lrc::api::NewAccountModel::connectToAccountManager(
                                   username? username : "",
                                   password? password : "",
                                   managerUri? managerUri : "");
    priv->accountId = g_strdup(qUtf8Printable(accountId));
    // NOTE: NewAccountModel::accountAdded will be triggered here and will call account_creation_wizard_account_added

    g_object_ref(view);  // ref so its not destroyed too early

    return G_SOURCE_REMOVE;
}

static gboolean
create_account(AccountCreationWizard *view,
                    gchar *display_name,
                    gchar *username,
                    gchar *password,
                    gchar *pin,
                    gchar *archivePath)
{
    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(view), G_SOURCE_REMOVE);
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_LOCK], 0);

    priv->username = g_strdup(username);
    priv->password = g_strdup(password);
    auto accountId = lrc::api::NewAccountModel::createNewAccount(
                                   lrc::api::profile::Type::RING,
                                   display_name? display_name : "",
                                   archivePath? archivePath : "",
                                   password? password : "",
                                   pin? pin : "");
    priv->accountId = g_strdup(qUtf8Printable(accountId));
    priv->avatar = g_strdup(avatar_manipulation_get_temporary(AVATAR_MANIPULATION(priv->avatar_manipulation)));
    // NOTE: NewAccountModel::accountAdded will be triggered here and will call account_creation_wizard_account_added

    g_object_ref(view);  // ref so its not destroyed too early

    return G_SOURCE_REMOVE;
}

void
account_creation_wizard_account_added(AccountCreationWizard *view, const std::string& id)
{
    g_return_if_fail(IS_ACCOUNT_CREATION_WIZARD(view));
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    if (id != priv->accountId) {
        // Not for this account. Abort
        return;
    }
    // Register username
    if (priv->username) {
        priv->accountModel_->registerName(priv->accountId, priv->password, priv->username);
    }
    // Set avatar if any.
    if (priv->avatar) {
        try {
            priv->accountModel_->setAvatar(priv->accountId, priv->avatar);
        } catch (std::out_of_range&) {
            g_warning("Can't set avatar for unknown account");
        }
    }

    if (priv->mode == Mode::ADD_RENDEZVOUS) {
        auto conf = priv->accountModel_->getAccountConfig(priv->accountId);
        conf.isRendezVous = true;
        priv->accountModel_->setAccountConfig(priv->accountId, conf);
    }

    if (!g_settings_get_boolean(priv->settings, "never-show-backup-again") && priv->mode == Mode::ADD_LOCAL) {
        gtk_button_set_label(GTK_BUTTON(priv->button_export_account), _("Export account"));
        gtk_widget_show_all(priv->info_backup);
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->info_backup);
    } else {
        g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED], 0);
    }

    g_object_unref(view);
}

static gboolean
create_new_account(AccountCreationWizard *win)
{
    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(win), G_SOURCE_REMOVE);
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

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

    auto status = create_account(win, display_name, username, password, nullptr, nullptr);

    // Now that we've use the preview to generate the avatar, we can safely close it. Don't
    // assume owner will do it for us, this might not always be the case
    account_creation_wizard_show_preview(win, FALSE);

    g_free(display_name);
    g_free(password);
    g_free(username);

    return status;
}

static gboolean
connect_to_account_manager(AccountCreationWizard *win)
{
    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(win), G_SOURCE_REMOVE);
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    gchar *username = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_account_manager_username)));
    gchar *password = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_account_manager_password)));
    gchar *managerUri = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->entry_account_manager_uri)));

    auto status = connect_to_manager(win, username, password, managerUri);

    g_free(username);
    g_free(password);
    g_free(managerUri);

    return status;
}

static gboolean
create_existing_ring_account(AccountCreationWizard *win)
{
    g_return_val_if_fail(IS_ACCOUNT_CREATION_WIZARD(win), G_SOURCE_REMOVE);
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

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

    auto status = create_account(win, NULL, NULL, password, pin, archive);

    g_free(archive);
    g_free(password);
    g_free(pin);

    return status;
}

static void
show_generating_account_spinner(AccountCreationWizard *view)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    if (priv->mode == Mode::ADD_RENDEZVOUS) {
        gtk_label_set_text(GTK_LABEL(priv->generating_label), _("Generating your rendez-vous…"));
    } else {
        gtk_label_set_text(GTK_LABEL(priv->generating_label), _("Generating your Jami account…"));
    }
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->vbox_generating_account_spinner);
}

static void
account_creation_next_clicked(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *win)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

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
    g_timeout_add_full(G_PRIORITY_DEFAULT, 300, (GSourceFunc)create_new_account, win, NULL);
}

static void
connect_account_manager_next_clicked(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *win)
{
    show_generating_account_spinner(win);

    /* now create account after a short timeout so that the the save doesn't
     * happen freeze the client before the widget changes happen;
     * the timeout function should then display the next step in account creation
     */
    g_timeout_add_full(G_PRIORITY_DEFAULT, 300, (GSourceFunc)connect_to_account_manager, win, NULL);
}

static void
show_retrieving_account(AccountCreationWizard *win)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);
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
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    /* Make sure other stack elements are hidden, otherwise their width will prevent
       the window to be correctly resized at this stage. */
    gtk_widget_hide(priv->account_creation);
    gtk_widget_hide(priv->existing_account);
    gtk_widget_hide(priv->connect_to_account_manager);

    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->choose_account_type_vbox);
}

static void
show_pin_label(AccountCreationWizard *view)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gtk_widget_set_visible(priv->row_pin_label, !gtk_widget_is_visible(priv->row_pin_label));
}

static void
show_backup_label(AccountCreationWizard *view)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gtk_widget_set_visible(priv->row_backup_label, !gtk_widget_is_visible(priv->row_backup_label));
}

static void
error_ok_clicked(AccountCreationWizard *view)
{
    g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_UNLOCK], 0);
    show_choose_account_type(view);
}

static void
skip_backup_clicked(AccountCreationWizard *view)
{
    g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED], 0);
}

static void
account_creation_previous_clicked(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *view)
{
    // make sure to stop preview before going back to choose account type
    account_creation_wizard_show_preview(view, FALSE);
    show_choose_account_type(view);
}

static void
connect_account_manager_previous_clicked(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *view)
{
    show_choose_account_type(view);
}

static void
show_import_from_device(AccountCreationWizard *view)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    gtk_label_set_text(GTK_LABEL(priv->existing_account_label), _("Enter Jami account password"));
    gtk_button_set_label(GTK_BUTTON(priv->button_import_from_do), _("Link device"));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_existing_account_pin), "");
    gtk_entry_set_text(GTK_ENTRY(priv->entry_existing_account_password), "");
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->entry_existing_account_archive), nullptr);

    gtk_widget_show_all(priv->existing_account);
    gtk_widget_hide(priv->row_archive);
    gtk_widget_hide(priv->row_pin_label);
    gtk_widget_hide(priv->row_backup_label);

    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->existing_account);
    priv->mode = Mode::IMPORT_FROM_DEVICE;
}

static void
show_import_from_backup(AccountCreationWizard *view)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    gtk_label_set_text(GTK_LABEL(priv->existing_account_label), _("Import from backup"));
    gtk_button_set_label(GTK_BUTTON(priv->button_import_from_do), _("Restore an account from backup"));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_existing_account_pin), "");
    gtk_entry_set_text(GTK_ENTRY(priv->entry_existing_account_password), "");
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->entry_existing_account_archive), nullptr);

    gtk_widget_show_all(priv->existing_account);
    gtk_widget_hide(priv->row_pin);
    gtk_widget_hide(priv->row_pin_label);
    gtk_widget_hide(priv->row_backup_label);

    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->existing_account);
    priv->mode = Mode::IMPORT_FROM_BACKUP;
}

static void
show_connect_account_manager(AccountCreationWizard *view)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    gtk_widget_show(priv->connect_to_account_manager);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->connect_to_account_manager);
    priv->mode = Mode::CONNECT_TO_MANAGER;
}

static void
show_advanced(G_GNUC_UNUSED GtkButton *button, AccountCreationWizard *view)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    gtk_widget_set_visible(GTK_WIDGET(priv->button_connect_account_manager), !gtk_widget_is_visible(GTK_WIDGET(priv->button_connect_account_manager)));
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
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

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
        priv->button_import_from_do,
        (hasArchive || hasPin)
    );

    g_free(archive_path);
}

static void
entries_new_account_changed(AccountCreationWizard *view)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

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
entries_connect_account_manager_changed(AccountCreationWizard *view)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    const std::string username = gtk_entry_get_text(GTK_ENTRY(priv->entry_account_manager_username));
    const std::string managerUri = gtk_entry_get_text(GTK_ENTRY(priv->entry_account_manager_uri));
    const std::string password = gtk_entry_get_text(GTK_ENTRY(priv->entry_account_manager_password));

    gtk_widget_set_sensitive(priv->button_account_manager_connect_connect, (!username.empty() && !managerUri.empty() && !password.empty()));
}

static void
sign_up_blockchain_switched(GtkSwitch* switch_btn, GParamSpec*, AccountCreationWizard *view)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
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
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    priv->username_available = username_available;
    entries_new_account_changed(view);
}

// Export file
static void
choose_export_file(AccountCreationWizard *view)
{
    g_return_if_fail(IS_ACCOUNT_CREATION_WIZARD(view));
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    // Get preferred path
    GtkWidget* dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
    gint res;
    gchar* filename = nullptr;

    dialog = gtk_file_chooser_dialog_new(_("Save File"),
                                         GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view))),
                                         action,
                                         _("_Cancel"),
                                         GTK_RESPONSE_CANCEL,
                                         _("_Save"),
                                         GTK_RESPONSE_ACCEPT,
                                         nullptr);

    const auto& accountInfo = priv->accountModel_->getAccountInfo(priv->accountId);
    auto alias = accountInfo.profileInfo.alias;
    auto uri = alias.isEmpty() ? "export.gz" : alias + ".gz";
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), qUtf8Printable(uri));
    res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        auto chooser = GTK_FILE_CHOOSER(dialog);
        filename = gtk_file_chooser_get_filename(chooser);
    }
    gtk_widget_destroy(dialog);

    if (!filename) return;

    std::string password = {};
    auto prop = priv->accountModel_->getAccountConfig(priv->accountId);
    if (prop.archiveHasPassword) {
        auto* top_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
        auto* password_dialog = gtk_message_dialog_new(top_window,
            GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
            _("You need to enter your password to export the full archive."));
        gtk_window_set_title(GTK_WINDOW(password_dialog), _("Enter password to export archive"));
        gtk_dialog_set_default_response(GTK_DIALOG(password_dialog), GTK_RESPONSE_OK);

        auto* message_area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(password_dialog));
        auto* password_entry = gtk_entry_new();
        gtk_entry_set_visibility(GTK_ENTRY(password_entry), false);
        gtk_entry_set_invisible_char(GTK_ENTRY(password_entry), '*');
        gtk_box_pack_start(GTK_BOX(message_area), password_entry, true, true, 0);
        gtk_widget_show_all(password_dialog);

        auto res = gtk_dialog_run(GTK_DIALOG(password_dialog));
        if (res == GTK_RESPONSE_OK) {
            password = gtk_entry_get_text(GTK_ENTRY(password_entry));
        } else {
            gtk_widget_destroy(password_dialog);
            return;
        }

        gtk_widget_destroy(password_dialog);
    }

    // export account
    auto success = priv->accountModel_->exportToFile(priv->accountId, filename, password.c_str());
    std::string label = success? _("Account exported!") : _("Export account failure.");
    gtk_button_set_label(GTK_BUTTON(priv->button_export_account), label.c_str());
    g_free(filename);

    if (success) {
        g_signal_emit(G_OBJECT(view), account_creation_wizard_signals[ACCOUNT_CREATION_COMPLETED], 0);
    }
}

static void
build_creation_wizard_view(AccountCreationWizard *view)
{
    g_return_if_fail(IS_ACCOUNT_CREATION_WIZARD(view));
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);

    /* set ring logo */
    GError *error = NULL;
    GdkPixbuf* logo_ring = gdk_pixbuf_new_from_resource_at_scale("/net/jami/JamiGnome/jami-logo-blue",
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

    /* choose_account_type signals */
    g_signal_connect_swapped(priv->button_new_account, "clicked", G_CALLBACK(account_creation_wizard_show_preview), view);
    g_signal_connect_swapped(priv->button_new_rendezvous, "clicked", G_CALLBACK(show_rendezvous_creation_wizard), view);
    g_signal_connect_swapped(priv->button_import_from_device, "clicked", G_CALLBACK(show_import_from_device), view);
    g_signal_connect_swapped(priv->button_import_from_backup, "clicked", G_CALLBACK(show_import_from_backup), view);
    g_signal_connect(priv->button_show_advanced, "clicked", G_CALLBACK(show_advanced), view);
    g_signal_connect_swapped(priv->button_connect_account_manager, "clicked", G_CALLBACK(show_connect_account_manager), view);
    g_signal_connect(priv->button_new_sip_account, "clicked", G_CALLBACK(create_new_sip_account), view);

    /* account_creation signals */
    g_signal_connect_swapped(priv->entry_username, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect(priv->button_account_creation_next, "clicked", G_CALLBACK(account_creation_next_clicked), view);
    g_signal_connect(priv->button_account_creation_back, "clicked", G_CALLBACK(account_creation_previous_clicked), view);
    g_signal_connect_swapped(priv->entry_password, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect_swapped(priv->entry_password_confirm, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect_swapped(priv->entry_display_name, "changed", G_CALLBACK(entries_new_account_changed), view);
    g_signal_connect(priv->switch_register, "notify::active", G_CALLBACK(sign_up_blockchain_switched), view);
    g_signal_connect_swapped(priv->username_registration_box, "username-availability-changed", G_CALLBACK(username_availability_changed), view);

    /* existing_account signals */
    g_signal_connect_swapped(priv->button_import_from_back, "clicked", G_CALLBACK(show_choose_account_type), view);
    g_signal_connect_swapped(priv->button_pin_info, "clicked", G_CALLBACK(show_pin_label), view);
    g_signal_connect_swapped(priv->button_import_from_do, "clicked", G_CALLBACK(existing_account_next_clicked), view);
    g_signal_connect(priv->entry_existing_account_pin, "changed", G_CALLBACK(entries_existing_account_changed), view);
    g_signal_connect(priv->entry_existing_account_archive, "file-set", G_CALLBACK(entries_existing_account_changed), view);
    g_signal_connect(priv->entry_existing_account_password, "changed", G_CALLBACK(entries_existing_account_changed), view);

    /* Backup */
    g_signal_connect_swapped(priv->button_export_account, "clicked", G_CALLBACK(choose_export_file), view);
    g_signal_connect_swapped(priv->button_skip_backup, "clicked", G_CALLBACK(skip_backup_clicked), view);
    g_signal_connect_swapped(priv->button_backup_info, "clicked", G_CALLBACK(show_backup_label), view);

    /* error_view signals */
    g_signal_connect_swapped(priv->button_error_view_ok, "clicked", G_CALLBACK(error_ok_clicked), view);


    /* Connect to account_manager */
    g_signal_connect_swapped(priv->entry_account_manager_username, "changed", G_CALLBACK(entries_connect_account_manager_changed), view);
    g_signal_connect_swapped(priv->entry_account_manager_password, "changed", G_CALLBACK(entries_connect_account_manager_changed), view);
    g_signal_connect_swapped(priv->entry_account_manager_uri, "changed", G_CALLBACK(entries_connect_account_manager_changed), view);
    g_signal_connect(priv->button_account_manager_connect_back, "clicked", G_CALLBACK(connect_account_manager_previous_clicked), view);
    g_signal_connect(priv->button_account_manager_connect_connect, "clicked", G_CALLBACK(connect_account_manager_next_clicked), view);

    show_choose_account_type(view);

    gtk_button_set_relief(GTK_BUTTON(priv->button_show_advanced), GTK_RELIEF_NONE);

    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".black { color: grey; font-size: 0.8em; }\
        .transparent-button { margin-left: 0; border: 0; background-color: rgba(0,0,0,0); margin-right: 0; padding-right: 0;}\
        .infos-button { margin: 0; border: 0; background-color: rgba(0,0,0,0); padding: 0; box-shadow: 0;}",
        -1, nullptr
    );
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

GtkWidget *
account_creation_wizard_new(lrc::api::AVModel& avModel, lrc::api::NewAccountModel& accountModel)
{
    gpointer view = g_object_new(ACCOUNT_CREATION_WIZARD_TYPE, NULL);

    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(view);
    priv->avModel_ = &avModel;
    priv->accountModel_ = &accountModel;

    build_creation_wizard_view(ACCOUNT_CREATION_WIZARD(view));
    return (GtkWidget *)view;
}

void
account_creation_wizard_show_preview(AccountCreationWizard *win, gboolean show_preview)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    if (priv->account_creation) {
        gtk_label_set_text(GTK_LABEL(priv->label_register), _("Register username"));
        gtk_label_set_text(GTK_LABEL(priv->label_username), _("Username"));
        if (show_preview) {
            gtk_widget_show(priv->account_creation);
            priv->mode = Mode::ADD_LOCAL;
        } else
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
show_rendezvous_creation_wizard(AccountCreationWizard *win)
{
    auto* priv = ACCOUNT_CREATION_WIZARD_GET_PRIVATE(win);

    if (priv->account_creation) {
        gtk_label_set_text(GTK_LABEL(priv->label_register), _("Register name"));
        gtk_label_set_text(GTK_LABEL(priv->label_username), _("Name"));
        gtk_widget_show(priv->account_creation);
    }
    priv->mode = Mode::ADD_RENDEZVOUS;

    /* Similarily to general settings view, we construct and destroy the avatar manipulation widget
       each time the profile is made visible / hidden. While not the most elegant solution, this
       allows us to run the preview if and only if it is displayed, and always stop it when hidden. */
    if (!priv->avatar_manipulation) {
        priv->avatar_manipulation = avatar_manipulation_new_from_wizard(priv->avModel_);
        gtk_box_pack_start(GTK_BOX(priv->box_avatarselection), priv->avatar_manipulation, true, true, 0);
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_creation), priv->account_creation);
    } else {
        /* make sure the AvatarManipulation widget is destroyed so the VideoWidget inside of it is too;
         * NOTE: destorying its parent (box_avatarselection) first will cause a mystery 'BadDrawable'
         * crash due to X error */
        gtk_container_remove(GTK_CONTAINER(priv->box_avatarselection), priv->avatar_manipulation);
        priv->avatar_manipulation = nullptr;
    }
}

