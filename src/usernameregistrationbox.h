/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
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

class Account;

G_BEGIN_DECLS

#define USERNAME_REGISTRATION_BOX_TYPE            (username_registration_box_get_type ())
#define USERNAME_REGISTRATION_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), USERNAME_REGISTRATION_BOX_TYPE, UsernameRegistrationBox))
#define USERNAME_REGISTRATION_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), USERNAME_REGISTRATION_BOX_TYPE, UsernameRegistrationBoxClass))
#define IS_USERNAME_REGISTRATION_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), USERNAME_REGISTRATION_BOX_TYPE))
#define IS_USERNAME_REGISTRATION_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), USERNAME_REGISTRATION_BOX_TYPE))

typedef struct _UsernameRegistrationBox      UsernameRegistrationBox;
typedef struct _UsernameRegistrationBoxClass UsernameRegistrationBoxClass;

GType      username_registration_box_get_type           (void) G_GNUC_CONST;
GtkWidget *username_registration_box_new                (Account *account, gboolean register_button);
GtkEntry*  username_registration_box_get_entry          (UsernameRegistrationBox *view);
void       username_registration_box_set_use_blockchain (UsernameRegistrationBox* view, gboolean use_blockchain);

G_END_DECLS
