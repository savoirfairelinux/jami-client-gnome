/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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

// LRC
#include <accountmodel.h>

#if USE_LIBNOTIFY

static constexpr int MAX_NOTIFICATIONS = 10; // max unread chat msgs to display from the same contact
static constexpr const char* SERVER_NOTIFY_OSD = "notify-osd";

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

static struct RingNotifyServerInfo server_info;
#endif

void
ring_notify_init()
{
#if USE_LIBNOTIFY
    notify_init("Ring");

    /* get notify server info */
    if (notify_get_server_info(&server_info.name,
                               &server_info.vendor,
                               &server_info.version,
                               &server_info.spec)) {
        g_debug("notify server name: %s, vendor: %s, version: %s, spec: %s",
                server_info.name, server_info.vendor, server_info.version, server_info.spec);
    }

    /* check  notify server capabilities */
    auto list = notify_get_server_caps();
    while (list) {
        if (g_strcmp0((const char *)list->data, "append") == 0 ||
            g_strcmp0((const char *)list->data, "x-canonical-append") == 0) {
            server_info.append = TRUE;
        }
        if (g_strcmp0((const char *)list->data, "actions") == 0) {
            server_info.actions = TRUE;
        }

        list = g_list_next(list);
    }

    g_list_free_full(list, g_free);
#endif
}

void
ring_notify_uninit()
{
#if USE_LIBNOTIFY
    if (notify_is_initted())
        notify_uninit();
#endif
}

gboolean
ring_notify_is_initted()
{
#if USE_LIBNOTIFY
    return notify_is_initted();
#else
    return FALSE;
#endif
}

static void
ring_notify_show_cm(NotifyNotification*, char *, ContactMethod *cm)
{
    /* show the main window in case its hidden */
    if (auto action = g_action_map_lookup_action(G_ACTION_MAP(g_application_get_default()), "show-main-window")) {
        g_action_change_state(action, g_variant_new_boolean(TRUE));
    }
    /* select the relevant cm */
    auto idx = RecentModel::instance().getIndex(cm);
    if (idx.isValid()) {
        RecentModel::instance().selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
    }
}

gboolean
ring_notify_incoming_call(
#if !USE_LIBNOTIFY
    G_GNUC_UNUSED
#endif
    Call* call)
{
    gboolean success = FALSE;
#if USE_LIBNOTIFY
    g_return_val_if_fail(call, FALSE);

    gchar *body = g_markup_escape_text(call->formattedName().toUtf8().constData(), -1);
    std::shared_ptr<NotifyNotification> notification(
        notify_notification_new(_("Incoming call"), body, NULL), g_object_unref);
    g_free(body);

    /* select the revelant account */
    auto cm = call->peerContactMethod();
    if (cm->account() != AccountModel::instance().selectedAccount())
        AccountModel::instance().setSelectedAccount(cm->account());

    /* get photo */
    QVariant var_p = GlobalInstances::pixmapManipulator().callPhoto(
        call->peerContactMethod(), QSize(50, 50), false);
    std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
    notify_notification_set_image_from_pixbuf(notification.get(), photo.get());

    /* calls have highest urgency */
    notify_notification_set_urgency(notification.get(), NOTIFY_URGENCY_CRITICAL);
    notify_notification_set_timeout(notification.get(), NOTIFY_EXPIRES_DEFAULT);

    /* if the notification server supports actions, make the default action to show the call */
    if (server_info.actions) {
        notify_notification_add_action(notification.get(),
                                       "default",
                                       C_("notification action name", "Show"),
                                       (NotifyActionCallback)ring_notify_show_cm,
                                       call->peerContactMethod(),
                                       nullptr);
    }

    GError *error = NULL;
    success = notify_notification_show(notification.get(), &error);

    if (success) {
        /* monitor the life cycle of the call and try to close the notification
         * once the call has been aswered */
        auto state_changed_conn = std::make_shared<QMetaObject::Connection>();
        *state_changed_conn = QObject::connect(
            call,
            &Call::lifeCycleStateChanged,
            [notification, state_changed_conn] (Call::LifeCycleState newState, G_GNUC_UNUSED Call::LifeCycleState previousState)
                {
                    g_return_if_fail(NOTIFY_IS_NOTIFICATION(notification.get()));
                    if (newState > Call::LifeCycleState::INITIALIZATION) {
                        /* note: not all systems will actually close the notification
                         * even if the above function returns as true */
                        if (!notify_notification_close(notification.get(), NULL))
                            g_warning("could not close notification");

                        /* once we (try to) close the notification, we can
                         * disconnect from this signal; this should also destroy
                         * the notification shared_ptr as its ref count will
                         * drop to 0 */
                        QObject::disconnect(*state_changed_conn);
                    }
                }
            );
    } else {
        g_warning("failed to show notification: %s", error->message);
        g_clear_error(&error);
    }
#endif
    return success;
}

#if USE_LIBNOTIFY

static void
ring_notify_free_list(gpointer, GList *value, gpointer)
{
    if (value) {
        g_object_unref(G_OBJECT(value->data));
        g_list_free(value);
    }
}

static void
ring_notify_free_chat_table(GHashTable *table) {
    if (table) {
        g_hash_table_foreach(table, (GHFunc)ring_notify_free_list, nullptr);
        g_hash_table_destroy(table);
    }
}

/**
 * Returns a pointer to a GHashTable which contains key,value pairs where a ContactMethod pointer
 * is the key and a GList of notifications for that CM is the vlue.
 */
GHashTable *
ring_notify_get_chat_table()
{
    static std::unique_ptr<GHashTable, decltype(ring_notify_free_chat_table)&> chat_table(
        nullptr, ring_notify_free_chat_table);

    if (chat_table.get() == nullptr)
        chat_table.reset(g_hash_table_new(NULL, NULL));

    return chat_table.get();
}

static void
notification_closed(NotifyNotification *notification, ContactMethod *cm)
{
    g_return_if_fail(cm);

    /* remove from the list */
    auto chat_table = ring_notify_get_chat_table();
    if (auto list = (GList *)g_hash_table_lookup(chat_table, cm)) {
        list = g_list_remove(list, notification);
        if (list) {
            // the head of the list may have changed
            g_hash_table_replace(chat_table, cm, list);
        } else {
            g_hash_table_remove(chat_table, cm);
        }
    }

    g_object_unref(notification);
}

static gboolean
ring_notify_show_text_message(ContactMethod *cm, const QModelIndex& idx)
{
    g_return_val_if_fail(idx.isValid() && cm, FALSE);
    gboolean success = FALSE;

    auto title = g_markup_printf_escaped(C_("Text message notification", "%s says:"), idx.data(static_cast<int>(Ring::Role::Name)).toString().toUtf8().constData());
    auto body = g_markup_escape_text(idx.data(Qt::DisplayRole).toString().toUtf8().constData(), -1);

    NotifyNotification *notification_new = nullptr;
    NotifyNotification *notification_old = nullptr;

    /* try to get the previous notification */
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
     */

    if (notification_old && !server_info.append) {
        /* case 1 */
        notify_notification_update(notification_old, title, body, nullptr);
        notification_new = notification_old;
    } else if (notification_old && g_strcmp0(server_info.name, SERVER_NOTIFY_OSD) == 0) {
        /* case 2 */
        /* print up to MAX_NOTIFICATIONS unread messages */
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
        /* need new notification for case 1, 2, or 3 */
        notification_new = notify_notification_new(title, body, nullptr);

        /* track in hash table */
        auto list = (GList *)g_hash_table_lookup(chat_table, cm);
        list = g_list_append(list, notification_new);
        g_hash_table_replace(chat_table, cm, list);

        /* get photo */
        QVariant var_p = GlobalInstances::pixmapManipulator().callPhoto(
            cm, QSize(50, 50), false);
        std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
        notify_notification_set_image_from_pixbuf(notification_new, photo.get());

        /* normal priority for messages */
        notify_notification_set_urgency(notification_new, NOTIFY_URGENCY_NORMAL);

        /* remove the key and value from the hash table once the notification is
         * closed; note that this will also unref the notification */
        g_signal_connect(notification_new, "closed", G_CALLBACK(notification_closed), cm);

        /* if the notification server supports actions, make the default action to show the chat view */
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
    if (!success) {
        g_warning("failed to show notification: %s", error->message);
        g_clear_error(&error);
    }

    g_free(title);
    g_free(body);

    return success;
}

static gboolean
show_message_if_unread(const QModelIndex *idx)
{
    g_return_val_if_fail(idx && idx->isValid(), G_SOURCE_REMOVE);

    if (!idx->data(static_cast<int>(Media::TextRecording::Role::IsRead)).toBool()) {
        auto cm = idx->data(static_cast<int>(Media::TextRecording::Role::ContactMethod)).value<ContactMethod *>();
        ring_notify_show_text_message(cm, *idx);
    }

    return G_SOURCE_REMOVE;
}

static void
delete_idx(QModelIndex *idx)
{
    delete idx;
}

#endif

void
ring_notify_message(
#if !USE_LIBNOTIFY
    ContactMethod*, Media::TextRecording*, RingClient*)
#else
    ContactMethod *cm, Media::TextRecording *t, RingClient *client)
#endif
{

#if USE_LIBNOTIFY
    g_return_if_fail(cm && t && client);

    // get the message
    auto model = t->instantMessagingModel();
    auto msg_idx = model->index(model->rowCount()-1, 0);

    // make sure its a text message, or else there is nothing to do
    if (msg_idx.data(static_cast<int>(Media::TextRecording::Role::HasText)).toBool()) {
        auto main_window = ring_client_get_main_window(client);
        if ( main_window && gtk_window_is_active(main_window)) {
            /* in this case we only want to show the notification if the message is not marked as
             * read; this will only possibly be done after the the chatview has displayed it in
             * response to this or another signal; so we must check for the read status after the
             * chat view handler has completed, we do so via a g_idle function.
             */
            auto new_idx = new QModelIndex(msg_idx);
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)show_message_if_unread, new_idx, (GDestroyNotify)delete_idx);
        } else {
            /* always show a notification if the window is not active/visible */
            ring_notify_show_text_message(cm, msg_idx);
        }

    }
#endif
}

gboolean
ring_notify_close_chat_notification(
#if !USE_LIBNOTIFY
    G_GNUC_UNUSED
#endif
    ContactMethod *cm)
{
    gboolean notification_existed = FALSE;

#if USE_LIBNOTIFY
    /* checks if there exists a chat notification associated with the given ContactMethod
     * and tries to close it; if it did exist, then the function returns TRUE */
    g_return_val_if_fail(cm, FALSE);


    auto chat_table = ring_notify_get_chat_table();

    if (auto list = (GList *)g_hash_table_lookup(chat_table, cm)) {
        while (list) {
            notification_existed = TRUE;
            auto notification = (NotifyNotification *)list->data;

            GError *error = NULL;
            if (!notify_notification_close(notification, &error)) {
                g_warning("could not close notification: %s", error->message);
                g_clear_error(&error);
            }

            list = g_list_next(list);
        }
    }

#endif

    return notification_existed;
}
