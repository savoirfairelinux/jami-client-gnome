/*
 *  Copyright (C) 2016-2020 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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
#include <webkit2/webkit2.h>

// Qt
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

// LRC
#include <globalinstances.h>
#include <api/conversationmodel.h>
#include <api/account.h>
#include <api/chatview.h>

// Jami  Client
#include "native/pixbufmanipulator.h"

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
    gchar*     data_received;

    /* Array of javascript libraries to load. Used during initialization */
    GList*     js_libs_to_load;
    gboolean   js_libs_loaded;
};

G_DEFINE_TYPE_WITH_PRIVATE(WebKitChatContainer, webkit_chat_container, GTK_TYPE_BOX);

#define WEBKIT_CHAT_CONTAINER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), WEBKIT_CHAT_CONTAINER_TYPE, WebKitChatContainerPrivate))

/* signals */
enum {
    READY,
    SCRIPT_DIALOG,
    DATA_DROPPED,
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
                                                "/net/jami/JamiGnome/webkitchatcontainer.ui");

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

    webkit_chat_container_signals[SCRIPT_DIALOG] = g_signal_new("script-dialog",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);

    webkit_chat_container_signals[DATA_DROPPED] = g_signal_new("data-dropped",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);
}

static gboolean
webview_chat_context_menu(G_GNUC_UNUSED WebKitChatContainer *self,
                          WebKitContextMenu   *menu,
                          G_GNUC_UNUSED GdkEvent            *event,
                          G_GNUC_UNUSED WebKitHitTestResult *hit_test_result,
                          G_GNUC_UNUSED gpointer             user_data)
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

    // FALSE = custom menu, TRUE would mean no menu
    return FALSE;
}

static void
webkit_chat_container_execute_js(WebKitChatContainer *view, const gchar* function_call)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);
    webkit_web_view_run_javascript(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        function_call,
        NULL,
        NULL,
        NULL
    );
}

QJsonObject
build_interaction_json(lrc::api::ConversationModel& conversation_model,
                       const uint64_t msgId,
                       const lrc::api::interaction::Info& interaction)
{
    auto sender = interaction.authorUri;
    if (sender == "") {
        sender = conversation_model.owner.profileInfo.uri;
    }
    auto timestamp = QString::number(interaction.timestamp);
    QString direction = lrc::api::interaction::isOutgoing(interaction) ? "out" : "in";

    QJsonObject interaction_object = QJsonObject();
    interaction_object.insert("text", QJsonValue(interaction.body));
    interaction_object.insert("id", QJsonValue(QString::number(msgId)));
    interaction_object.insert("sender", QJsonValue(sender));
    interaction_object.insert("duration", QJsonValue(static_cast<int>(interaction.duration)));
    interaction_object.insert("sender_contact_method", QJsonValue(sender));
    interaction_object.insert("timestamp", QJsonValue(timestamp));
    interaction_object.insert("direction", QJsonValue(direction));

    switch (interaction.type)
    {
    case lrc::api::interaction::Type::TEXT:
        interaction_object.insert("type", QJsonValue("text"));
        break;
    case lrc::api::interaction::Type::CALL:
        interaction_object.insert("type", QJsonValue("call"));
        break;
    case lrc::api::interaction::Type::CONTACT:
        interaction_object.insert("type", QJsonValue("contact"));
        break;
    case lrc::api::interaction::Type::DATA_TRANSFER: {
        interaction_object.insert("type", QJsonValue("data_transfer"));
        lrc::api::datatransfer::Info info = {};
        conversation_model.getTransferInfo(msgId, info);
        if (info.status != lrc::api::datatransfer::Status::INVALID) {
            interaction_object.insert("totalSize", QJsonValue(qint64(info.totalSize)));
            interaction_object.insert("progress", QJsonValue(qint64(info.progress)));
        }
        break;
    }
    case lrc::api::interaction::Type::INVALID:
    default:
        interaction_object.insert("type", QJsonValue(""));
        break;
    }

    if (interaction.isRead) {
        interaction_object.insert("delivery_status", QJsonValue("read"));
    }

    switch (interaction.status)
    {
    case lrc::api::interaction::Status::SUCCESS:
        interaction_object.insert("delivery_status", QJsonValue("sent"));
        break;
    case lrc::api::interaction::Status::FAILURE:
    case lrc::api::interaction::Status::TRANSFER_ERROR:
        interaction_object.insert("delivery_status", QJsonValue("failure"));
        break;
    case lrc::api::interaction::Status::TRANSFER_UNJOINABLE_PEER:
        interaction_object.insert("delivery_status", QJsonValue("unjoinable peer"));
        break;
    case lrc::api::interaction::Status::SENDING:
        interaction_object.insert("delivery_status", QJsonValue("sending"));
        break;
    case lrc::api::interaction::Status::TRANSFER_CREATED:
        interaction_object.insert("delivery_status", QJsonValue("connecting"));
        break;
    case lrc::api::interaction::Status::TRANSFER_ACCEPTED:
        interaction_object.insert("delivery_status", QJsonValue("accepted"));
        break;
    case lrc::api::interaction::Status::TRANSFER_CANCELED:
        interaction_object.insert("delivery_status", QJsonValue("canceled"));
        break;
    case lrc::api::interaction::Status::TRANSFER_ONGOING:
        interaction_object.insert("delivery_status", QJsonValue("ongoing"));
        break;
    case lrc::api::interaction::Status::TRANSFER_AWAITING_PEER:
        interaction_object.insert("delivery_status", QJsonValue("awaiting peer"));
        break;
    case lrc::api::interaction::Status::TRANSFER_AWAITING_HOST:
        interaction_object.insert("delivery_status", QJsonValue("awaiting host"));
        break;
    case lrc::api::interaction::Status::TRANSFER_TIMEOUT_EXPIRED:
        interaction_object.insert("delivery_status", QJsonValue("awaiting peer timeout"));
        break;
    case lrc::api::interaction::Status::TRANSFER_FINISHED:
        interaction_object.insert("delivery_status", QJsonValue("finished"));
        break;
    case lrc::api::interaction::Status::INVALID:
    case lrc::api::interaction::Status::UNKNOWN:
    default:
        interaction_object.insert("delivery_status", QJsonValue("unknown"));
        break;
    }
    return interaction_object;
}

QString
interaction_to_json_interaction_object(lrc::api::ConversationModel& conversation_model,
                                       const uint64_t msgId,
                                       const lrc::api::interaction::Info& interaction)
{
    auto interaction_object = build_interaction_json(conversation_model, msgId, interaction);
    return QString(QJsonDocument(interaction_object).toJson(QJsonDocument::Compact));
}

QString
interactions_to_json_array_object(lrc::api::ConversationModel& conversation_model,
                                  const std::map<uint64_t, lrc::api::interaction::Info> interactions) {
    QJsonArray array;
    for (const auto& interaction: interactions)
        array.append(build_interaction_json(conversation_model, interaction.first, interaction.second));
    return QString(QJsonDocument(array).toJson(QJsonDocument::Compact));
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
webview_script_dialog(WebKitWebView      *self,
                      WebKitScriptDialog *dialog,
                      G_GNUC_UNUSED gpointer user_data)
{
    auto interaction = webkit_script_dialog_get_message(dialog);
    g_signal_emit(G_OBJECT(self), webkit_chat_container_signals[SCRIPT_DIALOG], 0, interaction);
    return true;
}

static void
init_js_i18n(WebKitChatContainer *view)
{
    gchar *function_call;
    
    auto translated = lrc::api::chatview::getTranslatedStrings();
    QJsonObject trjson;
    for (auto i = translated.begin(); i != translated.end(); ++i) {
        if (not i.value().isEmpty()) {
            trjson[i.key()] = i.value();
        }
    }

    QJsonDocument doc(trjson);
    QString strJson(doc.toJson(QJsonDocument::Compact));

    if (strJson.isEmpty()) {
        /* no translation available for current locale, use default */
        function_call = g_strdup("init_i18n()");
    } else {
        function_call = g_strdup_printf("init_i18n(%s)", qUtf8Printable(strJson));
    }

    webkit_chat_container_execute_js(view, function_call);

    g_free(function_call);
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
         /* load translations before anything else */
         init_js_i18n(self);

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
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/net/jami/JamiGnome/jed.js");
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/net/jami/JamiGnome/linkify.js");
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/net/jami/JamiGnome/chatview.js");
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/net/jami/JamiGnome/linkify-string.js");
    priv->js_libs_to_load = g_list_append(priv->js_libs_to_load, (gchar*) "/net/jami/JamiGnome/linkify-html.js");

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
webview_chat_on_drag_data_received(GtkWidget*,
                                   GdkDragContext*,
                                   gint,
                                   gint,
                                   GtkSelectionData *data,
                                   guint,
                                   guint32,
                                   WebKitChatContainer* self)
{
    g_return_if_fail(IS_WEBKIT_CHAT_CONTAINER(self));
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(self);
    auto* filename = (gchar*)(gtk_selection_data_get_data(data));
    if (filename) {
        priv->data_received = g_strdup_printf("%s", filename);
    }
}


static gboolean
webview_chat_on_drag_data(GtkWidget*,
                          GdkDragContext*,
                          gint,
                          gint,
                          guint,
                          WebKitChatContainer* self)
{
    g_return_val_if_fail(IS_WEBKIT_CHAT_CONTAINER(self), true);
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(self);
    g_signal_emit(G_OBJECT(self), webkit_chat_container_signals[DATA_DROPPED], 0, priv->data_received);
    return true;
}

static void
build_view(WebKitChatContainer *view)
{
    g_return_if_fail(IS_WEBKIT_CHAT_CONTAINER(view));
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);

    priv->chatview_debug = FALSE;
    auto chatview_debug = g_getenv("CHATVIEW_DEBUG");
    if (chatview_debug || g_strcmp0(chatview_debug, "true") == 0)
    {
        priv->chatview_debug = TRUE;
    }

    /* Prepare WebKitUserContentManager */
    WebKitUserContentManager* webkit_content_manager = webkit_user_content_manager_new();

    WebKitUserStyleSheet* chatview_style_sheet = webkit_user_style_sheet_new(
        (gchar*) g_bytes_get_data(
            g_resources_lookup_data(
                "/net/jami/JamiGnome/chatview.css",
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

    chatview_style_sheet = webkit_user_style_sheet_new(
        (gchar*) g_bytes_get_data(
            g_resources_lookup_data(
                "/net/jami/JamiGnome/chatview-gnome.css",
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

    g_signal_connect(priv->webview_chat, "drag-data-received", G_CALLBACK(webview_chat_on_drag_data_received), view);
    g_signal_connect(priv->webview_chat, "drag-drop", G_CALLBACK(webview_chat_on_drag_data), view);
    g_signal_connect(priv->webview_chat, "load-changed", G_CALLBACK(webview_chat_load_changed), view);
    g_signal_connect_swapped(priv->webview_chat, "context-menu", G_CALLBACK(webview_chat_context_menu), view);
    g_signal_connect_swapped(priv->webview_chat, "script-dialog", G_CALLBACK(webview_script_dialog), view);
#if WEBKIT_CHECK_VERSION(2, 6, 0)
    g_signal_connect(priv->webview_chat, "decide-policy", G_CALLBACK(webview_chat_decide_policy), view);
#endif

    GBytes* chatview_bytes = g_resources_lookup_data(
        "/net/jami/JamiGnome/chatview.html",
        G_RESOURCE_LOOKUP_FLAGS_NONE,
        NULL
    );

    // file:// allow the webview to load local files
    webkit_web_view_load_html(
        WEBKIT_WEB_VIEW(priv->webview_chat),
        (gchar*) g_bytes_get_data(chatview_bytes, NULL),
        "file://"
    );

    /* Now we wait for the load-changed event, before we
     * start loading javascript libraries */

    /* handle web view crash */
    g_signal_connect_swapped(priv->webview_chat, "web-process-crashed", G_CALLBACK(webview_crashed), view);
}

static gboolean
webview_crashed(WebKitChatContainer *self)
{
    g_warning("Gtk Web Process crashed! Recreating web view");

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
    gchar* function_call = g_strdup_printf("setDisplayLinks(%s);",
      display ? "true" : "false");
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}

void
webkit_chat_container_clear_sender_images(WebKitChatContainer *view)
{
    webkit_chat_container_execute_js(view, "clearSenderImages();");
}

void
webkit_chat_container_clear(WebKitChatContainer *view)
{
    webkit_chat_container_execute_js(view, "clearMessages();");
    webkit_chat_container_clear_sender_images(view);
}

void
webkit_chat_container_update_interaction(WebKitChatContainer *view,
                                         lrc::api::ConversationModel& conversation_model,
                                         uint64_t msgId,
                                         const lrc::api::interaction::Info& interaction)
{
    auto interaction_object = interaction_to_json_interaction_object(conversation_model, msgId, interaction).toUtf8();
    gchar* function_call = g_strdup_printf("updateMessage(%s);", interaction_object.constData());
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}

void
webkit_chat_container_remove_interaction(WebKitChatContainer *view, uint64_t interactionId)
{
    gchar* function_call = g_strdup_printf("removeInteraction(%lu);", interactionId);
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}


void
webkit_chat_container_print_new_interaction(WebKitChatContainer *view,
                                            lrc::api::ConversationModel& conversation_model,
                                            uint64_t msgId,
                                            const lrc::api::interaction::Info& interaction)
{
    auto interaction_object = interaction_to_json_interaction_object(conversation_model, msgId, interaction).toUtf8();
    gchar* function_call = g_strdup_printf("addMessage(%s);", interaction_object.constData());
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}

void
webkit_chat_container_print_history(WebKitChatContainer *view,
                                    lrc::api::ConversationModel& conversation_model,
                                    const std::map<uint64_t, lrc::api::interaction::Info> interactions)
{
    auto interactions_str = interactions_to_json_array_object(conversation_model, interactions).toUtf8();
    gchar* function_call = g_strdup_printf("printHistory(%s)", interactions_str.constData());
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}

void
webkit_chat_container_set_invitation(WebKitChatContainer *view, bool show,
                                     const std::string& contactUri, const std::string& contactId)
{
    // TODO better escape names
    gchar* function_call = g_strdup_printf(show ? "showInvitation(\"%s\", \"%s\")" : "showInvitation()", contactUri.c_str(), contactId.c_str());
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}

void
webkit_chat_container_set_sender_image(WebKitChatContainer *view, const std::string& sender, const std::string& senderImage)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);

    QJsonObject set_sender_image_object = QJsonObject();
    set_sender_image_object.insert("sender_contact_method", QJsonValue(QString(sender.c_str())));
    set_sender_image_object.insert("sender_image", QJsonValue(QString(senderImage.c_str())));

    auto set_sender_image_object_string = QString(QJsonDocument(set_sender_image_object).toJson(QJsonDocument::Compact));

    gchar* function_call = g_strdup_printf("setSenderImage(%s);", set_sender_image_object_string.toUtf8().constData());
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(priv->webview_chat), function_call, NULL, NULL, NULL);
    g_free(function_call);
}

gboolean
webkit_chat_container_is_ready(WebKitChatContainer *view)
{
    WebKitChatContainerPrivate *priv = WEBKIT_CHAT_CONTAINER_GET_PRIVATE(view);
    return priv->js_libs_loaded;
}

void
webkit_chat_set_header_visible(WebKitChatContainer *view, bool isVisible)
{
    gchar* function_call = g_strdup_printf("displayNavbar(%s)", isVisible ? "true" : "false");
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}

void
webkit_chat_set_record_visible(WebKitChatContainer *view, bool isVisible)
{
    gchar* function_call = g_strdup_printf("displayRecordControls(%s)", isVisible ? "true" : "false");
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}

void
webkit_chat_set_dark_mode(WebKitChatContainer *view, bool darkMode, const std::string& background)
{
    std::string theme = "";
    if (darkMode) {
        theme = "\
            --jami-light-blue: #003b4e;\
            --jami-dark-blue: #28b1ed;\
            --text-color: white;\
            --timestamp-color: #bbb;\
            --message-out-bg: #28b1ed;\
            --message-out-txt: white;\
            --message-in-bg: #616161;\
            --message-in-txt: white;\
            --file-in-timestamp-color: #999;\
            --file-out-timestamp-color: #eee;\
            --bg-color: " + background + ";\
            --non-action-icon-color: white;\
            --placeholder-text-color: #2b2b2b;\
            --invite-hover-color: black;\
            --hairline-color: #262626;\
        ";
    }
    gchar* function_call = g_strdup_printf("setTheme(\"%s\")", theme.c_str());
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}

void
webkit_chat_set_is_composing(WebKitChatContainer *view, const std::string& contactUri, bool isComposing)
{
    gchar* function_call = g_strdup_printf("showTypingIndicator(\"%s\", %s)", contactUri.c_str(), isComposing ? "1" : "0");
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}

void
webkit_chat_update_chatview_frame(WebKitChatContainer *view, bool accountEnabled, bool isBanned, bool isTemporary, const gchar* alias, const gchar* bestId)
{
    gchar* function_call = g_strdup_printf("update_chatview_frame(%s, %s, %s, \"%s\", \"%s\")",
                                           accountEnabled ? "true" : "false",
                                           isBanned ? "true" : "false", isTemporary ? "true" : "false", alias, bestId);
    webkit_chat_container_execute_js(view, function_call);
    g_free(function_call);
}
