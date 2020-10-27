/*
 *  Copyright (C) 2015-2020 Savoir-faire Linux Inc.
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

#ifndef _FILES_H
#define _FILES_H

#include <functional>
#include <gio/gio.h>

G_BEGIN_DECLS

void autostart_symlink(gboolean autostart);

GSettingsSchema *get_settings_schema();

/**
 * Split the string `uris' using `g_strsplit()' with "\r\n" as the delimiter,
 * passing each split part through `g_filename_from_uri()', and passing the
 * result as an argument to the provided `cb' callback function.
 */
void
foreach_file(const gchar *uris, const std::function<void(const char*)>& cb);

G_END_DECLS

#endif /* _FILES_H */
