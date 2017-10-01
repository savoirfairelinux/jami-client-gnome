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

// std
#include <algorithm>

// GTK+ related
#include <QSize>

// LRC
#include <globalinstances.h>
#include <api/conversationmodel.h>
#include <api/contactmodel.h>
#include <api/call.h>
#include <api/contact.h>
#include <api/newcallmodel.h>

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

    GtkWidget* popupMenu_;

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
    // Get active conversation
    auto path = gtk_tree_model_get_path(model, iter);
    auto row = std::atoi(gtk_tree_path_to_string(path));
    if (row == -1) return;
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);
    if (!priv) return;
    try
    {
        // Draw first contact.
        // TODO handle conferences?
        auto conversation = priv->accountContainer_->info.conversationModel->getConversation(row);
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
render_name_and_last_interaction(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                                 GtkCellRenderer *cell,
                                 GtkTreeModel *model,
                                 GtkTreeIter *iter,
                                 G_GNUC_UNUSED GtkTreeView *treeview)
{
    gchar *alias;
    gchar *registeredName;
    gchar *lastInteraction;
    gchar *text;
    gchar *uid;

    gtk_tree_model_get (model, iter,
                        0 /* col# */, &uid /* data */,
                        1 /* col# */, &alias /* data */,
                        2 /* col# */, &registeredName /* data */,
                        4 /* col# */, &lastInteraction /* data */,
                        -1);

    // Limit the size of lastInteraction to 20 chars and add …
    const auto maxSize = 20;
    const auto size = g_utf8_strlen (lastInteraction, maxSize + 1);
    if (size > maxSize) {
        g_utf8_strncpy (lastInteraction, lastInteraction, 20);
        lastInteraction = g_markup_printf_escaped("%s…", lastInteraction);
    }

    if (std::string(alias).empty()) {
        text = g_markup_printf_escaped(
            "<span font_weight=\"bold\">%s</span>\n<span size=\"smaller\" color=\"#666\">%s</span>",
            registeredName,
            lastInteraction
        );
    } else if (std::string(alias) == std::string(registeredName)
        || std::string(registeredName).empty() || std::string(uid).empty()) {
        text = g_markup_printf_escaped(
            "<span font_weight=\"bold\">%s</span>\n<span size=\"smaller\" color=\"#666\">%s</span>",
            alias,
            lastInteraction
        );
    } else {
        text = g_markup_printf_escaped(
            "<span font_weight=\"bold\">%s</span>\n<span size=\"smaller\" color=\"#666\">%s</span>\n<span size=\"smaller\" color=\"#666\">%s</span>",
            alias,
            registeredName,
            lastInteraction
        );
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(uid);
    g_free(alias);
    g_free(registeredName);
}

static void
render_time(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
            GtkCellRenderer *cell,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED GtkTreeView *treeview)
{

    // Get active conversation
    auto path = gtk_tree_model_get_path(model, iter);
    auto row = std::atoi(gtk_tree_path_to_string(path));
    if (row == -1) return;
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(treeview);
    if (!priv) return;

    gchar *text;

    try
    {
        auto conversation = priv->accountContainer_->info.conversationModel->getConversation(row);
        auto callId = conversation.callId;
        if (!callId.empty()) {
            auto call = priv->accountContainer_->info.callModel->getCall(callId);
            text = g_markup_printf_escaped("%s",
                lrc::api::call::to_string(call.status).c_str()
            );
        } else if (conversation.interactions.empty()) {
            text = "";
        } else {
            auto lastUid = conversation.lastMessageUid;
            if (conversation.interactions.find(lastUid) == conversation.interactions.end()) {
                text = "";
            } else {
                std::time_t lastInteractionTimestamp = conversation.interactions[lastUid].timestamp;
                std::time_t now = std::time(nullptr);
                char interactionDay[100];
                char nowDay[100];
                std::strftime(interactionDay, sizeof(interactionDay), "%D", std::localtime(&lastInteractionTimestamp));
                std::strftime(nowDay, sizeof(nowDay), "%D", std::localtime(&now));


                if (std::string(interactionDay) == std::string(nowDay)) {
                    char interactionTime[100];
                    std::strftime(interactionTime, sizeof(interactionTime), "%R", std::localtime(&lastInteractionTimestamp));
                    text = g_markup_printf_escaped("<span size=\"smaller\" color=\"#666\">%s</span>",
                        &interactionTime
                    );
                } else {
                    text = g_markup_printf_escaped("<span size=\"smaller\" color=\"#666\">%s</span>",
                        &interactionDay
                    );
                }
            }
        }
    }
    catch (const std::exception&)
    {
        g_warning("Can't get conversation at row %i", row);
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
}

static GtkTreeModel*
create_and_fill_model(ConversationsView *self)
{
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);
    auto store = gtk_list_store_new (5 /* # of cols */ ,
                                     G_TYPE_STRING,
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
        auto lastMessage = conversation.interactions.empty() ? "" :
            conversation.interactions.at(conversation.lastMessageUid).body;
        std::replace(lastMessage.begin(), lastMessage.end(), '\n', ' ');
        gtk_list_store_append (store, &iter);
        auto alias = contactInfo.profileInfo.alias;
        alias.erase(std::remove(alias.begin(), alias.end(), '\r'), alias.end());
        gtk_list_store_set (store, &iter,
            0 /* col # */ , conversation.uid.c_str() /* celldata */,
            1 /* col # */ , alias.c_str() /* celldata */,
            2 /* col # */ , contactInfo.registeredName.c_str() /* celldata */,
            3 /* col # */ , contactInfo.profileInfo.avatar.c_str() /* celldata */,
            4 /* col # */ , lastMessage.c_str() /* celldata */,
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
    // Update popupMenu_
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);
    if (priv->popupMenu_) {
        // Because popup menu is not up to date, we need to update it.
        auto isVisible = gtk_widget_get_visible(priv->popupMenu_);
        // Destroy the not up to date menu.
        gtk_widget_hide(priv->popupMenu_);
        gtk_widget_destroy(priv->popupMenu_);
        priv->popupMenu_ = conversation_popup_menu_new(GTK_TREE_VIEW(self), priv->accountContainer_);
        auto children = gtk_container_get_children (GTK_CONTAINER(priv->popupMenu_));
        auto nbItems = g_list_length(children);
        // Show the new popupMenu_ should be visible
        if (isVisible && nbItems > 0)
            gtk_menu_popup_at_pointer(GTK_MENU(priv->popupMenu_), nullptr);
    }
    GtkTreeIter iter;
    GtkTreeModel *model = nullptr;
    gchar *conversationUid = nullptr;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) return;

    gtk_tree_model_get(model, &iter,
                       0, &conversationUid,
                       -1);
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
    auto children = gtk_container_get_children (GTK_CONTAINER(priv->popupMenu_));
    auto nbItems = g_list_length(children);
    // Show the new popupMenu_ should be visible
    if (nbItems > 0)
        conversation_popup_menu_show(CONVERSATION_POPUP_MENU(priv->popupMenu_), event);
}

static void
on_drag_data_get(GtkWidget        *treeview,
                 G_GNUC_UNUSED GdkDragContext *context,
                 GtkSelectionData *data,
                 G_GNUC_UNUSED guint info,
                 G_GNUC_UNUSED guint time,
                 G_GNUC_UNUSED gpointer user_data)
{
    g_return_if_fail(IS_CONVERSATIONS_VIEW(treeview));

    /* we always drag the selected row */
    auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        auto path_str = gtk_tree_model_get_string_from_iter(model, &iter);

        gtk_selection_data_set(data,
                               gdk_atom_intern_static_string(CALL_TARGET),
                               8, /* bytes */
                               (guchar *)path_str,
                               strlen(path_str) + 1);

        g_free(path_str);
    } else {
        g_warning("drag selection not valid");
    }
}

static gboolean
on_drag_drop(GtkWidget      *treeview,
             GdkDragContext *context,
             gint            x,
             gint            y,
             guint           time,
             G_GNUC_UNUSED gpointer user_data)
{
    g_return_val_if_fail(IS_CONVERSATIONS_VIEW(treeview), FALSE);

    GtkTreePath *path = NULL;
    GtkTreeViewDropPosition drop_pos;

    if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(treeview),
                                          x, y, &path, &drop_pos)) {

        GdkAtom target_type = gtk_drag_dest_find_target(treeview, context, NULL);

        if (target_type != GDK_NONE) {
            g_debug("can drop");
            gtk_drag_get_data(treeview, context, target_type, time);
            return TRUE;
        }

        gtk_tree_path_free(path);
    }

    return FALSE;
}

static gboolean
on_drag_motion(GtkWidget      *treeview,
               GdkDragContext *context,
               gint            x,
               gint            y,
               guint           time,
               G_GNUC_UNUSED gpointer user_data)
{
    g_return_val_if_fail(IS_CONVERSATIONS_VIEW(treeview), FALSE);

    GtkTreePath *path = NULL;
    GtkTreeViewDropPosition drop_pos;

    if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(treeview),
                                          x, y, &path, &drop_pos)) {
        // we only want to drop on a row, not before or after
        if (drop_pos == GTK_TREE_VIEW_DROP_BEFORE) {
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(treeview), path, GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
        } else if (drop_pos == GTK_TREE_VIEW_DROP_AFTER) {
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(treeview), path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
        }
        gdk_drag_status(context, gdk_drag_context_get_suggested_action(context), time);
        return TRUE;
    } else {
        // not a row in the treeview, so we cannot drop
        return FALSE;
    }
}

static void
on_drag_data_received(GtkWidget        *treeview,
                      GdkDragContext   *context,
                      gint              x,
                      gint              y,
                      GtkSelectionData *data,
                      G_GNUC_UNUSED guint info,
                      guint             time,
                      G_GNUC_UNUSED gpointer user_data)
{
    g_return_if_fail(IS_CONVERSATIONS_VIEW(treeview));
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(treeview);

    gboolean success = FALSE;

    /* get the source and destination calls */
    auto path_str_source = (gchar *)gtk_selection_data_get_data(data);
    auto type = gtk_selection_data_get_data_type(data);
    g_debug("data type: %s", gdk_atom_name(type));
    if (path_str_source && strlen(path_str_source) > 0) {
        g_debug("source path: %s", path_str_source);

        /* get the destination path */
        GtkTreePath *dest_path = NULL;
        if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(treeview), x, y, &dest_path, NULL)) {
            auto model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

            GtkTreeIter source, dest;
            gtk_tree_model_get_iter_from_string(model, &source, path_str_source);
            gtk_tree_model_get_iter(model, &dest, dest_path);

            gchar *conversationUidSrc = nullptr;
            gchar *conversationUidDest = nullptr;

            gtk_tree_model_get(model, &source,
                               0, &conversationUidSrc,
                               -1);
            gtk_tree_model_get(model, &dest,
                               0, &conversationUidDest,
                               -1);

            priv->accountContainer_->info.conversationModel->addParticipant(
                conversationUidSrc,
                conversationUidDest
            );

            gtk_tree_path_free(dest_path);
        }
    }

    gtk_drag_finish(context, success, FALSE, time);
}

static void
build_conversations_view(ConversationsView *self)
{
    auto priv = CONVERSATIONS_VIEW_GET_PRIVATE(self);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);

    auto model = create_and_fill_model(self);
    gtk_tree_view_set_model(GTK_TREE_VIEW(self),
                            GTK_TREE_MODEL(model));

    // ringId method column
    auto area = gtk_cell_area_box_new();
    auto column = gtk_tree_view_column_new_with_area(area);

    // render the photo
    auto renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_contact_photo,
        self,
        NULL);

    // render name and last interaction
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_name_and_last_interaction,
        self,
        NULL);

    // render time of last interaction and number of unread
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_object_set(G_OBJECT(renderer), "xalign", 1.0, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_time,
        self,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self), column);

    // This view should be synchronized and redraw at each update.
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
    // One left click to select the conversation
    g_signal_connect(selectionNew, "changed", G_CALLBACK(select_conversation), self);
    // Two clicks to placeCall
    g_signal_connect(self, "row-activated", G_CALLBACK(call_conversation), NULL);

    priv->popupMenu_ = conversation_popup_menu_new(GTK_TREE_VIEW(self), priv->accountContainer_);
    // Right clic to show actions
    g_signal_connect_swapped(self, "button-press-event", G_CALLBACK(show_popup_menu), self);

    /* drag and drop */
    static GtkTargetEntry targetentries[] = {
        { (gchar *)CALL_TARGET, GTK_TARGET_SAME_WIDGET, CALL_TARGET_ID },
    };

    gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(self),
        GDK_BUTTON1_MASK, targetentries, 1, (GdkDragAction)(GDK_ACTION_DEFAULT | GDK_ACTION_MOVE));

    gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(self),
        targetentries, 1, GDK_ACTION_DEFAULT);

    g_signal_connect(self, "drag-data-get", G_CALLBACK(on_drag_data_get), nullptr);
    g_signal_connect(self, "drag-drop", G_CALLBACK(on_drag_drop), nullptr);
    g_signal_connect(self, "drag-motion", G_CALLBACK(on_drag_motion), nullptr);
    g_signal_connect(self, "drag_data_received", G_CALLBACK(on_drag_data_received), nullptr);
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

    gtk_widget_destroy(priv->popupMenu_);

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

/**
 * Select a conversation by uid (used to synchronize the selection)
 * @param self
 * @param uid of the conversation
 */
void
conversations_view_select_conversation(ConversationsView *self, const std::string& uid)
{
    auto idx = 0;
    auto model = gtk_tree_view_get_model (GTK_TREE_VIEW(self));
    auto iterIsCorrect = true;
    GtkTreeIter iter;

    while(iterIsCorrect) {
        iterIsCorrect = gtk_tree_model_iter_nth_child (model, &iter, nullptr, idx);
        if (!iterIsCorrect)
            break;
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
