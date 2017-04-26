/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
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

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>

// LRC
#include <account.h>

// Ring Client
#include "accountbanstab.h"
#include "models/gtkqtreemodel.h"
#include "utils/models.h"

/**
 * gtk structure
 */
struct _AccountBansTab
{
    GtkBox parent;
};

/**
 * gtk class structure
 */
struct _AccountBansTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountBansTabPrivate AccountBansTabPrivate;

/**
 * gtk private structure
 */
struct _AccountBansTabPrivate
{
    Account   *account;
    GtkWidget *treeview_bans;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountBansTab, account_bans_tab, GTK_TYPE_BOX);

#define ACCOUNT_BANS_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_BANS_TAB_TYPE, AccountBansTabPrivate))

/**
 * gtk dispose function
 */
static void
account_bans_tab_dispose(GObject *object)
{
    G_OBJECT_CLASS(account_bans_tab_parent_class)->dispose(object);
}

/**
 * gtk finalize function
 */
static void
account_bans_tab_finalize(GObject *object)
{
    G_OBJECT_CLASS(account_bans_tab_parent_class)->finalize(object);
}

/**
 * gtk init function
 */
static void
account_bans_tab_init(AccountBansTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

/**
 * gtk class init function
 */
static void
account_bans_tab_class_init(AccountBansTabClass *klass)
{

    G_OBJECT_CLASS(klass)->finalize = account_bans_tab_finalize;
    G_OBJECT_CLASS(klass)->dispose = account_bans_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass), "/cx/ring/RingGnome/accountbanstab.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountBansTab, treeview_bans);
}

/**
 * gtk new function
 */
GtkWidget *
account_bans_tab_new(Account *account)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_BANS_TAB_TYPE, NULL);

    AccountBansTabPrivate *priv = ACCOUNT_BANS_TAB_GET_PRIVATE(view);
    priv->account = account;

    return (GtkWidget *)view;
}
