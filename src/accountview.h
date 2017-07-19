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

#ifndef _ACCOUNTVIEW_H
#define _ACCOUNTVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ACCOUNT_VIEW_TYPE            (account_view_get_type ())
#define ACCOUNT_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCOUNT_VIEW_TYPE, AccountView))
#define ACCOUNT_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), ACCOUNT_VIEW_TYPE, AccountViewClass))
#define IS_ACCOUNT_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), ACCOUNT_VIEW_TYPE))
#define IS_ACCOUNT_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ACCOUNT_VIEW_TYPE))

typedef struct _AccountView      AccountView;
typedef struct _AccountViewClass AccountViewClass;

GType      account_view_get_type      (void) G_GNUC_CONST;
GtkWidget *account_view_new           (void);

G_END_DECLS

#endif /* _ACCOUNTVIEW_H */
