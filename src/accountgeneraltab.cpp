/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
 *  Author: Sebastien Blin <sebastien.blin@savoirfairelinux.com>
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#include <iostream>
#include <api/newdevicemodel.h>
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

    GtkWidget* stack_account_general;
    GtkWidget* account_general;

    GtkWidget* grid_account;
    GtkWidget* frame_parameters;
    GtkWidget* grid_parameters;

    GtkWidget* entry_current_password;
    GtkWidget* entry_new_password;
    GtkWidget* entry_confirm_password;
    GtkWidget* button_validate_password;
    GtkWidget* label_error_change_passowrd;

    GtkWidget* button_choose_file;
    GtkWidget* label_export_informations;

    GtkWidget* list_devices;
    GtkWidget* vbox_devices;
    GtkWidget* button_add_device;

    /* generated_pin view */
    GtkWidget* generated_pin;
    GtkWidget* label_generated_pin;
    GtkWidget* button_generated_pin_ok;

    /* add_device view */
    GtkWidget* add_device_box;
    GtkWidget* button_export_on_the_ring;
    GtkWidget* button_add_device_cancel;
    GtkWidget* entry_password;

    /* generating account spinner */
    GtkWidget* vbox_generating_pin_spinner;

    /* export on ring error */
    GtkWidget* export_on_ring_error;
    GtkWidget* label_export_on_ring_error;
    GtkWidget* button_export_on_ring_error_ok;

    QMetaObject::Connection account_updated;
    QMetaObject::Connection new_device_added_connection;
    QMetaObject::Connection device_updated_connection;
    QMetaObject::Connection device_removed_connection;
    QMetaObject::Connection export_on_ring_ended;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountGeneralTab, account_general_tab, GTK_TYPE_BOX);

#define ACCOUNT_GENERAL_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_GENERAL_TAB_TYPE, AccountGeneralTabPrivate))

static void
account_general_tab_dispose(GObject *object)
{
    AccountGeneralTab *view = ACCOUNT_GENERAL_TAB(object);
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);

    QObject::disconnect(priv->account_updated);
    QObject::disconnect(priv->new_device_added_connection);
    QObject::disconnect(priv->device_updated_connection);
    QObject::disconnect(priv->device_removed_connection);
    QObject::disconnect(priv->export_on_ring_ended);

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

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, stack_account_general);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, account_general);

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

    // Devices
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, list_devices);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, vbox_devices);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, button_add_device);

    // add_device view
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, add_device_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, button_export_on_the_ring);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, button_add_device_cancel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, entry_password);

    // generating pin spinner
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, vbox_generating_pin_spinner);

    // export on ring error
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, export_on_ring_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, label_export_on_ring_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, button_export_on_ring_error_ok);

    // generated_pin view
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, generated_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, label_generated_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, button_generated_pin_ok);
}

static void
show_account_general_view(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_general), priv->account_general);
}

static void
show_add_device_view(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_general), priv->add_device_box);
}

static void
show_generated_pin_view(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    gtk_widget_show(priv->generated_pin);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_general), priv->generated_pin);
}

static void
show_generating_pin_spinner(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_general), priv->vbox_generating_pin_spinner);
}

static void
show_export_on_ring_error(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account_general), priv->export_on_ring_error);
}

static void
export_on_the_ring_clicked(G_GNUC_UNUSED GtkButton *button, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);

    auto password = QString(gtk_entry_get_text(GTK_ENTRY(priv->entry_password)));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password), "");

    priv->export_on_ring_ended = QObject::connect(
        priv->account,
        &Account::exportOnRingEnded,
        [=] (Account::ExportOnRingStatus status, QString pin) {
            QObject::disconnect(priv->export_on_ring_ended);
            switch (status)
            {
                case Account::ExportOnRingStatus::SUCCESS:
                {
                    gtk_label_set_text(GTK_LABEL(priv->label_generated_pin), pin.toStdString().c_str());
                    show_generated_pin_view(view);
                    break;
                }
                case Account::ExportOnRingStatus::WRONG_PASSWORD:
                {
                    gtk_label_set_text(GTK_LABEL(priv->label_export_on_ring_error), _("Bad password"));
                    show_export_on_ring_error(view);
                    break;
                }
                case Account::ExportOnRingStatus::NETWORK_ERROR:
                {
                    gtk_label_set_text(GTK_LABEL(priv->label_export_on_ring_error), _("Network error, try again"));
                    show_export_on_ring_error(view);
                    break;
                }
            }
        }
    );

    show_generating_pin_spinner(view);
    if (!priv->account->exportOnRing(password))
    {
        QObject::disconnect(priv->export_on_ring_ended);
        gtk_label_set_text(GTK_LABEL(priv->label_export_on_ring_error), _("Could not initiate export to the Ring, try again"));
        g_debug("Could not initiate exportOnRing operation");
        show_export_on_ring_error(view);
    }
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

static GList*
get_row_iterator(GtkWidget* row)
{
    auto box = gtk_bin_get_child(GTK_BIN(row));
    return gtk_container_get_children(GTK_CONTAINER(box));
}

static GtkWidget*
get_button_from_row(GtkWidget* row)
{
    auto* list_iterator = get_row_iterator(row);
    auto current_child = g_list_last(list_iterator);  // button
    return GTK_WIDGET(current_child->data);
}

static GtkWidget*
get_name_from_row(GtkWidget* row)
{
    auto* list_iterator = get_row_iterator(row);
    auto* current_child = g_list_first(list_iterator);  // box infos
    auto box_devices_info = gtk_container_get_children(GTK_CONTAINER(current_child->data));
    current_child = g_list_first(box_devices_info);  // device.name
    return GTK_WIDGET(current_child->data);
}

static void
replace_name_from_row(GtkWidget* row, const std::string& name, const std::string& id)
{
    // Remove previous informations
    auto* list_iterator = get_row_iterator(row);
    auto* box_info = g_list_first(list_iterator);  // box infos
    auto* action_button = g_list_last(list_iterator);
    auto box_devices_info = gtk_container_get_children(GTK_CONTAINER(box_info->data));
    auto* device_name = g_list_first(box_devices_info);  // device.name
    auto isEntryName = G_TYPE_CHECK_INSTANCE_TYPE(device_name->data, gtk_entry_get_type());
    auto* label_id = g_list_next(box_devices_info);  // device.name
    gtk_container_remove(GTK_CONTAINER(box_info->data), GTK_WIDGET(device_name->data));
    gtk_container_remove(GTK_CONTAINER(box_info->data), GTK_WIDGET(label_id->data));

    // Rebuild item
    if (isEntryName) {
        auto* new_label_name = gtk_label_new(name.c_str());
        gtk_container_add(GTK_CONTAINER(box_info->data), new_label_name);
        gtk_widget_set_halign(GTK_WIDGET(new_label_name), GTK_ALIGN_START);
        auto image = gtk_image_new_from_resource("/cx/ring/RingGnome/edit");
        gtk_button_set_image(GTK_BUTTON(action_button->data), image);
    } else {
        auto* entry_name = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry_name), name.c_str());
        gtk_container_add(GTK_CONTAINER(box_info->data), entry_name);
        auto image = gtk_image_new_from_resource("/cx/ring/RingGnome/save");
        gtk_button_set_image(GTK_BUTTON(action_button->data), image);
    }
    auto* new_label_id = gtk_label_new("");
    std::string markup = "<span font_desc=\"7.0\">" + id + "</span>";
    gtk_label_set_markup(GTK_LABEL(new_label_id), markup.c_str());
    gtk_container_add(GTK_CONTAINER(box_info->data), new_label_id);
    gtk_widget_show_all(GTK_WIDGET(box_info->data));
}

static std::string
get_id_from_row(GtkWidget* row)
{
    auto* list_iterator = get_row_iterator(row);
    auto* current_child = g_list_first(list_iterator);  // box infos
    auto box_devices_info = gtk_container_get_children(GTK_CONTAINER(current_child->data));
    current_child = g_list_first(box_devices_info);  // device.name
    current_child = g_list_next(box_devices_info);  // device.id
    return gtk_label_get_text(GTK_LABEL(current_child->data));
}

static void
save_name(GtkButton* button, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);

    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_devices), row))) {
        if (GTK_BUTTON(get_button_from_row(children)) == button) {
            GtkWidget* nameWidget = get_name_from_row(children);
            auto deviceId = get_id_from_row(children);
            if (G_TYPE_CHECK_INSTANCE_TYPE(nameWidget, gtk_entry_get_type())) {
                std::string newName = gtk_entry_get_text(GTK_ENTRY(nameWidget));
                replace_name_from_row(children, newName, deviceId);
                (*priv->accountInfo_)->deviceModel->setCurrentDeviceName(newName);
            } else {
                std::string newName = gtk_label_get_text(GTK_LABEL(nameWidget));
                replace_name_from_row(children, newName, deviceId);
            }
        }
        ++row;
    }
}

static void
revoke_device(GtkButton* button, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);

    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_devices), row))) {
        if (GTK_BUTTON(get_button_from_row(children)) == button) {
            auto deviceId = get_id_from_row(children);
            auto password = "";
            auto* top_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
            auto* password_dialog = gtk_message_dialog_new(top_window,
                GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                _("Warning! This action will revoke the device!\nNote: this action cannot be undone."));
            gtk_window_set_title(GTK_WINDOW(password_dialog), _("Enter password to revoke device"));
            gtk_dialog_set_default_response(GTK_DIALOG(password_dialog), GTK_RESPONSE_OK);

            auto* message_area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(password_dialog));
            if (priv->account->archiveHasPassword()) {
                auto* password_entry = gtk_entry_new();
                gtk_entry_set_visibility(GTK_ENTRY(password_entry), false);
                gtk_entry_set_invisible_char(GTK_ENTRY(password_entry), '*');
                gtk_box_pack_start(GTK_BOX(message_area), password_entry, true, true, 0);
                gtk_widget_show_all(password_dialog);

                auto res = gtk_dialog_run(GTK_DIALOG(password_dialog));
                if (res == GTK_RESPONSE_OK) {
                    password = gtk_entry_get_text(GTK_ENTRY(password_entry));
                    (*priv->accountInfo_)->deviceModel->revokeDevice(deviceId, password);
                }
            } else {
                auto res = gtk_dialog_run(GTK_DIALOG(password_dialog));
                if (res == GTK_RESPONSE_OK)
                    (*priv->accountInfo_)->deviceModel->revokeDevice(deviceId, "");
            }

            gtk_widget_destroy(password_dialog);
        }
        ++row;
    }
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
add_device(AccountGeneralTab *view, const lrc::api::Device& device)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);

    auto* device_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    GtkStyleContext* context_box;
    context_box = gtk_widget_get_style_context(GTK_WIDGET(device_box));
    gtk_style_context_add_class(context_box, "device-box");
    // Fill with devices informations
    auto* device_info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    auto* label_name = gtk_label_new(device.name.c_str());
    gtk_container_add(GTK_CONTAINER(device_info_box), label_name);
    gtk_widget_set_halign(GTK_WIDGET(label_name), GTK_ALIGN_START);
    auto* label_id = gtk_label_new("");
    std::string markup = "<span font_desc=\"7.0\">" + device.id + "</span>";
    gtk_label_set_markup(GTK_LABEL(label_id), markup.c_str());
    gtk_container_add(GTK_CONTAINER(device_info_box), label_id);
    gtk_container_add(GTK_CONTAINER(device_box), device_info_box);
    // Add action button
    auto image = gtk_image_new_from_resource("/cx/ring/RingGnome/edit");
    if (!device.isCurrent)
        image = gtk_image_new_from_resource("/cx/ring/RingGnome/revoke");
    auto* action_device_button = gtk_button_new();
    gtk_widget_set_tooltip_text(action_device_button, device.isCurrent ? _("Save name") : _("Revoke device"));
    gtk_button_set_image(GTK_BUTTON(action_device_button), image);
    GtkStyleContext* context;
    context = gtk_widget_get_style_context(GTK_WIDGET(action_device_button));
    gtk_style_context_add_class(context, "transparent-button");
    std::string label_btn = "action_btn_" + device.id;
    gtk_widget_set_name(action_device_button, label_btn.c_str());
    g_signal_connect(action_device_button, "clicked", device.isCurrent ? G_CALLBACK(save_name) : G_CALLBACK(revoke_device), view);
    gtk_container_add(GTK_CONTAINER(device_box), action_device_button);
    // Insert at the end of the list
    gtk_list_box_insert(GTK_LIST_BOX(priv->list_devices), device_box, -1);
}

static void
show_revokation_error_dialog(AccountGeneralTab *view, const std::string& text)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    auto* top_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
    auto* error_dialog = gtk_message_dialog_new(top_window,
        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
        "%s", text.c_str());
    gtk_window_set_title(GTK_WINDOW(error_dialog), _("Error when revoking device"));
    gtk_dialog_set_default_response(GTK_DIALOG(error_dialog), GTK_RESPONSE_OK);

    gtk_dialog_run(GTK_DIALOG(error_dialog));
    gtk_widget_destroy(error_dialog);
}

static void
build_tab_view(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);

    // CSS styles
    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".transparent-button { margin-left: 10px; border: 0; background-color: rgba(0,0,0,0); margin-right: 0; padding-right: 0;}\
         .device-box { padding: 12px; }",
        -1, nullptr
    );
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    int grid_row = 0;
    GtkWidget* label = NULL;

    /* separate pointers for each so that we reference them in the account changed callback */
    GtkWidget* entry_alias = NULL;
    GtkWidget* entry_username = NULL;
    GtkWidget* entry_hostname = NULL;
    GtkWidget* entry_password = NULL;
    GtkWidget* entry_proxy = NULL;
    GtkWidget* entry_voicemail = NULL;

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
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry_username), _("auto-generating…"));
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
        auto username_registration_box = username_registration_box_new(*priv->accountInfo_, TRUE);
        gtk_grid_attach(GTK_GRID(priv->grid_account), username_registration_box, 1, grid_row, 2, 2);
        gtk_widget_show(username_registration_box);

        grid_row+=2;
    }


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
        GtkWidget* checkbutton = gtk_check_button_new_with_label(_("Show password"));
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

    priv->new_device_added_connection = QObject::connect(
        &*(*priv->accountInfo_)->deviceModel,
        &lrc::api::NewDeviceModel::deviceAdded,
        [view] (const std::string& id) {
            AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
            // Retrieve added device
            auto device = (*priv->accountInfo_)->deviceModel->getDevice(id);
            if (device.id.empty()) {
                g_debug("Can't add device with id: %s", device.id.c_str());
                return;
            }
            // if exists, add to list
            add_device(view, device);
            gtk_widget_show_all(priv->list_devices);
        }
    );

    priv->device_removed_connection = QObject::connect(
        &*(*priv->accountInfo_)->deviceModel,
        &lrc::api::NewDeviceModel::deviceRevoked,
        [view] (const std::string& id, const lrc::api::NewDeviceModel::Status status) {
            AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
            auto row = 0;
            while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_devices), row))) {
                auto deviceId = get_id_from_row(children);
                if (deviceId == id) {
                    switch (status)
                    {
                    case lrc::api::NewDeviceModel::Status::SUCCESS:
                        // remove device's line
                        gtk_container_remove(GTK_CONTAINER(priv->list_devices), children);
                        return;
                    case lrc::api::NewDeviceModel::Status::WRONG_PASSWORD:
                        show_revokation_error_dialog(view, _("Error: wrong password!"));
                        break;
                    case lrc::api::NewDeviceModel::Status::UNKNOWN_DEVICE:
                        show_revokation_error_dialog(view, _("Error: unknown device!"));
                        break;
                    default:
                        g_debug("unknown status for revoked device. BUG?");
                        return;
                    }
                }
                ++row;
            }
        }
    );

    priv->device_updated_connection = QObject::connect(
        &*(*priv->accountInfo_)->deviceModel,
        &lrc::api::NewDeviceModel::deviceUpdated,
        [view] (const std::string& id) {
            AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
            // Retrieve added device
            auto device = (*priv->accountInfo_)->deviceModel->getDevice(id);
            if (device.id.empty()) {
                g_debug("Can't add device with id: %s", device.id.c_str());
                return;
            }
            // if exists, update
            auto row = 0;
            while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_devices), row))) {
                auto device_name = get_name_from_row(children);
                auto deviceId = get_id_from_row(children);
                if (deviceId == id) {
                    if (device.isCurrent) {
                        if (G_TYPE_CHECK_INSTANCE_TYPE(device_name, gtk_entry_get_type())) {
                            gtk_entry_set_text(GTK_ENTRY(device_name), device.name.c_str());
                        } else {
                            gtk_label_set_text(GTK_LABEL(device_name), device.name.c_str());
                        }
                    } else {
                        gtk_label_set_text(GTK_LABEL(device_name), device.name.c_str());
                    }
                }
                ++row;
            }
            g_debug("deviceUpdated signal received, but device not found");
        }
    );


    g_signal_connect_swapped(priv->button_validate_password, "clicked", G_CALLBACK(change_password), view);
    g_signal_connect_swapped(priv->entry_current_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->entry_new_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->entry_confirm_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->button_choose_file, "clicked", G_CALLBACK(choose_export_file), view);
    g_signal_connect_swapped(priv->button_add_device, "clicked", G_CALLBACK(show_add_device_view), view);
    g_signal_connect_swapped(priv->button_add_device_cancel, "clicked", G_CALLBACK(show_account_general_view), view);
    g_signal_connect(priv->button_export_on_the_ring, "clicked", G_CALLBACK(export_on_the_ring_clicked), view);
    g_signal_connect_swapped(priv->button_generated_pin_ok, "clicked", G_CALLBACK(show_account_general_view), view);
    g_signal_connect_swapped(priv->button_export_on_ring_error_ok, "clicked", G_CALLBACK(show_add_device_view), view);
    gtk_widget_set_visible(priv->entry_current_password, priv->account->archiveHasPassword());

    if ((*priv->accountInfo_)->profileInfo.type == lrc::api::profile::Type::RING) {
        auto devices = (*priv->accountInfo_)->deviceModel->getAllDevices();
        for (const auto& device : devices)
            add_device(view, device);
        gtk_widget_set_halign(GTK_WIDGET(priv->list_devices), GTK_ALIGN_CENTER);
        gtk_widget_show_all(priv->list_devices);
    } else {
        gtk_widget_hide(priv->vbox_devices);
    }

}

GtkWidget*
account_general_tab_new(Account *account, AccountInfoPointer const & accountInfo)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_GENERAL_TAB_TYPE, NULL);

    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account = account;
    priv->accountInfo_ = &accountInfo;

    build_tab_view(ACCOUNT_GENERAL_TAB(view));

    return (GtkWidget* )view;
}
