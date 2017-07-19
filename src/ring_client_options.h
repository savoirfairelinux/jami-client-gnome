/*
 *  Copyright (C) 2013-2017 Savoir-faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifndef RING_CLIENT_OPTIONS_H_
#define RING_CLIENT_OPTIONS_H_

#include <gio/gio.h>

G_BEGIN_DECLS

#if GLIB_CHECK_VERSION(2,40,0)
void ring_client_add_options(GApplication *app);
#else
GOptionContext *ring_client_options_get_context(void);
#endif

G_END_DECLS

#endif /* RING_CLIENT_OPTIONS_H_ */
