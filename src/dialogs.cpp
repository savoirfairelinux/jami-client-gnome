/*
 *  Copyright (C) 2015-2021 Savoir-faire Linux Inc.
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
#include "revision.h"
#include "utils/drawing.h"

void
about_dialog_set_theme(GtkAboutDialog *dialog, gboolean use_dark_theme)
{
    GError *error = NULL;
    GdkPixbuf* logo = gdk_pixbuf_new_from_resource_at_scale(
        use_dark_theme
            ? "/net/jami/JamiGnome/jami-logo-white"
            : "/net/jami/JamiGnome/jami-logo-blue",
        350, -1, TRUE, &error);
    if (!logo) {
        g_debug("Could not load logo: %s", error->message);
        g_clear_error(&error);
    }
    gtk_about_dialog_set_logo(dialog, logo);
}

gboolean
about_dialog_on_redraw(GtkWidget *dialog,
                       G_GNUC_UNUSED cairo_t *cr,
                       G_GNUC_UNUSED gpointer user_data)
{
    about_dialog_set_theme(GTK_ABOUT_DIALOG(dialog),
                           use_dark_theme(get_ambient_color(dialog)));
    return FALSE;
}

void
about_dialog(GtkWidget *parent)
{
    /* get parent window */
    if (parent && GTK_IS_WIDGET(parent))
        parent = gtk_widget_get_toplevel(GTK_WIDGET(parent));

    gchar *version = g_strdup_printf(C_("Do not translate the release name nor the status (beta, final, ...)",
                                        "\"%s\"\nbuilt on %.25s"),
                                        "Mekhenty",
                                     CLIENT_BUILD_DATE);

    const gchar *authors[] = {
        "Adrien Béraud",
        "Albert Babí",
        "Alexandr Sergheev",
        "Alexandre Lision",
        "Alexandre Viau",
        "Aline Bonnet",
        "Aline Gondim Santos",
        "Amin Bandali",
        "AmirHossein Naghshzan",
        "Andreas Traczyk",
        "Anthony Léonard",
        "Brando Tovar",
        "Cyrille Béraud",
        "Dorina Mosku",
        "Edric Milaret",
        "Eden Abitbol",
        "Éloi Bail",
        "Emmanuel Lepage-Vallée",
        "Frédéric Guimont",
        "Guillaume Heller",
        "Guillaume Roguez",
        "Hadrien De Sousa",
        "Hugo Lefeuvre",
        "Julien Grossholtz",
        "Kateryna Kostiuk",
        "Loïc Siret",
        "Marianne Forget",
        "Maxim Cournoyer",
        "Michel Schmit",
        "Mingrui Zhang",
        "Mohamed Amine Younes Bouacida",
        "Mohamed Chibani",
        "Nicolas Jäger",
        "Nicolas Reynaud",
        "Olivier Gregoire",
        "Olivier Soldano",
        "Patrick Keroulas",
        "Peymane Marandi",
        "Pierre Duchemin",
        "Pierre Lespagnol",
        "Philippe Gorley",
        "Raphaël Brulé",
        "Romain Bertozzi",
        "Saher Azer",
        "Sébastien Blin",
        "Seva Ivanov",
        "Silbino Gonçalves Matado",
        "Simon Désaulniers",
        "Simon Zeni",
        "Stepan Salenikovich",
        "Thibault Wittemberg",
        "Yang Wang",
        "Based on the SFLPhone project",
        NULL,
    };

    GtkWidget *about = (GtkWidget*) g_object_get_data(G_OBJECT(GTK_WINDOW(parent)),
                                                      "gtk-about-dialog");
    if (!about) {
        about = gtk_about_dialog_new();
        g_object_ref_sink(about);
        gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about), "");
        gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about), "© 2021 Savoir-faire Linux");
        gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(about), GTK_LICENSE_GPL_3_0);
        about_dialog_set_theme(GTK_ABOUT_DIALOG(about), use_dark_theme(get_ambient_color(parent)));
        gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), version);
        gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about), _("The GNOME client for Jami.\nJami is free software for universal communication which respects the freedoms and privacy of its users."));
        gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about), authors);
        gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about), "https://jami.net/");
        gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(about), "jami.net");
        gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(about), "https://www.transifex.com/savoirfairelinux/jami");
        g_free(version);

        g_signal_connect(about, "delete-event",
                         G_CALLBACK(gtk_widget_hide_on_delete), NULL);
        g_signal_connect(about, "response",
                         G_CALLBACK(gtk_widget_hide), NULL);
        g_signal_connect(about, "draw",
                         G_CALLBACK(about_dialog_on_redraw), NULL);
        gtk_window_set_modal(GTK_WINDOW(about), TRUE);
        gtk_window_set_transient_for(GTK_WINDOW(about), GTK_WINDOW(parent));
        gtk_window_set_destroy_with_parent(GTK_WINDOW(about), TRUE);
        g_object_set_data_full(G_OBJECT(parent),
                               g_intern_static_string("gtk-about-dialog"),
                               about, g_object_unref);
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_window_present(GTK_WINDOW(about));
    G_GNUC_END_IGNORE_DEPRECATIONS
}
