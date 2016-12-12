/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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

#include "accountadvancedtab.h"

#include <gtk/gtk.h>
#include <account.h>
#include "accountaudiotab.h"
#include "accountvideotab.h"

struct _AccountAdvancedTab
{
    GtkBox parent;
};

struct _AccountAdvancedTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountAdvancedTabPrivate AccountAdvancedTabPrivate;

struct _AccountAdvancedTabPrivate
{
    Account   *account;
    GtkWidget *vbox_main;
    GtkWidget *frame_registration;
    GtkWidget *box_registration;
    GtkWidget *box_registration_expire;
    GtkWidget *adjustment_registration_timeout;
    GtkWidget *checkbutton_allow_incoming_unknown;
    GtkWidget *checkbutton_allow_incoming_history;
    GtkWidget *checkbutton_allow_incoming_contacts;
    GtkWidget *frame_network_interface;
    GtkWidget *box_network_interface;
    GtkWidget *checkbutton_use_random_port;
    GtkWidget *box_local_port;
    GtkWidget *adjustment_local_port;
    GtkWidget *radiobutton_same_as_local;
    GtkWidget *radiobutton_set_published;
    GtkWidget *box_published_address_port;
    GtkWidget *entry_published_address;
    GtkWidget *spinbutton_published_port;
    GtkWidget *adjustment_published_port;
    GtkWidget *checkbutton_use_stun;
    GtkWidget *grid_stun;
    GtkWidget *entry_stunserver;
    GtkWidget *checkbutton_use_turn;
    GtkWidget *grid_turn;
    GtkWidget *entry_turnserver;
    GtkWidget *entry_turnusername;
    GtkWidget *entry_turnpassword;
    GtkWidget *entry_turnrealm;
    GtkWidget *frame_ice_fallback;
    GtkWidget *adjustment_audio_port_min;
    GtkWidget *adjustment_audio_port_max;
    GtkWidget *adjustment_video_port_min;
    GtkWidget *adjustment_video_port_max;
    GtkWidget *frame_video_codecs;
    GtkWidget *frame_audio_codecs;

    QMetaObject::Connection account_updated;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountAdvancedTab, account_advanced_tab, GTK_TYPE_BOX);

#define ACCOUNT_ADVANCED_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_ADVANCED_TAB_TYPE, AccountAdvancedTabPrivate))

static void
account_advanced_tab_dispose(GObject *object)
{
    AccountAdvancedTab *view = ACCOUNT_ADVANCED_TAB(object);
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(view);

    QObject::disconnect(priv->account_updated);

    G_OBJECT_CLASS(account_advanced_tab_parent_class)->dispose(object);
}

static void
account_advanced_tab_init(AccountAdvancedTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
account_advanced_tab_class_init(AccountAdvancedTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_advanced_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountadvancedtab.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, vbox_main);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, frame_registration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, box_registration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, box_registration_expire);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_registration_timeout);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, checkbutton_allow_incoming_unknown);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, checkbutton_allow_incoming_history);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, checkbutton_allow_incoming_contacts);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, frame_network_interface);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, box_network_interface);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, checkbutton_use_random_port);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, box_local_port);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_local_port);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, radiobutton_same_as_local);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, radiobutton_set_published);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, box_published_address_port);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, entry_published_address);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, spinbutton_published_port);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_published_port);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, checkbutton_use_stun);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, grid_stun);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, entry_stunserver);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, checkbutton_use_turn);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, grid_turn);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, entry_turnserver);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, entry_turnusername);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, entry_turnpassword);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, entry_turnrealm);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, frame_ice_fallback);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_audio_port_min);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_audio_port_max);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_video_port_min);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_video_port_max);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, frame_audio_codecs);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, frame_video_codecs);
}

static void
registration_expire_changed(GtkAdjustment *adjustment, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    int timeout = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
    priv->account->setRegistrationExpire(timeout);
}

static void
local_port_changed(GtkAdjustment *adjustment, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    unsigned short int local_port = (unsigned short int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));

    if (priv->account->protocol() != Account::Protocol::RING) {
        priv->account->setLocalPort(local_port);
    } else {
        priv->account->setBootstrapPort(local_port);
    }
}

static void
same_as_local_toggled(GtkToggleButton *toggle_button, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    gboolean published_same_as_local = gtk_toggle_button_get_active(toggle_button);

    priv->account->setPublishedSameAsLocal(published_same_as_local);

    /* disactivate the published address and port fields if same as local is true */
    gtk_widget_set_sensitive(priv->box_published_address_port, !published_same_as_local);
}

static void
published_address_changed(GtkEntry *entry, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    priv->account->setPublishedAddress(gtk_entry_get_text(entry));
}

static void
published_port_changed(GtkAdjustment *adjustment, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    unsigned short int published_port = (unsigned short int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
    priv->account->setPublishedPort(published_port);
}

static void
stun_enabled_toggled(GtkToggleButton *toggle_button, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    gboolean use_stun = gtk_toggle_button_get_active(toggle_button);

    priv->account->setSipStunEnabled(use_stun);

    /* disactivate the stun server entry if stun is disabled */
    gtk_widget_set_sensitive(priv->grid_stun, use_stun);
}

static void
stun_server_changed(GtkEntry *entry, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    priv->account->setSipStunServer(gtk_entry_get_text(entry));
}

static void
turn_enabled_toggled(GtkToggleButton *toggle_button, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    gboolean use_turn = gtk_toggle_button_get_active(toggle_button);

    priv->account->setTurnEnabled(use_turn);

    /* disactivate the turn server entry if turn is disabled */
    gtk_widget_set_sensitive(priv->grid_turn, use_turn);
}

static void
turn_server_changed(GtkEntry *entry, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    priv->account->setTurnServer(gtk_entry_get_text(entry));
}

static void
turn_serverusername_changed(GtkEntry *entry, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    priv->account->setTurnServerUsername(gtk_entry_get_text(entry));
}

static void
turn_serverpassword_changed(GtkEntry *entry, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    priv->account->setTurnServerPassword(gtk_entry_get_text(entry));
}

static void
turn_serverrealm_changed(GtkEntry *entry, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    priv->account->setTurnServerRealm(gtk_entry_get_text(entry));
}

static void
audio_port_min_changed(GtkAdjustment *adjustment, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    int port = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
    priv->account->setAudioPortMin(port);
}

static void
audio_port_max_changed(GtkAdjustment *adjustment, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    int port = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
    priv->account->setAudioPortMax(port);
}

static void
video_port_min_changed(GtkAdjustment *adjustment, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    int port = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
    priv->account->setVideoPortMin(port);
}

static void
video_port_max_changed(GtkAdjustment *adjustment, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    int port = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
    priv->account->setVideoPortMax(port);
}

static void
allow_from_unknown_toggled(GtkToggleButton *toggle_button, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    gboolean allow = gtk_toggle_button_get_active(toggle_button);

    priv->account->setAllowIncomingFromUnknown(allow);
}

static void
allow_from_history_toggled(GtkToggleButton *toggle_button, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    gboolean allow = gtk_toggle_button_get_active(toggle_button);

    priv->account->setAllowIncomingFromHistory(allow);
}

static void
allow_from_contacts_toggled(GtkToggleButton *toggle_button, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    gboolean allow = gtk_toggle_button_get_active(toggle_button);

    priv->account->setAllowIncomingFromContact(allow);
}

static void
build_tab_view(AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    /* only show registration timeout for SIP account */
    if (priv->account->protocol() != Account::Protocol::SIP) {
        gtk_container_remove(GTK_CONTAINER(priv->box_registration), priv->box_registration_expire);
        priv->box_registration_expire = NULL;
    } else {
        gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_registration_timeout),
                                 priv->account->registrationExpire());
        g_signal_connect(priv->adjustment_registration_timeout,
                         "value-changed", G_CALLBACK(registration_expire_changed), self);
    }

    /* only show allow call options for Ring accounts */
    if (priv->account->protocol() != Account::Protocol::RING) {
        gtk_container_remove(GTK_CONTAINER(priv->box_registration), priv->checkbutton_allow_incoming_unknown);
        priv->checkbutton_allow_incoming_unknown = NULL;
        gtk_container_remove(GTK_CONTAINER(priv->box_registration), priv->checkbutton_allow_incoming_history);
        priv->checkbutton_allow_incoming_history = NULL;
        gtk_container_remove(GTK_CONTAINER(priv->box_registration), priv->checkbutton_allow_incoming_contacts);
        priv->checkbutton_allow_incoming_contacts = NULL;
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_allow_incoming_unknown),
                                     priv->account->allowIncomingFromUnknown());
        g_signal_connect(priv->checkbutton_allow_incoming_unknown,
                         "toggled", G_CALLBACK(allow_from_unknown_toggled), self);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_allow_incoming_history),
                                     priv->account->allowIncomingFromHistory());
        g_signal_connect(priv->checkbutton_allow_incoming_history,
                         "toggled", G_CALLBACK(allow_from_history_toggled), self);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_allow_incoming_contacts),
                                     priv->account->allowIncomingFromContact());
        g_signal_connect(priv->checkbutton_allow_incoming_contacts,
                         "toggled", G_CALLBACK(allow_from_contacts_toggled), self);
    }

    /* local port */
    if (priv->account->protocol() != Account::Protocol::RING) {
        gtk_container_remove(GTK_CONTAINER(priv->box_network_interface), priv->checkbutton_use_random_port);
        priv->checkbutton_use_random_port = NULL;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_local_port),
                                priv->account->localPort());
        g_signal_connect(priv->adjustment_local_port,
                         "value-changed", G_CALLBACK(local_port_changed), self);
    } else {
        /* TODO: when this option is added, for now just don't set it
         * gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_local_port),
         *                         priv->account->bootstrapPort());
         */
        gtk_container_remove(GTK_CONTAINER(priv->vbox_main), priv->frame_network_interface);
        priv->frame_network_interface = NULL;
    }

    /* published address/port same as local */
    if (priv->account->isPublishedSameAsLocal()) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->radiobutton_same_as_local), TRUE);

        /* disactivate the published address and port fields if same as local is true */
        gtk_widget_set_sensitive(priv->box_published_address_port, FALSE);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->radiobutton_set_published), TRUE);
    }

    g_signal_connect(priv->radiobutton_same_as_local,
                     "toggled", G_CALLBACK(same_as_local_toggled), self);

    /* published address and port */
    gtk_entry_set_text(GTK_ENTRY(priv->entry_published_address),
                       priv->account->publishedAddress().toUtf8().constData());
    g_signal_connect(priv->entry_published_address,
                     "changed", G_CALLBACK(published_address_changed), self);

    gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_published_port),
                             priv->account->publishedPort());
    g_signal_connect(priv->adjustment_published_port,
                     "value-changed", G_CALLBACK(published_port_changed), self);

    /* STUN */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_stun),
                                 priv->account->isSipStunEnabled());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_stunserver),
                       priv->account->sipStunServer().toUtf8().constData());
    gtk_widget_set_sensitive(priv->grid_stun,
                             priv->account->isSipStunEnabled());
    g_signal_connect(priv->checkbutton_use_stun,
                     "toggled", G_CALLBACK(stun_enabled_toggled), self);
    g_signal_connect(priv->entry_stunserver,
                     "changed", G_CALLBACK(stun_server_changed), self);

    /* TURN */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_turn),
                                 priv->account->isTurnEnabled());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnserver),
                       priv->account->turnServer().toUtf8().constData());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnusername),
                       priv->account->turnServerUsername().toUtf8().constData());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnpassword),
                       priv->account->turnServerPassword().toUtf8().constData());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turnrealm),
                       priv->account->turnServerRealm().toUtf8().constData());
    gtk_widget_set_sensitive(priv->grid_turn,
                             priv->account->isTurnEnabled());
    g_signal_connect(priv->checkbutton_use_turn,
                     "toggled", G_CALLBACK(turn_enabled_toggled), self);
    g_signal_connect(priv->entry_turnserver,
                     "changed", G_CALLBACK(turn_server_changed), self);
    g_signal_connect(priv->entry_turnusername,
                     "changed", G_CALLBACK(turn_serverusername_changed), self);
    g_signal_connect(priv->entry_turnpassword,
                     "changed", G_CALLBACK(turn_serverpassword_changed), self);
    g_signal_connect(priv->entry_turnrealm,
                     "changed", G_CALLBACK(turn_serverrealm_changed), self);

    /* audio/video rtp port range */

    /* ice fallback features are not relevant to Ring account, as every RING
       client supports ICE */
    gtk_widget_set_visible(
        priv->frame_ice_fallback,
        priv->account->protocol() != Account::Protocol::RING
    );
    gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_audio_port_min),
                             priv->account->audioPortMin());
    g_signal_connect(priv->adjustment_audio_port_min,
                     "value-changed", G_CALLBACK(audio_port_min_changed), self);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_audio_port_max),
                             priv->account->audioPortMax());
    g_signal_connect(priv->adjustment_audio_port_min,
                     "value-changed", G_CALLBACK(audio_port_max_changed), self);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_video_port_min),
                             priv->account->videoPortMin());
    g_signal_connect(priv->adjustment_audio_port_min,
                     "value-changed", G_CALLBACK(video_port_min_changed), self);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_video_port_max),
                             priv->account->videoPortMax());
    g_signal_connect(priv->adjustment_video_port_max,
                     "value-changed", G_CALLBACK(video_port_max_changed), self);

    /* audio and video codecs */
    auto video_codecs = account_video_tab_new(priv->account);
    gtk_container_add(
        GTK_CONTAINER(priv->frame_video_codecs),
        video_codecs
    );
    gtk_widget_show(video_codecs);

    auto audio_codecs = account_audio_tab_new(priv->account);
    gtk_container_add(
        GTK_CONTAINER(priv->frame_audio_codecs),
        audio_codecs
    );
    gtk_widget_show(audio_codecs);

    /* update account UI if model is updated */
    priv->account_updated = QObject::connect(
        priv->account,
        &Account::changed,
        [=] () {
            /* only show registration timeout for SIP account */
            if ( priv->box_registration_expire ) {
                gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_registration_timeout),
                                        priv->account->registrationExpire());
            }

            if (priv->checkbutton_allow_incoming_unknown)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_allow_incoming_unknown),
                                             priv->account->allowIncomingFromUnknown());

            if (priv->checkbutton_allow_incoming_history)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_allow_incoming_history),
                                             priv->account->allowIncomingFromHistory());

            if (priv->checkbutton_allow_incoming_contacts)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_allow_incoming_contacts),
                                             priv->account->allowIncomingFromContact());

            /* local port */
            if (priv->account->protocol() != Account::Protocol::RING) {
                gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_local_port),
                                        priv->account->localPort());
            }

            /* published address/port same as local */
            if (priv->account->isPublishedSameAsLocal()) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->radiobutton_same_as_local), TRUE);
            } else {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->radiobutton_set_published), TRUE);
            }

            /* published address and port */
            gtk_entry_set_text(GTK_ENTRY(priv->entry_published_address),
                            priv->account->publishedAddress().toUtf8().constData());
            gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_published_port),
                                    priv->account->publishedPort());

            /* STUN */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_stun),
                                        priv->account->isSipStunEnabled());
            gtk_widget_set_sensitive(priv->grid_stun, priv->account->isSipStunEnabled());
            gtk_entry_set_text(GTK_ENTRY(priv->entry_stunserver),
                               priv->account->sipStunServer().toUtf8().constData());

            /* TURN */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_turn),
                                        priv->account->isTurnEnabled());
            gtk_widget_set_sensitive(priv->grid_turn, priv->account->isTurnEnabled());
            gtk_entry_set_text(GTK_ENTRY(priv->entry_turnserver),
                               priv->account->turnServer().toUtf8().constData());
            gtk_entry_set_text(GTK_ENTRY(priv->entry_turnusername),
                               priv->account->turnServerUsername().toUtf8().constData());
            gtk_entry_set_text(GTK_ENTRY(priv->entry_turnpassword),
                               priv->account->turnServerPassword().toUtf8().constData());
            gtk_entry_set_text(GTK_ENTRY(priv->entry_turnrealm),
                               priv->account->turnServerRealm().toUtf8().constData());

            /* audio/video rtp port range */
            gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_audio_port_min),
                                    priv->account->audioPortMin());
            gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_audio_port_max),
                                    priv->account->audioPortMax());
            gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_video_port_min),
                                    priv->account->videoPortMin());
            gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_video_port_max),
                                    priv->account->videoPortMax());
        }
    );
}

GtkWidget *
account_advanced_tab_new(Account *account)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_ADVANCED_TAB_TYPE, NULL);

    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(view);
    priv->account = account;

    build_tab_view(ACCOUNT_ADVANCED_TAB(view));

    return (GtkWidget *)view;
}
