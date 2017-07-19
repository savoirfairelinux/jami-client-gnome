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

#ifndef _MENUS_H
#define _MENUS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

class ContactMethod;

/**
 * checks if the given contact method is already associated with a contact
 */
gboolean    contact_method_has_contact(ContactMethod *cm);

/**
 * Takes a GtkMenuItem and connects its activate signal to a popup which adds the given
 * ContactMethod to a Person
 */
GtkMenuItem* menu_item_add_to_contact(GtkMenuItem *item, ContactMethod *cm, GtkWidget *parent, const GdkRectangle *rect);

G_END_DECLS

#endif /* _MENUS_H */
