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
#endif

void
ring_notify_init()
{
#if USE_LIBNOTIFY
    notify_init("Ring");
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

    gchar *body = g_strdup_printf("%s", call->formattedName().toUtf8().constData());
    std::shared_ptr<NotifyNotification> notification(
        notify_notification_new(_("Incoming call"), body, NULL), g_object_unref);
    g_free(body);

    /* get photo */
    QVariant var_p = GlobalInstances::pixmapManipulator().callPhoto(
        call->peerContactMethod(), QSize(50, 50), false);
    std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
    notify_notification_set_image_from_pixbuf(notification.get(), photo.get());

    /* calls have highest urgency */
    notify_notification_set_urgency(notification.get(), NOTIFY_URGENCY_CRITICAL);
    notify_notification_set_timeout(notification.get(), NOTIFY_EXPIRES_DEFAULT);

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

GHashTable *
ring_notify_get_chat_table()
{
    static std::unique_ptr<GHashTable, decltype(g_hash_table_destroy)&> chat_table(
        nullptr, g_hash_table_destroy);

    if (chat_table.get() == nullptr)
        chat_table.reset(g_hash_table_new_full(NULL, NULL, NULL, g_object_unref));

    return chat_table.get();
}

static void
notification_closed(NotifyNotification *notification, ContactMethod *cm)
{
    g_return_if_fail(cm);

    if (!g_hash_table_remove(ring_notify_get_chat_table(), cm)) {
        g_warning("could not find notification associated with the given ContactMethod");
        /* normally removing the notification from the hash table will unref it,
         * but if it was not found we should do it here */
        g_object_unref(notification);
    }
}

static gboolean
ring_notify_show_text_message(ContactMethod *cm, const QModelIndex& idx)
{
    g_return_val_if_fail(idx.isValid() && cm, FALSE);
    gboolean success = FALSE;

    g_debug("showing text message notification");

    GHashTable *chat_table = ring_notify_get_chat_table();

    auto title = g_strdup_printf(C_("Text message notification", "%s says:"), idx.data(static_cast<int>(Ring::Role::Name)).toString().toUtf8().constData());
    auto body = g_strdup_printf("%s", idx.data(Qt::DisplayRole).toString().toUtf8().constData());

    /* check if a notification already exists for this CM */
    NotifyNotification *notification = (NotifyNotification *)g_hash_table_lookup(chat_table, cm);
    if (notification) {
        /* update notification; append the new message to the old */
        GValue body_value = G_VALUE_INIT;
        g_value_init(&body_value, G_TYPE_STRING);
        g_object_get_property(G_OBJECT(notification), "body", &body_value);
        const gchar* body_old = g_value_get_string(&body_value);
        if (body_old && (strlen(body_old) > 0)) {
            gchar *body_new = g_strconcat(body_old, "\n", body, NULL);
            g_free(body);
            body = body_new;
        }
        notify_notification_update(notification, title, body, NULL);
    } else {
        /* create new notification object and associate it with the CM in the
         * hash table; also store the pointer of the CM in the notification
         * object so that it knows it's key in the hash table */
        notification = notify_notification_new(title, body, NULL);
        g_hash_table_insert(chat_table, cm, notification);
        g_object_set_data(G_OBJECT(notification), "ContactMethod", cm);

        /* get photo */
        QVariant var_p = GlobalInstances::pixmapManipulator().callPhoto(
            cm, QSize(50, 50), false);
        std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
        notify_notification_set_image_from_pixbuf(notification, photo.get());

        /* normal priority for messages */
        notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);

        /* remove the key and value from the hash table once the notification is
         * closed; note that this will also unref the notification */
        g_signal_connect(notification, "closed", G_CALLBACK(notification_closed), cm);
    }

    g_free(title);
    g_free(body);

    GError *error = NULL;
    success = notify_notification_show(notification, &error);
    if (!success) {
        g_warning("failed to show notification: %s", error->message);
        g_clear_error(&error);
        g_hash_table_remove(chat_table, cm);
    }

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

static void
ring_notify_message(ContactMethod *cm, Media::TextRecording *t, RingClient *client)
{
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
}
#endif

void
ring_notify_monitor_chat_notifications(
#if !USE_LIBNOTIFY
    G_GNUC_UNUSED
#endif
    RingClient *client)
{
#if USE_LIBNOTIFY

    QObject::connect(
        &Media::RecordingModel::instance(),
        &Media::RecordingModel::newTextMessage,
        [client] (Media::TextRecording* t, ContactMethod* cm)
        {
            ring_notify_message(cm, t, client);
        }
     );
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


    GHashTable *chat_table = ring_notify_get_chat_table();

    NotifyNotification *notification = (NotifyNotification *)g_hash_table_lookup(chat_table, cm);

    if (notification) {
        notification_existed = TRUE;

        GError *error = NULL;
        if (!notify_notification_close(notification, &error)) {
            g_warning("could not close notification: %s", error->message);
            g_clear_error(&error);

            /* closing should remove and free the notification from the hash table
             * since it failed to close, try to remove the notification from the
             * table manually */
            g_hash_table_remove(chat_table, cm);
        }
    }
#endif

    return notification_existed;
}
