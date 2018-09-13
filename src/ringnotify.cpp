/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Sebastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "ringnotify.h"
#include "config.h"
#include "ring_client.h"

#if USE_CANBERRA
#include <canberra-gtk.h>
#endif // USE_CANBERRA

#if USE_LIBNOTIFY
#include <glib/gi18n.h>
#include <libnotify/notify.h>
#include <memory>
#include <globalinstances.h>
#include "native/pixbufmanipulator.h"
#include <call.h>
#include <QtCore/QSize>
#include <media/text.h>
#include <callmodel.h>
#include <media/textrecording.h>
#include <media/recordingmodel.h>
#include <recentmodel.h>
#endif


static constexpr const char* SERVER_NOTIFY_OSD = "notify-osd";
static constexpr const char* NOTIFICATION_FILE = SOUNDSDIR "/ringtone_notify.wav";

namespace details
{
class CppImpl;
}

struct _RingNotifier
{
    GtkBox parent;
};

struct _RingNotifierClass
{
    GtkBoxClass parent_class;
};

typedef struct _RingNotifierPrivate RingNotifierPrivate;

struct _RingNotifierPrivate
{
    details::CppImpl* cpp; ///< Non-UI and C++ only code0
};

G_DEFINE_TYPE_WITH_PRIVATE(RingNotifier, ring_notifier, GTK_TYPE_BOX);

#define RING_NOTIFIER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_NOTIFIER_TYPE, RingNotifierPrivate))

/* signals */
enum {
    SHOW_CHAT,
    ACCEPT_PENDING,
    REFUSE_PENDING,
    ACCEPT_CALL,
    DECLINE_CALL,
    LAST_SIGNAL
};

static guint ring_notifier_signals[LAST_SIGNAL] = { 0 };

namespace details
{

class CppImpl
{
public:
    explicit CppImpl(RingNotifier& widget);
    ~CppImpl();

    RingNotifier* self = nullptr; // The GTK widget itself
    RingNotifierPrivate* priv = nullptr;

    /* server info and capabilities */
    char *name = nullptr;
    char *vendor = nullptr;
    char *version = nullptr;
    char *spec = nullptr;
    gboolean append;
    gboolean actions;

    std::map<std::string, std::shared_ptr<NotifyNotification>> notifications_;
private:
    CppImpl() = delete;
    CppImpl(const CppImpl&) = delete;
    CppImpl& operator=(const CppImpl&) = delete;
};

CppImpl::CppImpl(RingNotifier& widget)
: self {&widget}
{
}

CppImpl::~CppImpl()
{
    if (name)
        g_free(name);
    if (vendor)
        g_free(vendor);
    if (version)
        g_free(version);
    if (spec)
        g_free(spec);
}

} // namespace details

static void
ring_notifier_dispose(GObject *object)
{
    auto* self = RING_NOTIFIER(object);
    auto* priv = RING_NOTIFIER_GET_PRIVATE(self);

    delete priv->cpp;
    priv->cpp = nullptr;

#if USE_LIBNOTIFY
    if (notify_is_initted())
        notify_uninit();
#endif

    G_OBJECT_CLASS(ring_notifier_parent_class)->dispose(object);
}

static void
ring_notifier_init(RingNotifier *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    RingNotifierPrivate *priv = RING_NOTIFIER_GET_PRIVATE(view);
    priv->cpp = new details::CppImpl {*view};

#if USE_LIBNOTIFY
    notify_init("Ring");

    /* get notify server info */
    if (notify_get_server_info(&priv->cpp->name,
                               &priv->cpp->vendor,
                               &priv->cpp->version,
                               &priv->cpp->spec)) {
        g_debug("notify server name: %s, vendor: %s, version: %s, spec: %s",
                priv->cpp->name, priv->cpp->vendor, priv->cpp->version, priv->cpp->spec);
    }

    /* check  notify server capabilities */
    auto list = notify_get_server_caps();
    while (list) {
        if (g_strcmp0((const char *)list->data, "append") == 0 ||
            g_strcmp0((const char *)list->data, "x-canonical-append") == 0) {
            priv->cpp->append = TRUE;
        }
        if (g_strcmp0((const char *)list->data, "actions") == 0) {
            priv->cpp->actions = TRUE;
        }

        list = g_list_next(list);
    }

    g_list_free_full(list, g_free);
#endif
}

static void
ring_notifier_class_init(RingNotifierClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = ring_notifier_dispose;

    ring_notifier_signals[SHOW_CHAT] = g_signal_new(
        "showChatView",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);

    ring_notifier_signals[ACCEPT_PENDING] = g_signal_new(
        "acceptPending",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);

    ring_notifier_signals[REFUSE_PENDING] = g_signal_new(
        "refusePending",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);

    ring_notifier_signals[ACCEPT_CALL] = g_signal_new(
        "acceptCall",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);

    ring_notifier_signals[DECLINE_CALL] = g_signal_new(
        "declineCall",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING);
}

GtkWidget *
ring_notifier_new()
{
    gpointer view = g_object_new(RING_NOTIFIER_TYPE, NULL);
    return (GtkWidget *)view;
}

#if USE_LIBNOTIFY
static void
show_chat_view(NotifyNotification*, char* id, RingNotifier* view)
{
    g_signal_emit(G_OBJECT(view), ring_notifier_signals[SHOW_CHAT], 0, id);
}

static void
accept_pending(NotifyNotification*, char* id, RingNotifier* view)
{
    std::string newId = id;
    g_signal_emit(G_OBJECT(view), ring_notifier_signals[ACCEPT_PENDING], 0, newId.substr(std::string("add:").length()).c_str());
}

static void
refuse_pending(NotifyNotification*, char* id, RingNotifier* view)
{
    std::string newId = id;
    g_signal_emit(G_OBJECT(view), ring_notifier_signals[REFUSE_PENDING], 0, newId.substr(std::string("rm:").length()).c_str());
}

static void
accept_call(NotifyNotification*, char* id, RingNotifier* view)
{
    std::string newId = id;
    g_signal_emit(G_OBJECT(view), ring_notifier_signals[ACCEPT_CALL], 0, newId.substr(std::string("accept:").length()).c_str());
}

static void
decline_call(NotifyNotification*, char* id, RingNotifier* view)
{
    std::string newId = id;
    g_signal_emit(G_OBJECT(view), ring_notifier_signals[DECLINE_CALL], 0, newId.substr(std::string("decline:").length()).c_str());
}

#endif

gboolean
ring_show_notification(RingNotifier* view, const std::string& icon,
                       const std::string& uri, const std::string& name,
                       const std::string& id, const std::string& title,
                       const std::string& body, NotificationType type)
{
    g_return_val_if_fail(IS_RING_NOTIFIER(view), false);
    gboolean success = FALSE;
    RingNotifierPrivate *priv = RING_NOTIFIER_GET_PRIVATE(view);

#if USE_LIBNOTIFY
    std::shared_ptr<NotifyNotification> notification(
        notify_notification_new(title.c_str(), body.c_str(), nullptr), g_object_unref);
    priv->cpp->notifications_.emplace(id, notification);

    // Draw icon
    auto firstLetter = name.empty() ? "" : QString(QString(name.c_str()).at(0)).toStdString();  // NOTE best way to be compatible with UTF-8
    auto default_avatar = Interfaces::PixbufManipulator().generateAvatar(firstLetter, uri);
    auto photo = Interfaces::PixbufManipulator().scaleAndFrame(default_avatar.get(), QSize(50, 50));
    if (!icon.empty()) {
        QByteArray byteArray(icon.c_str(), icon.length());
        QVariant avatar = Interfaces::PixbufManipulator().personPhoto(byteArray);
        auto pixbuf_photo = Interfaces::PixbufManipulator().scaleAndFrame(avatar.value<std::shared_ptr<GdkPixbuf>>().get(), QSize(50, 50));
        if (avatar.isValid()) {
            photo = pixbuf_photo;
        }
    }
    notify_notification_set_image_from_pixbuf(notification.get(), photo.get());

    if (type != NotificationType::CHAT) {
        notify_notification_set_urgency(notification.get(), NOTIFY_URGENCY_CRITICAL);
        notify_notification_set_timeout(notification.get(), NOTIFY_EXPIRES_DEFAULT);
    } else {
        notify_notification_set_urgency(notification.get(), NOTIFY_URGENCY_NORMAL);
    }

#if USE_CANBERRA
    if (type != NotificationType::CALL) {
        auto status = ca_context_play(ca_gtk_context_get(),
                                      0,
                                      CA_PROP_MEDIA_FILENAME,
                                      NOTIFICATION_FILE,
                                      nullptr);
        if (status != 0)
            g_warning("ca_context_play: %s", ca_strerror(status));
    }
#endif // USE_CANBERRA

    // if the notification server supports actions, make the default action to show the chat view
    if (priv->cpp->actions) {
        if (type != NotificationType::CALL) {
            notify_notification_add_action(notification.get(),
                id.c_str(),
                C_("", "Open conversation"),
                (NotifyActionCallback)show_chat_view,
                view,
                nullptr);
            if (type != NotificationType::CHAT) {
                auto addId = "add:" + id;
                notify_notification_add_action(notification.get(),
                    addId.c_str(),
                    C_("", "Accept"),
                    (NotifyActionCallback)accept_pending,
                    view,
                    nullptr);
                auto rmId = "rm:" + id;
                notify_notification_add_action(notification.get(),
                    rmId.c_str(),
                    C_("", "Refuse"),
                    (NotifyActionCallback)refuse_pending,
                    view,
                    nullptr);
            }
        } else {
            auto acceptId = "accept:" + id;
            notify_notification_add_action(notification.get(),
                acceptId.c_str(),
                C_("", "Accept"),
                (NotifyActionCallback)accept_call,
                view,
                nullptr);
            auto declineId = "decline:" + id;
            notify_notification_add_action(notification.get(),
                declineId.c_str(),
                C_("", "Decline"),
                (NotifyActionCallback)decline_call,
                view,
                nullptr);
        }
    }

    GError *error = nullptr;
    success = notify_notification_show(notification.get(), &error);

    if (error) {
        g_warning("failed to show notification: %s", error->message);
        g_clear_error(&error);
    }
#endif
    return success;
}

gboolean
ring_hide_notification(RingNotifier* view, const std::string& id)
{
    g_return_val_if_fail(IS_RING_NOTIFIER(view), false);
    RingNotifierPrivate *priv = RING_NOTIFIER_GET_PRIVATE(view);

#if USE_LIBNOTIFY
    // Search
    auto notification = priv->cpp->notifications_.find(id);
    if (notification == priv->cpp->notifications_.end()) {
        return FALSE;
    }

    // Close
    GError *error = nullptr;
    if (!notify_notification_close(notification->second.get(), &error)) {
        g_warning("could not close notification: %s", error->message);
        g_clear_error(&error);
        return FALSE;
    }

    // Erase
    priv->cpp->notifications_.erase(id);
#endif

    return TRUE;
}
