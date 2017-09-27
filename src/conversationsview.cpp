/****************************************************************************
 *   Copyright (C) 2017 Savoir-faire Linux                                  *
 *   Author : Nicolas Jäger <nicolas.jager@savoirfairelinux.com>            *
 *   Author : Sébastien Blin <sebastien.blin@savoirfairelinux.com>          *
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

#include "conversationsview.h"

// GTK+ related
#include <QSize>

// LRC
#include <globalinstances.h>
#include <api/conversationmodel.h>
#include <api/contactmodel.h>
#include <api/contact.h>

// Gnome client
#include "native/pixbufmanipulator.h"
#include "conversationpopupmenu.h"


static constexpr const char* CALL_TARGET    = "CALL_TARGET";
static constexpr int         CALL_TARGET_ID = 0;

struct _ConversationsView
{
    GtkTreeView parent;
};

struct _ConversationsViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _ConversationsViewPrivate ConversationsViewPrivate;

struct _ConversationsViewPrivate
{
    AccountContainer* accountContainer_;

    QMetaObject::Connection selection_updated;
    QMetaObject::Connection layout_changed;
    QMetaObject::Connection modelSortedConnection_;
    QMetaObject::Connection filterChangedConnection_;

};

G_DEFINE_TYPE_WITH_PRIVATE(ConversationsView, conversations_view, GTK_TYPE_TREE_VIEW);

#define CONVERSATIONS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONVERSATIONS_VIEW_TYPE, ConversationsViewPrivate))

static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     gpointer self)
{
    auto path = gtk_tree_model_get_path(model, iter);
    auto row = std::atoi(gtk_tree_path_to_string(path));
    if (row == -1) return;
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);
    if (!priv) return;
    try
    {
        auto conversation = priv->accountContainer_->info.conversationModel->getConversation(row);
        if (conversation.uid.empty())
            return;
        auto contact = priv->accountContainer_->info.contactModel->getContact(conversation.participants.front());
        std::shared_ptr<GdkPixbuf> image;
        auto var_photo = GlobalInstances::pixmapManipulator().conversationPhoto(
            conversation,
            priv->accountContainer_->info,
            QSize(50, 50),
            contact.isPresent
        );
        image = var_photo.value<std::shared_ptr<GdkPixbuf>>();

        // set the width of the cell rendered to the width of the photo
        // so that the other renderers are shifted to the right
        g_object_set(G_OBJECT(cell), "width", 50, NULL);
        g_object_set(G_OBJECT(cell), "pixbuf", image.get(), NULL);
    }
    catch (const std::exception&)
    {
        g_warning("Can't get conversation at row %i", row);
    }
}

static void
render_name_and_number(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *model,
                       GtkTreeIter *iter,
                       G_GNUC_UNUSED GtkTreeView *treeview)
{
    gchar *ringId;
    gchar *lastInformation;
    gchar *text;

    gtk_tree_model_get (model, iter,
                        1 /* col# */, &ringId /* data */,
                        3 /* col# */, &lastInformation /* data */,
                        -1);

    // Limit the size of lastInformation
    const auto maxSize = 20;
    const auto size = g_utf8_strlen (lastInformation, maxSize + 1);
    if (size > maxSize) {
        g_utf8_strncpy (lastInformation, lastInformation, 20);
        lastInformation = g_markup_printf_escaped("%s…", lastInformation);
    }

    text = g_markup_printf_escaped(
        "<span font_weight=\"bold\">%s</span>\n<span size=\"smaller\" color=\"#666\">%s</span>",
        ringId,
        lastInformation
    );

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(ringId);
}

static GtkTreeModel*
create_and_fill_model(ConversationsView *self)
{
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);
    auto store = gtk_list_store_new (4 /* # of cols */ ,
                                     G_TYPE_STRING,
                                     G_TYPE_STRING,
                                     G_TYPE_STRING,
                                     G_TYPE_STRING,
                                     G_TYPE_UINT);
    if(!priv) GTK_TREE_MODEL (store);
    GtkTreeIter iter;

    for (auto conversation : priv->accountContainer_->info.conversationModel->getFilteredConversations()) {
        if (conversation.participants.empty()) break; // Should not
        auto contactUri = conversation.participants.front();
        auto contactInfo = priv->accountContainer_->info.contactModel->getContact(contactUri);
        auto lastMessage = conversation.messages.empty() ? "" :
            conversation.messages.at(conversation.lastMessageUid).body;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
            0 /* col # */ , conversation.uid.c_str() /* celldata */,
            1 /* col # */ , contactInfo.profileInfo.alias.c_str() /* celldata */,
            2 /* col # */ , contactInfo.profileInfo.avatar.c_str() /* celldata */,
            3 /* col # */ , lastMessage.c_str() /* celldata */,
            -1 /* end */);
    }

    return GTK_TREE_MODEL (store);
}

static void
call_conversation(GtkTreeView *self,
                  GtkTreePath *path,
                  G_GNUC_UNUSED GtkTreeViewColumn *column,
                  G_GNUC_UNUSED gpointer user_data)
{
    auto row = std::atoi(gtk_tree_path_to_string(path));
    if (row == -1) return;
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);
    if (!priv) return;
    auto conversation = priv->accountContainer_->info.conversationModel->getConversation(row);
    priv->accountContainer_->info.conversationModel->placeCall(conversation.uid);
}

static void
select_conversation(GtkTreeSelection *selection, ConversationsView *self)
{
    GtkTreeIter iter;
    GtkTreeModel *model = nullptr;
    gchar *conversationUid = nullptr;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) return;

    gtk_tree_model_get(model, &iter,
                       0, &conversationUid,
                       -1);
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);
    priv->accountContainer_->info.conversationModel->selectConversation(std::string(conversationUid));
}

static void
conversations_view_init(ConversationsView *self)
{
    // Nothing to do
}

static void
show_popup_menu(ConversationsView *self, GdkEventButton *event)
{
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);
    auto popup_menu = conversation_popup_menu_new(GTK_TREE_VIEW(self), priv->accountContainer_);
    conversation_popup_menu_show(CONVERSATION_POPUP_MENU(popup_menu), event);
}

static void
build_conversations_view(ConversationsView *self)
{
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);

    auto model = create_and_fill_model(self);
    gtk_tree_view_set_model(GTK_TREE_VIEW(self),
                            GTK_TREE_MODEL(model));

    /* ringId method column */
    auto area = gtk_cell_area_box_new();
    auto column = gtk_tree_view_column_new_with_area(area);

    auto renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    /* get the photo */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_contact_photo,
        self,
        NULL);

    /* renderer */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_name_and_number,
        self,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self), column);

    priv->modelSortedConnection_ = QObject::connect(
    &*priv->accountContainer_->info.conversationModel,
    &lrc::api::ConversationModel::modelSorted,
    [self] () {
        auto model = create_and_fill_model(self);

        gtk_tree_view_set_model(GTK_TREE_VIEW(self),
                                GTK_TREE_MODEL(model));
    });

    priv->filterChangedConnection_ = QObject::connect(
    &*priv->accountContainer_->info.conversationModel,
    &lrc::api::ConversationModel::filterChanged,
    [self] () {
        auto model = create_and_fill_model(self);

        gtk_tree_view_set_model(GTK_TREE_VIEW(self),
                                GTK_TREE_MODEL(model));
    });

    gtk_widget_show_all(GTK_WIDGET(self));

    auto selectionNew = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
    g_signal_connect(selectionNew, "changed", G_CALLBACK(select_conversation), self);
    g_signal_connect(self, "row-activated", G_CALLBACK(call_conversation), NULL);

    g_signal_connect_swapped(self, "button-press-event", G_CALLBACK(show_popup_menu), self);
}

static void
conversations_view_dispose(GObject *object)
{
    auto self = CONVERSATIONS_VIEW(object);
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);

    QObject::disconnect(priv->selection_updated);
    QObject::disconnect(priv->layout_changed);
    QObject::disconnect(priv->modelSortedConnection_);
    QObject::disconnect(priv->filterChangedConnection_);

    G_OBJECT_CLASS(conversations_view_parent_class)->dispose(object);
}

static void
conversations_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(conversations_view_parent_class)->finalize(object);
}

static void
conversations_view_class_init(ConversationsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = conversations_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = conversations_view_dispose;
}

GtkWidget *
conversations_view_new(AccountContainer* accountContainer)
{
    auto self = CONVERSATIONS_VIEW(g_object_new(CONVERSATIONS_VIEW_TYPE, NULL));
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);

    priv->accountContainer_ = accountContainer;

    build_conversations_view(self);

    return (GtkWidget *)self;
}

void
conversations_view_select_conversation(ConversationsView *self, const std::string& uid)
{
    auto idx = 0;
    auto model = gtk_tree_view_get_model (GTK_TREE_VIEW(self));
    auto iterIsCorrect = true;
    GtkTreeIter iter;

    while(iterIsCorrect) {
        iterIsCorrect = gtk_tree_model_iter_nth_child (model, &iter, nullptr, idx);
        if (!iterIsCorrect) {
            break;
        }
        gchar *ringId;
        gtk_tree_model_get (model, &iter,
                            0 /* col# */, &ringId /* data */,
                            -1);
        if(std::string(ringId) == uid) {
            auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
            gtk_tree_selection_select_iter(selection, &iter);
            g_free(ringId);
            return;
        }
        g_free(ringId);
        idx++;
    }
}
