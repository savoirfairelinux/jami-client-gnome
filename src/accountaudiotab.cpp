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

#include "accountaudiotab.h"

#include <gtk/gtk.h>
#include <account.h>

struct _AccountAudioTab
{
    GtkBox parent;
};

struct _AccountAudioTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountAudioTabPrivate AccountAudioTabPrivate;

struct _AccountAudioTabPrivate
{
    Account *account;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountAudioTab, account_audio_tab, GTK_TYPE_BOX);

#define ACCOUNT_AUDIO_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_AUDIO_TAB_TYPE, AccountAudioTabPrivate))

static void
account_audio_tab_dispose(GObject *object)
{
    // AccountAudioTab *view;
    // AccountAudioTabPrivate *priv;

    // view = CURRENT_CALL_VIEW(object);
    // priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    G_OBJECT_CLASS(account_audio_tab_parent_class)->dispose(object);
}

static void
account_audio_tab_init(AccountAudioTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    // AccountAudioTabPrivate *priv = ACCOUNT_AUDIO_TAB_GET_PRIVATE(view);
}

static void
account_audio_tab_class_init(AccountAudioTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_audio_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountaudiotab.ui");

    // gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountAudioTab, treeview_account_list);
}

GtkWidget *
account_audio_tab_new(Account *account)
{
    gpointer view = g_object_new(ACCOUNT_AUDIO_TAB_TYPE, NULL);

    AccountAudioTabPrivate *priv = ACCOUNT_AUDIO_TAB_GET_PRIVATE(view);
    priv->account = account;

    return (GtkWidget *)view;
}
