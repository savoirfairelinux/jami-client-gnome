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

#ifndef _HISTORYVIEW_H
#define _HISTORYVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HISTORY_VIEW_TYPE            (history_view_get_type ())
#define HISTORY_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HISTORY_VIEW_TYPE, HistoryView))
#define HISTORY_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), HISTORY_VIEW_TYPE, HistoryViewClass))
#define IS_HISTORY_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), HISTORY_VIEW_TYPE))
#define IS_HISTORY_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), HISTORY_VIEW_TYPE))

typedef struct _HistoryView      HistoryView;
typedef struct _HistoryViewClass HistoryViewClass;

GType      history_view_get_type (void) G_GNUC_CONST;
GtkWidget *history_view_new      (void);
void       history_view_set_filter_string(HistoryView *, const char*);

G_END_DECLS

#endif /* _HISTORYVIEW_H */
