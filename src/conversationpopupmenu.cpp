/****************************************************************************
 *    Copyright (C) 2017-2020 Savoir-faire Linux Inc.                             *
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
#include <api/contact.h>
#include <api/contactmodel.h>
#include <api/conversationmodel.h>

#include "accountinfopointer.h"
#include "profileview.h"

// Qt
#include <QItemSelectionModel>

namespace { namespace details {
class CppImpl;
}}

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

    AccountInfoPointer const *accountInfo_;
    details::CppImpl* cpp {nullptr};
};

G_DEFINE_TYPE_WITH_PRIVATE(ConversationPopupMenu, conversation_popup_menu, GTK_TYPE_MENU);

#define CONVERSATION_POPUP_MENU_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONVERSATION_POPUP_MENU_TYPE, ConversationPopupMenuPrivate))

namespace { namespace details {
class CppImpl
{
public:
    explicit CppImpl();
    QString uid_;
};
CppImpl::CppImpl()
    : uid_("")
{}
}}

static void
show_profile(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    if (!priv or !priv->cpp) return;
    auto *profile = profile_view_new(*priv->accountInfo_, priv->cpp->uid_);
    if (profile) gtk_widget_show_all(GTK_WIDGET(profile));
}

static void
remove_history_conversation(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    if (!priv->cpp) return;
    auto convOpt = (*priv->accountInfo_)->conversationModel->getConversationForUid(priv->cpp->uid_);
    if (!convOpt) {
        g_warning("Can't get conversation %s", priv->cpp? priv->cpp->uid_.toStdString().c_str() : "");
        return;
    }

    (*priv->accountInfo_)->conversationModel->clearHistory(convOpt->get().uid);
}

static void
remove_conversation(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    try
    {
        if (!priv->cpp) return;
        (*priv->accountInfo_)->conversationModel->removeConversation(priv->cpp->uid_);
    }
    catch (...)
    {
        g_warning("Can't get conversation %s", priv->cpp? priv->cpp->uid_.toStdString().c_str() : "");
    }
}

static void
unblock_conversation(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    if (!priv->cpp) return;
    auto convOpt = (*priv->accountInfo_)->conversationModel->getConversationForUid(priv->cpp->uid_);
    if (!convOpt) {
        g_warning("Can't get conversation %s", priv->cpp? priv->cpp->uid_.toStdString().c_str() : "");
        return;
    }
    auto uri = convOpt->get().participants[0];

    auto contactInfo = (*priv->accountInfo_)->contactModel->getContact(uri);

    if (!contactInfo.isBanned) {
        g_debug("unblock_conversation: trying to unban a contact which isn't banned !");
        return;
    }

    (*priv->accountInfo_)->contactModel->addContact(contactInfo);
}

static void
block_conversation(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    try
    {
        if (!priv->cpp) return;
        (*priv->accountInfo_)->conversationModel->removeConversation(priv->cpp->uid_, true);
    }
    catch (...)
    {
        g_warning("Can't get conversation %s", priv->cpp? priv->cpp->uid_.toStdString().c_str() : "");
    }
}

static void
add_conversation(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    try
    {
        if (!priv->cpp) return;
        (*priv->accountInfo_)->conversationModel->makePermanent(priv->cpp->uid_);
    }
    catch (...)
    {
        g_warning("Can't get conversation %s", priv->cpp? priv->cpp->uid_.toStdString().c_str() : "");
    }
}

static void
place_video_call(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    if (!priv->cpp) return;
    auto convOpt = (*priv->accountInfo_)->conversationModel->getConversationForUid(priv->cpp->uid_);
    if (!convOpt) {
        g_warning("Can't get conversation %s", priv->cpp? priv->cpp->uid_.toStdString().c_str() : "");
    }

    (*priv->accountInfo_)->conversationModel->placeCall(convOpt->get().uid);
}

static void
place_audio_call(G_GNUC_UNUSED GtkWidget *menu, ConversationPopupMenuPrivate* priv)
{
    if (!priv->cpp) return;
    auto convOpt = (*priv->accountInfo_)->conversationModel->getConversationForUid(priv->cpp->uid_);
    if (!convOpt) {
        g_warning("Can't get conversation %s", priv->cpp? priv->cpp->uid_.toStdString().c_str() : "");
        return;
    }
    (*priv->accountInfo_)->conversationModel->placeAudioOnlyCall(convOpt->get().uid);
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

    gchar *uid;
    gtk_tree_model_get (model, &iter,
                        0 /* col# */, &uid /* data */,
                        -1);

    auto convOpt = (*priv->accountInfo_)->conversationModel->getConversationForUid(uid);
    if (priv->cpp) priv->cpp->uid_ = uid;
    try {
        if (!convOpt || convOpt->get().participants.isEmpty()) {
            g_free(uid);
            return;
        }
        auto contactInfo = (*priv->accountInfo_)->contactModel->getContact(convOpt->get().participants.front());
        if (contactInfo.profileInfo.uri.isEmpty()) {
            g_free(uid);
            return;
        }

        // we always build a menu, however in some cases some or all of the conversations will be deactivated
        // we prefer this to having an empty menu because GTK+ behaves weird in the empty menu case
        auto callId = convOpt->get().confId.isEmpty() ? convOpt->get().callId : convOpt->get().confId;

        // Not in call
        if (!contactInfo.isBanned && (*priv->accountInfo_)->enabled) {
            auto place_video_call_conversation = gtk_menu_item_new_with_mnemonic(_("Place _video call"));
            gtk_menu_shell_append(GTK_MENU_SHELL(self), place_video_call_conversation);
            g_signal_connect(place_video_call_conversation, "activate", G_CALLBACK(place_video_call), priv);
            auto place_audio_call_conversation = gtk_menu_item_new_with_mnemonic(_("Place _audio call"));
            gtk_menu_shell_append(GTK_MENU_SHELL(self), place_audio_call_conversation);
            g_signal_connect(place_audio_call_conversation, "activate", G_CALLBACK(place_audio_call), priv);
        }

        if (contactInfo.profileInfo.type == lrc::api::profile::Type::TEMPORARY ||
            contactInfo.profileInfo.type == lrc::api::profile::Type::PENDING) {
            // If we can add this conversation
            auto add_conversation_conversation = gtk_menu_item_new_with_mnemonic(_("_Add to conversations"));
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
            auto rm_history_conversation = gtk_menu_item_new_with_mnemonic(_("C_lear history"));
            gtk_menu_shell_append(GTK_MENU_SHELL(self), rm_history_conversation);
            g_signal_connect(rm_history_conversation, "activate", G_CALLBACK(remove_history_conversation), priv);
            auto rm_conversation_item = gtk_menu_item_new_with_mnemonic(_("_Remove conversation"));
            gtk_menu_shell_append(GTK_MENU_SHELL(self), rm_conversation_item);
            g_signal_connect(rm_conversation_item, "activate", G_CALLBACK(remove_conversation), priv);

            if (contactInfo.profileInfo.type != lrc::api::profile::Type::SIP) {
                if (!contactInfo.isBanned) {
                    auto block_conversation_item = gtk_menu_item_new_with_mnemonic(_("_Block contact"));
                    gtk_menu_shell_append(GTK_MENU_SHELL(self), block_conversation_item);
                    g_signal_connect(block_conversation_item, "activate", G_CALLBACK(block_conversation), priv);
                } else {
                    auto block_conversation_item = gtk_menu_item_new_with_mnemonic(_("_Unblock contact"));
                    gtk_menu_shell_append(GTK_MENU_SHELL(self), block_conversation_item);
                    g_signal_connect(block_conversation_item, "activate", G_CALLBACK(unblock_conversation), priv);
                }
            }
        }

        auto profile = gtk_menu_item_new_with_mnemonic(_("_Profile"));
        gtk_menu_shell_append(GTK_MENU_SHELL(self), profile);
        g_signal_connect(profile, "activate", G_CALLBACK(show_profile), priv);

        /* show all conversations */
        gtk_widget_show_all(GTK_WIDGET(self));
    } catch (const std::out_of_range&) {
        // ContactModel::getContact() exception
    }

    g_free(uid);
}

static void
conversation_popup_menu_dispose(GObject *object)
{
    auto* priv = CONVERSATION_POPUP_MENU_GET_PRIVATE(object);
    delete priv->cpp;
    priv->cpp = nullptr;
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
conversation_popup_menu_new (GtkTreeView *treeview, AccountInfoPointer const & accountInfo)
{
    gpointer self = g_object_new(CONVERSATION_POPUP_MENU_TYPE, NULL);
    ConversationPopupMenuPrivate *priv = CONVERSATION_POPUP_MENU_GET_PRIVATE(self);
    priv->accountInfo_ = &accountInfo;

    priv->treeview = treeview;
    priv->cpp = new details::CppImpl();
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
