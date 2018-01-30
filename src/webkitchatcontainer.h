/*
 *  Copyright (C) 2016-2018 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

// Gtk
#include <gtk/gtk.h>

// LRC
#include <api/interaction.h>

G_BEGIN_DECLS

#define WEBKIT_CHAT_CONTAINER_TYPE            (webkit_chat_container_get_type ())
#define WEBKIT_CHAT_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_CHAT_CONTAINER_TYPE, WebKitChatContainer))
#define WEBKIT_CHAT_CONTAINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_CHAT_CONTAINER_TYPE, WebKitChatContainerClass))
#define IS_WEBKIT_CHAT_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_CHAT_CONTAINER_TYPE))
#define IS_WEBKIT_CHAT_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_CHAT_CONTAINER_TYPE))

typedef struct _WebKitChatContainer      WebKitChatContainer;
typedef struct _WebKitChatContainerClass WebKitChatContainerClass;

GType      webkit_chat_container_get_type             (void) G_GNUC_CONST;
GtkWidget* webkit_chat_container_new                  (void);
void       webkit_chat_container_clear                (WebKitChatContainer *view);
void       webkit_chat_container_clear_sender_images  (WebKitChatContainer *view);
void       webkit_chat_container_print_new_interaction(WebKitChatContainer *view, uint64_t msgId, const lrc::api::interaction::Info& interaction);
void       webkit_chat_container_update_interaction   (WebKitChatContainer *view, uint64_t msgId, const lrc::api::interaction::Info& interaction);
void       webkit_chat_container_set_sender_image     (WebKitChatContainer *view, const std::string& sender, const std::string& senderImage);
gboolean   webkit_chat_container_is_ready             (WebKitChatContainer *view);
void       webkit_chat_container_set_display_links    (WebKitChatContainer *view, bool display);
void       webkit_chat_disable_send_interaction       (WebKitChatContainer *view, bool isDisabled);
void       webkit_chat_container_set_temporary        (WebKitChatContainer *view, bool temporary);
void       webkit_chat_container_set_invitation       (WebKitChatContainer *view, bool show, const std::string& contactUri);

G_END_DECLS
