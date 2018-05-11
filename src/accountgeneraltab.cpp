/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Sebastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "accountgeneraltab.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <account.h>
#include "defines.h"
#include "utils/models.h"
#include "usernameregistrationbox.h"

#include <api/newaccountmodel.h>

struct _AccountGeneralTab
{
    GtkBox parent;
};

struct _AccountGeneralTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountGeneralTabPrivate AccountGeneralTabPrivate;

struct _AccountGeneralTabPrivate
{
    Account   *account;
    AccountInfoPointer const *accountInfo_;

    GtkWidget *grid_account;
    GtkWidget* frame_parameters;
    GtkWidget *grid_parameters;

    GtkWidget* entry_current_password;
    GtkWidget* entry_new_password;
    GtkWidget* entry_confirm_password;
    GtkWidget* button_validate_password;
    GtkWidget* label_error_change_passowrd;

    GtkWidget* button_choose_file;
    GtkWidget* label_export_informations;

    QMetaObject::Connection account_updated;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountGeneralTab, account_general_tab, GTK_TYPE_BOX);

#define ACCOUNT_GENERAL_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_GENERAL_TAB_TYPE, AccountGeneralTabPrivate))

static void
account_general_tab_dispose(GObject *object)
{
    AccountGeneralTab *view = ACCOUNT_GENERAL_TAB(object);
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);

    QObject::disconnect(priv->account_updated);

    G_OBJECT_CLASS(account_general_tab_parent_class)->dispose(object);
}

static void
account_general_tab_init(AccountGeneralTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
account_general_tab_class_init(AccountGeneralTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_general_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountgeneraltab.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, grid_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, frame_parameters);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, grid_parameters);

    /* change password view */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, entry_current_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, entry_new_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, entry_confirm_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, button_validate_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, label_error_change_passowrd);

    /* export account */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, button_choose_file);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, label_export_informations);

}

static void
account_alias_changed(GtkEditable *entry, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account->setAlias(QString(gtk_editable_get_chars(entry, 0, -1)));
    if (priv->account->protocol() == Account::Protocol::RING)
        priv->account->setDisplayName(gtk_entry_get_text(GTK_ENTRY(entry)));
}

static void
account_hostname_changed(GtkEditable *entry, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account->setHostname(QString(gtk_editable_get_chars(entry, 0, -1)));
}

static void
account_username_changed(GtkEditable *entry, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account->setUsername(QString(gtk_editable_get_chars(entry, 0, -1)));
}

static void
account_password_changed(GtkEditable *entry, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account->setPassword(QString(gtk_editable_get_chars(entry, 0, -1)));
}

static void
show_password(GtkToggleButton *checkbutton, GtkEntry *entry)
{
    gtk_entry_set_visibility(GTK_ENTRY(entry), gtk_toggle_button_get_active(checkbutton));
}

static void
account_proxy_changed(GtkEditable *entry, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account->setProxy(QString(gtk_editable_get_chars(entry, 0, -1)));
}

static void
account_mailbox_changed(GtkEditable *entry, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account->setMailbox(QString(gtk_editable_get_chars(entry, 0, -1)));
}

static void
reset_change_password(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    gtk_label_set_text(GTK_LABEL(priv->label_error_change_passowrd), "");
    gtk_button_set_label(GTK_BUTTON(priv->button_validate_password), _("Change password"));
}


static void
choose_export_file(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    // Sauvegarder path
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

    std::string alias = priv->account->alias().toLocal8Bit().constData();
    std::string uri = alias.empty()? "export.gz" : alias + ".gz";
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), uri.c_str());
    res = gtk_dialog_run (GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        auto chooser = GTK_FILE_CHOOSER(dialog);
        filename = gtk_file_chooser_get_filename(chooser);
    }
    gtk_widget_destroy (dialog);

    if (!filename) return;

    // export account
    priv->account->exportToFile(filename);
    // Set informations label
    std::string label = std::string(_("File exported to destination: ")) + std::string(filename);
    gtk_label_set_text(GTK_LABEL(priv->label_export_informations), label.c_str());
}

static void
change_password(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);

    std::string current_password = gtk_entry_get_text(GTK_ENTRY(priv->entry_current_password));
    std::string new_password = gtk_entry_get_text(GTK_ENTRY(priv->entry_new_password));
    std::string confirm_password = gtk_entry_get_text(GTK_ENTRY(priv->entry_confirm_password));
    if (new_password != confirm_password) {
        gtk_label_set_text(GTK_LABEL(priv->label_error_change_passowrd), _("New password and its confirmation are different"));
        return;
    }
    if (priv->account->changePassword(current_password.c_str(), new_password.c_str())) {
        gtk_label_set_text(GTK_LABEL(priv->label_error_change_passowrd), "");
        gtk_entry_set_text(GTK_ENTRY(priv->entry_current_password), "");
        gtk_entry_set_text(GTK_ENTRY(priv->entry_new_password), "");
        gtk_entry_set_text(GTK_ENTRY(priv->entry_confirm_password), "");
        // NOTE: can't use account->archiveHasPassword here, because the account is not updated.
        gtk_widget_set_visible(priv->entry_current_password, !new_password.empty());
        gtk_button_set_label(GTK_BUTTON(priv->button_validate_password), _("Password changed!"));
    } else {
        gtk_button_set_label(GTK_BUTTON(priv->button_validate_password), _("Change password"));
        gtk_label_set_text(GTK_LABEL(priv->label_error_change_passowrd), _("Incorrect password"));
    }

}

static void
build_tab_view(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);

    int grid_row = 0;
    GtkWidget *label = NULL;

    /* separate pointers for each so that we reference them in the account changed callback */
    GtkWidget *entry_alias = NULL;
    GtkWidget *entry_username = NULL;
    GtkWidget *entry_hostname = NULL;
    GtkWidget *entry_password = NULL;
    GtkWidget *entry_proxy = NULL;
    GtkWidget *entry_voicemail = NULL;

    /* build account grid */

    /* account alias */
    label = gtk_label_new(_("Alias"));
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(priv->grid_account), label, 0, grid_row, 1, 1);
    entry_alias = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_alias), priv->account->alias().toLocal8Bit().constData());
    gtk_widget_set_halign(entry_alias, GTK_ALIGN_START);
    g_signal_connect(entry_alias, "changed", G_CALLBACK(account_alias_changed), view);
    gtk_grid_attach(GTK_GRID(priv->grid_account), entry_alias, 1, grid_row, 1, 1);
    ++grid_row;

    /* account type */
    label = gtk_label_new(_("Type"));
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(priv->grid_account), label, 0, grid_row, 1, 1);

    label = gtk_label_new("");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    switch (priv->account->protocol()) {
        case Account::Protocol::SIP:
            gtk_label_set_text(GTK_LABEL(label), "SIP");
            break;
        case Account::Protocol::RING:
            gtk_label_set_text(GTK_LABEL(label), "RING");
            break;
        case Account::Protocol::COUNT__:
            break;
    }

    gtk_grid_attach(GTK_GRID(priv->grid_account), label, 1, grid_row, 1, 1);
    ++grid_row;

    if (priv->account->protocol() == Account::Protocol::RING) {
        label = gtk_label_new("RingID");
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_account), label, 0, grid_row, 1, 1);
        entry_username = gtk_entry_new();
        gtk_widget_set_halign(entry_username, GTK_ALIGN_START);
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry_username), _("auto-generating..."));
        gtk_entry_set_text(GTK_ENTRY(entry_username), priv->account->username().toLocal8Bit().constData());
        g_object_set(G_OBJECT(entry_username), "editable", FALSE, NULL);
        g_object_set(G_OBJECT(entry_username), "max-width-chars", 50, NULL);
        PangoAttrList *attrs = pango_attr_list_new();
        PangoAttribute *font_desc = pango_attr_font_desc_new(pango_font_description_from_string("monospace"));
        pango_attr_list_insert(attrs, font_desc);
        gtk_entry_set_attributes(GTK_ENTRY(entry_username), attrs);
        pango_attr_list_unref(attrs);
        gtk_entry_set_alignment(GTK_ENTRY(entry_username), 0.5);
        gtk_grid_attach(GTK_GRID(priv->grid_account), entry_username, 1, grid_row, 1, 1);
        ++grid_row;

        label = gtk_label_new(_("Username"));
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_account), label, 0, grid_row, 1, 1);
        auto username_registration_box = username_registration_box_new(priv->account, TRUE);
        gtk_grid_attach(GTK_GRID(priv->grid_account), username_registration_box, 1, grid_row, 2, 2);
        grid_row+=2;
    }

    gtk_widget_show_all(priv->grid_account);

    /* build parameters grid */
    grid_row = 0;
    if (priv->account->protocol() != Account::Protocol::RING) {
        /* SIP account */

        /* host name */
        label = gtk_label_new(_("Hostname"));
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
        entry_hostname = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry_hostname), priv->account->hostname().toLocal8Bit().constData());
        g_signal_connect(entry_hostname, "changed", G_CALLBACK(account_hostname_changed), view);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry_hostname, 1, grid_row, 1, 1);
        ++grid_row;

        /* user name */
        label = gtk_label_new(_("Username"));
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
        entry_username = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry_username), priv->account->username().toLocal8Bit().constData());
        g_signal_connect(entry_username, "changed", G_CALLBACK(account_username_changed), view);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry_username, 1, grid_row, 1, 1);
        ++grid_row;

        /* password */
        label = gtk_label_new(_("Password"));
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
        entry_password = gtk_entry_new();
        gtk_entry_set_input_purpose(GTK_ENTRY(entry_password), GTK_INPUT_PURPOSE_PASSWORD);
        gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry_password), GTK_ENTRY_ICON_PRIMARY, "dialog-password");
        gtk_entry_set_visibility(GTK_ENTRY(entry_password), FALSE);
        gtk_entry_set_text(GTK_ENTRY(entry_password), priv->account->password().toLocal8Bit().constData());
        g_signal_connect(entry_password, "changed", G_CALLBACK(account_password_changed), view);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry_password, 1, grid_row, 1, 1);
        ++grid_row;

        /* show password */
        GtkWidget *checkbutton = gtk_check_button_new_with_label(_("Show password"));
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), checkbutton, 1, grid_row, 1, 1);
        g_signal_connect(checkbutton, "toggled", G_CALLBACK(show_password), entry_password);
        ++grid_row;

        /* proxy */
        label = gtk_label_new(_("Proxy"));
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
        entry_proxy = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry_proxy), priv->account->proxy().toLocal8Bit().constData());
        g_signal_connect(entry_proxy, "changed", G_CALLBACK(account_proxy_changed), view);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry_proxy, 1, grid_row, 1, 1);
        ++grid_row;

        /* voicemail number */
        label = gtk_label_new(_("Voicemail number"));
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
        entry_voicemail = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry_voicemail), priv->account->mailbox().toLocal8Bit().constData());
        g_signal_connect(entry_voicemail, "changed", G_CALLBACK(account_mailbox_changed), view);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry_voicemail, 1, grid_row, 1, 1);
        ++grid_row;

    } else {
        gtk_widget_hide(priv->frame_parameters);
    }
    gtk_widget_show_all(priv->grid_parameters);

    /* update account parameters if model is updated */
    priv->account_updated = QObject::connect(
        priv->account,
        &Account::changed,
        [=] () {
            gtk_entry_set_text(GTK_ENTRY(entry_alias), priv->account->alias().toLocal8Bit().constData());
            gtk_entry_set_text(GTK_ENTRY(entry_username), priv->account->username().toLocal8Bit().constData());

            if (priv->account->protocol() != Account::Protocol::RING) {
                gtk_entry_set_text(GTK_ENTRY(entry_hostname), priv->account->hostname().toLocal8Bit().constData());
                gtk_entry_set_text(GTK_ENTRY(entry_password), priv->account->password().toLocal8Bit().constData());
                gtk_entry_set_text(GTK_ENTRY(entry_proxy), priv->account->proxy().toLocal8Bit().constData());
                gtk_entry_set_text(GTK_ENTRY(entry_voicemail), priv->account->mailbox().toLocal8Bit().constData());
            }
        }
    );

    g_signal_connect_swapped(priv->button_validate_password, "clicked", G_CALLBACK(change_password), view);
    g_signal_connect_swapped(priv->entry_current_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->entry_new_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->entry_confirm_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->button_choose_file, "clicked", G_CALLBACK(choose_export_file), view);
    gtk_widget_set_visible(priv->entry_current_password, priv->account->archiveHasPassword());

}

GtkWidget *
account_general_tab_new(Account *account, AccountInfoPointer const & accountInfo)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_GENERAL_TAB_TYPE, NULL);

    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account = account;
    priv->accountInfo_ = &accountInfo;

    build_tab_view(ACCOUNT_GENERAL_TAB(view));

    return (GtkWidget *)view;
}
