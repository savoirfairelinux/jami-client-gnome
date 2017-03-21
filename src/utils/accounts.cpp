/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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

#include "accounts.h"

#include <accountmodel.h>

// LRC
#include <QItemSelectionModel>

/**
 * returns TRUE if a RING account exists; FALSE otherwise
 */
gboolean
has_ring_account()
{
    return !AccountModel::instance().getAccountsByProtocol(Account::Protocol::RING).isEmpty();
}

/**
 * itterates through all existing accounts and make sure all RING accounts have
 * a display name set; if a display name is empty, it is set to the alias of the
 * account
 */
void
force_ring_display_name()
{
    auto ringaccounts = AccountModel::instance().getAccountsByProtocol(Account::Protocol::RING);
    for (int i = 0; i < ringaccounts.size(); ++i) {
        auto account = ringaccounts.at(i);
        if (account->displayName().isEmpty()) {
            g_debug("updating RING account display name to: %s", account->alias().toUtf8().constData());
            account->setDisplayName(account->alias());
            account << Account::EditAction::SAVE;
        }
    }
}

/**
 * Finds and returns the first RING account, in order of priority:
 * 1. registered
 * 2. enabled
 * 3. existing
 *
 * Returns a nullptr if no RING acconts exist
 */
Account*
get_active_ring_account()
{
    auto account = AccountModel::instance().userChosenAccount();
    return (account && account->protocol() == Account::Protocol::RING) ? account : nullptr;
}
