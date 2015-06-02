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

#ifndef _CURRENTCALLVIEW_H
#define _CURRENTCALLVIEW_H

#include <gtk/gtk.h>

class QModelIndex;

G_BEGIN_DECLS

#define CURRENT_CALL_VIEW_TYPE            (current_call_view_get_type ())
#define CURRENT_CALL_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CURRENT_CALL_VIEW_TYPE, CurrentCallView))
#define CURRENT_CALL_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CURRENT_CALL_VIEW_TYPE, CurrentCallViewClass))
#define IS_CURRENT_CALL_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CURRENT_CALL_VIEW_TYPE))
#define IS_CURRENT_CALL_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CURRENT_CALL_VIEW_TYPE))

typedef struct _CurrentCallView      CurrentCallView;
typedef struct _CurrentCallViewClass CurrentCallViewClass;


GType      current_call_view_get_type      (void) G_GNUC_CONST;
GtkWidget *current_call_view_new           (void);
void       current_call_view_set_call_info (CurrentCallView *view, const QModelIndex& idx);

G_END_DECLS

#endif /* _CURRENTCALLVIEW_H */
