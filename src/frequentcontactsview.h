/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef _FREQUENTCONTACTSVIEW_H
#define _FREQUENTCONTACTSVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define FREQUENT_CONTACTS_VIEW_TYPE            (frequent_contacts_view_get_type ())
#define FREQUENT_CONTACTS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FREQUENT_CONTACTS_VIEW_TYPE, FrequentContactsView))
#define FREQUENT_CONTACTS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), FREQUENT_CONTACTS_VIEW_TYPE, FrequentContactsViewClass))
#define IS_FREQUENT_CONTACTS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), FREQUENT_CONTACTS_VIEW_TYPE))
#define IS_FREQUENT_CONTACTS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), FREQUENT_CONTACTS_VIEW_TYPE))

typedef struct _FrequentContactsView      FrequentContactsView;
typedef struct _FrequentContactsViewClass FrequentContactsViewClass;

GType      frequent_contacts_view_get_type (void) G_GNUC_CONST;
GtkWidget *frequent_contacts_view_new      (void);

G_END_DECLS

#endif /* _FREQUENTCONTACTSVIEW_H */
