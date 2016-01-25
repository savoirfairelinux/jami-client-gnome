/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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

#ifndef _DRAWING_H
#define _DRAWING_H

#include <gtk/gtk.h>

GdkPixbuf *ring_draw_fallback_avatar(int size);

GdkPixbuf *ring_draw_conference_avatar(int size);

GdkPixbuf *ring_frame_avatar(GdkPixbuf *avatar);

GdkPixbuf *ring_draw_unread_messages(const GdkPixbuf *avatar, int unread_count);

#endif /* _DRAWING */
