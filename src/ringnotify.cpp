/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "ringnotify.h"
#include "config.h"

#if USE_LIBNOTIFY
#include <libnotify/notify.h>
#include <memory>
#include "delegates/pixbufdelegate.h"
#include <call.h>
#include <QtCore/QSize>
#include <media/text.h>
#include <callmodel.h>
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
        notify_notification_new("Incoming call", body, NULL), g_object_unref);
    g_free(body);

    /* get photo */
    QVariant var_p = PixbufDelegate::instance()->callPhoto(
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
notification_closed(NotifyNotification *notification, Call *call)
{
    g_return_if_fail(call);

    if (!g_hash_table_remove(ring_notify_get_chat_table(), call)) {
        g_warning("could not find notification associated with the given call");
        /* normally removing the notification from the hash table will unref it,
         * but if it was not found we should do it here */
        g_object_unref(notification);
    }
}

static gboolean
ring_notify_message_recieved(Call *call, const QMap<QString,QString>& msg)
{
    g_return_val_if_fail(call, FALSE);
    gboolean success = FALSE;

    GHashTable *chat_table = ring_notify_get_chat_table();

    gchar *title = g_strdup_printf("%s says:", call->formattedName().toUtf8().constData());
    gchar *body = g_strdup_printf("%s", msg["text/plain"].toUtf8().constData());

    /* check if a notification already exists for this call */
    NotifyNotification *notification = (NotifyNotification *)g_hash_table_lookup(chat_table, call);
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
        /* create new notification object and associate it with the call in the
         * hash table; also store the pointer of the call in the notification
         * object so that it knows it's key in the hash table */
        notification = notify_notification_new(title, body, NULL);
        g_hash_table_insert(chat_table, call, notification);
        g_object_set_data(G_OBJECT(notification), "call", call);

        /* get photo */
        QVariant var_p = PixbufDelegate::instance()->callPhoto(
            call->peerContactMethod(), QSize(50, 50), false);
        std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
        notify_notification_set_image_from_pixbuf(notification, photo.get());

        /* normal priority for messages */
        notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);

        /* remove the key and value from the hash table once the notification is
         * closed; note that this will also unref the notification */
        g_signal_connect(notification, "closed", G_CALLBACK(notification_closed), call);
    }

    g_free(title);
    g_free(body);

    GError *error = NULL;
    success = notify_notification_show(notification, &error);
    if (!success) {
        g_warning("failed to show notification: %s", error->message);
        g_clear_error(&error);
        g_hash_table_remove(chat_table, call);
    }

    return success;
}

static void
ring_notify_call_messages(Call *call, Media::Text *media, RingClient *client)
{
    QObject::connect(
        media,
        &Media::Text::messageReceived,
        [call, client] (const QMap<QString,QString>& message) {
            g_return_if_fail(call && client);
            GtkWindow *main_window = ring_client_get_main_windw(client);
            if ( main_window && gtk_window_is_active(main_window)) {
                /* only notify about messages not in the currently selected call */
                if (CallModel::instance()->selectedCall() != call) {
                    ring_notify_message_recieved(call, message);
                }
            } else {
                /* send notifications for all messages if the window is not visibles*/
                ring_notify_message_recieved(call, message);
            }
        }
    );
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
        CallModel::instance(),
        &QAbstractItemModel::rowsInserted,
        [client] (const QModelIndex &parent, int first, int last)
        {
            g_return_if_fail(client);

            for (int row = first; row <= last; ++row) {
                QModelIndex idx = CallModel::instance()->index(row, 0, parent);
                auto call = CallModel::instance()->getCall(idx);
                if (call) {

                    /* check if text media is already present */
                    if (call->hasMedia(Media::Media::Type::TEXT, Media::Media::Direction::IN)) {
                        Media::Text *media = call->firstMedia<Media::Text>(Media::Media::Direction::IN);
                        ring_notify_call_messages(call, media, client);
                    } else if (call->hasMedia(Media::Media::Type::TEXT, Media::Media::Direction::OUT)) {
                        Media::Text *media = call->firstMedia<Media::Text>(Media::Media::Direction::OUT);
                        ring_notify_call_messages(call, media, client);
                    } else {
                        /* monitor media for messaging text messaging */
                        QObject::connect(
                            call,
                            &Call::mediaAdded,
                            [call, client] (Media::Media* media) {
                                if (media->type() == Media::Media::Type::TEXT)
                                    ring_notify_call_messages(call, (Media::Text *)media, client);
                            }
                        );
                    }
                }
            }
        }
     );
#endif
 }

gboolean
ring_notify_close_chat_notification(
#if !USE_LIBNOTIFY
    G_GNUC_UNUSED
#endif
    Call *call)
{
    gboolean notification_existed = FALSE;

#if USE_LIBNOTIFY
    /* checks if there exists a chat notification associated with the given call
     * and tries to close it; if it did exist, then the function returns TRUE */
    g_return_val_if_fail(call, FALSE);


    GHashTable *chat_table = ring_notify_get_chat_table();

    NotifyNotification *notification = (NotifyNotification *)g_hash_table_lookup(chat_table, call);

    if (notification) {
        notification_existed = TRUE;

        GError *error = NULL;
        if (!notify_notification_close(notification, &error)) {
            g_warning("could not close notification: %s", error->message);
            g_clear_error(&error);

            /* closing should remove and free the notification from the hash table
             * since it failed to close, try to remove the notification from the
             * table manually */
            g_hash_table_remove(chat_table, call);
        }
    }
#endif

    return notification_existed;
}
