
/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
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

#include "accountdevicestab.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <account.h>
#include <codecmodel.h>
#include "models/gtkqsortfiltertreemodel.h"
#include "utils/models.h"

struct _AccountDevicesTab
{
    GtkBox parent;
};

struct _AccountDevicesTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountDevicesTabPrivate AccountDevicesTabPrivate;

struct _AccountDevicesTabPrivate
{
    Account   *account;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountDevicesTab, account_devices_tab, GTK_TYPE_BOX);

#define ACCOUNT_DEVICES_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_DEVICES_TAB_TYPE, AccountDevicesTabPrivate))

static void
account_devices_tab_dispose(GObject *object)
{
    G_OBJECT_CLASS(account_devices_tab_parent_class)->dispose(object);
}

static void
account_devices_tab_init(AccountDevicesTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
account_devices_tab_class_init(AccountDevicesTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_devices_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountdevicestab.ui");
}

static void
build_tab_view(AccountDevicesTab *view)
{
    g_return_if_fail(IS_ACCOUNT_DEVICES_TAB(view));
    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);
}

GtkWidget *
account_devices_tab_new(Account *account)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_DEVICES_TAB_TYPE, NULL);

    AccountDevicesTabPrivate *priv = ACCOUNT_DEVICES_TAB_GET_PRIVATE(view);
    priv->account = account;

    build_tab_view(ACCOUNT_DEVICES_TAB(view));

    return (GtkWidget *)view;
}
