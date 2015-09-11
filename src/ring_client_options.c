/*
 *  Copyright (C) 2013-2015 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "ring_client_options.h"

#include "config.h"
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

G_GNUC_NORETURN static gboolean
option_version_cb(G_GNUC_UNUSED const gchar *option_name,
                  G_GNUC_UNUSED const gchar *value,
                  G_GNUC_UNUSED gpointer data,
                  G_GNUC_UNUSED GError **error)
{
    /* TODO: replace with auto generated version */
    g_print("%d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    exit(EXIT_SUCCESS);
}

static gboolean
option_debug_cb(G_GNUC_UNUSED const gchar *option_name,
                G_GNUC_UNUSED const gchar *value,
                G_GNUC_UNUSED gpointer data,
                G_GNUC_UNUSED GError **error)
{
    g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
    g_debug("debug enabled");
    return TRUE;
}

static const GOptionEntry all_options[] = {
    {"version", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL},
    {"debug", 'd', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_debug_cb, "Enable debug", NULL},
    {NULL} /* list must be NULL-terminated */
};

#if GLIB_CHECK_VERSION(2,40,0)
void
ring_client_add_options(GApplication *app) {
    g_application_add_main_option_entries(app, all_options);
}

#else
GOptionContext *
ring_client_options_get_context()
{
    /* TODO: for some reason the given description and added options aren't printed
     * when '--help' is invoked... possibly a GTK bug.
     */
    GOptionContext *context = g_option_context_new(_("- GNOME client for Ring"));
    g_option_context_set_ignore_unknown_options(context, TRUE);

    /* TODO: add translation domain */
    g_option_context_add_main_entries(context, all_options, NULL);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
    return context;
}
#endif
