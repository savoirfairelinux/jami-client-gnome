/*
 *  Copyright (C) 2015-2021 Savoir-faire Linux Inc.
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

#ifndef NOTIFY_H_
#define NOTIFY_H_

#include <gtk/gtk.h>
#include <string>

G_BEGIN_DECLS
#define NOTIFIER_TYPE            (notifier_get_type ())
#define NOTIFIER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOTIFIER_TYPE, Notifier))
#define NOTIFIER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), NOTIFIER_TYPE, NotifierClass))
#define IS_NOTIFIER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), NOTIFIER_TYPE))
#define IS_NOTIFIER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), NOTIFIER_TYPE))

typedef struct _Notifier      Notifier;
typedef struct _NotifierClass NotifierClass;

enum class NotificationType
{
    CALL,
    REQUEST,
    CHAT
};

GType      notifier_get_type             (void) G_GNUC_CONST;
GtkWidget* notifier_new                  (void);

gboolean    show_notification(Notifier* view,
                                   const std::string& icon,
                                   const std::string& uri,
                                   const std::string& name,
                                   const std::string& id,
                                   const std::string& title,
                                   const std::string& body,
                                   NotificationType type,
                                   const std::string& conversation = "");
gboolean    hide_notification(Notifier* view, const std::string& id);
gboolean    has_notification(Notifier* view, const std::string& id);
std::string get_notification_conversation(Notifier* view, const std::string& id);

G_END_DECLS

#endif /* NOTIFY_H_ */
