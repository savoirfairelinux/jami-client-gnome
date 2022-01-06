/*
 *  Copyright (C) 2015-2022 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#ifndef _NEWACCOUNTSETTINGSVIEW_H
#define _NEWACCOUNTSETTINGSVIEW_H

#include <gtk/gtk.h>

#include <api/account.h>

#include "accountinfopointer.h"

namespace lrc
{
namespace api
{
    class AVModel;
}
}

G_BEGIN_DECLS

#define NEW_ACCOUNT_SETTINGS_VIEW_TYPE            (new_account_settings_view_get_type ())
#define NEW_ACCOUNT_SETTINGS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEW_ACCOUNT_SETTINGS_VIEW_TYPE, NewAccountSettingsView))
#define NEW_ACCOUNT_SETTINGS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), NEW_ACCOUNT_SETTINGS_VIEW_TYPE, NewAccountSettingsViewClass))
#define IS_NEW_ACCOUNT_SETTINGS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), NEW_ACCOUNT_SETTINGS_VIEW_TYPE))
#define IS_NEW_ACCOUNT_SETTINGS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), NEW_ACCOUNT_SETTINGS_VIEW_TYPE))

typedef struct _NewAccountSettingsView      NewAccountSettingsView;
typedef struct _NewAccountSettingsViewClass NewAccountSettingsViewClass;

GType      new_account_settings_view_get_type      (void) G_GNUC_CONST;
GtkWidget *new_account_settings_view_new           (AccountInfoPointer const & accountInfo, lrc::api::AVModel& avModel);
void       new_account_settings_view_show          (NewAccountSettingsView *view, gboolean show_profile);
void       new_account_settings_view_update        (NewAccountSettingsView *view, gboolean reset_view = true);
void       new_account_settings_view_save_account  (NewAccountSettingsView *view);

G_END_DECLS

#endif /* _NEWACCOUNTSETTINGSVIEW_H */
