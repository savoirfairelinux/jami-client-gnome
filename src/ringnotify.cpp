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
        g_warning("failed to send notification: %s", error->message);
        g_clear_error(&error);
    }
#endif
    return success;
}
