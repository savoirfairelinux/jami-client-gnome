/*
 *  Copyright (C) 2015-2021 Savoir-faire Linux Inc.
 *  Author: Sebastien Blin <sebastien.blin@savoirfairelinux.com>
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
#include <QString>

#include "accountinfopointer.h"

#define PROFILE_VIEW_TYPE (profile_view_get_type ())
#define IS_PROFILE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PROFILE_VIEW_TYPE))
#define IS_PROFILE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PROFILE_VIEW_TYPE))

G_DECLARE_FINAL_TYPE (ProfileView, profile_view, PROFILE, VIEW, GtkDialog)

GtkWidget*        profile_view_new(AccountInfoPointer const & accountInfo, const QString& uid);