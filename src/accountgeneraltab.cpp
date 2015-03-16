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

#include "accountgeneraltab.h"

#include <gtk/gtk.h>
#include <account.h>

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
    GtkWidget *grid_account;
    GtkWidget *grid_parameters;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountGeneralTab, account_general_tab, GTK_TYPE_BOX);

#define ACCOUNT_GENERAL_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_GENERAL_TAB_TYPE, AccountGeneralTabPrivate))

static void
account_general_tab_dispose(GObject *object)
{
    // AccountGeneralTab *view;
    // AccountGeneralTabPrivate *priv;

    // view = CURRENT_CALL_VIEW(object);
    // priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    G_OBJECT_CLASS(account_general_tab_parent_class)->dispose(object);
}

static void
account_general_tab_init(AccountGeneralTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    // AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
}

static void
account_general_tab_class_init(AccountGeneralTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_general_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountgeneraltab.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, grid_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, grid_parameters);
}

static void
account_alias_changed(GtkEditable *entry, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account->setAlias(QString(gtk_editable_get_chars(entry, 0, -1)));
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
auto_answer(GtkToggleButton *checkbutton, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account->setAutoAnswer(gtk_toggle_button_get_active(checkbutton));
}

static void
upnp_enabled(GtkToggleButton *checkbutton, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account->setUpnpEnabled(gtk_toggle_button_get_active(checkbutton));
}


static void
build_tab_view(AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);

    int grid_row = 0;
    GtkWidget *label = NULL;
    GtkWidget *entry = NULL;
    GtkWidget *checkbutton = NULL;

    /* build account grid */
    
    /* check if its ip2ip account */
    const QByteArray& alias = priv->account->alias().toLocal8Bit();

    if (strcmp(alias.constData(), "IP2IP") == 0) {
        label = gtk_label_new("IP2IP");
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_account), label, 0, grid_row, 1, 1);
        ++grid_row;
    } else {
        /* account alias */
        label = gtk_label_new("Alias");
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_account), label, 0, grid_row, 1, 1);
        entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), alias.constData());
        gtk_widget_set_halign(entry, GTK_ALIGN_START);
        g_signal_connect(entry, "changed", G_CALLBACK(account_alias_changed), view);
        gtk_grid_attach(GTK_GRID(priv->grid_account), entry, 1, grid_row, 1, 1);
        ++grid_row;

        /* account type */
        label = gtk_label_new("Type");
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_account), label, 0, grid_row, 1, 1);
        
        label = gtk_label_new("");
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        switch (priv->account->protocol()) {
            case Account::Protocol::SIP:
                gtk_label_set_text(GTK_LABEL(label), "SIP");
                break;
            case Account::Protocol::IAX:
                gtk_label_set_text(GTK_LABEL(label), "IAX");
                break;
            case Account::Protocol::RING:
                gtk_label_set_text(GTK_LABEL(label), "RING");
                break;
            case Account::Protocol::COUNT__:
                break;
        }
        
        gtk_grid_attach(GTK_GRID(priv->grid_account), label, 1, grid_row, 1, 1);
        ++grid_row;
    }

    if (priv->account->protocol() == Account::Protocol::RING) {
        label = gtk_label_new("Hash");
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_account), label, 0, grid_row, 1, 1);
        entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), priv->account->username().toLocal8Bit().constData());
        g_object_set(G_OBJECT(entry), "editable", FALSE, NULL);
        // gtk_widget_set_hexpand(entry, TRUE);
        gtk_entry_set_max_width_chars(GTK_ENTRY(entry), 50);
        gtk_widget_override_font(entry, pango_font_description_from_string("monospace"));
        gtk_entry_set_alignment(GTK_ENTRY(entry), 0.5);
        gtk_grid_attach(GTK_GRID(priv->grid_account), entry, 1, grid_row, 1, 1);
        ++grid_row;
    }

    gtk_widget_show_all(priv->grid_account);

    /* build parameters grid */
    grid_row = 0;
    if (strcmp(alias.constData(), "IP2IP") != 0) {
        if (priv->account->protocol() != Account::Protocol::RING) {
            /* SIP and IAX have the same params */

            /* host name */
            label = gtk_label_new("Host name");
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
            entry = gtk_entry_new();
            gtk_entry_set_text(GTK_ENTRY(entry), priv->account->hostname().toLocal8Bit().constData());
            g_signal_connect(entry, "changed", G_CALLBACK(account_hostname_changed), view);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry, 1, grid_row, 1, 1);
            ++grid_row;

            /* user name */
            label = gtk_label_new("User name");
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
            entry = gtk_entry_new();
            gtk_entry_set_text(GTK_ENTRY(entry), priv->account->username().toLocal8Bit().constData());
            g_signal_connect(entry, "changed", G_CALLBACK(account_username_changed), view);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry, 1, grid_row, 1, 1);
            ++grid_row;

            /* password */
            label = gtk_label_new("Password");
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
            entry = gtk_entry_new();
            gtk_entry_set_input_purpose(GTK_ENTRY(entry), GTK_INPUT_PURPOSE_PASSWORD);
            gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, "dialog-password");
            gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
            gtk_entry_set_text(GTK_ENTRY(entry), priv->account->username().toLocal8Bit().constData());
            g_signal_connect(entry, "changed", G_CALLBACK(account_password_changed), view);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry, 1, grid_row, 1, 1);
            ++grid_row;

            /* show password */
            checkbutton = gtk_check_button_new_with_label("Show password");
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), checkbutton, 1, grid_row, 1, 1);
            g_signal_connect(checkbutton, "toggled", G_CALLBACK(show_password), entry);
            ++grid_row;

            /* proxy */
            label = gtk_label_new("Proxy");
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
            entry = gtk_entry_new();
            gtk_entry_set_text(GTK_ENTRY(entry), priv->account->proxy().toLocal8Bit().constData());
            g_signal_connect(entry, "changed", G_CALLBACK(account_proxy_changed), view);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry, 1, grid_row, 1, 1);
            ++grid_row;

            /* voicemail number */
            label = gtk_label_new("Voicemail number");
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
            entry = gtk_entry_new();
            gtk_entry_set_text(GTK_ENTRY(entry), priv->account->mailbox().toLocal8Bit().constData());
            g_signal_connect(entry, "changed", G_CALLBACK(account_mailbox_changed), view);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry, 1, grid_row, 1, 1);
            ++grid_row;
        } else {
            /* RING accoutn */

            /* bootstrap */
            label = gtk_label_new("Bootstrap");
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
            entry = gtk_entry_new();
            gtk_entry_set_text(GTK_ENTRY(entry), priv->account->hostname().toLocal8Bit().constData());
            g_signal_connect(entry, "changed", G_CALLBACK(account_hostname_changed), view);
            gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry, 1, grid_row, 1, 1);
            ++grid_row;
        }
    }

    /* auto answer */
    checkbutton = gtk_check_button_new_with_label("Auto-answer calls");
    gtk_widget_set_halign(checkbutton, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(priv->grid_parameters), checkbutton, 0, grid_row, 1, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), priv->account->isAutoAnswer());
    g_signal_connect(checkbutton, "toggled", G_CALLBACK(auto_answer), view);
    ++grid_row;

    /* upnp */
    checkbutton = gtk_check_button_new_with_label("UPnP enabled");
    gtk_widget_set_halign(checkbutton, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(priv->grid_parameters), checkbutton, 0, grid_row, 1, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), priv->account->isUpnpEnabled());
    g_signal_connect(checkbutton, "toggled", G_CALLBACK(upnp_enabled), view);
    ++grid_row;

    gtk_widget_show_all(priv->grid_parameters);

}

GtkWidget *
account_general_tab_new(Account *account)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_GENERAL_TAB_TYPE, NULL);

    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account = account;

    build_tab_view(ACCOUNT_GENERAL_TAB(view));

    return (GtkWidget *)view;
}
