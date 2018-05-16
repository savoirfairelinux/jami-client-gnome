/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU NewAccount Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU NewAccount Public License for more details.
 *
 *  You should have received a copy of the GNU NewAccount Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "newaccountsettingsview.h"

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib.h>

// LRC
#include <person.h>
#include <profile.h>
#include <profilemodel.h>
#include <categorizedhistorymodel.h>
#include <media/recordingmodel.h>

// Ring client
#include "utils/files.h"
#include "avatarmanipulation.h"

enum
{
  PROP_RING_MAIN_WIN_PNT = 1,
};

struct _NewAccountSettingsView
{
    GtkScrolledWindow parent;
};

struct _NewAccountSettingsViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _NewAccountSettingsViewPrivate NewAccountSettingsViewPrivate;

struct _NewAccountSettingsViewPrivate
{
    GSettings *settings;

    GtkWidget *avatar_box;
    GtkWidget *avatarmanipulation;

    GtkWidget* button_advanced_settings;

    /* ring main window pointer */
    GtkWidget* ring_main_window_pnt;
};

G_DEFINE_TYPE_WITH_PRIVATE(NewAccountSettingsView, new_account_settings_view, GTK_TYPE_SCROLLED_WINDOW);

#define NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), NEW_ACCOUNT_SETTINGS_VIEW_TYPE, NewAccountSettingsViewPrivate))

static void
new_account_settings_view_dispose(GObject *object)
{
    NewAccountSettingsViewPrivate *priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(object);

    g_clear_object(&priv->settings);

    //make sure the VideoWidget is destroyed
    new_account_settings_view_show(NEW_ACCOUNT_SETTINGS_VIEW(object), FALSE);

    G_OBJECT_CLASS(new_account_settings_view_parent_class)->dispose(object);
}

static void
new_account_settings_view_init(NewAccountSettingsView *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));
}


static void
new_account_settings_view_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    NewAccountSettingsView *self = NEW_ACCOUNT_SETTINGS_VIEW (object);
    NewAccountSettingsViewPrivate *priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(self);

    if (property_id == PROP_RING_MAIN_WIN_PNT) {
        GtkWidget *ring_main_window_pnt = (GtkWidget*) g_value_get_pointer(value);

        if (!ring_main_window_pnt) {
            g_debug("Internal error: NULL main window pointer passed to set_property");
            return;
        }

        priv->ring_main_window_pnt = ring_main_window_pnt;
    }
    else {
        // Invalid property id passed
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
new_account_settings_view_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    NewAccountSettingsView *self = NEW_ACCOUNT_SETTINGS_VIEW (object);
    NewAccountSettingsViewPrivate *priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(self);

    if (property_id == PROP_RING_MAIN_WIN_PNT) {
        g_value_set_pointer(value, priv->ring_main_window_pnt);
    }
    else {
        // Invalid property id passed
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
new_account_settings_view_class_init(NewAccountSettingsViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = new_account_settings_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/newaccountsettingsview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), NewAccountSettingsView, avatar_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), NewAccountSettingsView, button_advanced_settings);

    /* Define class properties: e.g. pointer to main window, etc.*/
    object_class->set_property = new_account_settings_view_set_property;
    object_class->get_property = new_account_settings_view_get_property;

    GParamFlags flags = (GParamFlags) (G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_RING_MAIN_WIN_PNT,
        g_param_spec_pointer ("ring_main_window_pnt",
                              "RingMainWindow pointer",
                              "Pointer to the Ring Main Window. This property is used by modal dialogs.",
                              flags));
}

GtkWidget *
new_account_settings_view_new(GtkWidget* ring_main_window_pointer)
{
    gpointer view = g_object_new(NEW_ACCOUNT_SETTINGS_VIEW_TYPE, NULL);

    // set_up ring main window pointer (needed by modal dialogs)
    GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_POINTER);
    g_value_set_pointer (&val, ring_main_window_pointer);
    g_object_set_property (G_OBJECT (view), "ring_main_window_pnt", &val);
    g_value_unset (&val);

    return (GtkWidget *)view;
}

void
new_account_settings_view_show(NewAccountSettingsView *self, gboolean show_profile)
{
    g_return_if_fail(NEW_ACCOUNT_SETTINGS_VIEW(self));
    NewAccountSettingsViewPrivate *priv = NEW_ACCOUNT_SETTINGS_VIEW_GET_PRIVATE(self);

    /* We will construct and destroy the profile (AvatarManipulation widget) each time the profile
     * should be visible and hidden, respectively. It is not the "prettiest" way of doing things,
     * but this way we ensure 1. that the profile is updated correctly when it is shown and 2. that
     * the VideoWidget inside is destroyed when it is not being shown.
     */
    if (show_profile) {
        /* avatar manipulation widget */
        priv->avatarmanipulation = avatar_manipulation_new();
        gtk_widget_set_halign(priv->avatar_box, GtkAlign::GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(priv->avatar_box), priv->avatarmanipulation, true, true, 0);
        gtk_widget_set_visible(priv->avatarmanipulation, true);

    } else {
        if (priv->avatarmanipulation) {
            gtk_container_remove(GTK_CONTAINER(priv->avatar_box), priv->avatarmanipulation);
            priv->avatarmanipulation = nullptr;
        }
    }
}
