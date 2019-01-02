/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
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

#ifndef RING_NOTIFY_H_
#define RING_NOTIFY_H_

#include <gtk/gtk.h>
#include <string>

G_BEGIN_DECLS
#define RING_NOTIFIER_TYPE            (ring_notifier_get_type ())
#define RING_NOTIFIER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RING_NOTIFIER_TYPE, RingNotifier))
#define RING_NOTIFIER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), RING_NOTIFIER_TYPE, RingNotifierClass))
#define IS_RING_NOTIFIER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_NOTIFIER_TYPE))
#define IS_RING_NOTIFIER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RING_NOTIFIER_TYPE))

typedef struct _RingNotifier      RingNotifier;
typedef struct _RingNotifierClass RingNotifierClass;

enum class NotificationType
{
    CALL,
    REQUEST,
    CHAT
};

GType      ring_notifier_get_type             (void) G_GNUC_CONST;
GtkWidget* ring_notifier_new                  (void);

gboolean    ring_show_notification(RingNotifier* view,
                                   const std::string& icon,
                                   const std::string& uri,
                                   const std::string& name,
                                   const std::string& id,
                                   const std::string& title,
                                   const std::string& body,
                                   NotificationType type);
gboolean    ring_hide_notification(RingNotifier* view, const std::string& id);

G_END_DECLS

#endif /* RING_NOTIFY_H_ */
