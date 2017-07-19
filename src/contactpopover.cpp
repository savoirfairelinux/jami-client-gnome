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

#include "contactpopover.h"

#include <contactmethod.h>
#include "choosecontactview.h"
#include "editcontactview.h"

struct _ContactPopover
{
#if GTK_CHECK_VERSION(3,12,0)
    GtkPopover parent;
#else
    GtkWindow parent;
#endif
};

struct _ContactPopoverClass
{
#if GTK_CHECK_VERSION(3,12,0)
    GtkPopoverClass parent_class;
#else
    GtkWindowClass parent_class;
#endif
};

typedef struct _ContactPopoverPrivate ContactPopoverPrivate;

struct _ContactPopoverPrivate
{
    GtkWidget *choosecontactview;
    GtkWidget *editcontactview;

    ContactMethod *cm;
};

#if GTK_CHECK_VERSION(3,12,0)
    G_DEFINE_TYPE_WITH_PRIVATE(ContactPopover, contact_popover, GTK_TYPE_POPOVER);
#else
    G_DEFINE_TYPE_WITH_PRIVATE(ContactPopover, contact_popover, GTK_TYPE_WINDOW);
#endif


#define CONTACT_POPOVER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONTACT_POPOVER_TYPE, ContactPopoverPrivate))

#if !GTK_CHECK_VERSION(3,12,0)
static gboolean
contact_popover_button_release(GtkWidget *self, GdkEventButton *event)
{
    auto child = gtk_bin_get_child(GTK_BIN(self));

    auto event_widget = gtk_get_event_widget((GdkEvent *) event);

    GtkAllocation child_alloc;

    gtk_widget_get_allocation(child, &child_alloc);

    if (event->x < child_alloc.x ||
        event->x > child_alloc.x + child_alloc.width ||
        event->y < child_alloc.y ||
        event->y > child_alloc.y + child_alloc.height)
        gtk_widget_destroy(self);
    else if (!gtk_widget_is_ancestor(event_widget, self))
        gtk_widget_destroy(self);

    return GDK_EVENT_PROPAGATE;
}

static gboolean
contact_popover_key_press(GtkWidget *self, GdkEventKey *event)
{
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_destroy(self);
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}
#endif

static void
contact_popover_init(ContactPopover *self)
{
#if GTK_CHECK_VERSION(3,12,0)
    /* for now, destroy the popover on close, as we will construct a new one
     * each time we need it */
    g_signal_connect(self, "closed", G_CALLBACK(gtk_widget_destroy), NULL);
#else
    /* destroy the window on ESC, or when the user clicks outside of it */
    g_signal_connect(self, "button_release_event", G_CALLBACK(contact_popover_button_release), NULL);
    g_signal_connect(self, "key_press_event", G_CALLBACK(contact_popover_key_press), NULL);
#endif
}

static void
contact_popover_dispose(GObject *object)
{
    G_OBJECT_CLASS(contact_popover_parent_class)->dispose(object);
}

static void
contact_popover_finalize(GObject *object)
{
    G_OBJECT_CLASS(contact_popover_parent_class)->finalize(object);
}
static void
contact_popover_class_init(ContactPopoverClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = contact_popover_finalize;
    G_OBJECT_CLASS(klass)->dispose = contact_popover_dispose;
}

static void
construct_edit_contact_view(ContactPopover *self, Person *p)
{
    g_return_if_fail(IS_CONTACT_POPOVER(self));
    ContactPopoverPrivate *priv = CONTACT_POPOVER_GET_PRIVATE(self);

    priv->editcontactview = edit_contact_view_new(priv->cm, p);
    g_object_add_weak_pointer(G_OBJECT(priv->editcontactview), (gpointer *)&priv->editcontactview);

    gtk_container_remove(GTK_CONTAINER(self), priv->choosecontactview);
    gtk_container_add(GTK_CONTAINER(self), priv->editcontactview);

#if !GTK_CHECK_VERSION(3,12,0)
    /* resize the window to shrink to the new view */
    gtk_window_resize(GTK_WINDOW(self), 1, 1);
#endif

    /* destroy this popover when the contact is saved */
    g_signal_connect_swapped(priv->editcontactview, "person-saved", G_CALLBACK(gtk_widget_destroy), self);
}

static void
new_person_clicked(ContactPopover *self)
{
    g_return_if_fail(IS_CONTACT_POPOVER(self));
    construct_edit_contact_view(self, NULL);
}

static void
person_selected(ContactPopover *self, Person *p)
{
    g_return_if_fail(IS_CONTACT_POPOVER(self));
    construct_edit_contact_view(self, p);
}

/**
 * For gtk+ >= 3.12 this will create a GtkPopover pointing to the parent and if
 * given, the GdkRectangle. Otherwise, this will create an undecorated GtkWindow
 * which will be centered on the toplevel window of the given parent.
 * This is to ensure cmpatibility with gtk+3.10.
 */
GtkWidget *
contact_popover_new(ContactMethod *cm, GtkWidget *parent,
#if !GTK_CHECK_VERSION(3,12,0)
                    G_GNUC_UNUSED
#endif
                    GdkRectangle *rect)
{
    g_return_val_if_fail(cm, NULL);

#if GTK_CHECK_VERSION(3,12,0)
    gpointer self = g_object_new(CONTACT_POPOVER_TYPE,
                                 "relative-to", parent,
                                 "position", GTK_POS_RIGHT,
                                 NULL);

    if (rect)
        gtk_popover_set_pointing_to(GTK_POPOVER(self), rect);
#else
    /* get the toplevel parent and try to center on it */
    if (parent && GTK_IS_WIDGET(parent)) {
        parent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
        if (!gtk_widget_is_toplevel(parent)) {
            parent = NULL;
            g_debug("could not get top level parent");
        }
    }

    gpointer self = g_object_new(CONTACT_POPOVER_TYPE,
                                 "modal", TRUE,
                                 "transient-for", parent,
                                 "window-position", GTK_WIN_POS_CENTER_ON_PARENT,
                                 "decorated", FALSE,
                                 "resizable", FALSE,
                                 NULL);
#endif

    ContactPopoverPrivate *priv = CONTACT_POPOVER_GET_PRIVATE(self);
    priv->cm = cm;

    priv->choosecontactview = choose_contact_view_new(cm);
    gtk_container_add(GTK_CONTAINER(self), priv->choosecontactview);
    g_object_add_weak_pointer(G_OBJECT(priv->choosecontactview), (gpointer *)&priv->choosecontactview);

    g_signal_connect_swapped(priv->choosecontactview, "new-person-clicked", G_CALLBACK(new_person_clicked), self);
    g_signal_connect_swapped(priv->choosecontactview, "person-selected", G_CALLBACK(person_selected), self);

    return (GtkWidget *)self;
}
