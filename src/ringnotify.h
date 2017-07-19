/*
 *  Copyright (C) 2015-2017 Savoir-faire Linux Inc.
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

#ifndef RING_NOTIFY_H_
#define RING_NOTIFY_H_

#include <gtk/gtk.h>
#include "ring_client.h"

class Call;
class ContactMethod;
namespace Media {
class TextRecording;
}

G_BEGIN_DECLS

void     ring_notify_init();
void     ring_notify_uninit();
gboolean ring_notify_is_initted();
gboolean ring_notify_incoming_call(Call*);
void     ring_notify_message(ContactMethod*, Media::TextRecording*, RingClient*);
gboolean ring_notify_close_chat_notification(ContactMethod*);

G_END_DECLS

#endif /* RING_NOTIFY_H_ */
