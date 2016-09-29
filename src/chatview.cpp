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
#include <media/textrecording.h>
#include <person.h>
#include <media/media.h>
#include <media/text.h>
#include <media/textrecording.h>
#include "ringnotify.h"
#include "profilemodel.h"
#include "profile.h"
#include "numbercategory.h"
#include <QtCore/QDateTime>
#include "utils/calling.h"
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <webkit2/webkit2.h>
#include <globalinstances.h>
#include "native/pixbufmanipulator.h"

static constexpr GdkRGBA RING_BLUE  = {0.0508, 0.594, 0.676, 1.0}; // outgoing msg color: (13, 152, 173)

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
    GtkWidget* webview_chat;
    GtkWidget *button_chat_input;
    GtkWidget *entry_chat_input;
    GtkWidget *scrolledwindow_chat;
    GtkWidget *hbox_chat_info;
    GtkWidget *label_peer;
    GtkWidget *combobox_cm;
    GtkWidget *button_close_chatview;
    GtkWidget *button_placecall;

    bool chatview_debug;

    /* only one of the three following pointers should be non void;
     * either this is an in-call chat (and so the in-call chat APIs will be used)
     * or it is an out of call chat (and so the account chat APIs will be used)
     */
    Call          *call;
    Person        *person;
    ContactMethod *cm;

    /* initial TextRecording to print when the chat is ready */
    Media::TextRecording* initial_text_recording;

    /* Array of javascript libraries to load. Used during initialization */
    GList* js_libs_to_load;

    QMetaObject::Connection new_message_connection;
    QMetaObject::Connection message_changed_connection;
};

G_DEFINE_TYPE_WITH_PRIVATE(ChatView, chat_view, GTK_TYPE_BOX);

#define CHAT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CHAT_VIEW_TYPE, ChatViewPrivate))

enum {
    NEW_MESSAGES_DISPLAYED,
    HIDE_VIEW_CLICKED,
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
    QObject::disconnect(priv->message_changed_connection);

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
                g_warning("no ContactMethod chosen; message not sent");
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
hide_chat_view(G_GNUC_UNUSED GtkWidget *widget, ChatView *self)
{
    g_signal_emit(G_OBJECT(self), chat_view_signals[HIDE_VIEW_CLICKED], 0);
}

static void
placecall_clicked(ChatView *self)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    if (priv->person) {
        // get the chosen cm
        auto active = gtk_combo_box_get_active(GTK_COMBO_BOX(priv->combobox_cm));
        if (active >= 0) {
            auto cm = priv->person->phoneNumbers().at(active);
            place_new_call(cm);
        } else {
            g_warning("no ContactMethod chosen; cannot place call");
        }
    } else if (priv->cm) {
        place_new_call(priv->cm);
    } else {
        g_warning("no Person or ContactMethod set; cannot place call");
    }
}

static void
chat_view_init(ChatView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(view);

    g_signal_connect(priv->button_chat_input, "clicked", G_CALLBACK(send_chat), view);
    g_signal_connect(priv->entry_chat_input, "activate", G_CALLBACK(send_chat), view);
    g_signal_connect(priv->button_close_chatview, "clicked", G_CALLBACK(hide_chat_view), view);
    g_signal_connect_swapped(priv->button_placecall, "clicked", G_CALLBACK(placecall_clicked), view);
}

static void
chat_view_class_init(ChatViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = chat_view_dispose;

    /* This must be called at least once before we render chatview.ui
     * in order to allow us to use WebKitWebView in the template file. */
    webkit_web_view_get_type();

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/chatview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, webview_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, button_chat_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, entry_chat_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, scrolledwindow_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, hbox_chat_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, label_peer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, combobox_cm);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, button_close_chatview);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), ChatView, button_placecall);

    chat_view_signals[NEW_MESSAGES_DISPLAYED] = g_signal_new (
        "new-messages-displayed",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    chat_view_signals[HIDE_VIEW_CLICKED] = g_signal_new (
        "hide-view-clicked",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

QString
message_index_to_json_message_object(const QModelIndex &idx)
{
    auto message = idx.data().value<QString>();
    auto sender = idx.data(static_cast<int>(Media::TextRecording::Role::AuthorDisplayname)).value<QString>();
    auto timestamp = idx.data(static_cast<int>(Media::TextRecording::Role::Timestamp)).value<time_t>();
    auto direction = idx.data(static_cast<int>(Media::TextRecording::Role::Direction)).value<Media::Media::Direction>();
    auto message_id = idx.row();

    QJsonObject message_object = QJsonObject();
    message_object.insert("text", QJsonValue(message));
    message_object.insert("id", QJsonValue(QString().setNum(message_id)));
    message_object.insert("sender", QJsonValue(sender));
    message_object.insert("timestamp", QJsonValue((int) timestamp));
    message_object.insert("direction", QJsonValue((direction == Media::Media::Direction::IN) ? "in" : "out"));

    switch(idx.data(static_cast<int>(Media::TextRecording::Role::DeliveryStatus)).value<Media::TextRecording::Status>())
    {
        case Media::TextRecording::Status::FAILURE:
        {
            message_object.insert("delivery_status", QJsonValue("failure"));
            break;
        }
        case Media::TextRecording::Status::COUNT__:
        {
            message_object.insert("delivery_status", QJsonValue("count__"));
            break;
        }
        case Media::TextRecording::Status::SENDING:
        {
            message_object.insert("delivery_status", QJsonValue("sending"));
            break;
        }
        case Media::TextRecording::Status::UNKNOWN:
        {
            message_object.insert("delivery_status", QJsonValue("unknown"));
            break;
        }
        case Media::TextRecording::Status::READ:
        {
            message_object.insert("delivery_status", QJsonValue("read"));
            break;
        }
        case Media::TextRecording::Status::SENT:
        {
            message_object.insert("delivery_status", QJsonValue("sent"));
            break;
        }
    }

    return QString(QJsonDocument(message_object).toJson(QJsonDocument::Compact));
}

static void
print_message_to_buffer(ChatView* self, const QModelIndex &idx)
{
    if (!idx.isValid()) {
        g_warning("QModelIndex in im model is not valid");
        return;
    }

    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    auto message_object = message_index_to_json_message_object(idx).toUtf8().constData();

    gchar* function_call = g_strdup_printf("ring.chatview.addMessage(%s);", message_object);

    webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        function_call,
        NULL,
        NULL,
        NULL
    );

    g_free(function_call);
}

static void
print_text_recording(Media::TextRecording *recording, ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    /* only text messages are supported for now */
    auto model = recording->instantTextMessagingModel();

    /* new model, disconnect from the old model updates and clear the text buffer */
    QObject::disconnect(priv->new_message_connection);

    /* put all the messages in the im model into the text view */
    for (int row = 0; row < model->rowCount(); ++row) {
        QModelIndex idx = model->index(row, 0);
        print_message_to_buffer(self, idx);
    }
    /* mark all messages as read */
    recording->setAllRead();


    /* messages modified */
    /* connecting on instantMessagingModel and not textMessagingModel */
    priv->message_changed_connection = QObject::connect(
        model,
        &QAbstractItemModel::dataChanged,
        [self, priv] (const QModelIndex & topLeft, G_GNUC_UNUSED const QModelIndex & bottomRight)
        {
            auto message_object = message_index_to_json_message_object(topLeft).toUtf8().constData();
            gchar* function_call = g_strdup_printf("ring.chatview.updateMessage(%s);", message_object);
            webkit_web_view_run_javascript(
                WEBKIT_WEB_VIEW(priv->webview_chat),
                function_call,
                NULL,
                NULL,
                NULL
            );
            g_free(function_call);
        }
    );

    /* append new messages */
    priv->new_message_connection = QObject::connect(
        model,
        &QAbstractItemModel::rowsInserted,
        [self, priv, model] (const QModelIndex &parent, int first, int last) {
            for (int row = first; row <= last; ++row) {
                QModelIndex idx = model->index(row, 0, parent);
                print_message_to_buffer(self, idx);
                /* make sure these messages are marked as read */
                model->setData(idx, true, static_cast<int>(Media::TextRecording::Role::IsRead));
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
        print_text_recording(cms.at(active)->textRecording(), self);
    } else {
        g_warning("no valid ContactMethod selected to display chat conversation");
    }
}

static void
render_contact_method(G_GNUC_UNUSED GtkCellLayout *cell_layout,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    GValue value = G_VALUE_INIT;
    gtk_tree_model_get_value(model, iter, 0, &value);
    auto cm = (ContactMethod *)g_value_get_pointer(&value);

    gchar *number = nullptr;
    if (cm && cm->category()) {
        // try to get the number category, eg: "home"
        number = g_strdup_printf("(%s) %s", cm->category()->name().toUtf8().constData(),
                                            cm->uri().toUtf8().constData());
    } else if (cm) {
        number = g_strdup_printf("%s", cm->uri().toUtf8().constData());
    }

    g_object_set(G_OBJECT(cell), "text", number, NULL);
    g_free(number);
}

static void
update_contact_methods(ChatView *self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    g_return_if_fail(priv->person || priv->cm);

    /* model for the combobox for the choice of ContactMethods */
    auto cm_model = gtk_list_store_new(
        1, G_TYPE_POINTER
    );

    Person::ContactMethods cms;

    if (priv->person)
        cms = priv->person->phoneNumbers();
    else
        cms << priv->cm;

    for (int i = 0; i < cms.size(); ++i) {
        GtkTreeIter iter;
        gtk_list_store_append(cm_model, &iter);
        gtk_list_store_set(cm_model, &iter,
                           0, cms.at(i),
                           -1);
    }

    gtk_combo_box_set_model(GTK_COMBO_BOX(priv->combobox_cm), GTK_TREE_MODEL(cm_model));
    g_object_unref(cm_model);

    auto renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_cm), renderer, FALSE);
    gtk_cell_layout_set_cell_data_func(
        GTK_CELL_LAYOUT(priv->combobox_cm),
        renderer,
        (GtkCellLayoutDataFunc)render_contact_method,
        nullptr, nullptr);

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

    /* if there is only one CM, make the combo box insensitive */
    if (cms.size() < 2)
        gtk_widget_set_sensitive(priv->combobox_cm, FALSE);

    /* if no CMs make the call button insensitive */
    gtk_widget_set_sensitive(priv->button_placecall, !cms.isEmpty());
}

static void
update_name(ChatView *self)
{
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

static void
set_sender_image(WebKitWebView* webview_chat, QString sender_name, QVariant sender_image)
{
    auto sender_image_base64 = (QString) GlobalInstances::pixmapManipulator().toByteArray(sender_image).toBase64();

    QJsonObject set_sender_image_object = QJsonObject();
    set_sender_image_object.insert("sender", QJsonValue(sender_name));
    set_sender_image_object.insert("sender_image", QJsonValue(sender_image_base64));

    auto set_sender_image_object_string = QString(QJsonDocument(set_sender_image_object).toJson(QJsonDocument::Compact)).toUtf8().constData();

    gchar* function_call = g_strdup_printf("ring.chatview.setSenderImage(%s);", set_sender_image_object_string);
    webkit_web_view_run_javascript(webview_chat, function_call, NULL, NULL, NULL);
    g_free(function_call);
}

static void
set_participant_images(ChatView* self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    /* set sender image for "ME" */
    auto profile = ProfileModel::instance().selectedProfile();
    if (profile)
    {
        auto person = profile->person();
        if (person)
        {
            auto photo_variant_me = person->photo();
            if (photo_variant_me.isValid())
            {
                set_sender_image(
                    WEBKIT_WEB_VIEW(priv->webview_chat),
                    "Me",
                    photo_variant_me
                );
            }
        }
    }

    /* Set the sender image for the peer */
    QString sender_name_peer;
    QVariant photo_variant_peer;

    if (priv->person)
    {
        photo_variant_peer = priv->person->photo();
        sender_name_peer = priv->person->roleData(static_cast<int>(Ring::Role::Name)).toString();
    }
    else
    {
        ContactMethod *contact_method;
        if (priv->cm)
        {
            contact_method = priv->cm;
        }
        else
        {
            contact_method = priv->call->peerContactMethod();
        }
        sender_name_peer = contact_method->roleData(static_cast<int>(Ring::Role::Name)).toString();
        photo_variant_peer = contact_method->roleData((int) Call::Role::Photo);
    }

    if (photo_variant_peer.isValid())
    {
        set_sender_image(WEBKIT_WEB_VIEW(priv->webview_chat), sender_name_peer, photo_variant_peer);
    }
}

static void
javascript_library_loaded(WebKitWebView *webview_chat,
                          GAsyncResult *result,
                          ChatView* self)
{
    g_return_if_fail(IS_CHAT_VIEW(self));
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    auto loaded_library = g_list_first(priv->js_libs_to_load);

    GError *error = NULL;
    WebKitJavascriptResult* js_result = webkit_web_view_run_javascript_from_gresource_finish(webview_chat, result, &error);
    if (!js_result) {
        g_warning("Error loading %s: %s", (const gchar*) loaded_library->data, error->message);
        g_error_free(error);
        g_object_unref(self);
        /* Stop loading view, most likely resulting in a blank page */
        return;
    }
    webkit_javascript_result_unref(js_result);


    //REVIEW: do I have to free the string? how?
    priv->js_libs_to_load = g_list_remove(priv->js_libs_to_load, loaded_library->data);

    if(g_list_length(priv->js_libs_to_load) > 0)
    {
        /* keep loading... */
        webkit_web_view_run_javascript_from_gresource(
            webview_chat,
            (const gchar*) g_list_first(priv->js_libs_to_load)->data,
            NULL,
            (GAsyncReadyCallback) javascript_library_loaded,
            self
        );
    }
    else
    {
        /* All of the needed javascript libraries were loaded */

        /* set the photos of the chat participants */
        set_participant_images(self);

        /* we can now print the text recordings */
        if (priv->initial_text_recording)
        {
            print_text_recording(priv->initial_text_recording, self);
        }

        /* connect to the changed signal before setting the cm combo box, so that the correct
         * conversation will get displayed */
         if(priv->person)
         {
             g_signal_connect(priv->combobox_cm, "changed", G_CALLBACK(selected_cm_changed), self);
             update_contact_methods(self);
             update_name(self);
         }

         g_object_unref(self);
    }
}

static void
load_javascript_libs(WebKitWebView *webview_chat,
                     ChatView* self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    /* Create the list of libraries to load */
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/cx/ring/RingGnome/jquery.js");
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/cx/ring/RingGnome/linkify.js");
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/cx/ring/RingGnome/linkify-string.js");
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/cx/ring/RingGnome/linkify-html.js");
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/cx/ring/RingGnome/linkify-jquery.js");

    /* ref the chat view so that its not destroyed while we load
     * we will unref in javascript_library_loaded
     */
    g_object_ref(self);

   /* start loading */
    webkit_web_view_run_javascript_from_gresource(
        WEBKIT_WEB_VIEW(webview_chat),
        (const gchar*) g_list_first(priv->js_libs_to_load)->data,
        NULL,
        (GAsyncReadyCallback) javascript_library_loaded,
        self
    );

}

static void
webview_chat_load_changed(WebKitWebView  *webview_chat,
                          WebKitLoadEvent load_event,
                          ChatView* self)
{
    switch (load_event) {
        case WEBKIT_LOAD_REDIRECTED:
        {
            g_warning("webview_chat load is being redirected, this should not happen");
        }
        case WEBKIT_LOAD_STARTED:
        case WEBKIT_LOAD_COMMITTED:
        {
            break;
        }
        case WEBKIT_LOAD_FINISHED:
        {
            load_javascript_libs(webview_chat, self);
            //TODO: disconnect? It shouldn't happen more than once
            break;
        }
    }
}

#if HAVE_WEBKIT2GTK4
static gboolean
webview_chat_decide_policy (G_GNUC_UNUSED WebKitWebView *web_view,
                            WebKitPolicyDecision *decision,
                            WebKitPolicyDecisionType type)
{
    switch (type)
    {
        case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
        case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
        {
            WebKitNavigationPolicyDecision* navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
            WebKitNavigationAction* navigation_action = webkit_navigation_policy_decision_get_navigation_action(navigation_decision);
            WebKitNavigationType navigation_type = webkit_navigation_action_get_navigation_type(navigation_action);

            switch (navigation_type)
            {
                case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED:
                case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD:
                case WEBKIT_NAVIGATION_TYPE_RELOAD:
                case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED:
                case WEBKIT_NAVIGATION_TYPE_OTHER:
                {
                    /* make no decision */
                    return FALSE;

                }
                case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED:
                {
                    webkit_policy_decision_ignore(decision);

                    WebKitURIRequest* uri_request = webkit_navigation_action_get_request(navigation_action);
                    const gchar* uri = webkit_uri_request_get_uri(uri_request);

                    gtk_show_uri(NULL, uri, GDK_CURRENT_TIME, NULL);
                }
            }

            webkit_policy_decision_ignore(decision);
            break;
        }
        case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
        {
            //WebKitResponsePolicyDecision *response = WEBKIT_RESPONSE_POLICY_DECISION (decision);
            //break;
            return FALSE;
        }
        default:
        {
            /* Making no decision results in webkit_policy_decision_use(). */
            return FALSE;
        }
    }
    return TRUE;
}
#endif

static gboolean
webview_chat_context_menu(ChatView *self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);
    return !priv->chatview_debug;
}

static void
build_chat_view(ChatView* self)
{
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    priv->chatview_debug = FALSE;
    auto ring_chatview_debug = g_getenv("RING_CHATVIEW_DEBUG");
    if (ring_chatview_debug || g_strcmp0(ring_chatview_debug, "true") == 0)
    {
        priv->chatview_debug = TRUE;
    }

    WebKitSettings* webkit_settings = webkit_settings_new_with_settings(
        "enable-javascript", TRUE,
        "enable-developer-extras", priv->chatview_debug,
        "enable-java", FALSE,
        "enable-plugins", FALSE,
        "enable-site-specific-quirks", FALSE,
        "enable-smooth-scrolling", TRUE,
        NULL
    );
    webkit_web_view_set_settings(WEBKIT_WEB_VIEW(priv->webview_chat), webkit_settings);

    g_signal_connect(priv->webview_chat, "load-changed", G_CALLBACK(webview_chat_load_changed), self);
    g_signal_connect_swapped(priv->webview_chat, "context-menu", G_CALLBACK(webview_chat_context_menu), self);
#if HAVE_WEBKIT2GTK4
    g_signal_connect(priv->webview_chat, "decide-policy", G_CALLBACK(webview_chat_decide_policy), self);
#endif

    GBytes* chatview_bytes = g_resources_lookup_data(
        "/cx/ring/RingGnome/chatview.html",
        G_RESOURCE_LOOKUP_FLAGS_NONE,
        NULL
    );

    webkit_web_view_load_html(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        (gchar*) g_bytes_get_data(chatview_bytes, NULL),
        NULL
    );

    /* Now we wait for the load-changed event, before we
     * start loading javascript libraries */
}

GtkWidget *
chat_view_new_call(Call *call)
{
    g_return_val_if_fail(call, nullptr);

    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));
    build_chat_view(self);
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    priv->call = call;
    auto cm = priv->call->peerContactMethod();
    priv->initial_text_recording = cm->textRecording();

    return (GtkWidget *)self;
}

GtkWidget *
chat_view_new_cm(ContactMethod *cm)
{
    g_return_val_if_fail(cm, nullptr);

    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));
    build_chat_view(self);
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    priv->cm = cm;
    priv->initial_text_recording = priv->cm->textRecording();
    update_contact_methods(self);
    update_name(self);

    gtk_widget_show(priv->hbox_chat_info);

    return (GtkWidget *)self;
}

GtkWidget *
chat_view_new_person(Person *p)
{
    g_return_val_if_fail(p, nullptr);

    ChatView *self = CHAT_VIEW(g_object_new(CHAT_VIEW_TYPE, NULL));
    build_chat_view(self);
    ChatViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    priv->person = p;

    gtk_widget_show(priv->hbox_chat_info);

    return (GtkWidget *)self;
}

Call*
chat_view_get_call(ChatView *self)
{
    g_return_val_if_fail(IS_CHAT_VIEW(self), nullptr);
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    return priv->call;
}

ContactMethod*
chat_view_get_cm(ChatView *self)
{
    g_return_val_if_fail(IS_CHAT_VIEW(self), nullptr);
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    return priv->cm;
}

Person*
chat_view_get_person(ChatView *self)
{
    g_return_val_if_fail(IS_CHAT_VIEW(self), nullptr);
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    return priv->person;
}

void
chat_view_set_header_visible(ChatView *self, gboolean visible)
{
    auto priv = CHAT_VIEW_GET_PRIVATE(self);

    gtk_widget_set_visible(priv->hbox_chat_info, visible);
}
