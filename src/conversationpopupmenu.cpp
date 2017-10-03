/****************************************************************************
 *   Copyright (C) 2017 Savoir-faire Linux                                  *
 *   Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>             *
 *   Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>           *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "conversationpopupmenu.h"

// GTK+ related
#include <glib/gi18n.h>

// Lrc
#include <api/conversationmodel.h>
#include <api/contactmodel.h>
#include <api/contact.h>

struct _ConversationPopupMenu
{
    GtkMenu parent;
};

struct _ConversationPopupMenuClass
{
    GtkMenuClass parent_class;
};

typedef struct _ConversationPopupMenuPrivate ConversationPopupMenuPrivate;

struct _ConversationPopupMenuPrivate
{
    GtkTreeView *treeview;

    AccountContainer* accountContainer_;
    int row_;
};

G_DEFINE_TYPE_WITH_PRIVATE(ConversationPopupMenu, conversation_popup_menu, GTK_TYPE_MENU);

#define CONVERSATION_POPUP_MENU_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONVERSATION_POPUP_MENU_TYPE, ConversationPopupMenuPrivate))

static void
remove_history_conversation(GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    try
    {
        auto conversation = priv->accountContainer_->info.conversationModel->filteredConversation(priv->row_);
        priv->accountContainer_->info.conversationModel->clearHistory(conversation.uid);
    }
    catch (...)
    {
        g_warning("Can't get conversation at row %i", priv->row_);
    }
}

static void
remove_conversation(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    try
    {
        auto conversationUid = priv->accountContainer_->info.conversationModel->filteredConversation(priv->row_).uid;
        priv->accountContainer_->info.conversationModel->removeConversation(conversationUid);
    }
    catch (...)
    {
        g_warning("Can't get conversation at row %i", priv->row_);
    }
}

static void
block_conversation(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    try
    {
        auto conversationUid = priv->accountContainer_->info.conversationModel->filteredConversation(priv->row_).uid;
        priv->accountContainer_->info.conversationModel->removeConversation(conversationUid, true);
    }
    catch (...)
    {
        g_warning("Can't get conversation at row %i", priv->row_);
    }
}

static void
add_conversation(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    try
    {
        auto conversation = priv->accountContainer_->info.conversationModel->filteredConversation(priv->row_);
        priv->accountContainer_->info.conversationModel->makePermanent(conversation.uid);
    }
    catch (...)
    {
        g_warning("Can't get conversation at row %i", priv->row_);
    }
}

static void
place_call(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    try
    {
        auto conversation = priv->accountContainer_->info.conversationModel->filteredConversation(priv->row_);
        priv->accountContainer_->info.conversationModel->placeCall(conversation.uid);
    }
    catch (...)
    {
        g_warning("Can't get conversation at row %i", priv->row_);
    }
}

/**
 * Update the menu when the selected conversation in the treeview changes.
 */
static void
update(GtkTreeSelection *selection, ConversationPopupMenu *self)
{
    ConversationPopupMenuPrivate *priv = CONVERSATION_POPUP_MENU_GET_PRIVATE(self);
    /* clear the current menu */
    gtk_container_forall(GTK_CONTAINER(self), (GtkCallback)gtk_widget_destroy, nullptr);

    // Retrieve conversation
    GtkTreeIter iter;
    GtkTreeModel *model;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) return;
    auto path = gtk_tree_model_get_path(model, &iter);
    auto idx = gtk_tree_path_get_indices(path);
    auto conversation = priv->accountContainer_->info.conversationModel->filteredConversation(idx[0]);
    priv->row_ = idx[0];
    auto contactInfo = priv->accountContainer_->info.contactModel->getContact(conversation.participants.front());
    if (contactInfo.profileInfo.uri.empty()) return;

    // we always build a menu, however in some cases some or all of the conversations will be deactivated
    // we prefer this to having an empty menu because GTK+ behaves weird in the empty menu case
    auto place_call_conversation = gtk_menu_item_new_with_mnemonic(_("_Place call"));
    gtk_menu_shell_append(GTK_MENU_SHELL(self), place_call_conversation);
    g_signal_connect(place_call_conversation, "activate", G_CALLBACK(place_call), priv);
    if (contactInfo.profileInfo.type == lrc::api::profile::Type::TEMPORARY ||
        contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING) {
        // If we can add this conversation
        auto add_conversation_conversation = gtk_menu_item_new_with_mnemonic(_("_Add conversation"));
        gtk_menu_shell_append(GTK_MENU_SHELL(self), add_conversation_conversation);
        g_signal_connect(add_conversation_conversation, "activate", G_CALLBACK(add_conversation), priv);
        if (contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING) {
            auto rm_conversation_item = gtk_menu_item_new_with_mnemonic(_("_Discard invitation"));
            gtk_menu_shell_append(GTK_MENU_SHELL(self), rm_conversation_item);
            g_signal_connect(rm_conversation_item, "activate", G_CALLBACK(remove_conversation), priv);
            auto block_conversation_item = gtk_menu_item_new_with_mnemonic(_("_Block invitations"));
            gtk_menu_shell_append(GTK_MENU_SHELL(self), block_conversation_item);
            g_signal_connect(block_conversation_item, "activate", G_CALLBACK(block_conversation), priv);
        }
    } else {
        auto rm_history_conversation = gtk_menu_item_new_with_mnemonic(_("_Clear history"));
        gtk_menu_shell_append(GTK_MENU_SHELL(self), rm_history_conversation);
        g_signal_connect(rm_history_conversation, "activate", G_CALLBACK(remove_history_conversation), priv);
        auto rm_conversation_item = gtk_menu_item_new_with_mnemonic(_("_Remove conversation"));
        gtk_menu_shell_append(GTK_MENU_SHELL(self), rm_conversation_item);
        g_signal_connect(rm_conversation_item, "activate", G_CALLBACK(remove_conversation), priv);
        auto block_conversation_item = gtk_menu_item_new_with_mnemonic(_("_Block contact"));
        gtk_menu_shell_append(GTK_MENU_SHELL(self), block_conversation_item);
        g_signal_connect(block_conversation_item, "activate", G_CALLBACK(block_conversation), priv);
    }

    /* show all conversations */
    gtk_widget_show_all(GTK_WIDGET(self));

}

static void
conversation_popup_menu_dispose(GObject *object)
{
    G_OBJECT_CLASS(conversation_popup_menu_parent_class)->dispose(object);
}

static void
conversation_popup_menu_finalize(GObject *object)
{
    G_OBJECT_CLASS(conversation_popup_menu_parent_class)->finalize(object);
}

static void
conversation_popup_menu_class_init(ConversationPopupMenuClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = conversation_popup_menu_finalize;
    G_OBJECT_CLASS(klass)->dispose = conversation_popup_menu_dispose;
}


static void
conversation_popup_menu_init(G_GNUC_UNUSED ConversationPopupMenu *self)
{
    // nothing to do
}

GtkWidget *
conversation_popup_menu_new (GtkTreeView *treeview, AccountContainer* accountContainer)
{
    gpointer self = g_object_new(CONVERSATION_POPUP_MENU_TYPE, NULL);
    ConversationPopupMenuPrivate *priv = CONVERSATION_POPUP_MENU_GET_PRIVATE(self);
    priv->accountContainer_ = accountContainer;

    priv->treeview = treeview;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(priv->treeview);

    // build the menu for the first time
    update(selection, CONVERSATION_POPUP_MENU(self));

    return (GtkWidget *)self;
}

gboolean
conversation_popup_menu_show(ConversationPopupMenu *self, GdkEventButton *event)
{
    if (!self) return GDK_EVENT_PROPAGATE;
    if (event->type == GDK_BUTTON_PRESS
        && event->button == GDK_BUTTON_SECONDARY) {
        // Show popup menu. Will be updated later.
        gtk_menu_popup(GTK_MENU(self), NULL, NULL, NULL, NULL, event->button, event->time);
    }

    return GDK_EVENT_PROPAGATE; // so that the conversation selection changes
}
