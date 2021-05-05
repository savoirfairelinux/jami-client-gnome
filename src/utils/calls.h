/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *  Author: Amin Bandali <amin.bandali@savoirfairelinux.com>
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

#ifndef __CALLS_H__
#define __CALLS_H__

// LRC
#include "api/account.h"

#include "../accountinfopointer.h"

namespace lrc {
    namespace api {
        namespace conversation {
            struct Info;
        }
    }
}

/* G_BEGIN_DECLS */

enum class CallMuteState {
    UNMUTED,
    LOCAL_MUTED,
    MODERATOR_MUTED,
    BOTH_MUTED
};

bool call_is_current_host(
    const lrc::api::conversation::Info& conversation,
    AccountInfoPointer const & accountInfo);
bool call_is_host(
    const lrc::api::conversation::Info& conversation,
    AccountInfoPointer const & accountInfo,
    const QString& uri);
bool call_is_current_moderator(
    const lrc::api::conversation::Info& conversation,
    AccountInfoPointer const & accountInfo);
bool call_is_moderator(
    const lrc::api::conversation::Info& conversation,
    AccountInfoPointer const & accountInfo,
    const QString& uri);
void call_set_moderator(
    const lrc::api::conversation::Info& conversation,
    AccountInfoPointer const & accountInfo,
    const QString& uri, const bool state);
CallMuteState call_get_mute_state(
    const lrc::api::conversation::Info& conversation,
    AccountInfoPointer const & accountInfo,
    const QString& uri);
void call_mute_participant(
    const lrc::api::conversation::Info& conversation,
    AccountInfoPointer const & accountInfo,
    const QString& uri, const bool state);
void call_toggle_mute_current_call(
    const lrc::api::conversation::Info& conversation,
    AccountInfoPointer const & accountInfo);
void call_hangup_participant(
    const lrc::api::conversation::Info& conversation,
    AccountInfoPointer const & accountInfo,
    const QString& uri);

/* G_END_DECLS */

#endif /* __CALLS_H__ */
