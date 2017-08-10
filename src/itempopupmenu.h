/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#define ITEM_POPUP_MENU_TYPE            (item_popup_menu_get_type ())
#define ITEM_POPUP_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ITEM_POPUP_MENU_TYPE, ItemPopupMenu))
#define ITEM_POPUP_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), ITEM_POPUP_MENU_TYPE, ItemPopupMenuClass))
#define IS_ITEM_POPUP_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), ITEM_POPUP_MENU_TYPE))
#define IS_ITEM_POPUP_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ITEM_POPUP_MENU_TYPE))

typedef struct _ItemPopupMenu      ItemPopupMenu;
typedef struct _ItemPopupMenuClass ItemPopupMenuClass;

GType      item_popup_menu_get_type (void) G_GNUC_CONST;
GtkWidget *item_popup_menu_new      (GtkTreeView *treeview);
gboolean   item_popup_menu_show     (ItemPopupMenu *self, GdkEventButton *event);

G_END_DECLS
