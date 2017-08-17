/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "itempopupmenu.h"

// GTK+ related
#include <glib/gi18n.h>
#include <QString>

// LRC
#include <availableaccountmodel.h>
#include <smartlistitem.h>
#include <smartlistmodel.h>
#include <database.h>
#include <newconversationitem.h>

struct _ItemPopupMenu
{
    GtkMenu parent;
};

struct _ItemPopupMenuClass
{
    GtkMenuClass parent_class;
};

typedef struct _ItemPopupMenuPrivate ItemPopupMenuPrivate;

struct _ItemPopupMenuPrivate
{
    GtkTreeView *treeview;
};

G_DEFINE_TYPE_WITH_PRIVATE(ItemPopupMenu, item_popup_menu, GTK_TYPE_MENU);

#define ITEM_POPUP_MENU_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ITEM_POPUP_MENU_TYPE, ItemPopupMenuPrivate))

static void
remove_history_item(G_GNUC_UNUSED GtkWidget *menu, gint* row)
{
    auto item = SmartListModel::instance().getItem(*row);
    g_return_if_fail(item);
    auto account = AvailableAccountModel::instance().currentDefaultAccount();
    g_return_if_fail(account);
    //TODO add item->getURI and update chatview.
    //TODO should not use Database here but item->removeHistory
    DataBase::instance().removeHistory(QString(item->getAlias().c_str()), QString(account->id()));
}

static void
remove_conversation(G_GNUC_UNUSED GtkWidget *menu, gint* row)
{
    auto item = SmartListModel::instance().getItem(*row);
    g_return_if_fail(item);
    SmartListModel::instance().removeConversation(item->getTitle());
}

static void
add_conversation(G_GNUC_UNUSED GtkWidget *menu, gint* row)
{
    auto item = qobject_cast<NewConversationItem*>(SmartListModel::instance().getItem(*row).get());
    g_return_if_fail(item);
    item->sendInvitation();
}

/**
 * Update the menu when the selected item in the treeview changes.
 */
static void
update(GtkTreeSelection *selection, ItemPopupMenu *self)
{
    /* clear the current menu */
    gtk_container_forall(GTK_CONTAINER(self), (GtkCallback)gtk_widget_destroy, nullptr);

    // Retrieve item
    GtkTreeIter iter;
    GtkTreeModel *model;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;
    auto path = gtk_tree_model_get_path(model, &iter);
    auto idx = gtk_tree_path_get_indices(path);
    auto item = SmartListModel::instance().getItem(idx[0]);
    auto temporaryItem = std::dynamic_pointer_cast<NewConversationItem>(item); //TODO wait for enum LRC side to get type

    /* we always build a menu, however in some cases some or all of the items will be deactivated;
     * we prefer this to having an empty menu because GTK+ behaves weird in the empty menu case
     */
    if (temporaryItem) {
        // If we can add this conversation
        if (temporaryItem->getContact().uri.length() > 0) {
            auto add_conversation_item = gtk_menu_item_new_with_mnemonic(_("_Add conversation"));
            gtk_menu_shell_append(GTK_MENU_SHELL(self), add_conversation_item);
            g_signal_connect(add_conversation_item, "activate", G_CALLBACK(add_conversation), idx);
        }
    } else {
        auto rm_history_item = gtk_menu_item_new_with_mnemonic(_("_Clear history"));
        gtk_menu_shell_append(GTK_MENU_SHELL(self), rm_history_item);
        g_signal_connect(rm_history_item, "activate", G_CALLBACK(remove_history_item), idx);
        auto rm_conversation_item = gtk_menu_item_new_with_mnemonic(_("_Remove conversation"));
        gtk_menu_shell_append(GTK_MENU_SHELL(self), rm_conversation_item);
        g_signal_connect(rm_conversation_item, "activate", G_CALLBACK(remove_conversation), idx);
    }


    /* show all items */
    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
item_popup_menu_dispose(GObject *object)
{
    G_OBJECT_CLASS(item_popup_menu_parent_class)->dispose(object);
}

static void
item_popup_menu_finalize(GObject *object)
{
    G_OBJECT_CLASS(item_popup_menu_parent_class)->finalize(object);
}

static void
item_popup_menu_class_init(ItemPopupMenuClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = item_popup_menu_finalize;
    G_OBJECT_CLASS(klass)->dispose = item_popup_menu_dispose;
}


static void
item_popup_menu_init(G_GNUC_UNUSED ItemPopupMenu *self)
{
    // nothing to do
}

GtkWidget *
item_popup_menu_new(GtkTreeView *treeview)
{
    gpointer self = g_object_new(ITEM_POPUP_MENU_TYPE, NULL);
    ItemPopupMenuPrivate *priv = ITEM_POPUP_MENU_GET_PRIVATE(self);

    priv->treeview = treeview;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(priv->treeview);
    g_signal_connect(selection, "changed", G_CALLBACK(update), self);

    // build the menu for the first time
    update(selection, ITEM_POPUP_MENU(self));

    return (GtkWidget *)self;
}

gboolean
item_popup_menu_show(ItemPopupMenu *self, GdkEventButton *event)
{
    /* check for right click */
    if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY ) {
        /* the menu will automatically get updated when the selection changes */
        gtk_menu_popup(GTK_MENU(self), NULL, NULL, NULL, NULL, event->button, event->time);
    }

    return GDK_EVENT_PROPAGATE; /* so that the item selection changes */
}
