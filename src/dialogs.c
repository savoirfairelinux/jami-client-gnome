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
 */

#include "dialogs.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include "config.h"
#include "revision.h"

GtkWidget *
ring_dialog_working(GtkWidget *parent, const gchar *msg)
{
    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    if (parent && GTK_IS_WIDGET(parent)) {
        /* get parent window so we can center on it */
        parent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
        if (gtk_widget_is_toplevel(parent)) {
            gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
            gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        }
    }

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content_area), 10);
    gtk_widget_set_size_request(content_area, 250, -1);
    gtk_widget_set_margin_top(content_area, 25);

    GtkWidget *message = NULL;
    if (msg) {
        message = gtk_label_new(msg);
    } else {
        message = gtk_label_new(_("Working..."));
    }

    gtk_box_pack_start(GTK_BOX(content_area), message, FALSE, TRUE, 0);

    GtkWidget *spinner = gtk_spinner_new();
    gtk_spinner_start(GTK_SPINNER(spinner));

    gtk_box_pack_start(GTK_BOX(content_area), spinner, FALSE, TRUE, 0);

    gtk_widget_show_all(content_area);

    return dialog;
}

void
ring_about_dialog(GtkWidget *parent)
{
    /* get parent window */
    if (parent && GTK_IS_WIDGET(parent))
        parent = gtk_widget_get_toplevel(GTK_WIDGET(parent));

    /* get logo */
    GError *error = NULL;
    GdkPixbuf* logo = gdk_pixbuf_new_from_resource("/cx/ring/RingGnome/ring-logo-blue", &error);
    if (logo == NULL) {
        g_debug("Could not load logo: %s", error->message);
        g_clear_error(&error);
    }

    gchar *name = g_strdup_printf("Gnome Ring");
    gchar *version = g_strdup_printf(C_("Do not translate the release name", "release: Samuel de Champlain\nv%d.%d.%d - %.10s"),
                                     VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, RING_CLIENT_REVISION);

    const gchar *authors[] = {
        [0] = "Adrien Béraud",
        [1] = "Alexandre Lision",
        [2] = "Edric Milaret",
        [3] = "Éloi Bail",
        [4] = "Emmanuel Lepage-Vallée",
        [5] = "Guillaume Roguez",
        [6] = "Stepan Salenikovich",
        [7] = "Based on the SFLPhone project",
        [8] = NULL,
    };

    const gchar *artists[] = {
        [0] = "Marianne Forget",
        [1] = NULL,
    };

    gtk_show_about_dialog(
        GTK_WINDOW(parent),
        "program-name", name,
        "copyright", "© 2015 Savoir-faire Linux",
        "license-type", GTK_LICENSE_GPL_3_0,
        "logo", logo,
        "version", version,
        "comments", _("The GNOME client for Ring.\nRing is a secured and distributed communication software."),
        "authors", authors,
        "website", "http://www.ring.cx/",
        "website-label", "www.ring.cx",
        "artists", artists,
        NULL
    );

    g_free(name);
    g_free(version);
}
