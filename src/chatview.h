/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
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

// Client related
#include "accountcontainer.h"
#include "conversationcontainer.h"
#include "webkitchatcontainer.h"

G_BEGIN_DECLS

#define CHAT_VIEW_TYPE            (chat_view_get_type ())
#define CHAT_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CHAT_VIEW_TYPE, ChatView))
#define CHAT_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CHAT_VIEW_TYPE, ChatViewClass))
#define IS_CHAT_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CHAT_VIEW_TYPE))
#define IS_CHAT_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CHAT_VIEW_TYPE))

typedef struct _ChatView      ChatView;
typedef struct _ChatViewClass ChatViewClass;

GType          chat_view_get_type   (void) G_GNUC_CONST;
GtkWidget     *chat_view_new        (WebKitChatContainer* view,
                                     AccountContainer* accountContainer,
                                     ConversationContainer* conversationContainer);
lrc::api::conversation::Info chat_view_get_conversation(ChatView*);
bool chat_view_get_temporary(ChatView*);
void chat_view_update_temporary(ChatView*, bool);
void           chat_view_set_header_visible(ChatView*, gboolean);

G_END_DECLS
