/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
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

#define ACCOUNT_MIGRATION_VIEW_TYPE            (account_migration_view_get_type ())
#define ACCOUNT_MIGRATION_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCOUNT_MIGRATION_VIEW_TYPE, AccountMigrationView))
#define ACCOUNT_MIGRATION_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), ACCOUNT_MIGRATION_VIEW_TYPE, AccountMigrationViewClass))
#define IS_ACCOUNT_MIGRATION_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), ACCOUNT_MIGRATION_VIEW_TYPE))
#define IS_ACCOUNT_MIGRATION_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ACCOUNT_MIGRATION_VIEW_TYPE))

typedef struct _AccountMigrationView      AccountMigrationView;
typedef struct _AccountMigrationViewClass AccountMigrationViewClass;

GType      account_migration_view_get_type      (void) G_GNUC_CONST;
GtkWidget *account_migration_view_new           (Account *account);

G_END_DECLS
