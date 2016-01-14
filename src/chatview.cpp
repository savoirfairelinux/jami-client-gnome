/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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
 */

#include "chatview.h"

#include <gtk/gtk.h>
#include <call.h>
#include <callmodel.h>
#include <contactmethod.h>
#include <person.h>
#include <media/media.h>
#include <media/text.h>
#include <media/textrecording.h>
#include "ringnotify.h"

struct _ChatView
{
    GtkBox parent;
};

struct _ChatViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _ChatViewPrivate ChatViewPrivate;

struct _ChatViewPrivate
{
    GtkWidget *textview_chat;
    GtkWidget *button_chat_input;
    GtkWidget *entry_chat_input;
    GtkWidget *scrolledwindow_chat;
    GtkWidget *hbox_chat_info;
    GtkWidget *label_peer;
    GtkWidget *combobox_cm;

    /* only one of the three following pointers should be non void;
     * either this is an in-call chat (and so the in-call chat APIs will be used)
     * or it is an out of call chat (and so the account chat APIs will be used)
     */
    Call          *call;
    Person        *person;
    ContactMethod *cm;

    QMetaObject::Connection new_message_connection;
};

G_DEFINE_TYPE_WITH_PRIVATE(ChatView, chat_view, GTK_TYPE_BOX);

#define CHAT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CHAT_VIEW_TYPE, ChatViewPrivate))

enum {
    NEW_MESSAGES_DISPLAYED,
    LAST_SIGNAL
};

static guint chat_view_signals[LAST_SIGNAL] = { 0 };

static void
chat_view_dispose(GObject *object)
{
    ChatView *view;
    ChatViewPrivate *priv;

    view = CHAT_VIEW(object);
    priv = CHAT_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->new_message_connection);

    G_OBJECT_CLASS(chat_view_parent_class)->dispose(object);
}


static void
send_chat(G_GNUC_UNUSED GtkWidget *widget, ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    /* make sure there is text to send */
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(priv->entry_chat_input));
    if (text && strlen(text) > 0) {
        QMap<QString, QString> messages;
        messages["text/plain"] = text;

        if (priv->call) {
            // in call message
            priv->call->addOutgoingMedia<Media::Text>()->send(messages);
        } else if (priv->person) {
            // get the chosen cm
            auto active = gtk_combo_box_get_active(GTK_COMBO_BOX(priv->combobox_cm));
            if (active >= 0) {
                auto cm = priv->person->phoneNumbers().at(active);
                if (!cm->sendOfflineTextMessage(messages))
                    g_warning("message failed to send"); // TODO: warn the user about this in the UI
            } else {
                g_warning("no ContactMethod chosen; message not esnt");
            }
        } else if (priv->cm) {
            if (!priv->cm->sendOfflineTextMessage(messages))
                g_warning("message failed to send"); // TODO: warn the user about this in the UI
        } else {
            g_warning("no Call, Person, or ContactMethod set; message not sent");
        }

        /* clear the entry */
        gtk_entry_set_text(GTK_ENTRY(priv->entry_chat_input), "");
    }
}

static void
scroll_to_bottom(GtkAdjustment *adjustment, G_GNUC_UNUSED gpointer user_data)
{
    gtk_adjustment_set_value(adjustment,
        gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment));
}

static void
chat_view_init(ChatView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(view);

    g_signal_connect(priv->button_chat_input, "clicked", G_CALLBACK(send_chat), view);
    g_signal_connect(priv->entry_chat_input, "activate", G_CALLBACK(send_chat), view);

    /* the adjustment params will change only when the model is created and when
     * new messages are added; in these cases we want to scroll to the bottom of
     * the chat treeview */
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(priv->scrolledwindow_chat));
    g_signal_connect(adjustment, "changed", G_CALLBACK(scroll_to_bottom), NULL);
}

static void
chat_view_class_init(ChatViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = chat_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/chatview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, textview_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, button_chat_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, entry_chat_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, scrolledwindow_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, hbox_chat_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, label_peer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, combobox_cm);

    chat_view_signals[NEW_MESSAGES_DISPLAYED] = g_signal_new (
        "new-messages-displayed",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

static void
print_message_to_buffer(const QModelIndex &idx, GtkTextBuffer *buffer)
{
    if (idx.isValid()) {
        auto message = idx.data().value<QString>().toUtf8();
        auto sender = idx.data(static_cast<int>(Media::TextRecording::Role::AuthorDisplayname)).value<QString>().toUtf8();

        GtkTextIter iter;

        /* unless its the very first message, insert a new line */
        if (idx.row() != 0) {
            gtk_text_buffer_get_end_iter(buffer, &iter);
            gtk_text_buffer_insert(buffer, &iter, "\n", -1);
        }

        auto format_sender = g_strconcat(sender.constData(), ": ", NULL);
        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
                                                 format_sender, -1,
                                                 "bold", NULL);
        g_free(format_sender);

        /* if the sender name is too long, insert a new line after it */
        if (sender.length() > 20) {
            gtk_text_buffer_get_end_iter(buffer, &iter);
            gtk_text_buffer_insert(buffer, &iter, "\n", -1);
        }

        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert(buffer, &iter, message.constData(), -1);

    } else {
        g_warning("QModelIndex in im model is not valid");
    }
}

static void
parse_chat_model(QAbstractItemModel *model, ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    /* new model, disconnect from the old model updates and clear the text buffer */
    QObject::disconnect(priv->new_message_connection);

    GtkTextBuffer *new_buffer = gtk_text_buffer_new(NULL);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(priv->textview_chat), new_buffer);

    /* add tags to the buffer */
    gtk_text_buffer_create_tag(new_buffer, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);

    g_object_unref(new_buffer);

    /* put all the messages in the im model into the text view */
    for (int row = 0; row < model->rowCount(); ++row) {
        QModelIndex idx = model->index(row, 0);
        print_message_to_buffer(idx, new_buffer);
        /* make sure these messages are marked as read */
        auto is_read = idx.data(static_cast<int>(Media::TextRecording::Role::IsRead)).toBool();
        if (!is_read) {
            g_debug("marking message as read");
            model->setData(idx, true, static_cast<int>(Media::TextRecording::Role::IsRead));
        }
    }

    /* append new messages */
    priv->new_message_connection = QObject::connect(
        model,
        &QAbstractItemModel::rowsInserted,
        [self, priv, model] (const QModelIndex &parent, int first, int last) {
            for (int row = first; row <= last; ++row) {
                QModelIndex idx = model->index(row, 0, parent);
                print_message_to_buffer(idx, gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->textview_chat)));
                /* make sure these messages are marked as read */
                auto is_read = idx.data(static_cast<int>(Media::TextRecording::Role::IsRead)).toBool();
                if (!is_read) {
                    g_debug("marking message as read");
                    model->setData(idx, true, static_cast<int>(Media::TextRecording::Role::IsRead));
                }
                g_signal_emit(G_OBJECT(self), chat_view_signals[NEW_MESSAGES_DISPLAYED], 0);
            }
        }
    );
}

static void
selected_cm_changed(GtkComboBox *box, ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    auto cms = priv->person->phoneNumbers();
    auto active = gtk_combo_box_get_active(box);
    if (active >= 0 && active < cms.size()) {
        parse_chat_model(cms.at(active)->textRecording()->instantMessagingModel(), self);
    } else {
        g_warning("no valid ContactMethod selected to display chat conversation");
    }
}

static void
update_contact_methods(ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    g_return_if_fail(priv->person);

    /* model for the combobox for the choice of ContactMethods */
    auto cm_model = gtk_list_store_new(
        2, G_TYPE_STRING, G_TYPE_POINTER
    );

    auto cms = priv->person->phoneNumbers();
    for (int i = 0; i < cms.size(); ++i) {
        GtkTreeIter iter;
        gtk_list_store_append(cm_model, &iter);
        gtk_list_store_set(cm_model, &iter,
                           0, cms.at(i)->uri().toUtf8().constData(),
                           1, cms.at(i),
                           -1);
    }

    gtk_combo_box_set_model(GTK_COMBO_BOX(priv->combobox_cm), GTK_TREE_MODEL(cm_model));
    g_object_unref(cm_model);

    auto renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_cm), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(priv->combobox_cm), renderer, "text", 0, NULL);

    /* select the last used cm */
    if (!cms.isEmpty()) {
        auto last_used_cm = cms.at(0);
        int last_used_cm_idx = 0;
        for (int i = 1; i < cms.size(); ++i) {
            auto new_cm = cms.at(i);
            if (difftime(new_cm->lastUsed(), last_used_cm->lastUsed()) > 0) {
                last_used_cm = new_cm;
                last_used_cm_idx = i;
            }
        }

        gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_cm), last_used_cm_idx);
    }

    /* show the combo box if there is more than one cm to choose from */
    if (cms.size() > 1)
        gtk_widget_show_all(priv->combobox_cm);
    else
        gtk_widget_hide(priv->combobox_cm);
}

static void
update_name(ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    g_return_if_fail(priv->person || priv->cm);

    QString name;
    if (priv->person) {
        name = priv->person->roleData(static_cast<int>(Ring::Role::Name)).toString();
    } else {
        name = priv->cm->roleData(static_cast<int>(Ring::Role::Name)).toString();
    }
    gtk_label_set_text(GTK_LABEL(priv->label_peer), name.toUtf8().constData());
}

GtkWidget *
chat_view_new_call(Call *call)
{
    g_return_val_if_fail(call, nullptr);

    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    priv->call = call;
    auto cm = priv->call->peerContactMethod();
    parse_chat_model(cm->textRecording()->instantMessagingModel(), self);

    return (GtkWidget *)self;
}

GtkWidget *
chat_view_new_cm(ContactMethod *cm)
{
    g_return_val_if_fail(cm, nullptr);

    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    priv->cm = cm;
    parse_chat_model(priv->cm->textRecording()->instantMessagingModel(), self);
    update_name(self);

    gtk_widget_show(priv->hbox_chat_info);

    return (GtkWidget *)self;
}

GtkWidget *
chat_view_new_person(Person *p)
{
    g_return_val_if_fail(p, nullptr);

    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    priv->person = p;

    /* connect to the changed signal before setting the cm combo box, so that the correct
     * conversation will get displayed */
    g_signal_connect(priv->combobox_cm, "changed", G_CALLBACK(selected_cm_changed), self);
    update_contact_methods(self);
    update_name(self);

    gtk_widget_show(priv->hbox_chat_info);

    return (GtkWidget *)self;
}
