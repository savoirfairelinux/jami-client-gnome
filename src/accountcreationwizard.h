/*
 *  Copyright (C) 2016-2022 Savoir-faire Linux Inc.
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

#include <string>

#include <api/account.h>

#include "accountinfopointer.h"

namespace lrc
{
namespace api
{
    class AVModel;
    class NewAccountModel;
}
}

G_BEGIN_DECLS

#define ACCOUNT_CREATION_WIZARD_TYPE            (account_creation_wizard_get_type ())
#define ACCOUNT_CREATION_WIZARD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCOUNT_CREATION_WIZARD_TYPE, AccountCreationWizard))
#define ACCOUNT_CREATION_WIZARD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), ACCOUNT_CREATION_WIZARD_TYPE, AccountCreationWizardClass))
#define IS_ACCOUNT_CREATION_WIZARD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), ACCOUNT_CREATION_WIZARD_TYPE))
#define IS_ACCOUNT_CREATION_WIZARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ACCOUNT_CREATION_WIZARD_TYPE))

typedef struct _AccountCreationWizard      AccountCreationWizard;
typedef struct _AccountCreationWizardClass AccountCreationWizardClass;

GType      account_creation_wizard_get_type      (void) G_GNUC_CONST;
GtkWidget *account_creation_wizard_new           (lrc::api::AVModel& avModel, lrc::api::NewAccountModel& accountModel, bool useDarkTheme);

void       account_creation_wizard_show_preview  (AccountCreationWizard *win, gboolean show_preview = TRUE);
void       show_rendezvous_creation_wizard       (AccountCreationWizard *win);
void       account_creation_wizard_account_added (AccountCreationWizard *view, const std::string& id);
void       account_creation_show_error_view      (AccountCreationWizard *view, const std::string& id);

G_END_DECLS
