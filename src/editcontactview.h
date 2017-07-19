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

#ifndef _EDITCONTACTVIEW_H
#define _EDITCONTACTVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

class ContactMethod;
class Person;

#define EDIT_CONTACT_VIEW_TYPE            (edit_contact_view_get_type ())
#define EDIT_CONTACT_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EDIT_CONTACT_VIEW_TYPE, EditContactView))
#define EDIT_CONTACT_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EDIT_CONTACT_VIEW_TYPE, EditContactViewClass))
#define IS_EDIT_CONTACT_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), EDIT_CONTACT_VIEW_TYPE))
#define IS_EDIT_CONTACT_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EDIT_CONTACT_VIEW_TYPE))

typedef struct _EditContactView      EditContactView;
typedef struct _EditContactViewClass EditContactViewClass;

GType      edit_contact_view_get_type(void) G_GNUC_CONST;

/**
 * If a Person is specified, a view to edit the given Person will be created;
 * if no Person is given (NULL), then a new Person object will be created when
 * 'save' is clicked
 */
GtkWidget *edit_contact_view_new     (ContactMethod *cm, Person *p);

G_END_DECLS

#endif /* _EDITCONTACTVIEW_H */
