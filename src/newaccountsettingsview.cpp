/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU NewAccount Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU NewAccount Public License for more details.
 *
 *  You should have received a copy of the GNU NewAccount Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "newaccountsettingsview.h"

// std
#include <string>

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib.h>

// Lrc
#include <api/newdevicemodel.h>
#include <api/contactmodel.h>
#include <api/contact.h>
#include <api/newaccountmodel.h>

// Ring client
#include "utils/files.h"
#include "avatarmanipulation.h"

enum
{
  PROP_RING_MAIN_WIN_PNT = 1,
};

struct _NewAccountSettingsView
{
    GtkScrolledWindow parent;
};

struct _NewAccountSettingsViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _NewAccountSettingsViewPrivate NewAccountSettingsViewPrivate;

struct _NewAccountSettingsViewPrivate
{
    AccountInfoPointer const *accountInfo_ = nullptr;
    lrc::api::account::ConfProperties_t* currentProp_ = nullptr;

    GSettings *settings;
    GtkWidget* stack_account;

    GtkWidget* general_settings_box;
        GtkWidget* account_enabled;
        GtkWidget* label_status;
        GtkWidget* avatar_box;
        GtkWidget* avatarmanipulation;
        GtkWidget* entry_display_name;
        GtkWidget* type_box;
            GtkWidget* label_type_info;
    GtkWidget* vbox_devices;
        GtkWidget* list_devices;
    GtkWidget* vbox_banned_contacts;
        GtkWidget* scrolled_window_banned_contacts;
            GtkWidget* list_banned_contacts;
        GtkWidget* button_show_banned;
    GtkWidget* entry_current_password;
    GtkWidget* entry_new_password;
    GtkWidget* entry_confirm_password;
    GtkWidget* button_validate_password;
    GtkWidget* label_error_change_password;
    GtkWidget* button_export_account;
    GtkWidget* label_export_message;
    GtkWidget* button_delete_account;
    GtkWidget* button_advanced_settings;

    GtkWidget* advanced_settings_box;
    GtkWidget* button_general_settings;
    GtkWidget* call_allow_button;
    GtkWidget* auto_answer_button;
    GtkWidget* entry_name_server;
    GtkWidget* entry_dht_proxy;
    GtkWidget* dht_proxy_button;
    GtkWidget* checkbutton_use_turn;
    GtkWidget* entry_turnserver;
    GtkWidget* entry_turnusername;
    GtkWidget* entry_turnpassword;
    GtkWidget* entry_turnrealm;
    GtkWidget* upnp_button;

    QMetaObject::Connection new_device_added_connection;
    QMetaObject::Connection device_updated_connection;
    QMetaObject::Connection device_removed_connection;
    QMetaObject::Connection banned_status_changed_connection;
};

G_DEFINE_TYPE_WITH_PRIVATE(NewAccountSettingsView, new_account_settings_view, GTK_TYPE_SCROLLED_WINDOW);

#define NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), NEW_ACCOUNT_SETTINGS_VIEW_TYPE, NewAccountSettingsViewPrivate))

static void
new_account_settings_view_dispose(GObject *object)
{
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(object);

    g_clear_object(&priv->settings);

    if (priv->currentProp_) {
        delete priv->currentProp_;
        priv->currentProp_ = nullptr;
    }

    QObject::disconnect(priv->new_device_added_connection);
    QObject::disconnect(priv->device_updated_connection);
    QObject::disconnect(priv->device_removed_connection);
    QObject::disconnect(priv->banned_status_changed_connection);

    // make sure the VideoWidget is destroyed
    new_account_settings_view_show(NEW_ACCOUNT_SETTINGS_VIEW(object), FALSE);

    G_OBJECT_CLASS(new_account_settings_view_parent_class)->dispose(object);
}

static void
new_account_settings_view_init(NewAccountSettingsView *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));
}

static void
new_account_settings_view_class_init(NewAccountSettingsViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = new_account_settings_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/cx/ring/RingGnome/newaccountsettingsview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, stack_account);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, general_settings_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, account_enabled);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, avatar_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_display_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, type_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_type_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, vbox_devices);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, list_devices);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, vbox_banned_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, scrolled_window_banned_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, list_banned_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_show_banned);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_current_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_new_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_confirm_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_validate_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_error_change_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_export_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_export_message);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_delete_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_advanced_settings);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, advanced_settings_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_general_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, call_allow_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, auto_answer_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_name_server);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_dht_proxy);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, dht_proxy_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, checkbutton_use_turn);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_turnserver);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_turnusername);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_turnpassword);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_turnrealm);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, upnp_button);
}

static void
show_advanced_settings(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), priv->advanced_settings_box);
}

static void
show_general_settings(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), priv->general_settings_box);
}

// Edit general

gboolean
update_display_name(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    g_return_val_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view), false);
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return false;
    }

    priv->currentProp_->displayName = gtk_entry_get_text(GTK_ENTRY(priv->entry_display_name));
    new_account_settings_view_save_account(view);

    return false;
}
// Device related

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
get_devicename_from_row(GtkWidget* row)
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
        auto image = gtk_image_new_from_icon_name("document-edit-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(action_button->data), image);
    } else {
        auto* entry_name = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry_name), name.c_str());
        gtk_container_add(GTK_CONTAINER(box_info->data), entry_name);
        auto image = gtk_image_new_from_icon_name("document-save-symbolic", GTK_ICON_SIZE_BUTTON);
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
    current_child = g_list_last(box_devices_info);  // device.id
    return gtk_label_get_text(GTK_LABEL(current_child->data));
}

static void
save_name(GtkButton* button, NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_devices), row))) {
        if (GTK_BUTTON(get_button_from_row(children)) == button) {
            GtkWidget* nameWidget = get_devicename_from_row(children);
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
revoke_device(GtkButton* button, NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

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
            if (priv->currentProp_->archiveHasPassword) {
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
add_device(NewAccountSettingsView *view, const lrc::api::Device& device)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto* device_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    GtkStyleContext* context_box;
    context_box = gtk_widget_get_style_context(GTK_WIDGET(device_box));
    gtk_style_context_add_class(context_box, "boxitem");
    // Fill with devices informations
    auto* device_info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    auto* label_name = gtk_label_new(device.name.c_str());
    gtk_container_add(GTK_CONTAINER(device_info_box), label_name);
    gtk_widget_set_halign(GTK_WIDGET(label_name), GTK_ALIGN_FILL);
    auto* label_id = gtk_label_new("");
    std::string markup = "<span font_desc=\"7.0\">" + device.id + "</span>";
    gtk_label_set_markup(GTK_LABEL(label_id), markup.c_str());
    gtk_container_add(GTK_CONTAINER(device_info_box), label_id);
    gtk_container_add(GTK_CONTAINER(device_box), device_info_box);
    // Add action button
    auto image = gtk_image_new_from_icon_name("document-edit-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    if (!device.isCurrent)
        image = gtk_image_new_from_icon_name("dialog-error-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
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
show_revokation_error_dialog(NewAccountSettingsView *view, const std::string& text)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* top_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
    auto* error_dialog = gtk_message_dialog_new(top_window,
        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
        text.c_str());
    gtk_window_set_title(GTK_WINDOW(error_dialog), _("Error when revoking device"));
    gtk_dialog_set_default_response(GTK_DIALOG(error_dialog), GTK_RESPONSE_OK);

    gtk_dialog_run(GTK_DIALOG(error_dialog));
    gtk_widget_destroy(error_dialog);
}

// Banned contacts related
static void
on_show_banned(GtkToggleButton*, NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->button_show_banned));
    if (active) {
        gtk_button_set_label(GTK_BUTTON(priv->button_show_banned), _("Hide banned contacts"));
        auto image = gtk_image_new_from_icon_name("pan-up-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(priv->button_show_banned), image);
        gtk_widget_show_all(priv->scrolled_window_banned_contacts);
    } else {
        gtk_button_set_label(GTK_BUTTON(priv->button_show_banned), _("Show banned contacts"));
        auto image = gtk_image_new_from_icon_name("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(priv->button_show_banned), image);
        gtk_widget_hide(priv->scrolled_window_banned_contacts);
    }
}

static void
unban_contact(GtkButton* button, NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_banned_contacts), row))) {
        if (GTK_BUTTON(get_button_from_row(children)) == button) {
            auto contactId = get_id_from_row(children);
            auto contact = (*priv->accountInfo_)->contactModel->getContact(contactId);
            (*priv->accountInfo_)->contactModel->addContact(contact);
        }
        ++row;
    }
}

static void
add_banned(NewAccountSettingsView *view, const std::string& contactUri)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto* banned_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    auto contact = (*priv->accountInfo_)->contactModel->getContact(contactUri);

    GtkStyleContext* context_box;
    context_box = gtk_widget_get_style_context(GTK_WIDGET(banned_box));
    gtk_style_context_add_class(context_box, "boxitem");

    // Fill with devices informations
    auto* contact_info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    if (!contact.registeredName.empty()) {
        auto* label_name = gtk_label_new(contact.registeredName.c_str());
        gtk_container_add(GTK_CONTAINER(contact_info_box), label_name);
        gtk_widget_set_halign(GTK_WIDGET(label_name), GTK_ALIGN_START);
    }
    auto* label_id = gtk_label_new(contactUri.c_str());
    gtk_container_add(GTK_CONTAINER(contact_info_box), label_id);
    gtk_container_add(GTK_CONTAINER(banned_box), contact_info_box);
    // Add action button
    auto image = gtk_image_new_from_icon_name("contact-new-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    auto* action_banned_button = gtk_button_new();
    gtk_widget_set_tooltip_text(action_banned_button, _("Unban"));
    gtk_button_set_image(GTK_BUTTON(action_banned_button), image);
    GtkStyleContext* context;
    context = gtk_widget_get_style_context(GTK_WIDGET(action_banned_button));
    gtk_style_context_add_class(context, "transparent-button");
    std::string label_btn = "action_btn_" + contactUri;
    gtk_widget_set_name(action_banned_button, label_btn.c_str());
    g_signal_connect(action_banned_button, "clicked", G_CALLBACK(unban_contact), view);
    gtk_container_add(GTK_CONTAINER(banned_box), action_banned_button);
    // Insert at the end of the list
    gtk_list_box_insert(GTK_LIST_BOX(priv->list_banned_contacts), banned_box, -1);
}

// Password

static void
reset_change_password(NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    gtk_label_set_text(GTK_LABEL(priv->label_error_change_password), "");
    gtk_widget_hide(GTK_WIDGET(priv->label_error_change_password));
    gtk_button_set_label(GTK_BUTTON(priv->button_validate_password), _("Change password"));
}

static void
change_password(NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    std::string current_password = gtk_entry_get_text(GTK_ENTRY(priv->entry_current_password));
    std::string new_password = gtk_entry_get_text(GTK_ENTRY(priv->entry_new_password));
    std::string confirm_password = gtk_entry_get_text(GTK_ENTRY(priv->entry_confirm_password));
    if (new_password != confirm_password) {
        gtk_label_set_text(GTK_LABEL(priv->label_error_change_password), _("New password and its confirmation are different!"));
        gtk_widget_show(GTK_WIDGET(priv->label_error_change_password));
        return;
    }
    if ((*priv->accountInfo_)->accountModel->changeAccountPassword(
        (*priv->accountInfo_)->id, current_password.c_str(), new_password.c_str())) {
        gtk_label_set_text(GTK_LABEL(priv->label_error_change_password), "");
        gtk_widget_hide(GTK_WIDGET(priv->label_error_change_password));
        gtk_entry_set_text(GTK_ENTRY(priv->entry_current_password), "");
        gtk_entry_set_text(GTK_ENTRY(priv->entry_new_password), "");
        gtk_entry_set_text(GTK_ENTRY(priv->entry_confirm_password), "");
        // NOTE: can't use archiveHasPassword here, because the account is not updated yet
        gtk_widget_set_visible(priv->entry_current_password, !new_password.empty());
        gtk_button_set_label(GTK_BUTTON(priv->button_validate_password), _("Password changed!"));
    } else {
        gtk_button_set_label(GTK_BUTTON(priv->button_validate_password), _("Change password"));
        gtk_label_set_text(GTK_LABEL(priv->label_error_change_password), _("Incorrect password!"));
        gtk_widget_show(GTK_WIDGET(priv->label_error_change_password));
    }
}

// Export file

static void
choose_export_file(NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    // Get prefered path
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

    std::string alias = (*priv->accountInfo_)->profileInfo.alias;
    std::string uri = alias.empty()? "export.gz" : alias + ".gz";
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), uri.c_str());
    res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        auto chooser = GTK_FILE_CHOOSER(dialog);
        filename = gtk_file_chooser_get_filename(chooser);
    }
    gtk_widget_destroy(dialog);

    if (!filename) return;

    // export account
    auto success = (*priv->accountInfo_)->accountModel->exportToFile((*priv->accountInfo_)->id, filename);
    std::string label = success? std::string(_("Account exported to destination: ")) + std::string(filename) : _("Export account failure.");
    gtk_label_set_text(GTK_LABEL(priv->label_export_message), label.c_str());
    gtk_label_set_max_width_chars(GTK_LABEL(priv->label_export_message), 60);
    gtk_label_set_line_wrap(GTK_LABEL(priv->label_export_message), true);
    GtkStyleContext* context;
    context = gtk_widget_get_style_context(GTK_WIDGET(priv->label_export_message));
    if (gtk_style_context_has_class(context, "green_label"))
        gtk_style_context_remove_class(context, "green_label");
    if (gtk_style_context_has_class(context, "red_label"))
        gtk_style_context_remove_class(context, "red_label");
    gtk_style_context_add_class(context, success ? "green_label" : "red_label");
    gtk_widget_show_all(priv->label_export_message);
}

// Remove account

static void
remove_account(NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto* top_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
    auto* password_dialog = gtk_message_dialog_new(top_window,
        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
        _("Warning! This action will remove this account on this device!\nNote: this action cannot be undone. Also, your registered name can be lost!"));
    gtk_window_set_title(GTK_WINDOW(password_dialog), _("Delete account"));
    gtk_dialog_set_default_response(GTK_DIALOG(password_dialog), GTK_RESPONSE_OK);

    auto res = gtk_dialog_run(GTK_DIALOG(password_dialog));
    if (res == GTK_RESPONSE_OK)
        (*priv->accountInfo_)->accountModel->removeAccount((*priv->accountInfo_)->id);

    gtk_widget_destroy(password_dialog);
}

// Advanced settings

static void
update_allow_call(GtkWidget*, NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return;
    }

    auto newState = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->call_allow_button));
    if (newState != priv->currentProp_->allowIncomingFromTrusted) {
        priv->currentProp_->allowIncomingFromTrusted = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
update_auto_answer(GtkWidget*, NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return;
    }

    auto newState = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->auto_answer_button));
    if (newState != priv->currentProp_->autoAnswer) {
        priv->currentProp_->autoAnswer = newState;
        new_account_settings_view_save_account(view);
    }
}

static gboolean
update_nameserver(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    g_return_val_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view), false);
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return false;
    }

    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_name_server));
    if (newState != priv->currentProp_->RingNS.uri) {
        priv->currentProp_->RingNS.uri = newState;
        new_account_settings_view_save_account(view);
    }

    return false;
}

static gboolean
update_dhtproxy(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    g_return_val_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view), false);
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return false;
    }

    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_dht_proxy));
    if (newState != priv->currentProp_->proxyServer) {
        priv->currentProp_->proxyServer = newState;
        new_account_settings_view_save_account(view);
    }

    return false;
}

static void
enable_dhtproxy(GtkWidget*, NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return;
    }

    auto newState = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->dht_proxy_button));
    if (newState != priv->currentProp_->proxyEnabled) {
        priv->currentProp_->proxyEnabled = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
enable_turn(GtkWidget*, NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return;
    }

    auto newState = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_turn));
    if (newState != priv->currentProp_->TURN.enable) {
        priv->currentProp_->TURN.enable = newState;
        new_account_settings_view_save_account(view);
    }
}

static gboolean
update_turnserver(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    g_return_val_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view), false);
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return false;
    }

    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_turnserver));
    if (newState != priv->currentProp_->TURN.server) {
        priv->currentProp_->TURN.server = newState;
        new_account_settings_view_save_account(view);
    }

    return false;
}

static gboolean
update_turnpassword(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    g_return_val_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view), false);
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return false;
    }

    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_turnpassword));
    if (newState != priv->currentProp_->TURN.password) {
        priv->currentProp_->TURN.password = newState;
        new_account_settings_view_save_account(view);
    }

    return false;
}

static gboolean
update_turnusername(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    g_return_val_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view), false);
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return false;
    }

    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_turnusername));
    if (newState != priv->currentProp_->TURN.username) {
        priv->currentProp_->TURN.username = newState;
        new_account_settings_view_save_account(view);
    }

    return false;
}

static gboolean
update_turnrealm(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    g_return_val_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view), false);
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return false;
    }

    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_turnrealm));
    if (newState != priv->currentProp_->TURN.realm) {
        priv->currentProp_->TURN.realm = newState;
        new_account_settings_view_save_account(view);
    }

    return false;
}

static void
enable_upnp(GtkWidget*, NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return;
    }

    auto newState = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->upnp_button));
    if (newState != priv->currentProp_->upnpEnabled) {
        priv->currentProp_->upnpEnabled = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
build_settings_view(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    // CSS styles
    auto provider = gtk_css_provider_new();
    std::string css = ".transparent-button { margin-left: 10px; border: 0; background-color: rgba(0,0,0,0); margin-right: 0; padding-right: 0;}";
    css += ".boxitem { padding: 12px; }";
    css += ".green_label { color: white; background: #27ae60; width: 100%; border-radius: 3px; padding: 5px;}";
    css += ".red_label { color: white; background: #dc3a37; width: 100%; border-radius: 3px; padding: 5px;}";
    css += ".button_red { color: white; background: #dc3a37; border: 0; }";
    css += ".button_red:hover { background: #dc2719; }";
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    priv->new_device_added_connection = QObject::connect(
        &*(*priv->accountInfo_)->deviceModel,
        &lrc::api::NewDeviceModel::deviceAdded,
        [view] (const std::string& id) {
            auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
            // Retrieve added device
            auto device = (*priv->accountInfo_)->deviceModel->getDevice(id);
            if (device.id.empty()) {
                g_debug("Can't add device with id: %s", device.id.c_str());
                return;
            }
            // if exists, add to list
            add_device(view, device);
            gtk_widget_show_all(priv->list_devices);
        });

    priv->device_removed_connection = QObject::connect(
        &*(*priv->accountInfo_)->deviceModel,
        &lrc::api::NewDeviceModel::deviceRevoked,
        [view] (const std::string& id, const lrc::api::NewDeviceModel::Status status) {
            auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
            auto row = 0;
            while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_devices), row))) {
                auto deviceId = get_id_from_row(children);
                if (deviceId == id) {
                    switch (status) {
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
        });

    priv->device_updated_connection = QObject::connect(
        &*(*priv->accountInfo_)->deviceModel,
        &lrc::api::NewDeviceModel::deviceUpdated,
        [view] (const std::string& id) {
            auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
            // Retrieve added device
            auto device = (*priv->accountInfo_)->deviceModel->getDevice(id);
            if (device.id.empty()) {
                g_debug("Can't add device with id: %s", device.id.c_str());
                return;
            }
            // if exists, update
            auto row = 0;
            while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_devices), row))) {
                auto device_name = get_devicename_from_row(children);
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
        });

    priv->banned_status_changed_connection = QObject::connect(
        &*(*priv->accountInfo_)->contactModel,
        &lrc::api::ContactModel::bannedStatusChanged,
        [view] (const std::string& contactUri, bool banned) {
            auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
            if (banned) {
                add_banned(view, contactUri);
            } else {
                auto row = 0;
                while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_banned_contacts), row))) {
                    auto uri = get_id_from_row(children);
                    if (contactUri == uri)
                        gtk_container_remove(GTK_CONTAINER(priv->list_banned_contacts), children);
                    ++row;
                }
            }
            gtk_widget_show_all(priv->list_banned_contacts);
        });

    new_account_settings_view_update(view);

    GtkStyleContext* context;
    context = gtk_widget_get_style_context(GTK_WIDGET(priv->button_delete_account));
    gtk_style_context_add_class(context, "button_red");

    GtkStyleContext* context_error_label;
    context_error_label = gtk_widget_get_style_context(GTK_WIDGET(priv->label_error_change_password));
    gtk_style_context_add_class(context_error_label, "red_label");



    g_signal_connect_swapped(priv->button_advanced_settings, "clicked", G_CALLBACK(show_advanced_settings), view);
    g_signal_connect_swapped(priv->button_general_settings, "clicked", G_CALLBACK(show_general_settings), view);
    g_signal_connect(priv->button_show_banned, "toggled", G_CALLBACK(on_show_banned), view);
    g_signal_connect(priv->entry_display_name, "focus-out-event", G_CALLBACK(update_display_name), view);
    g_signal_connect_swapped(priv->button_validate_password, "clicked", G_CALLBACK(change_password), view);
    g_signal_connect_swapped(priv->entry_current_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->entry_new_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->entry_confirm_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->button_export_account, "clicked", G_CALLBACK(choose_export_file), view);
    g_signal_connect_swapped(priv->button_delete_account, "clicked", G_CALLBACK(remove_account), view);
    g_signal_connect(priv->call_allow_button, "toggled", G_CALLBACK(update_allow_call), view);
    g_signal_connect(priv->auto_answer_button, "toggled", G_CALLBACK(update_auto_answer), view);
    // TODO FAIL
    g_signal_connect(priv->entry_name_server, "focus-out-event", G_CALLBACK(update_nameserver), view);
    g_signal_connect(priv->entry_dht_proxy, "focus-out-event", G_CALLBACK(update_dhtproxy), view);
    g_signal_connect(priv->dht_proxy_button, "toggled", G_CALLBACK(enable_dhtproxy), view);
    g_signal_connect(priv->checkbutton_use_turn, "toggled", G_CALLBACK(enable_turn), view);
    g_signal_connect(priv->entry_turnserver, "focus-out-event", G_CALLBACK(update_turnserver), view);
    g_signal_connect(priv->entry_turnusername, "focus-out-event", G_CALLBACK(update_turnusername), view);
    g_signal_connect(priv->entry_turnpassword, "focus-out-event", G_CALLBACK(update_turnpassword), view);
    g_signal_connect(priv->entry_turnrealm, "focus-out-event", G_CALLBACK(update_turnrealm), view);
    g_signal_connect(priv->upnp_button, "toggled", G_CALLBACK(enable_upnp), view);
}

void
new_account_settings_view_update(NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    if (!(*priv->accountInfo_)) {
        g_debug("No specified account info");
        return;
    }

    if (priv->currentProp_) {
        delete priv->currentProp_;
        priv->currentProp_ = nullptr;
    }

    try {
        std::string accountId = (*priv->accountInfo_)->id;
        priv->currentProp_ = new lrc::api::account::ConfProperties_t();
        *priv->currentProp_ = (*priv->accountInfo_)->accountModel->getAccountConfig(accountId);
    } catch (std::out_of_range& e) {
        g_debug("Can't get acount config for current account");
        return;
    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->account_enabled), (*priv->accountInfo_)->enabled);
    gtk_widget_hide(priv->label_export_message);

    switch ((*priv->accountInfo_)->status)
    {
    case lrc::api::account::Status::INITIALIZING:
        gtk_label_set_text(GTK_LABEL(priv->label_status), _("Initializing..."));
        break;
    case lrc::api::account::Status::UNREGISTERED:
        gtk_label_set_text(GTK_LABEL(priv->label_status), _("Offline"));
        break;
    case lrc::api::account::Status::TRYING:
        gtk_label_set_text(GTK_LABEL(priv->label_status), _("Connecting..."));
        break;
    case lrc::api::account::Status::REGISTERED:
        gtk_label_set_text(GTK_LABEL(priv->label_status), _("Online"));
        break;
    case lrc::api::account::Status::INVALID:
    default:
        gtk_label_set_text(GTK_LABEL(priv->label_status), _("Unknown status"));
        break;
    }

    gtk_widget_set_visible(priv->entry_current_password, priv->currentProp_->archiveHasPassword);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_display_name), priv->currentProp_->displayName.c_str());

    if ((*priv->accountInfo_)->profileInfo.type == lrc::api::profile::Type::RING) {
        gtk_widget_show_all(priv->vbox_devices);
        gtk_widget_show_all(priv->vbox_banned_contacts);
        gtk_widget_show(priv->button_export_account);
        gtk_widget_show(priv->type_box);
        // Show ring id
        gtk_label_set_text(GTK_LABEL(priv->label_type_info), (*priv->accountInfo_)->profileInfo.uri.c_str());
        // Build devices list
        while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_devices), 0)))
            gtk_container_remove(GTK_CONTAINER(priv->list_devices), children);
        auto devices = (*priv->accountInfo_)->deviceModel->getAllDevices();
        for (const auto& device : devices)
            add_device(view, device);
        gtk_widget_set_halign(GTK_WIDGET(priv->list_devices), GTK_ALIGN_FILL);
        gtk_widget_show_all(priv->list_devices);
        // Build banned contacts list
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->button_show_banned), false);
        gtk_button_set_label(GTK_BUTTON(priv->button_show_banned), _("Show banned contacts"));
        auto image = gtk_image_new_from_icon_name("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(priv->button_show_banned), image);
        while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_banned_contacts), 0)))
            gtk_container_remove(GTK_CONTAINER(priv->list_banned_contacts), children);
        for (const auto& contact : (*priv->accountInfo_)->contactModel->getBannedContacts())
            add_banned(view, contact);
        gtk_widget_hide(priv->scrolled_window_banned_contacts);
    } else {
        gtk_widget_hide(priv->type_box);
        gtk_widget_hide(priv->vbox_devices);
        gtk_widget_hide(priv->vbox_banned_contacts);
        gtk_widget_hide(priv->button_export_account);
    }

    // advanced
    // TODO(sblin, atrazcyk) merge allowCallFrom* here?
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->call_allow_button), priv->currentProp_->allowIncomingFromTrusted);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->auto_answer_button), priv->currentProp_->autoAnswer);

    gtk_entry_set_text(GTK_ENTRY(priv->entry_name_server), priv->currentProp_->RingNS.uri.c_str());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_dht_proxy), priv->currentProp_->proxyServer.c_str());
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->dht_proxy_button), priv->currentProp_->proxyEnabled);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_turn), priv->currentProp_->TURN.enable);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnserver), priv->currentProp_->TURN.server.c_str());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnusername), priv->currentProp_->TURN.username.c_str());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnpassword), priv->currentProp_->TURN.password.c_str());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnrealm), priv->currentProp_->TURN.realm.c_str());
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->upnp_button), priv->currentProp_->upnpEnabled);
}

void
new_account_settings_view_save_account(NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    if (priv->currentProp_ && (*priv->accountInfo_))
        (*priv->accountInfo_)->accountModel->setAccountConfig((*priv->accountInfo_)->id, *priv->currentProp_);
}

GtkWidget*
new_account_settings_view_new(AccountInfoPointer const & accountInfo)
{
    gpointer view = g_object_new(NEW_ACCOUNT_SETTINGS_VIEW_TYPE, NULL);

    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    priv->accountInfo_ = &accountInfo;
    if (*priv->accountInfo_)
        build_settings_view(NEW_ACCOUNT_SETTINGS_VIEW(view));

    return reinterpret_cast<GtkWidget*>(view);
}

void
new_account_settings_view_show(NewAccountSettingsView *self, gboolean show_profile)
{
    g_return_if_fail(NEW_ACCOUNT_SETTINGS_VIEW(self));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(self);

    /* We will construct and destroy the profile (AvatarManipulation widget) each time the profile
     * should be visible and hidden, respectively. It is not the "prettiest" way of doing things,
     * but this way we ensure 1. that the profile is updated correctly when it is shown and 2. that
     * the VideoWidget inside is destroyed when it is not being shown.
     */
    if (show_profile) {
        /* avatar manipulation widget */
        priv->avatarmanipulation = avatar_manipulation_new();
        gtk_widget_set_halign(priv->avatar_box, GtkAlign::GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(priv->avatar_box), priv->avatarmanipulation, true, true, 0);
        gtk_widget_set_visible(priv->avatarmanipulation, true);

    } else {
        if (priv->avatarmanipulation) {
            gtk_container_remove(GTK_CONTAINER(priv->avatar_box), priv->avatarmanipulation);
            priv->avatarmanipulation = nullptr;
        }
    }
}
