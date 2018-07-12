/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
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

#if USE_LIBNOTIFY

static constexpr int MAX_NOTIFICATIONS = 10; // max unread chat msgs to display from the same contact
static constexpr const char* SERVER_NOTIFY_OSD = "notify-osd";
static constexpr const char* NOTIFICATION_FILE = SOUNDSDIR "/ringtone_notify.wav";

/* struct to store the parsed list of the notify server capabilities */
struct RingNotifyServerInfo
{
    /* info */
    char *name;
    char *vendor;
    char *version;
    char *spec;

    /* capabilities */
    gboolean append;
    gboolean actions;

    /* the info strings must be freed */
    ~RingNotifyServerInfo() {
        g_free(name);
        g_free(vendor);
        g_free(version);
        g_free(spec);
    }
};

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
    RingNotifyServerInfo serverInfo;

    details::CppImpl* cpp; ///< Non-UI and C++ only code0
};

G_DEFINE_TYPE_WITH_PRIVATE(RingNotifier, ring_notifier, GTK_TYPE_BOX);

#define RING_NOTIFIER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_NOTIFIER_TYPE, RingNotifierPrivate))

/* signals */
enum {
    SHOW_CONVERSATION,
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
    RingNotifierPrivate* priv = nullptr; // The GTK widget itself

    std::map<std::string, std::shared_ptr<NotifyNotification>> notifications_;
private:
    CppImpl() = delete;
    CppImpl(const CppImpl&) = delete;
    CppImpl& operator=(const CppImpl&) = delete;

};

CppImpl::CppImpl(RingNotifier& widget)
    : self {&widget}
{}

CppImpl::~CppImpl()
{}

}

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
    if (notify_get_server_info(&(priv->serverInfo).name,
                               &(priv->serverInfo).vendor,
                               &(priv->serverInfo).version,
                               &(priv->serverInfo).spec)) {
        g_debug("notify server name: %s, vendor: %s, version: %s, spec: %s",
                priv->serverInfo.name, priv->serverInfo.vendor, priv->serverInfo.version, priv->serverInfo.spec);
    }

    /* check  notify server capabilities */
    auto list = notify_get_server_caps();
    while (list) {
        if (g_strcmp0((const char *)list->data, "append") == 0 ||
            g_strcmp0((const char *)list->data, "x-canonical-append") == 0) {
            priv->serverInfo.append = TRUE;
        }
        if (g_strcmp0((const char *)list->data, "actions") == 0) {
            priv->serverInfo.actions = TRUE;
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
}

GtkWidget *
ring_notifier_new()
{
    gpointer view = g_object_new(RING_NOTIFIER_TYPE, NULL);
    return (GtkWidget *)view;
}

gboolean
ring_show_notification(RingNotifier* view, const std::string& id, const std::string& icon, const std::string& title, const std::string& body, bool urgent, bool playSound)
{
    g_return_val_if_fail(IS_RING_NOTIFIER(view), false);
    gboolean success = FALSE;
    RingNotifierPrivate *priv = RING_NOTIFIER_GET_PRIVATE(view);

#if USE_LIBNOTIFY
    std::shared_ptr<NotifyNotification> notification(
        notify_notification_new(title.c_str(), body.c_str(), nullptr), g_object_unref);
    priv->cpp->notifications_.emplace(id, notification);

    // Draw icon
    auto default_avatar = Interfaces::PixbufManipulator().generateAvatar("", "");
    auto default_scaled = Interfaces::PixbufManipulator().scaleAndFrame(default_avatar.get(), QSize(50, 50));
    auto photo = default_scaled;
    if (!icon.empty()) {
        QByteArray byteArray(icon.c_str(), icon.length());
        QVariant avatar = Interfaces::PixbufManipulator().personPhoto(byteArray);
        auto pixbuf_photo = Interfaces::PixbufManipulator().scaleAndFrame(avatar.value<std::shared_ptr<GdkPixbuf>>().get(), QSize(50, 50));
        if (avatar.isValid()) {
            photo = pixbuf_photo;
        }
    }
    notify_notification_set_image_from_pixbuf(notification.get(), photo.get());

    if (urgent) {
        notify_notification_set_urgency(notification.get(), NOTIFY_URGENCY_CRITICAL);
        notify_notification_set_timeout(notification.get(), NOTIFY_EXPIRES_DEFAULT);
    } else {
        notify_notification_set_urgency(notification.get(), NOTIFY_URGENCY_NORMAL);
    }

#if USE_CANBERRA
    if (playSound) {
        auto status = ca_context_play(ca_gtk_context_get(),
                                      0,
                                      CA_PROP_MEDIA_FILENAME,
                                      NOTIFICATION_FILE,
                                      nullptr);
        if (status != 0)
            g_warning("ca_context_play: %s", ca_strerror(status));
    }
#endif // USE_CANBERRA

    // TODO (sblin) add actions

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
    gboolean success = FALSE;
    RingNotifierPrivate *priv = RING_NOTIFIER_GET_PRIVATE(view);

#if USE_LIBNOTIFY
    // Search
    auto notification = priv->cpp->notifications_.find(id);
    if (notification == priv->cpp->notifications_.end()) {
        return success;
    }
    // Close
    GError *error = nullptr;
    if (!notify_notification_close(notification->second.get(), &error)) {
        g_warning("could not close notification: %s", error->message);
        g_clear_error(&error);
    }
    // Erase
    priv->cpp->notifications_.erase(id);
#endif
    return success;
}





































static struct RingNotifyServerInfo server_info;
#endif

#if USE_LIBNOTIFY

static gboolean
ring_notify_show_text_message(ContactMethod *cm, const QModelIndex& idx)
{
    g_return_val_if_fail(idx.isValid() && cm, FALSE);
    gboolean success = FALSE;

    /*auto title = g_markup_printf_escaped(C_("Text message notification", "%s says:"), idx.data(static_cast<int>(Ring::Role::Name)).toString().toUtf8().constData());
    auto body = g_markup_escape_text(idx.data(Qt::DisplayRole).toString().toUtf8().constData(), -1);

    NotifyNotification *notification_new = nullptr;
    NotifyNotification *notification_old = nullptr;

    /* try to get the previous notification * /
    auto chat_table = ring_notify_get_chat_table();
    auto list = (GList *)g_hash_table_lookup(chat_table, cm);
    if (list)
        notification_old = (NotifyNotification *)list->data;

    /* we display chat notifications in different ways to suit different notification servers and
     * their capabilities:
     * 1. if the server doesn't support appending (eg: Notification Daemon) then we update the
     *    previous notification (if exists) with new text; otherwise it takes we have many
     *    notifications from the same person... we don't concatinate the old messages because
     *    servers which don't support append usually don't support multi line bodies
     * 2. the notify-osd server supports appending; however it doesn't clear the old notifications
     *    on demand, which means in our case that chat messages which have already been read could
     *    still be displayed when a new notification is appended, thus in this case, we update
     *    the old notification body manually to only contain the unread messages
     * 3. the 3rd case is that the server supports append but is not notify-osd, then we simply use
     *    the append feature
     * /

    if (notification_old && !server_info.append) {
        /* case 1 * /
        notify_notification_update(notification_old, title, body, nullptr);
        notification_new = notification_old;
    } else if (notification_old && g_strcmp0(server_info.name, SERVER_NOTIFY_OSD) == 0) {
        /* case 2 * /
        /* print up to MAX_NOTIFICATIONS unread messages * /
        int msg_count = 0;
        auto idx_next = idx.sibling(idx.row() - 1, idx.column());
        auto read = idx_next.data(static_cast<int>(Media::TextRecording::Role::IsRead)).toBool();
        while (idx_next.isValid() && !read && msg_count < MAX_NOTIFICATIONS) {

            auto body_prev = body;
            body = g_markup_printf_escaped("%s\n%s", body_prev, idx_next.data(Qt::DisplayRole).toString().toUtf8().constData());
            g_free(body_prev);

            idx_next = idx_next.sibling(idx_next.row() - 1, idx_next.column());
            read = idx_next.data(static_cast<int>(Media::TextRecording::Role::IsRead)).toBool();
            ++msg_count;
        }

        notify_notification_update(notification_old, title, body, nullptr);

        notification_new = notification_old;
    } else {
        /* need new notification for case 1, 2, or 3 * /
        notification_new = notify_notification_new(title, body, nullptr);

        /* track in hash table * /
        auto list = (GList *)g_hash_table_lookup(chat_table, cm);
        list = g_list_append(list, notification_new);
        g_hash_table_replace(chat_table, cm, list);

        /* get photo * /
        QVariant var_p = GlobalInstances::pixmapManipulator().callPhoto(
            cm, QSize(50, 50), false);
        std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
        notify_notification_set_image_from_pixbuf(notification_new, photo.get());

        /* normal priority for messages * /
        notify_notification_set_urgency(notification_new, NOTIFY_URGENCY_NORMAL);

        /* if the notification server supports actions, make the default action to show the chat view * /
        if (server_info.actions) {
            notify_notification_add_action(notification_new,
                                           "default",
                                           C_("notification action name", "Show"),
                                           (NotifyActionCallback)ring_notify_show_cm,
                                           cm,
                                           nullptr);
        }
    }

    GError *error = nullptr;
    success = notify_notification_show(notification_new, &error);

#if USE_CANBERRA
    auto status = ca_context_play(ca_gtk_context_get(),
                                  0,
                                  CA_PROP_MEDIA_FILENAME,
                                  NOTIFICATION_FILE,
                                  NULL);
    if (status != 0)
        g_warning("ca_context_play: %s", ca_strerror(status));
#endif // USE_CANBERRA

    if (!success) {
        g_warning("failed to show notification: %s", error->message);
        g_clear_error(&error);
    }

    g_free(title);
    g_free(body);*/

    return success;
}


#endif
