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

#ifndef _CONTACTPOPOVER_H
#define _CONTACTPOPOVER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

class ContactMethod;

#define CONTACT_POPOVER_TYPE            (contact_popover_get_type ())
#define CONTACT_POPOVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CONTACT_POPOVER_TYPE, ContactPopover))
#define CONTACT_POPOVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CONTACT_POPOVER_TYPE, ContactPopoverClass))
#define IS_CONTACT_POPOVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CONTACT_POPOVER_TYPE))
#define IS_CONTACT_POPOVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CONTACT_POPOVER_TYPE))

typedef struct _ContactPopover      ContactPopover;
typedef struct _ContactPopoverClass ContactPopoverClass;

GType      contact_popover_get_type  (void) G_GNUC_CONST;

/**
 * For gtk+ >= 3.12 this will create a GtkPopover pointing to the parent and if
 * given, the GdkRectangle. Otherwise, this will create an undecorated GtkWindow
 * which will be centered on the toplevel window of the given parent.
 * This is to ensure cmpatibility with gtk+3.10.
 */
GtkWidget *contact_popover_new       (ContactMethod *cm, GtkWidget *parent, GdkRectangle *rect);

G_END_DECLS

#endif /* _CONTACTPOPOVER_H */
