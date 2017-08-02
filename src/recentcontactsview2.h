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

G_BEGIN_DECLS

#define RECENT_CONTACTS_VIEW_TYPE            (recent_contacts_view2_get_type ())
#define RECENT_CONTACTS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RECENT_CONTACTS_VIEW_TYPE, RecentContactsView2))
#define RECENT_CONTACTS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), RECENT_CONTACTS_VIEW_TYPE, RecentContactsView2Class))
#define IS_RECENT_CONTACTS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), RECENT_CONTACTS_VIEW_TYPE))
#define IS_RECENT_CONTACTS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RECENT_CONTACTS_VIEW_TYPE))

typedef struct _RecentContactsView2      RecentContactsView2;
typedef struct _RecentContactsView2Class RecentContactsView2Class;

GType      recent_contacts_view2_get_type (void) G_GNUC_CONST;
GtkWidget *recent_contacts_view2_new      (void);

G_END_DECLS
