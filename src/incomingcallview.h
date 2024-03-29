/*
 *  Copyright (C) 2015-2022 Savoir-faire Linux Inc.
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

// client
#include "api/account.h"
#include "webkitchatcontainer.h"
#include "accountinfopointer.h"

namespace lrc
{
namespace api
{
    class AVModel;
    class PluginModel;
namespace conversation
{
    struct Info;
}
}
}

G_BEGIN_DECLS

#define INCOMING_CALL_VIEW_TYPE            (incoming_call_view_get_type ())
#define INCOMING_CALL_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), INCOMING_CALL_VIEW_TYPE, IncomingCallView))
#define INCOMING_CALL_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), INCOMING_CALL_VIEW_TYPE, IncomingCallViewClass))
#define IS_INCOMING_CALL_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), INCOMING_CALL_VIEW_TYPE))
#define IS_INCOMING_CALL_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), INCOMING_CALL_VIEW_TYPE))

typedef struct _IncomingCallView      IncomingCallView;
typedef struct _IncomingCallViewClass IncomingCallViewClass;


GType      incoming_call_view_get_type (void) G_GNUC_CONST;
GtkWidget *incoming_call_view_new (WebKitChatContainer* view,
                                   lrc::api::AVModel& avModel,
                                   lrc::api::PluginModel& pluginModel,
                                   AccountInfoPointer const & accountInfo,
                                   lrc::api::conversation::Info& conversation);
void incoming_call_view_let_a_message(IncomingCallView* view, const lrc::api::conversation::Info& conv);
bool is_showing_let_a_message_view(IncomingCallView* view, lrc::api::conversation::Info& conv);
lrc::api::conversation::Info& incoming_call_view_get_conversation (IncomingCallView*);

G_END_DECLS
