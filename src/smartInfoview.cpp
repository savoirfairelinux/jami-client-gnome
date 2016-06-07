/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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
 */

#include "smartInfoview.h"

#include <gtk/gtk.h>
#include <call.h>
#include <callmodel.h>
#include <contactmethod.h>
#include <person.h>
#include <media/media.h>
#include <media/text.h>
#include <QtCore/QDateTime>

typedef struct _SmartInfoViewPrivate SmartInfoViewPrivate;

struct _SmartInfoViewPrivate
{

};

struct _SmartInfoView
{
    GtkBox parent;
};



GtkWidget *
smartInfo_view_new_call(Call *call)
{
    g_return_val_if_fail(call, nullptr);

    SmartInfoView *self = CHAT_VIEW(g_object_new(SMARTINFO_VIEW_TYPE, NULL));
    SmartInfoViewPrivate *priv = CHAT_VIEW_GET_PRIVATE(self);

    priv->call = call;
    auto cm = priv->call->peerContactMethod();
    print_text_recording(cm->textRecording(), self);

    return (GtkWidget *)self;
}
