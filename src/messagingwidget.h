/*
 *  Copyright (C) 2018-2021 Savoir-faire Linux Inc.
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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

// GTK
#include <gtk/gtk.h>

// std
#include <string>

// client
#include "accountinfopointer.h"

namespace lrc
{
namespace api
{
    class AVModel;
namespace conversation
{
    struct Info;
}
}
}

G_BEGIN_DECLS

#define MESSAGING_WIDGET_TYPE            (messaging_widget_get_type ())
#define MESSAGING_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MESSAGING_WIDGET_TYPE, MessagingWidget))
#define MESSAGING_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), MESSAGING_WIDGET_TYPE, MessagingWidget))
#define IS_MESSAGING_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), MESSAGING_WIDGET_TYPE))
#define IS_MESSAGING_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MESSAGING_WIDGET_TYPE))

typedef struct _MessagingWidget      MessagingWidget;
typedef struct _MessagingWidgetClass MessagingWidgetClass;

typedef enum
{
    MESSAGING_WIDGET_STATE_INIT,
    MESSAGING_WIDGET_REC_AUDIO,
    MESSAGING_WIDGET_AUDIO_REC_SUCCESS,
    MESSAGING_WIDGET_REC_SENT
} MessagingWidgetState;

GType       messaging_widget_get_type      (void) G_GNUC_CONST;
GtkWidget*  messaging_widget_new           (lrc::api::AVModel& avModel,
                                            lrc::api::conversation::Info& conversation,
                                            AccountInfoPointer const & accountInfo);
void        messaging_widget_set_peer_name (MessagingWidget *self, std::string name);

G_END_DECLS
