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

#ifndef _CONTACTSVIEW_H
#define _CONTACTSVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CONTACTS_VIEW_TYPE            (contacts_view_get_type ())
#define CONTACTS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CONTACTS_VIEW_TYPE, ContactsView))
#define CONTACTS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CONTACTS_VIEW_TYPE, ContactsViewClass))
#define IS_CONTACTS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CONTACTS_VIEW_TYPE))
#define IS_CONTACTS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CONTACTS_VIEW_TYPE))

typedef struct _ContactsView      ContactsView;
typedef struct _ContactsViewClass ContactsViewClass;

GType      contacts_view_get_type  (void) G_GNUC_CONST;
GtkWidget *contacts_view_new       (void);
void       contacts_view_set_filter_string(ContactsView *, const char*);
// void       contact_view_set_sorting(ContactsView *self, int sort);

G_END_DECLS

#endif /* _CONTACTSVIEW_H */
