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
    Account *account;
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

    // gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountGeneralTab, treeview_account_list);
}

GtkWidget *
account_general_tab_new(Account *account)
{
    gpointer view = g_object_new(ACCOUNT_GENERAL_TAB_TYPE, NULL);

    AccountGeneralTabPrivate *priv = ACCOUNT_GENERAL_TAB_GET_PRIVATE(view);
    priv->account = account;

    return (GtkWidget *)view;
}
