/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
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

#pragma once

#include <gtk/gtk.h>

class QModelIndex;
class QString;
class QVariant;
class ContactMethod;

G_BEGIN_DECLS

#define WEBKIT_CHAT_CONTAINER_TYPE            (webkit_chat_container_get_type ())
#define WEBKIT_CHAT_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_CHAT_CONTAINER_TYPE, WebKitChatContainer))
#define WEBKIT_CHAT_CONTAINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_CHAT_CONTAINER_TYPE, WebKitChatContainerClass))
#define IS_WEBKIT_CHAT_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_CHAT_CONTAINER_TYPE))
#define IS_WEBKIT_CHAT_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_CHAT_CONTAINER_TYPE))

typedef struct _WebKitChatContainer      WebKitChatContainer;
typedef struct _WebKitChatContainerClass WebKitChatContainerClass;

GType      webkit_chat_container_get_type            (void) G_GNUC_CONST;
GtkWidget* webkit_chat_container_new                 (void);
void       webkit_chat_container_clear               (WebKitChatContainer *view);
void       webkit_chat_container_clear_sender_images (WebKitChatContainer *view);
void       webkit_chat_container_print_new_message   (WebKitChatContainer *view, const QModelIndex &idx);
void       webkit_chat_container_update_message      (WebKitChatContainer *view, const QModelIndex &idx);
void       webkit_chat_container_set_sender_image    (WebKitChatContainer *view, ContactMethod *sender_contact_method, QVariant sender_image);
gboolean   webkit_chat_container_is_ready            (WebKitChatContainer *view);

G_END_DECLS
