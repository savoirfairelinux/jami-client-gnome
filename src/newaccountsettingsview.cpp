/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *  Author: Sebastien Blin <sebastien.blin@savoirfairelinux.com>
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
#include <api/newcodecmodel.h>

// Ring client
#include "avatarmanipulation.h"
#include "defines.h"
#include "utils/files.h"
#include "usernameregistrationbox.h"

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
        GtkWidget* username_box;
        GtkWidget* sip_info_box;
            GtkWidget* entry_sip_hostname;
            GtkWidget* entry_sip_username;
            GtkWidget* entry_sip_password;
            GtkWidget* entry_sip_proxy;
            GtkWidget* entry_sip_voicemail;
    GtkWidget* vbox_devices;
        GtkWidget* list_devices;
    GtkWidget* vbox_banned_contacts;
        GtkWidget* scrolled_window_banned_contacts;
            GtkWidget* list_banned_contacts;
        GtkWidget* button_show_banned;
    GtkWidget* vbox_change_password;
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
    GtkWidget* allow_call_row;
        GtkWidget* call_allow_button;
    GtkWidget* auto_answer_button;
    GtkWidget* custom_ringtone_button;
    GtkWidget* filechooserbutton_custom_ringtone;
    GtkWidget* box_name_server;
        GtkWidget* entry_name_server;
    GtkWidget* box_dht_proxy;
        GtkWidget* entry_dht_proxy;
        GtkWidget* dht_proxy_button;
    GtkWidget* frame_bootstrap_servers;
        GtkWidget* box_bootstrap_servers;
    GtkWidget *treeview_bootstrap_servers;
    GtkWidget* sip_encrypt_media_row;
        GtkWidget* sip_encrypt_media;
    GtkWidget* sip_key_exchange_row;
        GtkWidget* enable_sdes;
    GtkWidget* sip_fallback_rtp_row;
        GtkWidget* sip_fallback_rtp;
    GtkWidget* sip_encrypt_negotiation;
        GtkWidget* sip_enable_TLS;
    GtkWidget* sip_verify_certs_server_row;
        GtkWidget* sip_verify_certs_server;
    GtkWidget* sip_verify_certs_client_row;
        GtkWidget* sip_verify_certs_client;
    GtkWidget* sip_require_incoming_tls_certs_row;
        GtkWidget* sip_require_incoming_tls_certs;
    GtkWidget* sip_tls_protocol_row;
        GtkWidget* combobox_tls_protocol_method;
    GtkWidget* sip_tls_server_name_row;
        GtkWidget* entry_tls_server_name;
    GtkWidget* sip_negotiation_timeout_row;
        GtkWidget* spinbutton_negotiation_timeout;
    GtkWidget* filechooserbutton_ca_list;
    GtkWidget* filechooserbutton_certificate;
    GtkWidget* filechooserbutton_private_key;
    GtkWidget* entry_password;
    GtkWidget* sip_registration_expire_row;
        GtkWidget* spinbutton_registration_timeout;
    GtkWidget* sip_network_interface_row;
        GtkWidget* spinbutton_network_interface;
    GtkWidget* upnp_button;
    GtkWidget* switch_use_turn;
    GtkWidget* entry_turnserver;
    GtkWidget* entry_turnusername;
    GtkWidget* entry_turnpassword;
    GtkWidget* entry_turnrealm;
    GtkWidget* switch_use_stun;
    GtkWidget* entry_stunserver;
    GtkWidget* box_published_address;
        GtkWidget* button_custom_published;
        GtkWidget* entry_published_address;
        GtkWidget* spinbutton_published_port;
    GtkWidget* list_video_codecs;
    GtkWidget* button_up_video;
    GtkWidget* button_down_video;
    GtkWidget* switch_enable_video;
    GtkWidget* list_audio_codecs;
    GtkWidget* button_up_audio;
    GtkWidget* button_down_audio;
    GtkWidget* box_sdp_session;
        GtkWidget* spinbutton_audio_rtp_min;
        GtkWidget* spinbutton_audio_rtp_max;
        GtkWidget* spinbutton_video_rtp_min;
        GtkWidget* spinbutton_video_rtp_max;

    // Link
    GtkWidget* button_add_device;

    /* generated_pin view */
    GtkWidget* generated_pin;
    GtkWidget* label_generated_pin;
    GtkWidget* button_generated_pin_ok;

    /* add_device view */
    GtkWidget* add_device_box;
    GtkWidget* button_export_on_the_ring;
    GtkWidget* button_add_device_cancel;
    GtkWidget* entry_password_export;

    /* generating account spinner */
    GtkWidget* vbox_generating_pin_spinner;

    /* export on ring error */
    GtkWidget* export_on_ring_error;
    GtkWidget* label_export_on_ring_error;
    GtkWidget* button_export_on_ring_error_ok;

    QMetaObject::Connection new_device_added_connection;
    QMetaObject::Connection device_updated_connection;
    QMetaObject::Connection device_removed_connection;
    QMetaObject::Connection banned_status_changed_connection;
    QMetaObject::Connection export_on_ring_ended;
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
    QObject::disconnect(priv->export_on_ring_ended);

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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, username_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_info_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_sip_hostname);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_sip_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_sip_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_sip_proxy);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_sip_voicemail);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, vbox_devices);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, list_devices);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, vbox_banned_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, scrolled_window_banned_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, list_banned_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_show_banned);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, vbox_change_password);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, allow_call_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, call_allow_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, auto_answer_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, custom_ringtone_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, filechooserbutton_custom_ringtone);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, box_name_server);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_name_server);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, box_dht_proxy);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_dht_proxy);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, dht_proxy_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, frame_bootstrap_servers);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, box_bootstrap_servers);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_encrypt_media_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_encrypt_media);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_key_exchange_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, enable_sdes);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_fallback_rtp_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_fallback_rtp);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_encrypt_negotiation);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_enable_TLS);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_verify_certs_server_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_verify_certs_server);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_verify_certs_client_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_verify_certs_client);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_require_incoming_tls_certs_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_require_incoming_tls_certs);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_tls_protocol_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, combobox_tls_protocol_method);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_tls_server_name_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_tls_server_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_negotiation_timeout_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, spinbutton_negotiation_timeout);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, filechooserbutton_ca_list);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, filechooserbutton_certificate);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, filechooserbutton_private_key);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_registration_expire_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, spinbutton_registration_timeout);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_network_interface_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, spinbutton_network_interface);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, upnp_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, switch_use_turn);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_turnserver);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_turnusername);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_turnpassword);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_turnrealm);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, switch_use_stun);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_stunserver);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, box_published_address);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_custom_published);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_published_address);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, spinbutton_published_port);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, list_video_codecs);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_up_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_down_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, switch_enable_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, list_audio_codecs);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_up_audio);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_down_audio);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, box_sdp_session);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, spinbutton_audio_rtp_min);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, spinbutton_audio_rtp_max);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, spinbutton_video_rtp_min);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, spinbutton_video_rtp_max);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_add_device);

    // add_device view
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, add_device_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_export_on_the_ring);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_add_device_cancel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_password_export);

    // generating pin spinner
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, vbox_generating_pin_spinner);

    // export on ring error
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, export_on_ring_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_export_on_ring_error);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_export_on_ring_error_ok);

    // generated_pin view
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, generated_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_generated_pin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_generated_pin_ok);
}

static void
show_advanced_settings(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    gtk_widget_show(GTK_WIDGET(priv->advanced_settings_box));
    gtk_widget_hide(GTK_WIDGET(priv->general_settings_box));
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), priv->advanced_settings_box);
}

static void
show_general_settings(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    gtk_widget_hide(GTK_WIDGET(priv->advanced_settings_box));
    gtk_widget_show(GTK_WIDGET(priv->general_settings_box));
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), priv->general_settings_box);
}

static void
show_generated_pin_view(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    gtk_widget_show(priv->generated_pin);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), priv->generated_pin);
}

static void
show_generating_pin_spinner(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    gtk_widget_show(priv->vbox_generating_pin_spinner);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), priv->vbox_generating_pin_spinner);
}

static void
show_export_on_ring_error(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), priv->export_on_ring_error);
}


// Edit general

// TODO(sblin) expliquer le return false

static bool
is_config_ok(NewAccountSettingsView *view) {
    g_return_val_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view), false);
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (!priv->currentProp_) {
        g_debug("Can't get current properties for this account");
        return false;
    }
    return true;
}

gboolean
update_display_name(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    priv->currentProp_->displayName = gtk_entry_get_text(GTK_ENTRY(priv->entry_display_name));
    new_account_settings_view_save_account(view);
    return false;
}

gboolean
update_sip_hostname(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    priv->currentProp_->hostname = gtk_entry_get_text(GTK_ENTRY(priv->entry_sip_hostname));
    new_account_settings_view_save_account(view);
    return false;
}

gboolean
update_sip_username(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    priv->currentProp_->username = gtk_entry_get_text(GTK_ENTRY(priv->entry_sip_username));
    new_account_settings_view_save_account(view);
    return false;
}

gboolean
update_sip_password(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    priv->currentProp_->password = gtk_entry_get_text(GTK_ENTRY(priv->entry_sip_password));
    new_account_settings_view_save_account(view);
    return false;
}

gboolean
update_sip_proxy(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    priv->currentProp_->routeset = gtk_entry_get_text(GTK_ENTRY(priv->entry_sip_proxy));
    new_account_settings_view_save_account(view);
    return false;
}

gboolean
update_sip_voicemail(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    priv->currentProp_->mailbox = gtk_entry_get_text(GTK_ENTRY(priv->entry_sip_voicemail));
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
        auto* image = gtk_image_new_from_icon_name("document-edit-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(action_button->data), image);
    } else {
        auto* entry_name = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry_name), name.c_str());
        gtk_container_add(GTK_CONTAINER(box_info->data), entry_name);
        auto* image = gtk_image_new_from_icon_name("document-save-symbolic", GTK_ICON_SIZE_BUTTON);
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
            auto* password = "";
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
    gtk_box_pack_start(GTK_BOX(device_info_box), label_name, false, false, 0);
    gtk_widget_set_halign(label_name, GtkAlign::GTK_ALIGN_START);
    auto* label_id = gtk_label_new("");
    std::string markup = "<span font_desc=\"8.0\">" + device.id + "</span>";
    gtk_label_set_markup(GTK_LABEL(label_id), markup.c_str());
    gtk_box_pack_start(GTK_BOX(device_info_box), label_id, false, false, 0);
    gtk_box_pack_start(GTK_BOX(device_box), device_info_box, false, false, 0);
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
    gtk_box_pack_end(GTK_BOX(device_box), GTK_WIDGET(action_device_button), false, false, 0);
    // Insert at the end of the list
    gtk_list_box_insert(GTK_LIST_BOX(priv->list_devices), device_box, -1);
    gtk_widget_set_halign(GTK_WIDGET(device_box), GTK_ALIGN_FILL);
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
    gtk_widget_set_halign(GTK_WIDGET(banned_box), GTK_ALIGN_CENTER);
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
    g_free(filename);
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
update_allow_call(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->call_allow_button));
    if (newState != priv->currentProp_->allowIncomingFromTrusted) {
        priv->currentProp_->allowIncomingFromTrusted = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
update_auto_answer(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->auto_answer_button));
    if (newState != priv->currentProp_->autoAnswer) {
        priv->currentProp_->autoAnswer = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
enable_custom_ringtone(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->custom_ringtone_button));
    if (newState != priv->currentProp_->Ringtone.ringtoneEnabled) {
        priv->currentProp_->Ringtone.ringtoneEnabled = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
update_custom_ringtone(GtkFileChooser *file_chooser, NewAccountSettingsView* view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto* newState = gtk_file_chooser_get_filename(file_chooser);
    if (newState != priv->currentProp_->Ringtone.ringtonePath) {
        priv->currentProp_->Ringtone.ringtonePath = newState;
        new_account_settings_view_save_account(view);
    }
    g_free(newState);
}

static gboolean
update_nameserver(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
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
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_dht_proxy));
    if (newState != priv->currentProp_->proxyServer) {
        priv->currentProp_->proxyServer = newState;
        new_account_settings_view_save_account(view);
    }
    return false;
}

static void
enable_dhtproxy(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->dht_proxy_button));
    if (newState != priv->currentProp_->proxyEnabled) {
        priv->currentProp_->proxyEnabled = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
set_encrypt_media_enabled(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->sip_encrypt_media));
    if (newState != priv->currentProp_->SRTP.enable) {
        priv->currentProp_->SRTP.enable = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
set_fallback_rtp_enabled(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->sip_fallback_rtp));
    if (newState != priv->currentProp_->SRTP.rtpFallback) {
        priv->currentProp_->SRTP.rtpFallback = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
set_sdes_enabled(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->enable_sdes)) ? lrc::api::account::KeyExchangeProtocol::SDES : lrc::api::account::KeyExchangeProtocol::NONE;
    if (newState != priv->currentProp_->SRTP.keyExchange) {
        priv->currentProp_->SRTP.keyExchange = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
remove_bootstrap_server(GtkWidget *item, NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_bootstrap_servers));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gchar* remove_hostname = (gchar*)g_object_get_data(G_OBJECT(item), "bootstrap-server-address");
    gchar* remove_port = (gchar*)g_object_get_data(G_OBJECT(item), "bootstrap-server-port");

    auto iterIsCorrect = true, removed = false;
    auto idx = 0;

    std::vector<lrc::api::account::Bootstrap> bootstrapList;

    while (iterIsCorrect) {
        iterIsCorrect = gtk_tree_model_iter_nth_child(model, &iter, nullptr, idx);
        if (!iterIsCorrect)
            break;
        gchar *hostname;
        gchar *port;
        gtk_tree_model_get (model, &iter,
                            0 /* col# */, &hostname /* data */,
                            1 /* col# */, &port /* data */,
                            -1);

        std::string url = hostname;
        lrc::api::account::Bootstrap bootstrap;
        bootstrap.hostname = url;
        try {
            bootstrap.port = std::stoi(std::string(port));
        } catch (...) {
            bootstrap.port = 4222;
        }

        if (!removed && std::string(port) == std::string(remove_port) && bootstrap.hostname == std::string(remove_hostname)) {
            removed = true;
        } else if (!bootstrap.hostname.empty()) {
            bootstrapList.emplace_back(bootstrap);
        }

        g_free(hostname);
        g_free(port);
        idx++;
    }

    if (*priv->accountInfo_) {
        std::string accountId = (*priv->accountInfo_)->id;
        try {
            priv->currentProp_->hostname = lrc::api::NewAccountModel::bootstrapListToString(bootstrapList);
            new_account_settings_view_save_account(view);
        } catch (const std::out_of_range&) {
            g_warning("Can't set bootstraps for account %s", accountId.c_str());
        }
    }
}

// Bootstrap
static gboolean
bootstrap_servers_popup_menu(G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event, NewAccountSettingsView *view)
{
    g_return_val_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view), false);
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    // check for right click
    if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
        return false;

    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_bootstrap_servers));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return false;

    gchar *hostname;
    gchar *port;
    gtk_tree_model_get (model, &iter,
                        0 /* col# */, &hostname /* data */,
                        1 /* col# */, &port /* data */,
                        -1);

    GtkWidget *menu = gtk_menu_new();

    GtkWidget *remove_server_item = gtk_menu_item_new_with_mnemonic(_("_Remove server"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), remove_server_item);
    g_object_set_data(G_OBJECT(remove_server_item), "bootstrap-server-address", hostname);
    g_object_set_data(G_OBJECT(remove_server_item), "bootstrap-server-port", port);
    g_signal_connect(remove_server_item,
                     "activate",
                     G_CALLBACK(remove_bootstrap_server),
                     view);

    // show menu
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), nullptr, nullptr, nullptr, nullptr, event->button, event->time);

    g_free(hostname);
    g_free(port);

    return true;  // we handled the event
}

static void
bootstrap_server_edited(GtkCellRendererText *renderer, gchar *path, gchar *new_text, NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto model = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->treeview_bootstrap_servers));

    auto idx = 0, pathIdx = 0;
    try {
        pathIdx = std::stoi(std::string(path));
    } catch (...) {}
    auto iterIsCorrect = true;
    GtkTreeIter iter;

    std::vector<lrc::api::account::Bootstrap> bootstrapList;

    while (iterIsCorrect) {
        iterIsCorrect = gtk_tree_model_iter_nth_child(model, &iter, nullptr, idx);
        if (!iterIsCorrect)
            break;
        gchar *hostname;
        gchar *port;
        gtk_tree_model_get (model, &iter,
                            0 /* col# */, &hostname /* data */,
                            1 /* col# */, &port /* data */,
                            -1);

        std::string url = hostname;
        lrc::api::account::Bootstrap bootstrap;
        bootstrap.hostname = url;
        try {
            bootstrap.port = std::stoi(std::string(port));
        } catch (...) {
            bootstrap.port = 4222;
        }

        if (idx == pathIdx) {
            if (g_object_get_data(G_OBJECT(renderer), "model-column") == 0) {
                bootstrap.hostname = new_text;
            } else {
                try {
                    bootstrap.port = std::stoi(std::string(new_text));
                } catch (...) {
                    bootstrap.port = 4222;
                }
            }
        }

        if (!bootstrap.hostname.empty()) {
            bootstrapList.emplace_back(bootstrap);
        }

        g_free(hostname);
        g_free(port);
        idx++;
    }

    if (*priv->accountInfo_) {
        std::string accountId = (*priv->accountInfo_)->id;
        try {
            priv->currentProp_->hostname = lrc::api::NewAccountModel::bootstrapListToString(bootstrapList);
            new_account_settings_view_save_account(view);
        } catch (const std::out_of_range&) {
            g_warning("Can't set bootstraps for account %s", accountId.c_str());
        }
    }
}

static void
update_ca_list(GtkFileChooser *file_chooser, NewAccountSettingsView* view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto* newState = gtk_file_chooser_get_filename(file_chooser);
    if (newState != priv->currentProp_->TLS.certificateListFile) {
        priv->currentProp_->TLS.certificateListFile = newState;
        new_account_settings_view_save_account(view);
    }
    g_free(newState);
}

static void
update_certificate(GtkFileChooser *file_chooser, NewAccountSettingsView* view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto* newState = gtk_file_chooser_get_filename(file_chooser);
    if (newState != priv->currentProp_->TLS.certificateFile) {
        priv->currentProp_->TLS.certificateFile = newState;
        new_account_settings_view_save_account(view);
    }
    g_free(newState);
}

static void
update_private_key(GtkFileChooser *file_chooser, NewAccountSettingsView* view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto* newState = gtk_file_chooser_get_filename(file_chooser);
    if (newState != priv->currentProp_->TLS.privateKeyFile) {
        priv->currentProp_->TLS.privateKeyFile = newState;
        new_account_settings_view_save_account(view);
    }
    g_free(newState);
}

static gboolean
update_password(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_password));
    if (newState != priv->currentProp_->TLS.password) {
        priv->currentProp_->TLS.password = newState;
        new_account_settings_view_save_account(view);
    }
    return false;
}

static gboolean
update_tls_server_name(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_tls_server_name));
    if (newState != priv->currentProp_->TLS.serverName) {
        priv->currentProp_->TLS.serverName = newState;
        new_account_settings_view_save_account(view);
    }
    return false;
}

static void
update_negotiation_timeout(GtkWidget*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spinbutton_negotiation_timeout));
    if (newState != priv->currentProp_->TLS.negotiationTimeoutSec) {
        priv->currentProp_->TLS.negotiationTimeoutSec = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
update_registration_timeout(GtkWidget*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spinbutton_registration_timeout));
    if (newState != priv->currentProp_->Registration.expire) {
        priv->currentProp_->Registration.expire = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
update_network_interface(GtkWidget*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spinbutton_network_interface));
    if (newState != priv->currentProp_->localPort) {
        priv->currentProp_->localPort = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
enable_turn(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState =  gtk_switch_get_active(GTK_SWITCH(priv->switch_use_turn));
    if (newState != priv->currentProp_->TURN.enable) {
        priv->currentProp_->TURN.enable = newState;
        new_account_settings_view_save_account(view);
    }
}

static gboolean
update_turnserver(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
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
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
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
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
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
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_turnrealm));
    if (newState != priv->currentProp_->TURN.realm) {
        priv->currentProp_->TURN.realm = newState;
        new_account_settings_view_save_account(view);
    }
    return false;
}

static void
enable_stun(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState =  gtk_switch_get_active(GTK_SWITCH(priv->switch_use_stun));
    if (newState != priv->currentProp_->STUN.enable) {
        priv->currentProp_->STUN.enable = newState;
        new_account_settings_view_save_account(view);
    }
}

static gboolean
update_stunserver(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_stunserver));
    if (newState != priv->currentProp_->STUN.server) {
        priv->currentProp_->STUN.server = newState;
        new_account_settings_view_save_account(view);
    }
    return false;
}

static void
enable_upnp(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState =  gtk_switch_get_active(GTK_SWITCH(priv->upnp_button));
    if (newState != priv->currentProp_->upnpEnabled) {
        priv->currentProp_->upnpEnabled = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
enable_custom_address(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = !gtk_switch_get_active(GTK_SWITCH(priv->button_custom_published));
    if (newState != priv->currentProp_->publishedSameAsLocal) {
        priv->currentProp_->publishedSameAsLocal = newState;
        new_account_settings_view_save_account(view);
    }
}

static gboolean
update_published_address(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_published_address));
    if (newState != priv->currentProp_->publishedAddress) {
        priv->currentProp_->publishedAddress = newState;
        new_account_settings_view_save_account(view);
    }
    return false;
}

static void
update_published_port(GtkWidget*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spinbutton_published_port));
    if (newState != priv->currentProp_->publishedPort) {
        priv->currentProp_->publishedPort = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
enable_video(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = !gtk_switch_get_active(GTK_SWITCH(priv->switch_enable_video));
    if (newState != priv->currentProp_->Video.videoEnabled) {
        priv->currentProp_->Video.videoEnabled = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
enable_codec(GObject* switch_btn, GParamSpec*, NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto id = g_object_get_data(switch_btn, "id");
    if (id) (*priv->accountInfo_)->codecModel->enable(GPOINTER_TO_UINT(id), gtk_switch_get_active(GTK_SWITCH(switch_btn)));
}

static void
set_account_enabled(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->account_enabled));
    if (newState != (*priv->accountInfo_)->enabled) {
        (*priv->accountInfo_)->accountModel->enableAccount((*priv->accountInfo_)->id, newState);
        new_account_settings_view_save_account(view);
    }
}

static void
sip_TLS_enabled(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->sip_enable_TLS));
    if (newState != priv->currentProp_->TLS.enable) {
        priv->currentProp_->TLS.enable = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
sip_verify_certs_server_enabled(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->sip_verify_certs_server));
    if (newState != priv->currentProp_->TLS.verifyServer) {
        priv->currentProp_->TLS.verifyServer = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
sip_verify_certs_client_enabled(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->sip_verify_certs_client));
    if (newState != priv->currentProp_->TLS.verifyClient) {
        priv->currentProp_->TLS.verifyClient = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
sip_require_incoming_tls_certs_enabled(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->sip_require_incoming_tls_certs));
    if (newState != priv->currentProp_->TLS.requireClientCertificate) {
        priv->currentProp_->TLS.requireClientCertificate = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
tls_method_changed(NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto* text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(priv->combobox_tls_protocol_method));
    if (!text) return;
    std::string activeText = text;
    auto newState = lrc::api::account::TlsMethod::DEFAULT;
    if (activeText == "TLSv1") {
        newState = lrc::api::account::TlsMethod::TLSv1;
    } else if (activeText == "TLSv1.1") {
        newState = lrc::api::account::TlsMethod::TLSv1_1;
    } else if (activeText == "TLSv1.2") {
        newState = lrc::api::account::TlsMethod::TLSv1_2;
    }
    if (newState != priv->currentProp_->TLS.method) {
        priv->currentProp_->TLS.method = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
remove_cb(GtkWidget *widget, gpointer user_data) {
    gtk_container_remove(GTK_CONTAINER(user_data), widget);
}

static void
draw_codecs(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    // Clear video codecs
    gtk_container_foreach(GTK_CONTAINER(priv->list_video_codecs), (GtkCallback)remove_cb, priv->list_video_codecs);
    // Add video codecs
    auto videoCodecs = (*priv->accountInfo_)->codecModel->getVideoCodecs();
    for (const auto& codec : videoCodecs) {
        auto* codec_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_margin_end(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_start(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_top(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_bottom(GTK_WIDGET(codec_box), 10);
        auto* label_name = gtk_label_new(codec.name.c_str());
        gtk_container_add(GTK_CONTAINER(codec_box), label_name);
        auto* switch_enabled = gtk_switch_new();
        g_object_set_data(G_OBJECT(switch_enabled), "id", GUINT_TO_POINTER(codec.id));
        g_signal_connect(switch_enabled, "notify::active", G_CALLBACK(enable_codec), view);  // TODO(sblin) spamming here!
        gtk_switch_set_active(GTK_SWITCH(switch_enabled), codec.enabled);
        gtk_box_pack_end(GTK_BOX(codec_box), GTK_WIDGET(switch_enabled), false, false, 0);
        gtk_list_box_insert(GTK_LIST_BOX(priv->list_video_codecs), codec_box, -1);
    }
    gtk_widget_show_all(priv->list_video_codecs);
    gtk_switch_set_active(GTK_SWITCH(priv->switch_enable_video), priv->currentProp_->Video.videoEnabled);

    // Clear audio codecs
    gtk_container_foreach(GTK_CONTAINER(priv->list_audio_codecs), (GtkCallback)remove_cb, priv->list_audio_codecs);
    // Add audio codecs
    auto audioCodecs = (*priv->accountInfo_)->codecModel->getAudioCodecs();
    for (const auto& codec : audioCodecs) {
        auto* codec_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_margin_end(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_start(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_top(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_bottom(GTK_WIDGET(codec_box), 10);
        auto* codec_info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_halign(codec_info_box, GtkAlign::GTK_ALIGN_START);
        auto* label_name = gtk_label_new(codec.name.c_str());
        gtk_widget_set_halign(label_name, GtkAlign::GTK_ALIGN_START);
        gtk_container_add(GTK_CONTAINER(codec_info_box), label_name);
        auto* label_samplerate = gtk_label_new("");
        std::string markup = "<span font_desc=\"7.0\">" + codec.samplerate + " Hz</span>";
        gtk_label_set_markup(GTK_LABEL(label_samplerate), markup.c_str());
        gtk_container_add(GTK_CONTAINER(codec_info_box), label_samplerate);
        gtk_container_add(GTK_CONTAINER(codec_box), codec_info_box);
        auto* switch_enabled = gtk_switch_new();
        gtk_switch_set_active(GTK_SWITCH(switch_enabled), codec.enabled);
        g_object_set_data(G_OBJECT(switch_enabled), "id", GUINT_TO_POINTER(codec.id));
        g_signal_connect(switch_enabled, "notify::active", G_CALLBACK(enable_codec), view);  // TODO(sblin) spamming here!
        gtk_box_pack_end(GTK_BOX(codec_box), GTK_WIDGET(switch_enabled), false, false, 0);
        gtk_list_box_insert(GTK_LIST_BOX(priv->list_audio_codecs), codec_box, -1);
    }
    gtk_widget_show_all(priv->list_audio_codecs);
}

static void
up_video_priority_clicked(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(priv->list_video_codecs));
    auto* children = gtk_container_get_children(GTK_CONTAINER(row));
    if (children) {
        auto* box = gtk_container_get_children(GTK_CONTAINER(g_list_first(children)->data));
        auto* switch_btn = g_list_last(box)->data;
        auto id = g_object_get_data(G_OBJECT(switch_btn), "id");
        if (id) (*priv->accountInfo_)->codecModel->increasePriority(GPOINTER_TO_UINT(id), true);
        draw_codecs(view);
    }
}

static void
down_video_priority_clicked(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(priv->list_video_codecs));
    auto* children = gtk_container_get_children(GTK_CONTAINER(row));
    if (children) {
        auto* box = gtk_container_get_children(GTK_CONTAINER(g_list_first(children)->data));
        auto* switch_btn = g_list_last(box)->data;
        auto id = g_object_get_data(G_OBJECT(switch_btn), "id");
        if (id) (*priv->accountInfo_)->codecModel->decreasePriority(GPOINTER_TO_UINT(id), true);
        draw_codecs(view);
    }
}

static void
up_audio_priority_clicked(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(priv->list_audio_codecs));
    auto* children = gtk_container_get_children(GTK_CONTAINER(row));
    if (children) {
        auto* box = gtk_container_get_children(GTK_CONTAINER(g_list_first(children)->data));
        auto* switch_btn = g_list_last(box)->data;
        auto id = g_object_get_data(G_OBJECT(switch_btn), "id");
        if (id) (*priv->accountInfo_)->codecModel->increasePriority(GPOINTER_TO_UINT(id), false);
        draw_codecs(view);
    }
}

static void
down_audio_priority_clicked(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(priv->list_audio_codecs));
    auto* children = gtk_container_get_children(GTK_CONTAINER(row));
    if (children) {
        auto* box = gtk_container_get_children(GTK_CONTAINER(g_list_first(children)->data));
        auto* switch_btn = g_list_last(box)->data;
        auto id = g_object_get_data(G_OBJECT(switch_btn), "id");
        if (id) (*priv->accountInfo_)->codecModel->decreasePriority(GPOINTER_TO_UINT(id), false);
        draw_codecs(view);
    }
}

static void
update_audio_rtp_min(GtkWidget*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto newState = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spinbutton_audio_rtp_min));
    if (newState != priv->currentProp_->Audio.audioPortMin) {
        priv->currentProp_->Audio.audioPortMin = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
update_audio_rtp_max(GtkWidget*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spinbutton_audio_rtp_max));
    if (newState != priv->currentProp_->Audio.audioPortMax) {
        priv->currentProp_->Audio.audioPortMax = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
update_video_rtp_min(GtkWidget*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spinbutton_video_rtp_min));
    if (newState != priv->currentProp_->Video.videoPortMin) {
        priv->currentProp_->Video.videoPortMin = newState;
        new_account_settings_view_save_account(view);
    }
}

static void
update_video_rtp_max(GtkWidget*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spinbutton_video_rtp_max));
    if (newState != priv->currentProp_->Video.videoPortMax) {
        priv->currentProp_->Video.videoPortMax = newState;
        new_account_settings_view_save_account(view);
    }
}

// Link device
static void
show_add_device_view(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    gtk_widget_hide(GTK_WIDGET(priv->general_settings_box));
    gtk_widget_show(GTK_WIDGET(priv->add_device_box));
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), priv->add_device_box);
}

static void
export_on_the_ring_clicked(G_GNUC_UNUSED GtkButton *button, NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto* password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password_export));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password_export), "");

    priv->export_on_ring_ended = QObject::connect(
        (*priv->accountInfo_)->accountModel,
        &lrc::api::NewAccountModel::exportOnRingEnded,
        [=] (const std::string& accountID, lrc::api::account::ExportOnRingStatus status, const std::string& pin) {
            if (accountID != (*priv->accountInfo_)->id) return;
            QObject::disconnect(priv->export_on_ring_ended);
            switch (status)
            {
                case lrc::api::account::ExportOnRingStatus::SUCCESS:
                {
                    GtkStyleContext* context;
                    context = gtk_widget_get_style_context(GTK_WIDGET(priv->label_generated_pin));
                    gtk_style_context_add_class(context, "larger");
                    gtk_label_set_text(GTK_LABEL(priv->label_generated_pin), pin.c_str());
                    show_generated_pin_view(view);
                    break;
                }
                case lrc::api::account::ExportOnRingStatus::WRONG_PASSWORD:
                {
                    gtk_label_set_text(GTK_LABEL(priv->label_export_on_ring_error), _("Bad password"));
                    show_export_on_ring_error(view);
                    break;
                }
                case lrc::api::account::ExportOnRingStatus::NETWORK_ERROR:
                {
                    gtk_label_set_text(GTK_LABEL(priv->label_export_on_ring_error), _("Network error, try again"));
                    show_export_on_ring_error(view);
                    break;
                }
                default:
                {
                    gtk_label_set_text(GTK_LABEL(priv->label_export_on_ring_error), _("Unknown error"));
                    show_export_on_ring_error(view);
                    break;
                }
            }
        }
    );

    show_generating_pin_spinner(view);
    if (!(*priv->accountInfo_)->accountModel->exportOnRing((*priv->accountInfo_)->id, password))
    {
        QObject::disconnect(priv->export_on_ring_ended);
        gtk_label_set_text(GTK_LABEL(priv->label_export_on_ring_error), _("Could not initiate export to the Ring, try again"));
        g_debug("Could not initiate exportOnRing operation");
        show_export_on_ring_error(view);
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
    css += ".with_borders { border: 1px solid #cccccc; border-radius: 2px; border-top: 0; padding: 10px; }";
    css += ".larger { font-size: 300%; }";
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

    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_custom_ringtone),
        priv->currentProp_->Ringtone.ringtonePath.c_str());


    g_signal_connect_swapped(priv->button_advanced_settings, "clicked", G_CALLBACK(show_advanced_settings), view);
    g_signal_connect_swapped(priv->button_general_settings, "clicked", G_CALLBACK(show_general_settings), view);
    g_signal_connect(priv->account_enabled, "notify::active", G_CALLBACK(set_account_enabled), view);
    g_signal_connect(priv->sip_enable_TLS, "notify::active", G_CALLBACK(sip_TLS_enabled), view);
    g_signal_connect(priv->sip_verify_certs_server, "notify::active", G_CALLBACK(sip_verify_certs_server_enabled), view);
    g_signal_connect(priv->sip_verify_certs_client, "notify::active", G_CALLBACK(sip_verify_certs_client_enabled), view);
    g_signal_connect(priv->sip_require_incoming_tls_certs, "notify::active", G_CALLBACK(sip_require_incoming_tls_certs_enabled), view);
    g_signal_connect(priv->button_show_banned, "toggled", G_CALLBACK(on_show_banned), view);
    g_signal_connect(priv->entry_display_name, "focus-out-event", G_CALLBACK(update_display_name), view);
    g_signal_connect(priv->entry_sip_hostname, "focus-out-event", G_CALLBACK(update_sip_hostname), view);
    g_signal_connect(priv->entry_sip_username, "focus-out-event", G_CALLBACK(update_sip_username), view);
    g_signal_connect(priv->entry_sip_password, "focus-out-event", G_CALLBACK(update_sip_password), view);
    g_signal_connect(priv->entry_sip_proxy, "focus-out-event", G_CALLBACK(update_sip_proxy), view);
    g_signal_connect(priv->entry_sip_password, "focus-out-event", G_CALLBACK(update_sip_password), view);
    g_signal_connect_swapped(priv->button_validate_password, "clicked", G_CALLBACK(change_password), view);
    g_signal_connect_swapped(priv->entry_current_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->entry_new_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->entry_confirm_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->button_export_account, "clicked", G_CALLBACK(choose_export_file), view);
    g_signal_connect_swapped(priv->button_delete_account, "clicked", G_CALLBACK(remove_account), view);
    g_signal_connect(priv->call_allow_button, "notify::active", G_CALLBACK(update_allow_call), view);
    g_signal_connect(priv->auto_answer_button, "notify::active", G_CALLBACK(update_auto_answer), view);
    g_signal_connect(priv->custom_ringtone_button, "toggled", G_CALLBACK(enable_custom_ringtone), view);
    g_signal_connect(priv->filechooserbutton_custom_ringtone, "file-set", G_CALLBACK(update_custom_ringtone), view);
    g_signal_connect(priv->entry_dht_proxy, "focus-out-event", G_CALLBACK(update_dhtproxy), view);
    g_signal_connect(priv->dht_proxy_button, "notify::active", G_CALLBACK(enable_dhtproxy), view);
    g_signal_connect(priv->sip_encrypt_media, "notify::active", G_CALLBACK(set_encrypt_media_enabled), view);
    g_signal_connect(priv->sip_fallback_rtp, "notify::active", G_CALLBACK(set_fallback_rtp_enabled), view);
    g_signal_connect(priv->enable_sdes, "notify::active", G_CALLBACK(set_sdes_enabled), view);
    g_signal_connect(priv->filechooserbutton_ca_list, "file-set", G_CALLBACK(update_ca_list), view);
    g_signal_connect(priv->filechooserbutton_certificate, "file-set", G_CALLBACK(update_certificate), view);
    g_signal_connect(priv->filechooserbutton_private_key, "file-set", G_CALLBACK(update_private_key), view);
    g_signal_connect(priv->entry_password, "file-set", G_CALLBACK(update_custom_ringtone), view);
    g_signal_connect(priv->spinbutton_registration_timeout, "value-changed", G_CALLBACK(update_registration_timeout), view);
    g_signal_connect(priv->spinbutton_negotiation_timeout, "value-changed", G_CALLBACK(update_negotiation_timeout), view);
    g_signal_connect(priv->spinbutton_network_interface, "value-changed", G_CALLBACK(update_network_interface), view);
    g_signal_connect(priv->entry_tls_server_name, "focus-out-event", G_CALLBACK(update_tls_server_name), view);
    g_signal_connect(priv->upnp_button, "notify::active", G_CALLBACK(enable_upnp), view);
    g_signal_connect(priv->switch_use_turn, "notify::active", G_CALLBACK(enable_turn), view);
    g_signal_connect(priv->entry_turnserver, "focus-out-event", G_CALLBACK(update_turnserver), view);
    g_signal_connect(priv->entry_turnusername, "focus-out-event", G_CALLBACK(update_turnusername), view);
    g_signal_connect(priv->entry_turnpassword, "focus-out-event", G_CALLBACK(update_turnpassword), view);
    g_signal_connect(priv->entry_turnrealm, "focus-out-event", G_CALLBACK(update_turnrealm), view);
    g_signal_connect(priv->switch_use_stun, "notify::active", G_CALLBACK(enable_stun), view);
    g_signal_connect(priv->entry_stunserver, "focus-out-event", G_CALLBACK(update_stunserver), view);
    g_signal_connect(priv->button_custom_published, "notify::active", G_CALLBACK(enable_custom_address), view);
    g_signal_connect(priv->entry_published_address, "focus-out-event", G_CALLBACK(update_published_address), view);
    g_signal_connect(priv->spinbutton_published_port, "value-changed", G_CALLBACK(update_published_port), view);
    g_signal_connect(priv->switch_enable_video, "notify::active", G_CALLBACK(enable_video), view);
    g_signal_connect_swapped(priv->button_up_video, "clicked", G_CALLBACK(up_video_priority_clicked), view);
    g_signal_connect_swapped(priv->button_down_video, "clicked", G_CALLBACK(down_video_priority_clicked), view);
    g_signal_connect_swapped(priv->button_up_audio, "clicked", G_CALLBACK(up_audio_priority_clicked), view);
    g_signal_connect_swapped(priv->button_down_audio, "clicked", G_CALLBACK(down_audio_priority_clicked), view);
    g_signal_connect(priv->spinbutton_audio_rtp_min, "value-changed", G_CALLBACK(update_audio_rtp_min), view);
    g_signal_connect(priv->spinbutton_audio_rtp_max, "value-changed", G_CALLBACK(update_audio_rtp_max), view);
    g_signal_connect(priv->spinbutton_video_rtp_min, "value-changed", G_CALLBACK(update_video_rtp_min), view);
    g_signal_connect(priv->spinbutton_video_rtp_max, "value-changed", G_CALLBACK(update_video_rtp_max), view);

    g_signal_connect_swapped(priv->button_add_device, "clicked", G_CALLBACK(show_add_device_view), view);
    g_signal_connect_swapped(priv->button_add_device_cancel, "clicked", G_CALLBACK(show_general_settings), view);
    g_signal_connect(priv->button_export_on_the_ring, "clicked", G_CALLBACK(export_on_the_ring_clicked), view);
    g_signal_connect_swapped(priv->button_generated_pin_ok, "clicked", G_CALLBACK(show_general_settings), view);
    g_signal_connect_swapped(priv->button_export_on_ring_error_ok, "clicked", G_CALLBACK(show_general_settings), view);

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

    gtk_switch_set_active(GTK_SWITCH(priv->account_enabled), (*priv->accountInfo_)->enabled);
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

    gtk_entry_set_text(GTK_ENTRY(priv->entry_display_name), priv->currentProp_->displayName.c_str());

    if ((*priv->accountInfo_)->profileInfo.type == lrc::api::profile::Type::RING) {
        gtk_widget_show_all(priv->vbox_devices);
        gtk_widget_show_all(priv->vbox_banned_contacts);
        gtk_widget_show_all(priv->username_box);
        gtk_widget_hide(priv->sip_info_box);
        gtk_widget_show(priv->button_export_account);
        gtk_widget_show(priv->type_box);
        gtk_widget_show_all(priv->vbox_change_password);
        // Show ring id
        gtk_label_set_text(GTK_LABEL(priv->label_type_info), (*priv->accountInfo_)->profileInfo.uri.c_str());
        gtk_widget_set_visible(priv->entry_current_password, priv->currentProp_->archiveHasPassword);
        gtk_widget_hide(priv->label_error_change_password);
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


        // Clear username box
        auto* children = gtk_container_get_children(GTK_CONTAINER(priv->username_box));
        if (!children) {
            g_debug("No username label... abort");
        }
        auto* username_registration_widget = username_registration_box_new(*priv->accountInfo_, true);
        auto* current_child = g_list_first(children);  // label
        auto* ubox = g_list_next(current_child);
        if (ubox) {
            gtk_container_remove(GTK_CONTAINER(priv->username_box), GTK_WIDGET(ubox->data));
        }

        gtk_box_pack_end(GTK_BOX(priv->username_box), GTK_WIDGET(username_registration_widget), false, false, 0);
        gtk_widget_show_all(GTK_WIDGET(priv->username_box));

        // Clear bootstrap box
        children = gtk_container_get_children(GTK_CONTAINER(priv->box_bootstrap_servers));
        if (children) {
            auto* current_child = g_list_first(children);
            while (current_child) {
                auto* future_child = g_list_next(current_child);
                gtk_container_remove(GTK_CONTAINER(priv->box_bootstrap_servers), GTK_WIDGET(current_child->data));
                current_child = future_child;
            }
        }

        priv->treeview_bootstrap_servers = gtk_tree_view_new();
        gtk_widget_show(priv->treeview_bootstrap_servers);
        g_signal_connect(GTK_WIDGET(priv->treeview_bootstrap_servers),
                         "button-press-event",
                         G_CALLBACK(bootstrap_servers_popup_menu),
                         GTK_WIDGET(view));

        auto bootstrap_servers_frame = gtk_frame_new(nullptr);
        gtk_widget_show(bootstrap_servers_frame);
        gtk_container_add(GTK_CONTAINER(bootstrap_servers_frame), GTK_WIDGET(priv->treeview_bootstrap_servers));

        auto bootstrap_servers_scrolled_window = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_widget_show(bootstrap_servers_scrolled_window);
        gtk_widget_set_halign(bootstrap_servers_scrolled_window, GTK_ALIGN_FILL);
        gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(bootstrap_servers_scrolled_window), 100);
        gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(bootstrap_servers_scrolled_window), 400);
        gtk_container_add(GTK_CONTAINER(bootstrap_servers_scrolled_window), bootstrap_servers_frame);

        auto store = gtk_list_store_new(2 /* # of cols */,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        G_TYPE_UINT);
        std::string accountId = (*priv->accountInfo_)->id;
        for (const auto& bootstrap : (*priv->accountInfo_)->accountModel->accountBootstrapList(accountId)) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                    0 /* col # */ , bootstrap.hostname.c_str(),
                    1 /* col # */ , std::to_string(bootstrap.port).c_str(),
                    -1 /* end */);
        }
        // Add new empty item:
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                0 /* col # */ , "",
                1 /* col # */ , "",
                -1 /* end */);
        gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_bootstrap_servers),
                                GTK_TREE_MODEL(store));

        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "editable", true, nullptr);
        column = gtk_tree_view_column_new_with_attributes(_("Hostname"), renderer, "text", 0, nullptr);
        gtk_tree_view_column_set_expand(column, true);
        gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_bootstrap_servers), column);
        g_object_set_data(G_OBJECT(renderer), "model-column", GUINT_TO_POINTER(0));
        g_signal_connect(renderer, "edited", G_CALLBACK(bootstrap_server_edited), view);

        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "editable", true, nullptr);
        column = gtk_tree_view_column_new_with_attributes(_("Port"), renderer, "text", 1, nullptr);
        gtk_tree_view_column_set_expand(column, true);
        gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_bootstrap_servers), column);
        g_object_set_data(G_OBJECT(renderer), "model-column", GUINT_TO_POINTER(1));
        g_signal_connect(renderer, "edited", G_CALLBACK(bootstrap_server_edited), view);

        gtk_container_add(
            GTK_CONTAINER(priv->box_bootstrap_servers),
            bootstrap_servers_scrolled_window
        );
        gtk_widget_show_all(priv->frame_bootstrap_servers);

        gtk_widget_show_all(priv->allow_call_row);
        gtk_widget_show_all(priv->box_name_server);
        gtk_widget_show_all(priv->box_dht_proxy);
        gtk_widget_show_all(priv->box_bootstrap_servers);
        gtk_widget_show_all(priv->box_published_address);
        gtk_widget_hide(priv->sip_encrypt_media_row);
        gtk_widget_hide(priv->sip_key_exchange_row);
        gtk_widget_hide(priv->sip_fallback_rtp_row);
        gtk_widget_hide(priv->sip_encrypt_negotiation);
        gtk_widget_hide(priv->sip_verify_certs_server_row);
        gtk_widget_hide(priv->sip_tls_protocol_row);
        gtk_widget_hide(priv->sip_tls_server_name_row);
        gtk_widget_hide(priv->sip_negotiation_timeout_row);
        gtk_widget_hide(priv->sip_verify_certs_client_row);
        gtk_widget_hide(priv->sip_require_incoming_tls_certs_row);
        gtk_widget_hide(priv->sip_registration_expire_row);
        gtk_widget_hide(priv->sip_network_interface_row);
        gtk_widget_hide(priv->box_sdp_session);

        draw_codecs(view);
    } else {
        gtk_widget_show(priv->sip_info_box);
        gtk_widget_hide(priv->type_box);
        gtk_widget_hide(priv->vbox_devices);
        gtk_widget_hide(priv->vbox_banned_contacts);
        gtk_widget_hide(priv->button_export_account);
        gtk_widget_hide(priv->frame_bootstrap_servers);
        gtk_widget_hide(priv->username_box);
        gtk_widget_hide(priv->vbox_change_password);
        gtk_widget_hide(priv->vbox_change_password);
        gtk_widget_hide(priv->allow_call_row);
        gtk_widget_hide(priv->box_name_server);
        gtk_widget_hide(priv->box_dht_proxy);
        gtk_widget_hide(priv->box_bootstrap_servers);
        gtk_widget_hide(priv->box_published_address);
        gtk_widget_show_all(priv->sip_encrypt_negotiation);
        gtk_widget_show_all(priv->sip_verify_certs_server_row);
        gtk_widget_show_all(priv->sip_verify_certs_client_row);
        gtk_widget_show_all(priv->sip_require_incoming_tls_certs_row);
        gtk_widget_show_all(priv->sip_encrypt_media_row);
        gtk_widget_show_all(priv->sip_tls_protocol_row);
        gtk_widget_show_all(priv->sip_tls_server_name_row);
        gtk_widget_show_all(priv->sip_negotiation_timeout_row);
        gtk_widget_show_all(priv->sip_key_exchange_row);
        gtk_widget_show_all(priv->sip_fallback_rtp_row);
        gtk_widget_show_all(priv->sip_registration_expire_row);
        gtk_widget_show_all(priv->sip_network_interface_row);
        gtk_widget_show_all(priv->box_sdp_session);

        gtk_switch_set_active(GTK_SWITCH(priv->sip_enable_TLS), priv->currentProp_->TLS.enable);
        gtk_switch_set_active(GTK_SWITCH(priv->sip_verify_certs_server), priv->currentProp_->TLS.verifyServer);
        gtk_switch_set_active(GTK_SWITCH(priv->sip_verify_certs_client), priv->currentProp_->TLS.verifyClient);
        gtk_switch_set_active(GTK_SWITCH(priv->sip_require_incoming_tls_certs), priv->currentProp_->TLS.requireClientCertificate);
        gtk_switch_set_active(GTK_SWITCH(priv->sip_encrypt_media), priv->currentProp_->SRTP.enable);
        gtk_switch_set_active(GTK_SWITCH(priv->sip_fallback_rtp), priv->currentProp_->SRTP.rtpFallback);
        gtk_widget_set_sensitive(GTK_WIDGET(priv->sip_fallback_rtp), priv->currentProp_->SRTP.enable);
        gtk_switch_set_active(GTK_SWITCH(priv->enable_sdes), priv->currentProp_->SRTP.keyExchange == lrc::api::account::KeyExchangeProtocol::SDES);
        gtk_widget_set_sensitive(GTK_WIDGET(priv->enable_sdes), priv->currentProp_->SRTP.enable);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spinbutton_negotiation_timeout), priv->currentProp_->TLS.negotiationTimeoutSec);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spinbutton_registration_timeout), priv->currentProp_->Registration.expire);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spinbutton_network_interface), priv->currentProp_->localPort);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spinbutton_audio_rtp_min), priv->currentProp_->Audio.audioPortMin);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spinbutton_audio_rtp_max), priv->currentProp_->Audio.audioPortMax);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spinbutton_video_rtp_min), priv->currentProp_->Video.videoPortMin);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spinbutton_video_rtp_max), priv->currentProp_->Video.videoPortMax);
        gtk_entry_set_text(GTK_ENTRY(priv->entry_tls_server_name), priv->currentProp_->TLS.serverName.c_str());

        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(priv->combobox_tls_protocol_method));
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(priv->combobox_tls_protocol_method), nullptr, "Default");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(priv->combobox_tls_protocol_method), nullptr, "TLSv1");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(priv->combobox_tls_protocol_method), nullptr, "TLSv1.1");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(priv->combobox_tls_protocol_method), nullptr, "TLSv1.2");
        switch (priv->currentProp_->TLS.method)
        {
        case lrc::api::account::TlsMethod::TLSv1:
            gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_tls_protocol_method), 1);
            break;
        case lrc::api::account::TlsMethod::TLSv1_1:
            gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_tls_protocol_method), 2);
            break;
        case lrc::api::account::TlsMethod::TLSv1_2:
            gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_tls_protocol_method), 3);
            break;
        case lrc::api::account::TlsMethod::DEFAULT:
        default:
            gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_tls_protocol_method), 0);
            break;
        }
        g_signal_connect_swapped(priv->combobox_tls_protocol_method, "changed", G_CALLBACK(tls_method_changed), view);
        gtk_entry_set_text(GTK_ENTRY(priv->entry_sip_hostname), priv->currentProp_->hostname.c_str());
        gtk_entry_set_text(GTK_ENTRY(priv->entry_sip_username), priv->currentProp_->username.c_str());
        gtk_entry_set_text(GTK_ENTRY(priv->entry_sip_password), priv->currentProp_->password.c_str());
        gtk_entry_set_text(GTK_ENTRY(priv->entry_sip_proxy), priv->currentProp_->routeset.c_str());
        gtk_entry_set_text(GTK_ENTRY(priv->entry_sip_voicemail), priv->currentProp_->mailbox.c_str());
    }

    // advanced
    // TODO(sblin, atrazcyk) merge allowCallFrom* here?
    gtk_switch_set_active(GTK_SWITCH(priv->call_allow_button), priv->currentProp_->allowIncomingFromTrusted);
    gtk_switch_set_active(GTK_SWITCH(priv->auto_answer_button), priv->currentProp_->autoAnswer);

    gtk_entry_set_text(GTK_ENTRY(priv->entry_name_server), priv->currentProp_->RingNS.uri.c_str());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_dht_proxy), priv->currentProp_->proxyServer.c_str());
    gtk_switch_set_active(GTK_SWITCH(priv->dht_proxy_button), priv->currentProp_->proxyEnabled);
    if (!priv->currentProp_->TLS.certificateListFile.empty())
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_ca_list), priv->currentProp_->TLS.certificateListFile.c_str());
    if (!priv->currentProp_->TLS.certificateFile.empty())
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_certificate), priv->currentProp_->TLS.certificateFile.c_str());
    if (!priv->currentProp_->TLS.privateKeyFile.empty())
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_private_key), priv->currentProp_->TLS.privateKeyFile.c_str());
    if (!priv->currentProp_->TLS.password.empty())
        gtk_entry_set_text(GTK_ENTRY(priv->entry_password), priv->currentProp_->TLS.password.c_str());
    gtk_switch_set_active(GTK_SWITCH(priv->upnp_button), priv->currentProp_->upnpEnabled);
    gtk_switch_set_active(GTK_SWITCH(priv->switch_use_turn), priv->currentProp_->TURN.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_turnserver), priv->currentProp_->TURN.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_turnusername), priv->currentProp_->TURN.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_turnpassword), priv->currentProp_->TURN.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_turnrealm), priv->currentProp_->TURN.enable);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnserver), priv->currentProp_->TURN.server.c_str());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnusername), priv->currentProp_->TURN.username.c_str());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnpassword), priv->currentProp_->TURN.password.c_str());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnrealm), priv->currentProp_->TURN.realm.c_str());
    gtk_switch_set_active(GTK_SWITCH(priv->switch_use_stun), priv->currentProp_->STUN.enable);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_stunserver), priv->currentProp_->STUN.server.c_str());
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_stunserver), priv->currentProp_->STUN.enable);
    gtk_switch_set_active(GTK_SWITCH(priv->button_custom_published), !priv->currentProp_->publishedSameAsLocal);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_published_address), priv->currentProp_->publishedAddress.c_str());
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_published_address), !priv->currentProp_->publishedSameAsLocal);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spinbutton_published_port), priv->currentProp_->publishedPort);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->spinbutton_published_port), !priv->currentProp_->publishedSameAsLocal);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->filechooserbutton_ca_list), priv->currentProp_->TLS.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->filechooserbutton_certificate), priv->currentProp_->TLS.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->filechooserbutton_private_key), priv->currentProp_->TLS.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_password), priv->currentProp_->TLS.enable);

    new_account_settings_view_show(view, true);
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

    if (priv->avatarmanipulation) {
        gtk_container_remove(GTK_CONTAINER(priv->avatar_box), priv->avatarmanipulation);
        priv->avatarmanipulation = nullptr;
    }
    if (show_profile) {
        /* avatar manipulation widget */
        priv->avatarmanipulation = avatar_manipulation_new(*priv->accountInfo_);
        gtk_widget_set_halign(priv->avatar_box, GtkAlign::GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(priv->avatar_box), priv->avatarmanipulation, true, true, 0);
        gtk_widget_set_visible(priv->avatarmanipulation, true);
    }
}
