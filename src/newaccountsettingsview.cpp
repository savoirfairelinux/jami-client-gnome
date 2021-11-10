/*
 *  Copyright (C) 2015-2021 Savoir-faire Linux Inc.
 *  Author: Sebastien Blin <sebastien.blin@savoirfairelinux.com>
 *  Author: Alberto Eleuterio Flores Guerrero <albertoefg+bugs@posteo.mx>
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

// Jami Client
#include "avatarmanipulation.h"
#include "defines.h"
#include "utils/files.h"
#include "usernameregistrationbox.h"

enum
{
  PROP_MAIN_WIN_PNT = 1,
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
        GtkWidget* box_account;
        GtkWidget* sip_account_enabled;
        GtkWidget* account_enabled;
        GtkWidget* sip_label_status;
        GtkWidget* label_status;
        GtkWidget* avatar_box;
        GtkWidget* avatarmanipulation;
        GtkWidget* entry_display_name;
        GtkWidget* label_id;
        GtkWidget* label_username;
        GtkWidget* label_password;
        GtkWidget* account_options_box;
            GtkWidget* type_box;
                GtkWidget* label_type_info;
            GtkWidget* username_row;
                GtkWidget* username_box;
            GtkWidget* password_row;
                GtkWidget* label_change_password;
        GtkWidget* sip_info_box;
            GtkWidget* entry_sip_hostname;
            GtkWidget* entry_sip_username;
            GtkWidget* entry_sip_password;
            GtkWidget* entry_sip_proxy;
            GtkWidget* entry_sip_voicemail;
            GtkWidget* sip_enable_account;
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
        GtkWidget* change_password_dialog;
    GtkWidget* button_export_account;
    GtkWidget* button_delete_account;
    GtkWidget* sip_button_delete_account;
    GtkWidget* button_advanced_settings;

    GtkWidget* advanced_settings_box;
    GtkWidget* button_general_settings;
    GtkWidget* allow_call_row;
        GtkWidget* call_allow_button;
    GtkWidget* auto_answer_button;
    GtkWidget* rendez_vous_button;
    GtkWidget* custom_ringtone_button;
    GtkWidget* filechooserbutton_custom_ringtone;
    GtkWidget* box_name_server;
        GtkWidget* entry_name_server;
    GtkWidget* box_dht;
        GtkWidget* entry_dht_proxy;
        GtkWidget* dht_proxy_button;
        GtkWidget* dht_peer_discovery_button;
        GtkWidget* entry_bootstrap;
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
    GtkWidget* sip_auto_registration_box;
    GtkWidget* sip_auto_registration_button;
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
    GtkWidget* entry_password_export_label;
    GtkWidget* entry_password_export;
    GtkWidget* exporting_infos;
    GtkWidget* exporting_spinner;
    GtkWidget* exporting_label;

    QMetaObject::Connection new_device_added_connection;
    QMetaObject::Connection device_updated_connection;
    QMetaObject::Connection device_removed_connection;
    QMetaObject::Connection banned_status_changed_connection;
    QMetaObject::Connection export_on_ring_ended;

    lrc::api::AVModel* avModel_;
};

G_DEFINE_TYPE_WITH_PRIVATE(NewAccountSettingsView, new_account_settings_view, GTK_TYPE_SCROLLED_WINDOW);

#define NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), NEW_ACCOUNT_SETTINGS_VIEW_TYPE, NewAccountSettingsViewPrivate))

static void draw_codecs(NewAccountSettingsView* view, int codecSelected = -1);

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
                                                "/net/jami/JamiGnome/newaccountsettingsview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, stack_account);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, general_settings_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, box_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, account_enabled);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_account_enabled);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, avatar_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_id);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_display_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, account_options_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, type_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_type_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, username_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, username_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, password_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, label_change_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_info_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_sip_hostname);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_sip_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_sip_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_sip_proxy);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_sip_voicemail);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_enable_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, vbox_devices);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, list_devices);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, vbox_banned_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, scrolled_window_banned_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, list_banned_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_show_banned);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_export_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_delete_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_button_delete_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_advanced_settings);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, advanced_settings_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, button_general_settings);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, allow_call_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, call_allow_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, auto_answer_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, rendez_vous_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, custom_ringtone_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, filechooserbutton_custom_ringtone);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, box_name_server);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_name_server);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, box_dht);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_dht_proxy);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_bootstrap);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, dht_proxy_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, dht_peer_discovery_button);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_auto_registration_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, sip_auto_registration_box);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_password_export_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, entry_password_export);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, exporting_infos);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, exporting_spinner);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), NewAccountSettingsView, exporting_label);

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
    (*priv->accountInfo_)->accountModel->setAlias((*priv->accountInfo_)->id, gtk_entry_get_text(GTK_ENTRY(priv->entry_display_name)));
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
get_devicename_from_row(GtkWidget* row, gboolean isCurrent)
{
    auto* list_iterator = get_row_iterator(row);
    auto* current_child = g_list_first(list_iterator);  // image
    current_child = g_list_next(current_child);  // box infos
    auto* box_devices_info = gtk_container_get_children(GTK_CONTAINER(current_child->data));
    if (isCurrent)
        current_child = g_list_next(box_devices_info);  // device.name
    else
        current_child = g_list_first(box_devices_info);  // device.name
    return GTK_WIDGET(current_child->data);
}

static void
replace_name_from_row(GtkWidget* row, const QString& name, const QString& id)
{
    // Remove previous information
    auto* list_iterator = get_row_iterator(row);
    auto* box_info = g_list_next(g_list_first(list_iterator));  // box infos
    auto* action_button = g_list_last(list_iterator);
    auto* box_devices_info = gtk_container_get_children(GTK_CONTAINER(box_info->data));
    auto* device_name = g_list_next(box_devices_info);  // device.name
    auto isEntryName = G_TYPE_CHECK_INSTANCE_TYPE(device_name->data, gtk_entry_get_type());
    auto* label_id = g_list_next(g_list_next(box_devices_info));  // device.id
    gtk_container_remove(GTK_CONTAINER(box_info->data), GTK_WIDGET(device_name->data));
    gtk_container_remove(GTK_CONTAINER(box_info->data), GTK_WIDGET(label_id->data));

    // Rebuild item
    if (isEntryName) {
        auto* new_label_name = gtk_label_new(qUtf8Printable(name));
        gtk_box_pack_start(GTK_BOX(box_info->data), new_label_name, false, false, 0);
        gtk_widget_set_halign(new_label_name, GtkAlign::GTK_ALIGN_START);
        auto* image = gtk_image_new_from_icon_name("emblem-system-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_button_set_image(GTK_BUTTON(action_button->data), image);
    } else {
        auto* entry_name = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry_name), qUtf8Printable(name));
        gtk_container_add(GTK_CONTAINER(box_info->data), entry_name);
        auto* image = gtk_image_new_from_icon_name("document-save-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_button_set_image(GTK_BUTTON(action_button->data), image);
    }
    auto* new_label_id = gtk_label_new("");
    std::string markup = "<span font_desc=\"7.0\">" + id.toStdString() + "</span>";
    gtk_label_set_markup(GTK_LABEL(new_label_id), markup.c_str());
    gtk_container_add(GTK_CONTAINER(box_info->data), new_label_id);
    gtk_widget_show_all(GTK_WIDGET(box_info->data));
}

static QString
get_id_from_row(GtkWidget* row)
{
    auto* list_iterator = get_row_iterator(row);
    auto* current_child = g_list_first(list_iterator);  // image
    current_child = g_list_next(current_child);  // box infos
    auto box_devices_info = gtk_container_get_children(GTK_CONTAINER(current_child->data));
    current_child = g_list_last(box_devices_info);  // device.id
    return gtk_label_get_text(GTK_LABEL(current_child->data));
}

static QString
get_contact_id_from_row(GtkWidget* row)
{
    auto* list_iterator = get_row_iterator(row);
    auto* current_child = g_list_first(list_iterator);  // image
    auto box_devices_info = gtk_container_get_children(GTK_CONTAINER(current_child->data));
    current_child = g_list_last(box_devices_info);  // contact.id
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
            GtkWidget* nameWidget = get_devicename_from_row(children, true);
            auto deviceId = get_id_from_row(children);
            if (G_TYPE_CHECK_INSTANCE_TYPE(nameWidget, gtk_entry_get_type())) {
                QString newName(gtk_entry_get_text(GTK_ENTRY(nameWidget)));
                replace_name_from_row(children, newName, deviceId);
                (*priv->accountInfo_)->deviceModel->setCurrentDeviceName(qUtf8Printable(newName));
            } else {
                QString newName(gtk_label_get_text(GTK_LABEL(nameWidget)));
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
    gtk_box_set_spacing(GTK_BOX(device_box), 10);
    auto* image_computer = gtk_image_new_from_icon_name("computer-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(device_box), GTK_WIDGET(image_computer), false, false, 0);

    GtkStyleContext* context_box;
    context_box = gtk_widget_get_style_context(GTK_WIDGET(device_box));
    gtk_style_context_add_class(context_box, "boxitem");
    // Fill with devices information
    auto* device_info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    if (device.isCurrent) {
        auto* label_this_device = gtk_label_new("");
        std::string this_device_markup =
            "<span font_desc=\"8.0\" weight=\"bold\">"
            + std::string(_("This device"))
            + "</span>";
        gtk_label_set_markup(GTK_LABEL(label_this_device),
                             this_device_markup.c_str());
        gtk_box_pack_start(GTK_BOX(device_info_box),
                           label_this_device, false, false, 0);
        gtk_widget_set_halign(label_this_device,
                              GtkAlign::GTK_ALIGN_START);
    }
    auto* label_name = gtk_label_new(qUtf8Printable(device.name));
    gtk_box_pack_start(GTK_BOX(device_info_box), label_name, false, false, 0);
    gtk_widget_set_halign(label_name, GtkAlign::GTK_ALIGN_START);
    auto* label_id = gtk_label_new("");
    std::string markup = "<span font_desc=\"7.0\">" + device.id.toStdString() + "</span>";
    gtk_label_set_markup(GTK_LABEL(label_id), markup.c_str());
    gtk_box_pack_start(GTK_BOX(device_info_box), label_id, false, false, 0);
    gtk_box_pack_start(GTK_BOX(device_box), device_info_box, false, false, 0);
    // Add action button
    auto image = gtk_image_new_from_icon_name("emblem-system-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    if (!device.isCurrent)
        image = gtk_image_new_from_icon_name("dialog-error-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    auto* action_device_button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(action_device_button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(action_device_button, device.isCurrent ? _("Edit name") : _("Revoke device"));
    gtk_button_set_image(GTK_BUTTON(action_device_button), image);
    GtkStyleContext* context;
    context = gtk_widget_get_style_context(GTK_WIDGET(action_device_button));
    gtk_style_context_add_class(context, "transparent-button");
    std::string label_btn = "action_btn_" + device.id.toStdString();
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
        "%s", text.c_str());
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
        auto image = gtk_image_new_from_icon_name("pan-up-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(priv->button_show_banned), image);
        gtk_widget_show_all(priv->scrolled_window_banned_contacts);
        gtk_widget_show_all(priv->button_show_banned);
    } else {
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
            auto contactId = get_contact_id_from_row(children);
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

    auto contact = (*priv->accountInfo_)->contactModel->getContact(contactUri.c_str());

    GtkStyleContext* context_box;
    context_box = gtk_widget_get_style_context(GTK_WIDGET(banned_box));
    gtk_style_context_add_class(context_box, "boxitem");

    // Fill with devices information
    auto* contact_info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    if (!contact.registeredName.isEmpty()) {
        auto* label_name = gtk_label_new(qUtf8Printable(contact.registeredName));
        gtk_widget_set_halign(label_name, GtkAlign::GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(contact_info_box), label_name, FALSE, TRUE, 0);
    }
    auto* label_id = gtk_label_new(contactUri.c_str());
    gtk_widget_set_halign(label_id, GtkAlign::GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(contact_info_box), label_id, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(banned_box), contact_info_box, FALSE, TRUE, 0);
    // Add action button
    auto image = gtk_image_new_from_icon_name("contact-new-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    auto* action_banned_button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(action_banned_button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(action_banned_button, _("Unban"));
    gtk_button_set_image(GTK_BUTTON(action_banned_button), image);
    GtkStyleContext* context;
    context = gtk_widget_get_style_context(GTK_WIDGET(action_banned_button));
    gtk_style_context_add_class(context, "transparent-button");
    std::string label_btn = "action_btn_" + contactUri;
    gtk_widget_set_name(action_banned_button, label_btn.c_str());
    g_signal_connect(action_banned_button, "clicked", G_CALLBACK(unban_contact), view);
    gtk_box_pack_end(GTK_BOX(banned_box), action_banned_button, FALSE, TRUE, 0);
    // Insert at the end of the list
    gtk_list_box_insert(GTK_LIST_BOX(priv->list_banned_contacts), banned_box, -1);
    gtk_widget_set_halign(GTK_WIDGET(banned_box), GTK_ALIGN_FILL);
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
        priv->currentProp_->archiveHasPassword = new_password != "";
        new_account_settings_view_save_account(view);
        gtk_widget_destroy(priv->change_password_dialog);
    } else {
        gtk_button_set_label(GTK_BUTTON(priv->button_validate_password), _("Change password"));
        gtk_label_set_text(GTK_LABEL(priv->label_error_change_password), _("Incorrect password!"));
        gtk_widget_show(GTK_WIDGET(priv->label_error_change_password));
    }
}

static void
close_change_password_dialog(NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    gtk_widget_destroy(priv->change_password_dialog);
}

static void
show_change_password_dialog(GtkListBox*, GtkListBoxRow *row, NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    if (row != GTK_LIST_BOX_ROW(priv->password_row)) return;

    auto* parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
    GtkDialogFlags flags = (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT);
    priv->change_password_dialog = gtk_dialog_new_with_buttons(_("Change password"), parent, flags, nullptr);
    gtk_window_set_resizable(GTK_WINDOW(priv->change_password_dialog), FALSE);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(priv->change_password_dialog));
    gtk_box_set_spacing(GTK_BOX(content_area), 10);
    gtk_widget_set_size_request(content_area, 250, -1);
    gtk_widget_set_margin_end(content_area, 25);
    gtk_widget_set_margin_start(content_area, 25);
    gtk_widget_set_margin_top(content_area, 25);
    gtk_widget_set_margin_bottom(content_area, 25);


    priv->vbox_change_password = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_spacing(GTK_BOX(priv->vbox_change_password), 10);
    auto* label_information = gtk_label_new(_("Note: this will change the password only on this device."));
    gtk_container_add(GTK_CONTAINER(priv->vbox_change_password), label_information);
    priv->entry_current_password = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(priv->entry_current_password), _("Current password"));
    gtk_entry_set_input_purpose(GTK_ENTRY(priv->entry_current_password), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_entry_set_visibility(GTK_ENTRY(priv->entry_current_password), 0);
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(priv->entry_current_password), GTK_ENTRY_ICON_PRIMARY, "gtk-dialog-authentication");
    gtk_container_add(GTK_CONTAINER(priv->vbox_change_password), priv->entry_current_password);
    priv->entry_new_password = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(priv->entry_new_password), _("New password"));
    gtk_entry_set_visibility(GTK_ENTRY(priv->entry_new_password), 0);
    gtk_entry_set_input_purpose(GTK_ENTRY(priv->entry_new_password), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(priv->entry_new_password), GTK_ENTRY_ICON_PRIMARY, "gtk-dialog-authentication");
    gtk_container_add(GTK_CONTAINER(priv->vbox_change_password), priv->entry_new_password);
    priv->entry_confirm_password = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(priv->entry_confirm_password), _("Confirm password"));
    gtk_entry_set_visibility(GTK_ENTRY(priv->entry_confirm_password), 0);
    gtk_entry_set_input_purpose(GTK_ENTRY(priv->entry_confirm_password), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(priv->entry_confirm_password), GTK_ENTRY_ICON_PRIMARY, "gtk-dialog-authentication");
    gtk_container_add(GTK_CONTAINER(priv->vbox_change_password), priv->entry_confirm_password);
    auto* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    priv->button_validate_password = gtk_button_new();
    gtk_button_set_label(GTK_BUTTON(priv->button_validate_password), _("Change password"));
    gtk_container_add(GTK_CONTAINER(buttons), priv->button_validate_password);
    auto* close_button  = gtk_button_new();
    gtk_button_set_label(GTK_BUTTON(close_button), _("Cancel"));
    gtk_container_add(GTK_CONTAINER(buttons), close_button);
    gtk_box_set_homogeneous(GTK_BOX(buttons), true);
    gtk_container_add(GTK_CONTAINER(priv->vbox_change_password), buttons);
    priv->label_error_change_password = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(priv->vbox_change_password), priv->label_error_change_password);

    gtk_box_pack_start(GTK_BOX(content_area), priv->vbox_change_password, FALSE, TRUE, 0);

    g_signal_connect_swapped(close_button, "clicked", G_CALLBACK(close_change_password_dialog), view);
    g_signal_connect_swapped(priv->button_validate_password, "clicked", G_CALLBACK(change_password), view);
    g_signal_connect_swapped(priv->entry_current_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->entry_new_password, "changed", G_CALLBACK(reset_change_password), view);
    g_signal_connect_swapped(priv->entry_confirm_password, "changed", G_CALLBACK(reset_change_password), view);

    GtkStyleContext* context_error_label;
    context_error_label = gtk_widget_get_style_context(GTK_WIDGET(priv->label_error_change_password));
    gtk_style_context_add_class(context_error_label, "red_label");

    gtk_widget_show_all(content_area);
    gtk_widget_set_visible(priv->entry_current_password, priv->currentProp_->archiveHasPassword);
    gtk_widget_hide(priv->label_error_change_password);
    gtk_window_present(GTK_WINDOW(priv->change_password_dialog));
    gtk_widget_grab_focus(GTK_WIDGET(close_button));

}

// Export file

static void
choose_export_file(NewAccountSettingsView *view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    // Get preferred path
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
    gint res;
    gchar* filename = nullptr;

    QString alias = (*priv->accountInfo_)->profileInfo.alias;
    QString uri = alias.isEmpty() ? "export.gz" : alias + ".gz";

#if GTK_CHECK_VERSION(3,20,0)
    GtkFileChooserNative *native = gtk_file_chooser_native_new(
        _("Save File"),
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view))),
        action,
        _("_Save"),
        _("_Cancel"));

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(native),
                                      qUtf8Printable(uri));

    res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));
    if (res == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
    }

    g_object_unref(native);
#else
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        _("Save File"),
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view))),
        action,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Save"), GTK_RESPONSE_ACCEPT,
        nullptr);

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                      qUtf8Printable(uri));

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    }

    gtk_widget_destroy(dialog);
#endif

    if (!filename) return;

    std::string password = {};
    if (priv->currentProp_->archiveHasPassword) {
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
    auto success = (*priv->accountInfo_)->accountModel->exportToFile((*priv->accountInfo_)->id, filename, password.c_str());
    std::string label = success? _("Account exported!") : _("Export account failure.");
    gtk_button_set_label(GTK_BUTTON(priv->button_export_account), label.c_str());
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
    if (newState != priv->currentProp_->DHT.PublicInCalls) {
        priv->currentProp_->DHT.PublicInCalls = newState;
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
update_rendez_vous(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->rendez_vous_button));
    if (newState != priv->currentProp_->isRendezVous) {
        priv->currentProp_->isRendezVous = newState;
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

static gboolean
update_bootstrap(GtkWidget*, GdkEvent*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return false;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_entry_get_text(GTK_ENTRY(priv->entry_bootstrap));
    if (newState != priv->currentProp_->hostname) {
        priv->currentProp_->hostname = newState;
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
enable_dhtpeerdiscovery(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->dht_peer_discovery_button));
    if (newState != priv->currentProp_->peerDiscovery) {
        priv->currentProp_->peerDiscovery = newState;
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
enable_auto_registration(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState =  gtk_switch_get_active(GTK_SWITCH(priv->sip_auto_registration_button));
    if (newState != priv->currentProp_->keepAliveEnabled) {
        priv->currentProp_->keepAliveEnabled = newState;
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
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->switch_enable_video));
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
    if (id) {
        if ((*priv->accountInfo_)->codecModel->enable(GPOINTER_TO_UINT(id),
            gtk_switch_get_active(GTK_SWITCH(switch_btn)))) {
            draw_codecs(view);
        }
    }
}

static void
set_account_enabled(GObject*, GParamSpec*, NewAccountSettingsView *view)
{
    if (!is_config_ok(view)) return;
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    auto newState = false;
    if ((*priv->accountInfo_)->profileInfo.type == lrc::api::profile::Type::JAMI) {
        newState = gtk_switch_get_active(GTK_SWITCH(priv->account_enabled));
    } else {
        newState = gtk_switch_get_active(GTK_SWITCH(priv->sip_account_enabled));
    }
    if (newState != (*priv->accountInfo_)->enabled) {
        (*priv->accountInfo_)->accountModel->setAccountEnabled((*priv->accountInfo_)->id, newState);
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
draw_codecs(NewAccountSettingsView* view, int codecSelected)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    // Clear video codecs
    gtk_container_foreach(GTK_CONTAINER(priv->list_video_codecs), (GtkCallback)remove_cb, priv->list_video_codecs);
    // Add video codecs
    auto videoCodecs = (*priv->accountInfo_)->codecModel->getVideoCodecs();
    GtkListBoxRow* row_selected = nullptr;
    auto currentIdx = 0;
    for (const auto& codec : videoCodecs) {
        auto* codec_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_margin_end(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_start(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_top(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_bottom(GTK_WIDGET(codec_box), 10);
        auto* label_name = gtk_label_new(qUtf8Printable(codec.name));
        gtk_container_add(GTK_CONTAINER(codec_box), label_name);
        auto* switch_enabled = gtk_switch_new();
        g_object_set_data(G_OBJECT(switch_enabled), "id", GUINT_TO_POINTER(codec.id));
        g_signal_connect(switch_enabled, "notify::active", G_CALLBACK(enable_codec), view);
        gtk_switch_set_active(GTK_SWITCH(switch_enabled), codec.enabled);
        gtk_widget_set_sensitive(switch_enabled, priv->currentProp_->Video.videoEnabled);
        gtk_box_pack_end(GTK_BOX(codec_box), GTK_WIDGET(switch_enabled), false, false, 0);
        gtk_list_box_insert(GTK_LIST_BOX(priv->list_video_codecs), codec_box, -1);
        if (codecSelected != -1 && codec.id == codecSelected) {
            row_selected = gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_video_codecs), currentIdx);
        }
        currentIdx += 1;
    }
    if (row_selected) {
        gtk_list_box_select_row(GTK_LIST_BOX(priv->list_video_codecs), row_selected);
    }
    gtk_widget_show_all(priv->list_video_codecs);
    gtk_switch_set_active(GTK_SWITCH(priv->switch_enable_video), priv->currentProp_->Video.videoEnabled);

    // Clear audio codecs
    gtk_container_foreach(GTK_CONTAINER(priv->list_audio_codecs), (GtkCallback)remove_cb, priv->list_audio_codecs);
    // Add audio codecs
    auto audioCodecs = (*priv->accountInfo_)->codecModel->getAudioCodecs();
    row_selected = nullptr;
    currentIdx = 0;
    for (const auto& codec : audioCodecs) {
        auto* codec_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_margin_end(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_start(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_top(GTK_WIDGET(codec_box), 10);
        gtk_widget_set_margin_bottom(GTK_WIDGET(codec_box), 10);
        auto* codec_info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_halign(codec_info_box, GtkAlign::GTK_ALIGN_START);
        auto* label_name = gtk_label_new(qUtf8Printable(codec.name));
        gtk_widget_set_halign(label_name, GtkAlign::GTK_ALIGN_START);
        gtk_container_add(GTK_CONTAINER(codec_info_box), label_name);
        auto* label_samplerate = gtk_label_new("");
        std::string markup = "<span font_desc=\"7.0\">" + codec.samplerate.toStdString() + " Hz</span>";
        gtk_label_set_markup(GTK_LABEL(label_samplerate), markup.c_str());
        gtk_container_add(GTK_CONTAINER(codec_info_box), label_samplerate);
        gtk_container_add(GTK_CONTAINER(codec_box), codec_info_box);
        auto* switch_enabled = gtk_switch_new();
        gtk_switch_set_active(GTK_SWITCH(switch_enabled), codec.enabled);
        g_object_set_data(G_OBJECT(switch_enabled), "id", GUINT_TO_POINTER(codec.id));
        g_signal_connect(switch_enabled, "notify::active", G_CALLBACK(enable_codec), view);  // TODO(sblin) spamming here!
        gtk_box_pack_end(GTK_BOX(codec_box), GTK_WIDGET(switch_enabled), false, false, 0);
        gtk_list_box_insert(GTK_LIST_BOX(priv->list_audio_codecs), codec_box, -1);
        if (codecSelected != -1 && codec.id == codecSelected) {
            row_selected = gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_audio_codecs), currentIdx);
        }
        currentIdx += 1;
    }
    if (row_selected) {
        gtk_list_box_select_row(GTK_LIST_BOX(priv->list_audio_codecs), row_selected);
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
        draw_codecs(view, (int)GPOINTER_TO_UINT(id));
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
        draw_codecs(view, (int)GPOINTER_TO_UINT(id));
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
        draw_codecs(view, (int)GPOINTER_TO_UINT(id));
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
        draw_codecs(view, (int)GPOINTER_TO_UINT(id));
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
    if (!priv->currentProp_->archiveHasPassword) {
        gtk_widget_hide(priv->entry_password_export);
        gtk_widget_hide(priv->entry_password_export_label);
    }
    gtk_widget_hide(priv->exporting_infos);

}

static void
export_on_the_ring_clicked(G_GNUC_UNUSED GtkButton *button, NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    auto* password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password_export));
    std::string passwordStr = {};
    if (password) {
        passwordStr = password;
    }
    gtk_entry_set_text(GTK_ENTRY(priv->entry_password_export), "");
    gtk_label_set_text(GTK_LABEL(priv->exporting_label), _("Exporting account…"));
    gtk_widget_show_all(priv->exporting_infos);
    gtk_widget_set_sensitive(priv->button_export_on_the_ring, FALSE);

    priv->export_on_ring_ended = QObject::connect(
        (*priv->accountInfo_)->accountModel,
        &lrc::api::NewAccountModel::exportOnRingEnded,
        [=] (const QString& accountID, lrc::api::account::ExportOnRingStatus status, const QString& pin) {
            if (accountID != (*priv->accountInfo_)->id) return;
            QObject::disconnect(priv->export_on_ring_ended);
            gtk_widget_set_sensitive(priv->button_export_on_the_ring, TRUE);
            switch (status)
            {
                case lrc::api::account::ExportOnRingStatus::SUCCESS:
                {
                    GtkStyleContext* context;
                    context = gtk_widget_get_style_context(GTK_WIDGET(priv->label_generated_pin));
                    gtk_style_context_add_class(context, "larger");
                    gtk_label_set_text(GTK_LABEL(priv->label_generated_pin), pin.toStdString().c_str());
                    show_generated_pin_view(view);
                    break;
                }
                case lrc::api::account::ExportOnRingStatus::WRONG_PASSWORD:
                {
                    gtk_widget_hide(priv->exporting_spinner);
                    gchar* text = g_markup_printf_escaped("<b>%s</b>", _("Bad password"));
                    gtk_label_set_markup(GTK_LABEL(priv->exporting_label), text);
                    gtk_widget_show(priv->exporting_label);
                    break;
                }
                case lrc::api::account::ExportOnRingStatus::NETWORK_ERROR:
                {
                    gtk_widget_hide(priv->exporting_spinner);
                    gchar* text = g_markup_printf_escaped("<b>%s</b>", _("Network error, try again"));
                    gtk_label_set_markup(GTK_LABEL(priv->exporting_label), text);
                    gtk_widget_show(priv->exporting_label);
                    break;
                }
                default:
                {
                    gtk_widget_hide(priv->exporting_spinner);
                    gchar* text = g_markup_printf_escaped("<b>%s</b>", _("Unknown error"));
                    gtk_label_set_markup(GTK_LABEL(priv->exporting_label), text);
                    gtk_widget_show(priv->exporting_label);
                    break;
                }
            }
        }
    );

    if (!(*priv->accountInfo_)->accountModel->exportOnRing((*priv->accountInfo_)->id, passwordStr.c_str()))
    {
        QObject::disconnect(priv->export_on_ring_ended);
        gtk_widget_hide(priv->exporting_spinner);
        gtk_label_set_text(GTK_LABEL(priv->exporting_label), _("Could not initiate export to the Jami, try again"));
        gtk_widget_show(priv->exporting_label);
        gtk_widget_set_sensitive(priv->button_export_on_the_ring, TRUE);
    }
}

static void
build_settings_view(NewAccountSettingsView* view)
{
    g_return_if_fail(IS_NEW_ACCOUNT_SETTINGS_VIEW(view));
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);

    // CSS styles
    auto provider = gtk_css_provider_new();
    std::string css = ".transparent-button { margin-left: 0; border: 0; background-color: rgba(0,0,0,0); margin-right: 0; padding-right: 0;}";
    css += ".transparent-button:hover { border: 0; background-color: rgba(0,0,0,0);}";
    css += ".show-button { padding: 0;}";
    css += ".boxitem { padding: 12px; }";
    css += ".green_label { color: white; background: #27ae60; border-radius: 3px; padding: 5px;}";
    css += ".red_label { color: white; background: #dc3a37; border-radius: 3px; padding: 5px;}";
    css += ".button_red { color: white; background: #dc3a37; border: 0; }";
    css += ".button_red:hover { background: #dc2719; }";
    css += ".larger { font-size: 300%; }";
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    priv->new_device_added_connection = QObject::connect(
        &*(*priv->accountInfo_)->deviceModel,
        &lrc::api::NewDeviceModel::deviceAdded,
        [view] (const QString& id) {
            auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
            // Retrieve added device
            auto device = (*priv->accountInfo_)->deviceModel->getDevice(id);
            if (device.id.isEmpty()) {
                g_debug("Can't add device with id: %s", qUtf8Printable(device.id));
                return;
            }
            // if exists, add to list
            add_device(view, device);
            gtk_widget_show_all(priv->list_devices);
        });

    priv->device_removed_connection = QObject::connect(
        &*(*priv->accountInfo_)->deviceModel,
        &lrc::api::NewDeviceModel::deviceRevoked,
        [view] (const QString& id, const lrc::api::NewDeviceModel::Status status) {
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
        [view] (const QString& id) {
            auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
            // Retrieve added device
            auto device = (*priv->accountInfo_)->deviceModel->getDevice(id);
            if (device.id.isEmpty()) {
                g_debug("Can't add device with id: %s", qUtf8Printable(device.id));
                return;
            }
            // if exists, update
            auto row = 0;
            while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_devices), row))) {
                auto device_name = get_devicename_from_row(children, device.isCurrent);
                auto deviceId = get_id_from_row(children);
                if (deviceId == id) {
                    if (device.isCurrent) {
                        if (G_TYPE_CHECK_INSTANCE_TYPE(device_name, gtk_entry_get_type())) {
                            gtk_entry_set_text(GTK_ENTRY(device_name), qUtf8Printable(device.name));
                        } else {
                            gtk_label_set_text(GTK_LABEL(device_name), qUtf8Printable(device.name));
                        }
                    } else {
                        gtk_label_set_text(GTK_LABEL(device_name), qUtf8Printable(device.name));
                    }
                }
                ++row;
            }
            g_debug("deviceUpdated signal received, but device not found");
        });

    priv->banned_status_changed_connection = QObject::connect(
        &*(*priv->accountInfo_)->contactModel,
        &lrc::api::ContactModel::bannedStatusChanged,
        [view] (const QString& contactUri, bool banned) {
            auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
            if (banned) {
                add_banned(view, contactUri.toStdString());
            } else {
                auto row = 0;
                while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_banned_contacts), row))) {
                    auto uri = get_contact_id_from_row(children);
                    if (contactUri == uri)
                        gtk_container_remove(GTK_CONTAINER(priv->list_banned_contacts), children);
                    ++row;
                }
            }
            gtk_widget_show_all(priv->list_banned_contacts);
        });

    new_account_settings_view_update(view, false);

    GtkStyleContext* context;
    context = gtk_widget_get_style_context(GTK_WIDGET(priv->button_delete_account));
    gtk_style_context_add_class(context, "button_red");
    context = gtk_widget_get_style_context(GTK_WIDGET(priv->sip_button_delete_account));
    gtk_style_context_add_class(context, "button_red");

    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_custom_ringtone),
        qUtf8Printable(priv->currentProp_->Ringtone.ringtonePath));

    g_signal_connect_swapped(priv->button_advanced_settings, "clicked", G_CALLBACK(show_advanced_settings), view);
    g_signal_connect_swapped(priv->button_general_settings, "clicked", G_CALLBACK(show_general_settings), view);
    g_signal_connect(priv->type_box, "row-activated", G_CALLBACK(show_change_password_dialog), view);
    g_signal_connect(priv->account_enabled, "notify::active", G_CALLBACK(set_account_enabled), view);
    g_signal_connect(priv->sip_account_enabled, "notify::active", G_CALLBACK(set_account_enabled), view);
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
    g_signal_connect_swapped(priv->button_export_account, "clicked", G_CALLBACK(choose_export_file), view);
    g_signal_connect_swapped(priv->button_delete_account, "clicked", G_CALLBACK(remove_account), view);
    g_signal_connect_swapped(priv->sip_button_delete_account, "clicked", G_CALLBACK(remove_account), view);
    g_signal_connect(priv->call_allow_button, "notify::active", G_CALLBACK(update_allow_call), view);
    g_signal_connect(priv->auto_answer_button, "notify::active", G_CALLBACK(update_auto_answer), view);
    g_signal_connect(priv->rendez_vous_button, "notify::active", G_CALLBACK(update_rendez_vous), view);
    g_signal_connect(priv->custom_ringtone_button, "notify::active", G_CALLBACK(enable_custom_ringtone), view);
    g_signal_connect(priv->filechooserbutton_custom_ringtone, "file-set", G_CALLBACK(update_custom_ringtone), view);
    g_signal_connect(priv->entry_name_server, "focus-out-event", G_CALLBACK(update_nameserver), view);
    g_signal_connect(priv->entry_dht_proxy, "focus-out-event", G_CALLBACK(update_dhtproxy), view);
    g_signal_connect(priv->dht_proxy_button, "notify::active", G_CALLBACK(enable_dhtproxy), view);
    g_signal_connect(priv->dht_peer_discovery_button, "notify::active", G_CALLBACK(enable_dhtpeerdiscovery), view);
    g_signal_connect(priv->entry_bootstrap, "focus-out-event", G_CALLBACK(update_bootstrap), view);
    g_signal_connect(priv->sip_encrypt_media, "notify::active", G_CALLBACK(set_encrypt_media_enabled), view);
    g_signal_connect(priv->sip_fallback_rtp, "notify::active", G_CALLBACK(set_fallback_rtp_enabled), view);
    g_signal_connect(priv->enable_sdes, "notify::active", G_CALLBACK(set_sdes_enabled), view);
    g_signal_connect(priv->filechooserbutton_ca_list, "file-set", G_CALLBACK(update_ca_list), view);
    g_signal_connect(priv->filechooserbutton_certificate, "file-set", G_CALLBACK(update_certificate), view);
    g_signal_connect(priv->filechooserbutton_private_key, "file-set", G_CALLBACK(update_private_key), view);
    g_signal_connect(priv->entry_password, "focus-out-event", G_CALLBACK(update_password), view);
    g_signal_connect(priv->spinbutton_registration_timeout, "value-changed", G_CALLBACK(update_registration_timeout), view);
    g_signal_connect(priv->spinbutton_negotiation_timeout, "value-changed", G_CALLBACK(update_negotiation_timeout), view);
    g_signal_connect(priv->spinbutton_network_interface, "value-changed", G_CALLBACK(update_network_interface), view);
    g_signal_connect(priv->entry_tls_server_name, "focus-out-event", G_CALLBACK(update_tls_server_name), view);
    g_signal_connect(priv->upnp_button, "notify::active", G_CALLBACK(enable_upnp), view);
    g_signal_connect(priv->sip_auto_registration_button, "notify::active", G_CALLBACK(enable_auto_registration), view);
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

}

void
new_account_settings_view_update(NewAccountSettingsView *view, gboolean reset_view)
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
        QString accountId = (*priv->accountInfo_)->id;
        priv->currentProp_ = new lrc::api::account::ConfProperties_t();
        *priv->currentProp_ = (*priv->accountInfo_)->accountModel->getAccountConfig(accountId);
    } catch (std::out_of_range& e) {
        g_debug("Can't get account config for current account");
        return;
    }

    bool has_account_manager = !priv->currentProp_->managerUri.isEmpty();

    auto label_status = priv->label_status;
    if ((*priv->accountInfo_)->profileInfo.type != lrc::api::profile::Type::JAMI) {
        label_status = priv->sip_label_status;
    }

    switch ((*priv->accountInfo_)->status)
    {
    case lrc::api::account::Status::INITIALIZING:
        gtk_label_set_text(GTK_LABEL(label_status), _("Initializing…"));
        break;
    case lrc::api::account::Status::UNREGISTERED:
        gtk_label_set_text(GTK_LABEL(label_status), _("Offline"));
        break;
    case lrc::api::account::Status::TRYING:
        gtk_label_set_text(GTK_LABEL(label_status), _("Connecting…"));
        break;
    case lrc::api::account::Status::REGISTERED:
        gtk_label_set_text(GTK_LABEL(label_status), _("Online"));
        break;
    case lrc::api::account::Status::INVALID:
    default:
        gtk_label_set_text(GTK_LABEL(label_status), _("Unknown status"));
        break;
    }

    gtk_widget_set_sensitive(priv->button_add_device,
        ((*priv->accountInfo_)->status) == lrc::api::account::Status::REGISTERED);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_display_name), qUtf8Printable((*priv->accountInfo_)->profileInfo.alias));

    if ((*priv->accountInfo_)->profileInfo.type == lrc::api::profile::Type::JAMI) {
        gtk_switch_set_active(GTK_SWITCH(priv->account_enabled), (*priv->accountInfo_)->enabled);

        gtk_widget_show_all(priv->vbox_devices);
        gtk_widget_show_all(priv->vbox_banned_contacts);
        gtk_widget_show_all(priv->username_box);
        gtk_widget_hide(priv->sip_info_box);
	gtk_widget_hide(priv->sip_auto_registration_box);
        gtk_widget_show(priv->button_export_account);
        gtk_widget_show(priv->type_box);
        gtk_widget_hide(priv->sip_enable_account);
        gtk_widget_show_all(priv->account_options_box);
        // Show ring id
        gtk_label_set_text(GTK_LABEL(priv->label_type_info), qUtf8Printable((*priv->accountInfo_)->profileInfo.uri));
        // Update export label
        gtk_button_set_label(GTK_BUTTON(priv->button_export_account), _("Export account"));
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
        gtk_label_set_text(GTK_LABEL(priv->label_change_password), (priv->currentProp_->archiveHasPassword)? _("••••••••") : _("no password set"));
        auto image = gtk_image_new_from_icon_name("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(priv->button_show_banned), image);
        while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_banned_contacts), 0)))
            gtk_container_remove(GTK_CONTAINER(priv->list_banned_contacts), children);
        for (const auto& contact : (*priv->accountInfo_)->contactModel->getBannedContacts())
            add_banned(view, contact.toStdString());
        gtk_widget_hide(priv->scrolled_window_banned_contacts);

        // Clear username box
        auto* children = gtk_container_get_children(GTK_CONTAINER(priv->username_box));
        if (!children) {
            g_debug("No username label... abort");
        }
        auto* username_registration_widget = username_registration_box_new(*priv->accountInfo_, true);
        gtk_widget_set_can_focus(username_registration_widget, true);
        auto* current_child = g_list_first(children);  // label
        auto* ubox = g_list_next(current_child);
        if (ubox) {
            gtk_container_remove(GTK_CONTAINER(priv->username_box), GTK_WIDGET(ubox->data));
        }

        gtk_switch_set_active(GTK_SWITCH(priv->custom_ringtone_button), priv->currentProp_->Ringtone.ringtoneEnabled);
        gtk_widget_set_sensitive(priv->filechooserbutton_custom_ringtone, priv->currentProp_->Ringtone.ringtoneEnabled);

        gtk_box_pack_end(GTK_BOX(priv->username_box), GTK_WIDGET(username_registration_widget), false, false, 0);
        gtk_widget_show(GTK_WIDGET(priv->username_box));
        gtk_widget_show(GTK_WIDGET(username_registration_widget));

        gtk_widget_show_all(priv->allow_call_row);
        gtk_widget_show_all(priv->box_name_server);
        gtk_widget_show_all(priv->box_dht);
        gtk_widget_hide(priv->box_published_address);
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

        std::string label_id = gtk_label_get_text(GTK_LABEL(priv->label_id));
        std::string label_username = gtk_label_get_text(GTK_LABEL(priv->label_username));
        std::string label_password = gtk_label_get_text(GTK_LABEL(priv->label_password));
        auto max_width = label_id.size();
        max_width = std::max(max_width, label_username.size());
        max_width = std::max(max_width, label_password.size());
        if (label_id.size() != max_width)
            gtk_label_set_width_chars(GTK_LABEL(priv->label_id), max_width + 1);

        gtk_button_set_label(GTK_BUTTON(priv->button_delete_account), has_account_manager? _("Remove account") : _("Delete account"));
        if (has_account_manager) {
            gtk_widget_hide(priv->button_export_account);
            gtk_widget_hide(priv->password_row);
            gtk_widget_hide(priv->button_add_device);
        }
    } else {
        gtk_switch_set_active(GTK_SWITCH(priv->sip_account_enabled), (*priv->accountInfo_)->enabled);

        gtk_widget_show(priv->sip_info_box);
        gtk_widget_hide(priv->account_options_box);
        gtk_widget_hide(priv->type_box);
        gtk_widget_show_all(priv->sip_enable_account);
	gtk_widget_show_all(priv->sip_auto_registration_box);
        gtk_widget_hide(priv->vbox_devices);
        gtk_widget_hide(priv->vbox_banned_contacts);
        gtk_widget_hide(priv->button_export_account);
        gtk_widget_hide(priv->username_box);
        gtk_widget_hide(priv->allow_call_row);
        gtk_widget_hide(priv->box_name_server);
        gtk_widget_hide(priv->box_dht);
        gtk_widget_show_all(priv->box_published_address);
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
        gtk_entry_set_text(GTK_ENTRY(priv->entry_tls_server_name), qUtf8Printable(priv->currentProp_->TLS.serverName));

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
        gtk_entry_set_text(GTK_ENTRY(priv->entry_sip_hostname), qUtf8Printable(priv->currentProp_->hostname));
        gtk_entry_set_text(GTK_ENTRY(priv->entry_sip_username), qUtf8Printable(priv->currentProp_->username));
        gtk_entry_set_text(GTK_ENTRY(priv->entry_sip_password), qUtf8Printable(priv->currentProp_->password));
        gtk_entry_set_text(GTK_ENTRY(priv->entry_sip_proxy), qUtf8Printable(priv->currentProp_->routeset));
        gtk_entry_set_text(GTK_ENTRY(priv->entry_sip_voicemail), qUtf8Printable(priv->currentProp_->mailbox));

    }
    draw_codecs(view);

    // advanced
    gtk_switch_set_active(GTK_SWITCH(priv->call_allow_button), priv->currentProp_->DHT.PublicInCalls);
    gtk_switch_set_active(GTK_SWITCH(priv->auto_answer_button), priv->currentProp_->autoAnswer);
    gtk_switch_set_active(GTK_SWITCH(priv->rendez_vous_button), priv->currentProp_->isRendezVous);

    gtk_entry_set_text(GTK_ENTRY(priv->entry_name_server), qUtf8Printable(priv->currentProp_->RingNS.uri));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_dht_proxy), qUtf8Printable(priv->currentProp_->proxyServer));
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_dht_proxy), priv->currentProp_->proxyEnabled);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_bootstrap), qUtf8Printable(priv->currentProp_->hostname));
    gtk_switch_set_active(GTK_SWITCH(priv->dht_proxy_button), priv->currentProp_->proxyEnabled);
    gtk_switch_set_active(GTK_SWITCH(priv->dht_peer_discovery_button), priv->currentProp_->peerDiscovery);
    if (!priv->currentProp_->TLS.certificateListFile.isEmpty())
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_ca_list), qUtf8Printable(priv->currentProp_->TLS.certificateListFile));
    if (!priv->currentProp_->TLS.certificateFile.isEmpty())
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_certificate), qUtf8Printable(priv->currentProp_->TLS.certificateFile));
    if (!priv->currentProp_->TLS.privateKeyFile.isEmpty())
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_private_key), qUtf8Printable(priv->currentProp_->TLS.privateKeyFile));
    if (!priv->currentProp_->TLS.password.isEmpty())
        gtk_entry_set_text(GTK_ENTRY(priv->entry_password), qUtf8Printable(priv->currentProp_->TLS.password));
    gtk_switch_set_active(GTK_SWITCH(priv->upnp_button), priv->currentProp_->upnpEnabled);
    gtk_switch_set_active(GTK_SWITCH(priv->sip_auto_registration_button), priv->currentProp_->keepAliveEnabled);
    gtk_switch_set_active(GTK_SWITCH(priv->switch_use_turn), priv->currentProp_->TURN.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_turnserver), priv->currentProp_->TURN.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_turnusername), priv->currentProp_->TURN.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_turnpassword), priv->currentProp_->TURN.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_turnrealm), priv->currentProp_->TURN.enable);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnserver), qUtf8Printable(priv->currentProp_->TURN.server));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnusername), qUtf8Printable(priv->currentProp_->TURN.username));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnpassword), qUtf8Printable(priv->currentProp_->TURN.password));
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnrealm), qUtf8Printable(priv->currentProp_->TURN.realm));
    gtk_switch_set_active(GTK_SWITCH(priv->switch_use_stun), priv->currentProp_->STUN.enable);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_stunserver), qUtf8Printable(priv->currentProp_->STUN.server));
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_stunserver), priv->currentProp_->STUN.enable);
    gtk_switch_set_active(GTK_SWITCH(priv->button_custom_published), !priv->currentProp_->publishedSameAsLocal);
    gtk_entry_set_text(GTK_ENTRY(priv->entry_published_address), qUtf8Printable(priv->currentProp_->publishedAddress));
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_published_address), !priv->currentProp_->publishedSameAsLocal);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spinbutton_published_port), priv->currentProp_->publishedPort);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->spinbutton_published_port), !priv->currentProp_->publishedSameAsLocal);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->filechooserbutton_ca_list), priv->currentProp_->TLS.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->filechooserbutton_certificate), priv->currentProp_->TLS.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->filechooserbutton_private_key), priv->currentProp_->TLS.enable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->entry_password), priv->currentProp_->TLS.enable);

    new_account_settings_view_show(view, true);
    if (reset_view) {
        show_general_settings(view);
    }
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
new_account_settings_view_new(AccountInfoPointer const & accountInfo, lrc::api::AVModel& avModel)
{
    gpointer view = g_object_new(NEW_ACCOUNT_SETTINGS_VIEW_TYPE, NULL);
    auto* priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(view);
    priv->accountInfo_ = &accountInfo;
    if (*priv->accountInfo_) {
        build_settings_view(NEW_ACCOUNT_SETTINGS_VIEW(view));
    }
    priv->avModel_ = &avModel;

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
        priv->avatarmanipulation = avatar_manipulation_new(*priv->accountInfo_, priv->avModel_);
        gtk_widget_set_halign(priv->avatar_box, GtkAlign::GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(priv->avatar_box), priv->avatarmanipulation, true, true, 0);
        gtk_widget_set_visible(priv->avatarmanipulation, true);
    }
}
