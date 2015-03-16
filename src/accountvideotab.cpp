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

#include "accountvideotab.h"

#include <gtk/gtk.h>
#include <account.h>

struct _AccountVideoTab
{
    GtkBox parent;
};

struct _AccountVideoTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountVideoTabPrivate AccountVideoTabPrivate;

struct _AccountVideoTabPrivate
{
    Account *account;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountVideoTab, account_video_tab, GTK_TYPE_BOX);

#define ACCOUNT_VIDEO_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_VIDEO_TAB_TYPE, AccountVideoTabPrivate))

static void
account_video_tab_dispose(GObject *object)
{
    // AccountVideoTab *view;
    // AccountVideoTabPrivate *priv;

    // view = CURRENT_CALL_VIEW(object);
    // priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    G_OBJECT_CLASS(account_video_tab_parent_class)->dispose(object);
}

static void
account_video_tab_init(AccountVideoTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    // AccountVideoTabPrivate *priv = ACCOUNT_VIDEO_TAB_GET_PRIVATE(view);
}

static void
account_video_tab_class_init(AccountVideoTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_video_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountvideotab.ui");

    // gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountVideoTab, treeview_account_list);
}

GtkWidget *
account_video_tab_new(Account *account)
{
    gpointer view = g_object_new(ACCOUNT_VIDEO_TAB_TYPE, NULL);

    AccountVideoTabPrivate *priv = ACCOUNT_VIDEO_TAB_GET_PRIVATE(view);
    priv->account = account;

    return (GtkWidget *)view;
}
