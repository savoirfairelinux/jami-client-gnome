/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
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

#include "webkitchatcontainer.h"

// GTK+ related
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

// Qt
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

// LRC
#include <media/textrecording.h>
#include <globalinstances.h>
#include <contactmethod.h>
#include <message.h>

// Ring Client
#include "native/pixbufmanipulator.h"
#include "config.h"

struct _WebKitChatContainer
{
    GtkBox parent;
};

struct _WebKitChatContainerClass
{
    GtkBoxClass parent_class;
};

typedef struct _WebKitChatContainerPrivate WebKitChatContainerPrivate;

struct _WebKitChatContainerPrivate
{
    GtkWidget* webview_chat;
    GtkWidget* box_webview_chat;

    bool       chatview_debug;

    /* Array of javascript libraries to load. Used during initialization */
    GList*     js_libs_to_load;
    gboolean   js_libs_loaded;
};

G_DEFINE_TYPE_WITH_PRIVATE(WebKitChatContainer, webkit_chat_container, GTK_TYPE_BOX);

#define WEBKIT_CHAT_CONTAINER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), WEBKIT_CHAT_CONTAINER_TYPE, WebKitChatContainerPrivate))

/* signals */
enum {
    READY,
    SEND_MESSAGE,
    LAST_SIGNAL
};

static guint webkit_chat_container_signals[LAST_SIGNAL] = { 0 };

/* functions */
static gboolean webview_crashed(WebKitChatContainer *self);

static void
webkit_chat_container_dispose(GObject *object)
{
    G_OBJECT_CLASS(webkit_chat_container_parent_class)->dispose(object);
}

static void
webkit_chat_container_init(WebKitChatContainer *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
webkit_chat_container_class_init(WebKitChatContainerClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = webkit_chat_container_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/webkitchatcontainer.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), WebKitChatContainer, box_webview_chat);

    /* add signals */
    webkit_chat_container_signals[READY] = g_signal_new("ready",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    webkit_chat_container_signals[SEND_MESSAGE] = g_signal_new("send-message",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);
}

static gboolean
webview_chat_context_menu(WebKitChatContainer *self,
                          WebKitContextMenu   *menu,
                          GdkEvent            *event,
                          WebKitHitTestResult *hit_test_result,
                          gpointer             user_data)
{
    GList *items, *nextList;
    for (items = webkit_context_menu_get_items(menu) ; items ; items = nextList) {
        WebKitContextMenuAction action;
        nextList = items->next;
        auto item = (WebKitContextMenuItem*)items->data;
        action = webkit_context_menu_item_get_stock_action(item);

        if (action == WEBKIT_CONTEXT_MENU_ACTION_RELOAD ||
        action == WEBKIT_CONTEXT_MENU_ACTION_GO_FORWARD ||
        action == WEBKIT_CONTEXT_MENU_ACTION_GO_BACK ||
        action == WEBKIT_CONTEXT_MENU_ACTION_STOP) {
            webkit_context_menu_remove(menu, item);
        }
    }
    return false;
}

QString
message_index_to_json_message_object(const QModelIndex &idx)
{
    auto message = idx.data().value<QString>();
    auto sender = idx.data(static_cast<int>(Media::TextRecording::Role::AuthorDisplayname)).value<QString>();
    auto sender_contact_method = idx.data(static_cast<int>(Media::TextRecording::Role::ContactMethod)).value<ContactMethod*>();
    auto timestamp = idx.data(static_cast<int>(Media::TextRecording::Role::Timestamp)).value<time_t>();
    auto direction = idx.data(static_cast<int>(Media::TextRecording::Role::Direction)).value<Media::Media::Direction>();
    auto message_id = idx.row();

    QString sender_contact_method_str;
    if(direction == Media::Media::Direction::IN)
    {
        sender_contact_method_str = QString(g_strdup_printf("%p", sender_contact_method));
    }
    else
    {
        sender_contact_method_str = "self";
    }

    QJsonObject message_object = QJsonObject();
    message_object.insert("text", QJsonValue(message));
    message_object.insert("id", QJsonValue(QString().setNum(message_id)));
    message_object.insert("sender", QJsonValue(sender));
    message_object.insert("sender_contact_method", QJsonValue(sender_contact_method_str));
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


QString
message_to_json_message_object(const lrc::message::Info& message)
{
    auto sender = "TODO";
    auto sender_contact_method = "TODO";
    auto timestamp = QString::number(message.timestamp);
    auto direction = message.isOutgoing ? QString("out") : QString("in");
    auto message_id = 0; //TODO

    QJsonObject message_object = QJsonObject();
    message_object.insert("text", QJsonValue(QString(message.body.c_str())));
    message_object.insert("id", QJsonValue(QString().setNum(message_id)));
    message_object.insert("sender", QJsonValue(sender));
    message_object.insert("sender_contact_method", QJsonValue("sender_contact_method_str"));
    message_object.insert("timestamp", QJsonValue(timestamp));
    message_object.insert("direction", QJsonValue(direction));
    message_object.insert("delivery_status", QJsonValue("sent")); //TODO from status

    return QString(QJsonDocument(message_object).toJson(QJsonDocument::Compact));
}

#if WEBKIT_CHECK_VERSION(2, 6, 0)
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
webview_send_text(WebKitWebView      *self,
                  WebKitScriptDialog *dialog,
                  gpointer            user_data)
{
    auto message = webkit_script_dialog_get_message(dialog);
    g_signal_emit(G_OBJECT(self), webkit_chat_container_signals[SEND_MESSAGE], 0, message);
    return true;
}

static void
javascript_library_loaded(WebKitWebView *webview_chat,
                          GAsyncResult *result,
                          WebKitChatContainer* self)
{
    g_return_if_fail(IS_WEBKIT_CHAT_CONTAINER(self));
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(self);

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
         priv->js_libs_loaded = TRUE;
         g_signal_emit(G_OBJECT(self), webkit_chat_container_signals[READY], 0);

         /* The view could now be deleted without causing a crash */
         g_object_unref(self);
    }
}

static void
load_javascript_libs(WebKitWebView *webview_chat,
                     WebKitChatContainer* self)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(self);

    /* Create the list of libraries to load */
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/cx/ring/RingGnome/linkify.js");
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/cx/ring/RingGnome/linkify-string.js");
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/cx/ring/RingGnome/linkify-html.js");

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
                          WebKitChatContainer* self)
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

static void
build_view(WebKitChatContainer *view)
{
    g_return_if_fail(IS_WEBKIT_CHAT_CONTAINER(view));
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);

    priv->chatview_debug = FALSE;
    auto ring_chatview_debug = g_getenv("RING_CHATVIEW_DEBUG");
    if (ring_chatview_debug || g_strcmp0(ring_chatview_debug, "true") == 0)
    {
        priv->chatview_debug = TRUE;
    }

    /* Prepare WebKitUserContentManager */
    WebKitUserContentManager* webkit_content_manager = webkit_user_content_manager_new();

    WebKitUserStyleSheet* chatview_style_sheet = webkit_user_style_sheet_new(
        (gchar*) g_bytes_get_data(
            g_resources_lookup_data(
                "/cx/ring/RingGnome/chatview.css",
                G_RESOURCE_LOOKUP_FLAGS_NONE,
                NULL
            ),
            NULL
        ),
        WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
        WEBKIT_USER_STYLE_LEVEL_USER,
        NULL,
        NULL
    );
    webkit_user_content_manager_add_style_sheet(webkit_content_manager, chatview_style_sheet);

    /* Prepare WebKitSettings */
    WebKitSettings* webkit_settings = webkit_settings_new_with_settings(
        "enable-javascript", TRUE,
        "enable-developer-extras", priv->chatview_debug,
        "enable-java", FALSE,
        "enable-plugins", FALSE,
        "enable-site-specific-quirks", FALSE,
        "enable-smooth-scrolling", TRUE,
        NULL
    );

    /* Create the WebKitWebView */
    priv->webview_chat = GTK_WIDGET(
        webkit_web_view_new_with_user_content_manager(
            webkit_content_manager
        )
    );

    gtk_container_add(GTK_CONTAINER(priv->box_webview_chat), priv->webview_chat);
    gtk_widget_show(priv->webview_chat);
    gtk_widget_set_vexpand(GTK_WIDGET(priv->webview_chat), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->webview_chat), TRUE);

    /* Set the WebKitSettings */
    webkit_web_view_set_settings(WEBKIT_WEB_VIEW(priv->webview_chat), webkit_settings);

    gtk_drag_dest_unset(priv->webview_chat); // remove drag and drop to prevent unwanted reloading
    g_signal_connect(priv->webview_chat, "load-changed", G_CALLBACK(webview_chat_load_changed), view);
    g_signal_connect_swapped(priv->webview_chat, "context-menu", G_CALLBACK(webview_chat_context_menu), view);
    g_signal_connect_swapped(priv->webview_chat, "script-dialog", G_CALLBACK(webview_send_text), view);
#if WEBKIT_CHECK_VERSION(2, 6, 0)
    g_signal_connect(priv->webview_chat, "decide-policy", G_CALLBACK(webview_chat_decide_policy), view);
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

    /* handle web view crash */
    g_signal_connect_swapped(priv->webview_chat, "web-process-crashed", G_CALLBACK(webview_crashed), view);
}

static gboolean
webview_crashed(WebKitChatContainer *self)
{
    g_warning("Gtk Web Process crashed! Re-createing web view");

    auto priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(self);

    /* make sure we destroy previous WebView */
    if (priv->webview_chat) {
        gtk_widget_destroy(priv->webview_chat);
        priv->webview_chat = nullptr;
    }

    build_view(self);

    return G_SOURCE_CONTINUE;
}

GtkWidget *
webkit_chat_container_new()
{
    gpointer view = g_object_new(WEBKIT_CHAT_CONTAINER_TYPE, NULL);

    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);
    priv->js_libs_loaded = FALSE;

    build_view(WEBKIT_CHAT_CONTAINER(view));

    return (GtkWidget *)view;
}

void
webkit_chat_container_set_display_links(WebKitChatContainer *view, bool display)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);
    gchar* function_call = g_strdup_printf("ring.chatview.setDisplayLinks(%s);",
      display ? "true" : "false");

    webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        function_call,
        NULL,
        NULL,
        NULL
    );
}

void
webkit_chat_container_clear_sender_images(WebKitChatContainer *view)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);

    webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        "ring.chatview.clearSenderImages()",
        NULL,
        NULL,
        NULL
    );
}

void
webkit_chat_container_clear(WebKitChatContainer *view)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);

    webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        "ring.chatview.clearMessages()",
        NULL,
        NULL,
        NULL
    );

    webkit_chat_container_clear_sender_images(view);
}

/*
void
webkit_chat_container_print_new_message(WebKitChatContainer *view, const QModelIndex &idx)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);

    auto message_object = message_index_to_json_message_object(idx).toUtf8();
    gchar* function_call = g_strdup_printf("ring.chatview.addMessage(%s);", message_object.constData());
    webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        function_call,
        NULL,
        NULL,
        NULL
    );
    g_free(function_call);
}
*/
/**
 * TODO temp method. Just transform messages from database and breaks nothing in chatview.html for now
 */
void
webkit_chat_container_print_new_message(WebKitChatContainer *view, const lrc::message::Info& message)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);

    auto message_object = message_to_json_message_object(message).toUtf8();
    gchar* function_call = g_strdup_printf("ring.chatview.addMessage(%s);", message_object.constData());
    webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        function_call,
        NULL,
        NULL,
        NULL
    );
    g_free(function_call);
}

void
webkit_chat_container_update_message(WebKitChatContainer *view, const QModelIndex &idx)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);

    auto message_object = message_index_to_json_message_object(idx).toUtf8();
    gchar* function_call = g_strdup_printf("ring.chatview.updateMessage(%s);", message_object.constData());
    webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        function_call,
        NULL,
        NULL,
        NULL
    );
    g_free(function_call);
}

void
webkit_chat_container_set_temporary(WebKitChatContainer *view, bool temporary)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);

    gchar* function_call = g_strdup_printf("ring.chatview.setTemporary(%s)",
    temporary ? "true" : "false");
    webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        function_call,
        NULL,
        NULL,
        NULL
    );
    g_free(function_call);
}

void
webkit_chat_container_set_sender_image(WebKitChatContainer *view, ContactMethod *sender_contact_method, QVariant sender_image)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);

    /* The sender_contact_method should be set to nullptr if the sender is self */
    QString sender_contact_method_str;
    if (sender_contact_method)
    {
        sender_contact_method_str =  QString().sprintf("%p", sender_contact_method);
    }
    else
    {
        sender_contact_method_str = "self";
    }

    auto sender_image_base64 = (QString) GlobalInstances::pixmapManipulator().toByteArray(sender_image).toBase64();

    QJsonObject set_sender_image_object = QJsonObject();
    set_sender_image_object.insert("sender_contact_method", QJsonValue(sender_contact_method_str));
    set_sender_image_object.insert("sender_image", QJsonValue(sender_image_base64));

    auto set_sender_image_object_string = QString(QJsonDocument(set_sender_image_object).toJson(QJsonDocument::Compact));

    gchar* function_call = g_strdup_printf("ring.chatview.setSenderImage(%s);", set_sender_image_object_string.toUtf8().constData());
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(priv->webview_chat), function_call, NULL, NULL, NULL);
    g_free(function_call);
}

gboolean
webkit_chat_container_is_ready(WebKitChatContainer *view)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);
    return priv->js_libs_loaded;
}
