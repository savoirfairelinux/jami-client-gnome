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

#include "accountgeneraltab.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <account.h>
#include "defines.h"
#include "utils/models.h"
#include "usernameregistrationbox.h"

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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, grid_parameters);
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
entry_name_service_url_changed(GtkEditable *entry, AccountGeneralTab *view)
{
    g_return_if_fail(IS_ACCOUNT_GENERAL_TAB(view));
    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account->setNameServiceURL(QString(gtk_editable_get_chars(entry, 0, -1)));
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

    /* separate pointers for each so that we reference them in the account changed callback */
    GtkWidget *entry_alias = NULL;
    GtkWidget *entry_username = NULL;
    GtkWidget *entry_hostname = NULL;
    GtkWidget *entry_password = NULL;
    GtkWidget *entry_proxy = NULL;
    GtkWidget *entry_voicemail = NULL;
    GtkWidget *checkbutton_autoanswer = NULL;
    GtkWidget *checkbutton_upnp = NULL;

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
        /* RING account */

        /* Name service */
        label = gtk_label_new(_("Name service URL"));
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), label, 0, grid_row, 1, 1);
        GtkWidget* entry_name_service_url = gtk_entry_new();
        gtk_widget_set_halign(entry_name_service_url, GTK_ALIGN_START);
        gtk_entry_set_text(GTK_ENTRY(entry_name_service_url), priv->account->nameServiceURL().toLocal8Bit().constData());
        gtk_grid_attach(GTK_GRID(priv->grid_parameters), entry_name_service_url, 1, grid_row, 1, 1);
        g_signal_connect(entry_name_service_url, "changed", G_CALLBACK(entry_name_service_url_changed), view);
        ++grid_row;

    }

    /* auto answer */
    checkbutton_autoanswer = gtk_check_button_new_with_label(_("Auto-answer calls"));
    gtk_widget_set_halign(checkbutton_autoanswer, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(priv->grid_parameters), checkbutton_autoanswer, 0, grid_row, 1, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton_autoanswer), priv->account->isAutoAnswer());
    g_signal_connect(checkbutton_autoanswer, "toggled", G_CALLBACK(auto_answer), view);
    ++grid_row;

    /* upnp */
    checkbutton_upnp = gtk_check_button_new_with_label(_("UPnP enabled"));
    gtk_widget_set_halign(checkbutton_upnp, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(priv->grid_parameters), checkbutton_upnp, 0, grid_row, 1, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton_upnp), priv->account->isUpnpEnabled());
    g_signal_connect(checkbutton_upnp, "toggled", G_CALLBACK(upnp_enabled), view);
    ++grid_row;

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

            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton_autoanswer), priv->account->isAutoAnswer());
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton_upnp), priv->account->isUpnpEnabled());
        }
    );

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
