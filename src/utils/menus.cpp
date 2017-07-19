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

#include "menus.h"

#include <glib/gi18n.h>
#include <contactmethod.h>
#include "../contactpopover.h"

/**
 * checks if the given contact method is already associated with a contact
 */
gboolean
contact_method_has_contact(ContactMethod *cm)
{
    g_return_val_if_fail(cm, FALSE);

    return cm->contact() != NULL;
}

static void
choose_contact(GtkWidget *item, ContactMethod *contactmethod)
{
    // we get the parent widget which should be stored in the item object
    auto parent = g_object_get_data(G_OBJECT(item), "parent-widget");
    auto rect = g_object_get_data(G_OBJECT(item), "parent-rectangle");

    auto popover = contact_popover_new(contactmethod, GTK_WIDGET(parent), (GdkRectangle *)rect);

    gtk_widget_show(popover);
}

/**
 * Takes a GtkMenuItem and connects its activate signal to a popup which adds the given
 * ContactMethod to a Person
 */
GtkMenuItem*
menu_item_add_to_contact(GtkMenuItem *item, ContactMethod *cm, GtkWidget *parent, const GdkRectangle *rect)
{
    g_return_val_if_fail(item && cm, NULL);

    /* save the parent widget in the item object, so we can retrieve
     * it in the callback */
    g_object_set_data(G_OBJECT(item), "parent-widget", parent);

    GdkRectangle *copy_rect = NULL;
    if (rect) {
        copy_rect = g_new0(GdkRectangle, 1);
        memcpy(copy_rect, rect, sizeof(GdkRectangle));
    }
    g_object_set_data_full(G_OBJECT(item), "parent-rectangle", copy_rect, (GDestroyNotify)g_free);

    g_signal_connect(item, "activate", G_CALLBACK(choose_contact), cm);

    return item;
}
