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

#include "files.h"

#include <memory>
#include "config.h"
#include <unistd.h> // to create symlinks
#include <errno.h>
#include <glib/gstdio.h> // for glib file utils
#include <string.h>

void
autostart_symlink(gboolean autostart)
{
    /* autostart is enabled by creating a symlink to gnome-ring.desktop in
     * $XDG_CONFIG_HOME/autostart (by default ~/.config/autostart)
     * and removing it to disable autostart
     */

    GError *error = NULL;
    gchar *autostart_path = g_strconcat(g_get_user_config_dir(), "/autostart/gnome-ring.desktop", NULL);

    if (autostart) {
        g_debug("enabling autostart");

        gchar *desktop_path = NULL;
        /* find where the .desktop file is by checking the following dirs in order
         *  - /usr/<data dir>
         *  - /usr/local/<data dir>
         *  - install data dir (check this last, as it might not be where the
         *    bin is actually isntalled on the system)
         *
         *  note: no point in checking the local bin dir, even if the .desktop is
         *  there, if the binary is not installed on the system, the .desktop will
         *  not find it
         */
        int num_paths = 3;
        gchar *desktop_paths[num_paths];
        desktop_paths[0] = g_strconcat("/usr", RING_DATA_DIR, "/gnome-ring.desktop", NULL);
        desktop_paths[1] = g_strconcat("/usr/local", RING_DATA_DIR, "/gnome-ring.desktop", NULL);
        desktop_paths[2] = g_strconcat(RING_CLIENT_INSTALL, RING_DATA_DIR, "/gnome-ring.desktop", NULL);

        for (int i = 0; i < num_paths && !desktop_path; ++i) {
            g_debug("checking %s", desktop_paths[i]);
            if (g_file_test(desktop_paths[i], G_FILE_TEST_IS_REGULAR))
                desktop_path = desktop_paths[i];
        }

        if (!desktop_path) {
            /* could not find where the .desktop is... default to the install one */
            desktop_path = desktop_paths[1];
            g_warning("cannot locate '%s', will try to create a symlink to it anyways", desktop_path);
        }

        /* we want autostart_path to be a symlink to the current desktop_path
         * if it is not, delete it and create one */

        gboolean symlink_to_desktop = FALSE;
        if (g_file_test(autostart_path, G_FILE_TEST_IS_SYMLINK)) {
            /* is symlink, check if its to the right file */
            gchar *current_link = g_file_read_link(autostart_path, &error);
            if (error) {
                g_warning("could not read contents of symlink '%s': %s",autostart_path, error->message);
                g_clear_error(&error);
            }

            /* compare even if error occurs, as function handles nullptr */
            if (g_strcmp0(current_link, desktop_path) == 0) {
                g_debug("'%s' is already a symlink to '%s'", autostart_path, desktop_path);
                symlink_to_desktop = TRUE;
            } else {
                g_debug("'%s' exists but does not point to '%s', instead: '%s'", autostart_path, desktop_path, current_link);

                /* we need to delete it */
                if (g_remove(autostart_path) != 0) {
                    g_warning("could not remove '%s'", autostart_path);
                }
            }
            g_free(current_link);
        }

        if (!symlink_to_desktop) {
            g_debug("creating symlink");

            /* make sure the directory exists */
            gchar *autostart_dir = g_strconcat(g_get_user_config_dir(), "/autostart", NULL);
            if (g_mkdir_with_parents(autostart_dir, 0700) != 0)
                g_warning("'%s' dir doesn't exist and could not be created", autostart_dir);
            g_free(autostart_dir);

            if (symlink(desktop_path, autostart_path) != 0) {
                g_warning("could not create symlink: %s", strerror(errno));
            }
        }

        for (int i = 0; i < num_paths; ++i) {
            g_free(desktop_paths[i]);
        }
    } else {
        g_debug("disabling autostart");
        /* need to remove symlink or .desktop, if it exists
         * G_FILE_TEST_IS_REGULAR will test for both file and symlink, since it
         * follows symlinks */
        if (g_file_test(autostart_path, G_FILE_TEST_IS_REGULAR)) {
            g_debug("'%s' exists, removing", autostart_path);

            if (g_remove(autostart_path) != 0) {
                g_warning("could not remove '%s'", autostart_path);
            }
        } else {
            g_debug("'%s' doesn't exist, nothing to do", autostart_path);
        }

    }
    g_free(autostart_path);
}

GSettingsSchema *
get_ring_schema()
{
    static std::unique_ptr<GSettingsSchema, decltype(g_settings_schema_unref )&>
        ring_schema(nullptr, g_settings_schema_unref);

    if (ring_schema.get() == nullptr) {
        GSettingsSchema *schema = NULL;

        /* find gschema.compiled by checking the following dirs in order:
         *  - current bin dir
         *  - install data dir
         *  - default dir
         * note that the install and default dir may be the same
         */
        GError *error = NULL;

        /* try local first */
        g_debug("looking for schema locally");
        gchar *schema_dir_local = g_strconcat(".", NULL);
        GSettingsSchemaSource *schema_source_local = NULL;
        schema_source_local = g_settings_schema_source_new_from_directory(
            schema_dir_local,
            g_settings_schema_source_get_default(),
            FALSE,
            &error);

        if (!error) {
            schema = g_settings_schema_source_lookup(schema_source_local,
                                                     RING_CLIENT_APP_ID,
                                                     TRUE);
            g_settings_schema_source_unref(schema_source_local);
        } else {
            g_debug("error looking up ring schema locally: %s", error->message);
            g_clear_error(&error);
        }
        g_free(schema_dir_local);

        if (!schema) {
            /* try install dir */
            g_debug("looking for schema in insall dir");
            gchar *schema_dir_install = g_strconcat(RING_CLIENT_INSTALL, "/share/glib-2.0/schemas", NULL);
            GSettingsSchemaSource *schema_source_install = NULL;
            schema_source_install = g_settings_schema_source_new_from_directory(
                schema_dir_install,
                g_settings_schema_source_get_default(),
                TRUE,
                &error);

            if (!error) {
                schema = g_settings_schema_source_lookup(schema_source_install,
                                                         RING_CLIENT_APP_ID,
                                                         TRUE);
                g_settings_schema_source_unref(schema_source_install);
            } else {
                g_debug("error looking up ring schema in install dir: %s", error->message);
                g_clear_error(&error);
            }
            g_free(schema_dir_install);
        }

        if (!schema) {
            /* try default dir */
            g_debug("looking for schema in default dir");

            schema = g_settings_schema_source_lookup(g_settings_schema_source_get_default(),
                                                     RING_CLIENT_APP_ID,
                                                     TRUE);
        }
        ring_schema.reset(schema);
    }

    return ring_schema.get();
}
