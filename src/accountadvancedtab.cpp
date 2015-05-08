/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "accountadvancedtab.h"

#include <gtk/gtk.h>
#include <account.h>

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
    GtkWidget *adjustment_registration_timeout;
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
    GtkWidget *entry_stun_server;
    GtkWidget *checkbutton_use_turn;
    GtkWidget *entry_turn_server;
    GtkWidget *adjustment_audio_port_min;
    GtkWidget *adjustment_audio_port_max;
    GtkWidget *adjustment_video_port_min;
    GtkWidget *adjustment_video_port_max;

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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_registration_timeout);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, entry_stun_server);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, checkbutton_use_turn);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, entry_turn_server);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_audio_port_min);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_audio_port_max);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_video_port_min);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_video_port_max);
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
    gtk_widget_set_sensitive(priv->entry_stun_server, use_stun);
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
    gtk_widget_set_sensitive(priv->entry_turn_server, use_turn);
}

static void
turn_server_changed(GtkEntry *entry, AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    priv->account->setTurnServer(gtk_entry_get_text(entry));
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
build_tab_view(AccountAdvancedTab *self)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(self));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(self);

    /* check if its ip2ip account */
    const QByteArray& alias = priv->account->alias().toUtf8();

    /* only show registration timeout for SIP account */
    if ( (priv->account->protocol() != Account::Protocol::SIP)
        || (strcmp(alias.constData(), "IP2IP") == 0) ) {
        gtk_container_remove(GTK_CONTAINER(priv->vbox_main), priv->frame_registration);
        priv->frame_registration = NULL;
    } else {
        gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_registration_timeout),
                                 priv->account->registrationExpire());
        g_signal_connect(priv->adjustment_registration_timeout,
                         "value-changed", G_CALLBACK(registration_expire_changed), self);
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
    gtk_entry_set_text(GTK_ENTRY(priv->entry_stun_server),
                       priv->account->sipStunServer().toUtf8().constData());
    gtk_widget_set_sensitive(priv->entry_stun_server,
                             priv->account->isSipStunEnabled());
    g_signal_connect(priv->checkbutton_use_stun,
                     "toggled", G_CALLBACK(stun_enabled_toggled), self);
    g_signal_connect(priv->entry_stun_server,
                     "changed", G_CALLBACK(stun_server_changed), self);

    /* TURN */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_turn),
                                 priv->account->isTurnEnabled());
    gtk_entry_set_text(GTK_ENTRY(priv->entry_turn_server),
                       priv->account->turnServer().toUtf8().constData());
    gtk_widget_set_sensitive(priv->entry_turn_server,
                             priv->account->isTurnEnabled());
    g_signal_connect(priv->checkbutton_use_turn,
                     "toggled", G_CALLBACK(turn_enabled_toggled), self);
    g_signal_connect(priv->entry_turn_server,
                     "changed", G_CALLBACK(turn_server_changed), self);

    /* audio/video rtp port range */
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

    /* update account UI if model is updated */
    priv->account_updated = QObject::connect(
        priv->account,
        &Account::changed,
        [=] () {
            /* only show registration timeout for SIP account */
            if ( priv->frame_registration ) {
                gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_registration_timeout),
                                        priv->account->registrationExpire());
            }

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
            gtk_entry_set_text(GTK_ENTRY(priv->entry_stun_server),
                               priv->account->sipStunServer().toUtf8().constData());

            /* TURN */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_turn),
                                        priv->account->isTurnEnabled());
            gtk_entry_set_text(GTK_ENTRY(priv->entry_turn_server),
                               priv->account->turnServer().toUtf8().constData());

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
