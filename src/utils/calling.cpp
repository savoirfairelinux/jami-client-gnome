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

#include "calling.h"

#include <contactmethod.h>
#include <callmodel.h>
#include <QtCore/QItemSelectionModel>

// LRC
#include <accountmodel.h>

void
place_new_call(ContactMethod *n, Account *acc)
{
    /* check if this CM already has an ongoing call; likely we want the most recent one, so we
     * check the CallModel in reverse order
     */
    auto call_list = CallModel::instance().getActiveCalls();
    Call* call = nullptr;
    for (int i = call_list.size() - 1; i > -1 && call == nullptr; --i) {
        if (call_list.at(i)->peerContactMethod() == n) {
            call = call_list.at(i);
        }
    }

    /* use the selected account if none was passed to the function */
    if (not acc)
        acc = AccountModel::instance().selectedAccount();

    if (n->account() != acc)
        n->setAccount(acc);

    if (!call) {
        /* didn't find an existing call, so create a new one */
        call = CallModel::instance().dialingCall(n);
        call->setAccount(acc); // force account
        call->performAction(Call::Action::ACCEPT);
    }

    /* make this the currently selected call */
    CallModel::instance().selectCall(call);
}
