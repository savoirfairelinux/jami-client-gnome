/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "accounts.h"

#include <accountmodel.h>

/**
 * returns TRUE if a RING account exists; FALSE otherwise
 */
gboolean
has_ring_account()
{
    for (int i = 0; i < AccountModel::instance()->rowCount(); ++i) {
        auto idx = AccountModel::instance()->index(i, 0);
        if (idx.isValid()) {
            auto protocol = idx.data(static_cast<int>(Account::Role::Proto)).toUInt();
            if ((Account::Protocol)protocol == Account::Protocol::RING)
                return TRUE;
        }
    }

    return FALSE;
}

/**
 * itterates through all existing accounts and make sure all RING accounts have
 * a display name set; if a display name is empty, it is set to the alias of the
 * account
 */
void
force_ring_display_name()
{
    for (int i = 0; i < AccountModel::instance()->rowCount(); ++i) {
        QModelIndex idx = AccountModel::instance()->index(i, 0);
        if (idx.isValid()) {
            auto protocol = idx.data(static_cast<int>(Account::Role::Proto)).toUInt();
            if ((Account::Protocol)protocol == Account::Protocol::RING) {
                auto displayName = idx.data(static_cast<int>(Account::Role::DisplayName)).toString();
                if (displayName.isEmpty()) {
                    AccountModel::instance()->setData(
                        idx,
                        idx.data(static_cast<int>(Account::Role::Alias)),
                        static_cast<int>(Account::Role::Proto));
                }
            }
        }
    }
}
