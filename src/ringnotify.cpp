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
    NotifyNotification *notification = notify_notification_new("Incoming call", body, NULL);
    g_free(body);

    /* get photo */
    QVariant var_p = PixbufDelegate::instance()->callPhoto(
        call->peerContactMethod(), QSize(50, 50), false);
    std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
    notify_notification_set_image_from_pixbuf(notification, photo.get());

    /* calls have highest urgency */
    notify_notification_set_urgency(notification, NOTIFY_URGENCY_CRITICAL);
    notify_notification_set_timeout(notification, NOTIFY_EXPIRES_DEFAULT);

    /* set the notification pointer to NULL once it is destroyed so that we don't
     * try to use the pointer elsewhere */
    g_object_add_weak_pointer(G_OBJECT(notification), (gpointer *)&notification);

    GError *error = NULL;
    success = notify_notification_show(notification, &error);

    if (success) {
        /* unref the notification once its closed */
        g_signal_connect(notification, "closed", G_CALLBACK(g_object_unref), NULL);
        /* monitor the life cycle of the call and try to close the notification
         * once the call has been aswered */
        QObject::connect(
            call,
            &Call::lifeCycleStateChanged,
            [notification] (Call::LifeCycleState newState, G_GNUC_UNUSED Call::LifeCycleState previousState)
            {
                if (!notification) return;
                g_return_if_fail(NOTIFY_IS_NOTIFICATION(notification));
                if (newState > Call::LifeCycleState::INITIALIZATION) {
                    if (!notify_notification_close(notification, NULL))
                        g_warning("could not close notification");
                    /* note: not all systems will actually close the notification
                     * even if the above function returns as true */
                }
            }
        );

    } else {
        g_warning("failed to show notification: %s", error->message);
        g_clear_error(&error);
        g_object_unref(notification);
    }
#endif
    return success;
}

#if USE_LIBNOTIFY
static gboolean
ring_notify_message_recieved(NotifyNotification **notify_ptr, Call *call, const QString& msg)
{
    gboolean success = FALSE;
    g_return_val_if_fail(notify_ptr && call, FALSE);

    gchar *title = g_strdup_printf("%s says:", call->formattedName().toUtf8().constData());
    gchar *body = g_strdup_printf("%s", msg.toUtf8().constData());

    NotifyNotification *notification = *notify_ptr;
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
        /* create new notification object */
        notification = notify_notification_new(title, body, NULL);
        *notify_ptr = notification;

        /* get photo */
        QVariant var_p = PixbufDelegate::instance()->callPhoto(
            call->peerContactMethod(), QSize(50, 50), false);
        std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
        notify_notification_set_image_from_pixbuf(notification, photo.get());

        /* normal priority for messages */
        notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);

        /* set the notification pointer to NULL once it is destroyed so that we don't
         * try to use the pointer elsewhere */
        g_object_add_weak_pointer(G_OBJECT(notification), (gpointer *)notify_ptr);

        /* unref the notification once its closed */
        g_signal_connect_swapped(notification, "closed", G_CALLBACK(g_clear_object), notify_ptr);
    }

    g_free(title);
    g_free(body);

    GError *error = NULL;
    success = notify_notification_show(notification, &error);
    if (!success) {
        g_warning("failed to show notification: %s", error->message);
        g_clear_error(&error);
        g_clear_object(notify_ptr);
    }

    return success;
}

static void
ring_notify_call_messages(NotifyNotification **notify_ptr, Call *call, Media::Text *media, GtkWindow **win)
{
    QObject::connect(
        media,
        &Media::Text::messageReceived,
        [notify_ptr, call, win] (const QString& message) {
            g_return_if_fail(notify_ptr && call && win);
            if (*win && gtk_window_is_active(*win)) {
                /* only notify about messages not in the currently selected call */
                if (CallModel::instance()->selectedCall() != call) {
                    ring_notify_message_recieved(notify_ptr, call, message);
                }
            } else {
                /* send notifications for all messages if the window is not visibles*/
                ring_notify_message_recieved(notify_ptr, call, message);
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
    GtkWindow **win)
{
#if USE_LIBNOTIFY
    QObject::connect(
        CallModel::instance(),
        &QAbstractItemModel::rowsInserted,
        [win] (const QModelIndex &parent, int first, int last)
        {
            for (int row = first; row <= last; ++row) {
                QModelIndex idx = CallModel::instance()->index(row, 0, parent);
                auto call = CallModel::instance()->getCall(idx);
                if (call) {
                    /* For each call, we want to use the same Notification pointer;
                     * if the notification is null (expired or closed), we create
                     * a new one; otherwise we update the same one. This way, if
                     * we receive multiple consecutive messages in the same call
                     * we do not create a new notification bubble for each, but
                     * instead update the same one */
                    NotifyNotification **notify_ptr;
                    notify_ptr = (NotifyNotification **)g_malloc0(sizeof(NotifyNotification *));

                    /* check if text media is already present */
                    if (call->hasMedia(Media::Media::Type::TEXT, Media::Media::Direction::IN)) {
                        Media::Text *media = call->firstMedia<Media::Text>(Media::Media::Direction::IN);
                        ring_notify_call_messages(notify_ptr, call, (Media::Text *)media, win);
                    } else if (call->hasMedia(Media::Media::Type::TEXT, Media::Media::Direction::OUT)) {
                        Media::Text *media = call->firstMedia<Media::Text>(Media::Media::Direction::OUT);
                        ring_notify_call_messages(notify_ptr, call, (Media::Text *)media, win);
                    } else {
                        /* monitor media for messaging text messaging */
                        QObject::connect(
                            call,
                            &Call::mediaAdded,
                            [notify_ptr, win, call] (Media::Media* media) {
                                if (media->type() == Media::Media::Type::TEXT) {
                                    ring_notify_call_messages(notify_ptr, call, (Media::Text *)media, win);
                                }
                            }
                        );
                    }

                    /* clear notification pointer when call is over */
                    QObject::connect(
                        call,
                        &QObject::destroyed,
                        [&notify_ptr] (G_GNUC_UNUSED QObject *obj) {
                            /* if notification object is still alive; unref it */
                            if (*notify_ptr)
                                g_object_unref(*notify_ptr);
                            g_free(notify_ptr);
                            notify_ptr = NULL;
                        }
                    );
                }
            }
        }
     );
#endif
 }
