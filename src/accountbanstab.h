/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
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

#include <gtk/gtk.h>

class Account;

G_BEGIN_DECLS

#define ACCOUNT_BANS_TAB_TYPE            (account_bans_tab_get_type ())
#define ACCOUNT_BANS_TAB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCOUNT_BANS_TAB_TYPE, AccountBansTab))
#define ACCOUNT_BANS_TAB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), ACCOUNT_BANS_TAB_TYPE, AccountBansTabClass))
#define IS_ACCOUNT_BANS_TAB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), ACCOUNT_BANS_TAB_TYPE))
#define IS_ACCOUNT_BANS_TAB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ACCOUNT_BANS_TAB_TYPE))

typedef struct _AccountBansTab      AccountBansTab;
typedef struct _AccountBansTabClass AccountBansTabClass;

GType      account_bans_tab_get_type      (void) G_GNUC_CONST;
GtkWidget *account_bans_tab_new           (Account *account);

G_END_DECLS
