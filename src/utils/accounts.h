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

#ifndef _ACCOUNTS_H
#define _ACCOUNTS_H

#include <gtk/gtk.h>

class Account;

/**
 * returns TRUE if a RING account exists; FALSE otherwise
 */
gboolean
has_ring_account();

/**
 * iterates through all existing accounts and make sure all RING accounts have
 * a display name set; if a display name is empty, it is set to the alias of the
 * account
 */
void
force_ring_display_name();

/**
 * Finds and returns the first RING account, in order of priority:
 * 1. registered
 * 2. enabled
 * 3. existing
 *
 * Returns a nullptr if no RING acconts exist
 */
Account*
get_active_ring_account();

#endif /* _ACCOUNTS_H */
