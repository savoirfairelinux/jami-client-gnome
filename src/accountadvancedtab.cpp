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
    GtkWidget *adjustment_local_port;
    GtkWidget *radiobutton_set_published;
    GtkWidget *entry_published_address;
    GtkWidget *adjustment_published_port;
    GtkWidget *checkbutton_use_stun;
    GtkWidget *entry_stun_server;
    GtkWidget *checkbutton_use_turn;
    GtkWidget *entry_turn_server;
    GtkWidget *adjustment_audio_port_min;
    GtkWidget *adjustment_audio_port_max;
    GtkWidget *adjustment_video_port_min;
    GtkWidget *adjustment_video_port_max;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountAdvancedTab, account_advanced_tab, GTK_TYPE_BOX);

#define ACCOUNT_ADVANCED_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_ADVANCED_TAB_TYPE, AccountAdvancedTabPrivate))

static void
account_advanced_tab_dispose(GObject *object)
{
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, adjustment_local_port);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, radiobutton_set_published);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAdvancedTab, entry_published_address);
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
build_tab_view(AccountAdvancedTab *view)
{
    g_return_if_fail(IS_ACCOUNT_ADVANCED_TAB(view));
    AccountAdvancedTabPrivate *priv = ACCOUNT_ADVANCED_TAB_GET_PRIVATE(view);
    
    /* check if its ip2ip account */
    const QByteArray& alias = priv->account->alias().toUtf8();
    
    /* only show registration timeout for SIP account */
    if ( (priv->account->protocol() != Account::Protocol::SIP)
        || (strcmp(alias.constData(), "IP2IP") == 0) ) {
        gtk_container_remove(GTK_CONTAINER(priv->vbox_main), priv->frame_registration);
    }
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
