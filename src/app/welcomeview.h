/*
 *  Copyright (C) 2015-2022 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Nicolas Jäger <nicolas.jager@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "api/account.h"

#include "accountinfopointer.h"

G_BEGIN_DECLS

#define WELCOME_VIEW_TYPE            (welcome_view_get_type ())
#define WELCOME_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WELCOME_VIEW_TYPE, WelcomeView))
#define WELCOME_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), WELCOME_VIEW_TYPE, WelcomeViewClass))
#define IS_WELCOME_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WELCOME_VIEW_TYPE))
#define IS_WELCOME_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WELCOME_VIEW_TYPE))

typedef struct _WelcomeView      WelcomeView;
typedef struct _WelcomeViewClass WelcomeViewClass;

GType             welcome_view_get_type (void) G_GNUC_CONST;
GtkWidget*        welcome_view_new      (AccountInfoPointer const & accountInfo);
void              welcome_update_view   (WelcomeView* self);
void              welcome_set_theme     (WelcomeView* self, bool useDarkTheme);

G_END_DECLS
