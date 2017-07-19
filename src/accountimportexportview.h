/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ACCOUNT_IMPORTEXPORT_VIEW_TYPE            (account_importexport_view_get_type ())
#define ACCOUNT_IMPORTEXPORT_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCOUNT_IMPORTEXPORT_VIEW_TYPE, AccountImportExportView))
#define ACCOUNT_IMPORTEXPORT_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), ACCOUNT_IMPORTEXPORT_VIEW_TYPE, AccountImportExportViewClass))
#define IS_ACCOUNT_IMPORTEXPORT_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), ACCOUNT_IMPORTEXPORT_VIEW_TYPE))
#define IS_ACCOUNT_IMPORTEXPORT_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ACCOUNT_IMPORTEXPORT_VIEW_TYPE))

typedef struct _AccountImportExportView      AccountImportExportView;
typedef struct _AccountImportExportViewClass AccountImportExportViewClass;

GType      account_importexport_view_get_type      (void) G_GNUC_CONST;
GtkWidget *account_import_view_new                 (void);
GtkWidget *account_export_view_new                 (void);
void       account_export_view_set_accounts        (AccountImportExportView* self, GList *accounts);

G_END_DECLS
