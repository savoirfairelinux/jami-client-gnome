/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *  Author: Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
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

class ContactRequest;

G_BEGIN_DECLS

#define CONTACT_REQUEST_CONTENT_VIEW_TYPE            (contact_request_content_view_get_type ())
#define CONTACT_REQUEST_CONTENT_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CONTACT_REQUEST_CONTENT_VIEW_TYPE, ContactRequestContentView))
#define CONTACT_REQUEST_CONTENT_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CONTACT_REQUEST_CONTENT_VIEW_TYPE, ContactRequestContentViewClass))
#define IS_CONTACT_REQUEST_CONTENT_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CONTACT_REQUEST_CONTENT_VIEW_TYPE))
#define IS_CONTACT_REQUEST_CONTENT_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CONTACT_REQUEST_CONTENT_VIEW_TYPE))

typedef struct _ContactRequestContentView      ContactRequestContentView;
typedef struct _ContactRequestContentViewClass ContactRequestContentViewClass;


GType          contact_request_content_view_get_type   (void) G_GNUC_CONST;
GtkWidget*     contact_request_content_view_new (ContactRequest* contact_request);

G_END_DECLS
