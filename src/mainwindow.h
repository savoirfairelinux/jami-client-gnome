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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MAIN_WINDOW_TYPE (main_window_get_type ())
#define MAIN_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), MAIN_WINDOW_TYPE, MainWindow))
#define MAIN_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), MAIN_WINDOW_TYPE, MainWindowClass))
#define IS_MAIN_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), MAIN_WINDOW_TYPE))
#define IS_MAIN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MAIN_WINDOW_TYPE))


typedef struct _MainWindow      MainWindow;
typedef struct _MainWindowClass MainWindowClass;


GType      main_window_get_type (void) G_GNUC_CONST;
GtkWidget *main_window_new      (GtkApplication *app);
void       main_window_reset    (MainWindow *win);
bool       main_window_can_close(MainWindow *win);
void       main_window_display_account_list(MainWindow *win);
void       main_window_search(MainWindow *win);

void main_window_conversations_list(MainWindow *win);
void main_window_requests_list(MainWindow *win);
void main_window_audio_call(MainWindow *win);
void main_window_clear_history(MainWindow *win);
void main_window_remove_conversation(MainWindow *win);
void main_window_block_contact(MainWindow *win);
void main_window_unblock_contact(MainWindow *win);
void main_window_copy_contact(MainWindow *win);
void main_window_add_contact(MainWindow *win);
void main_window_accept_call(MainWindow *win);
void main_window_decline_call(MainWindow *win);
void main_window_toggle_fullscreen(MainWindow *win);

G_END_DECLS
