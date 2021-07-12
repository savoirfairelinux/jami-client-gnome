/*
 *  Copyright (C) 2013-2021 Savoir-faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "client_options.h"

#include "config.h"
#include "client.h"
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

G_GNUC_NORETURN static gboolean
option_version_cb(G_GNUC_UNUSED const gchar *option_name,
                  G_GNUC_UNUSED const gchar *value,
                  G_GNUC_UNUSED gpointer data,
                  G_GNUC_UNUSED GError **error)
{
    g_print("%s\n", VERSION);
    exit(EXIT_SUCCESS);
}

static gboolean
option_debug_cb(G_GNUC_UNUSED const gchar *option_name,
                G_GNUC_UNUSED const gchar *value,
                G_GNUC_UNUSED gpointer data,
                G_GNUC_UNUSED GError **error)
{
    g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
    g_setenv("CHATVIEW_DEBUG", "true", TRUE);
    g_debug("debug enabled");
    return TRUE;
}

static gboolean
option_restore_cb(G_GNUC_UNUSED const gchar *option_name,
                  G_GNUC_UNUSED const gchar *value,
                  G_GNUC_UNUSED gpointer data,
                  G_GNUC_UNUSED GError **error)
{
    GApplication *client = g_application_get_default();
    if (IS_CLIENT(client))
        client_set_restore_main_window_state(CLIENT(client), TRUE);
    return TRUE;
}

static const GOptionEntry all_options[] = {
    {"version", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL},
    {"debug", 'd', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_debug_cb, N_("Enable debug"), NULL},
    {"restore-last-window-state", 'r', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_restore_cb,
     N_("Restores the hidden state of the main window (only applicable to the primary instance)"), NULL},
    {NULL} /* list must be NULL-terminated */
};

#if GLIB_CHECK_VERSION(2,40,0)
void
client_add_options(GApplication *app) {
    /* NOTE: using this function, the options do not get translated in glib versions <2.45 due to
     * bug 750322: https://bugzilla.gnome.org/show_bug.cgi?id=750322
     */
    g_application_add_main_option_entries(app, all_options);
}

#else
GOptionContext *
client_options_get_context()
{
    GOptionContext *context = g_option_context_new(_("- GNOME client for Jami"));
    g_option_context_set_ignore_unknown_options(context, TRUE);
    g_option_context_add_main_entries(context, all_options, PACKAGE_NAME);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
    return context;
}
#endif
