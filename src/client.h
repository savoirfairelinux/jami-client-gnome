/*
 *  Copyright (C) 2015-2022 Savoir-faire Linux Inc.
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

#ifndef CLIENT_H_
#define CLIENT_H_

#include <gtk/gtk.h>
#include "config.h"

G_BEGIN_DECLS

#define GSETTINGS_SCHEMA JAMI_CLIENT_APP_ID

#define CLIENT_TYPE (client_get_type())
#define CLIENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), CLIENT_TYPE, Client))
#define CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CLIENT_TYPE, ClientClass))
#define IS_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CLIENT_TYPE))
#define IS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CLIENT_TYPE))

typedef struct _ClientClass   ClientClass;
typedef struct _Client        Client;

/* Public interface */
GType       client_get_type (void) G_GNUC_CONST;
Client *client_new      (int argc, char *argv[]);
GtkWindow  *client_get_main_window(Client *client);

/**
 * Sets if the client should attempt to restore the main window state (hidden or not) to what it was
 * when it was last quit (stored by the "show-main-window" gsetting). This function must be
 * called before the main window is created for the first time for it to have an effect.
 */
void        client_set_restore_main_window_state(Client *client, gboolean restore);

G_END_DECLS

#endif /* CLIENT_H_ */
