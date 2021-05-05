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

#include "calls.h"

// LRC
#include "api/conversation.h"
#include "api/newcallmodel.h"

bool
call_is_current_host(const lrc::api::conversation::Info& conversation,
                     AccountInfoPointer const & accountInfo)
{
    if (!(&conversation)->uid.isEmpty()) {
        try {
            auto confId = (&conversation)->confId;
            if (confId.isEmpty()) {
                confId = (&conversation)->callId;
            }
            auto call = accountInfo->callModel->getCall(confId);
            if (call.participantsInfos.size() == 0) {
                return true;
            } else {
                return !(&conversation)->confId.isEmpty()
                    && accountInfo->callModel->hasCall((&conversation)->confId);
            }
        } catch (...) {}
    }
    return true;
}

bool
call_is_host(const lrc::api::conversation::Info& conversation,
             AccountInfoPointer const & accountInfo,
             const QString& uri)
{
    if (!(&conversation)->uid.isEmpty()) {
        try {
            if (call_is_current_host(conversation, accountInfo)) {
                return uri == accountInfo->profileInfo.uri;
            } else {
                auto call = accountInfo->callModel->getCall((&conversation)->callId);
                auto peer = call.peerUri.remove("ring:");
                return uri == peer;
            }
        } catch (...) {}
    }
    return true;
}

bool
call_is_current_moderator(const lrc::api::conversation::Info& conversation,
                          AccountInfoPointer const & accountInfo)
{
    if (!(&conversation)->uid.isEmpty()) {
        try {
            auto call = accountInfo->callModel->getCall((&conversation)->callId);
            if (call.participantsInfos.size() == 0) {
                return true;
            } else {
                for (const auto& participant : call.participantsInfos) {
                    if (participant["uri"] == accountInfo->profileInfo.uri)
                        return participant["isModerator"] == "true";
                }
            }
            return false;
        } catch (...) {}
    }
    return true;
}

bool
call_is_moderator(const lrc::api::conversation::Info& conversation,
                  AccountInfoPointer const & accountInfo,
                  const QString& uri)
{
    auto confId = (&conversation)->confId;
    if (confId.isEmpty()) {
        confId = (&conversation)->callId;
    }
    try {
        return accountInfo->callModel->isModerator(confId, uri);
    } catch (...) {}
    return false;
}

void
call_set_moderator(const lrc::api::conversation::Info& conversation,
                   AccountInfoPointer const & accountInfo,
                   const QString& uri, const bool state)
{
    auto confId = (&conversation)->confId;
    if (confId.isEmpty()) {
        confId = (&conversation)->callId;
    }
    try {
        accountInfo->callModel->setModerator(confId, uri, state);
    } catch (...) {}
}

CallMuteState
call_get_mute_state(const lrc::api::conversation::Info& conversation,
                    AccountInfoPointer const & accountInfo,
                    const QString& uri)
{
    auto confId = (&conversation)->confId;
    if (confId.isEmpty()) {
        confId = (&conversation)->callId;
    }
    try {
        auto call = accountInfo->callModel->getCall(confId);
        if (call.participantsInfos.size() == 0) {
            return CallMuteState::UNMUTED;
        } else {
            for (const auto& participant : call.participantsInfos) {
                if (participant["uri"] == uri) {
                    if (participant["audioLocalMuted"] == "true") {
                        if (participant["audioModeratorMuted"] == "true") {
                            return CallMuteState::BOTH_MUTED;
                        } else {
                            return CallMuteState::LOCAL_MUTED;
                        }
                    } else if (participant["audioModeratorMuted"] == "true") {
                        return CallMuteState::MODERATOR_MUTED;
                    }
                    return CallMuteState::UNMUTED;
                }
            }
        }
        return CallMuteState::UNMUTED;
    } catch (...) {}
    return CallMuteState::UNMUTED;
}

void
call_mute_participant(const lrc::api::conversation::Info& conversation,
                      AccountInfoPointer const & accountInfo,
                      const QString& uri, const bool state)
{
    auto confId = (&conversation)->confId;
    if (confId.isEmpty()) {
        confId = (&conversation)->callId;
    }
    try {
        accountInfo->callModel->muteParticipant(confId, uri, state);
    } catch (...) {}
}

void
call_toggle_mute_current_call(const lrc::api::conversation::Info& conversation,
                              AccountInfoPointer const & accountInfo)
{
    auto callId = (&conversation)->callId;
    if (!callId.isEmpty() && accountInfo->callModel->hasCall(callId)) {
        accountInfo->callModel->toggleMedia(callId,
            lrc::api::NewCallModel::Media::AUDIO);
    }
}

void
call_hangup_participant(const lrc::api::conversation::Info& conversation,
                        AccountInfoPointer const & accountInfo,
                        const QString& uri)
{
    auto confId = (&conversation)->confId;
    if (confId.isEmpty()) {
        confId = (&conversation)->callId;
    }
    try {
        accountInfo->callModel->hangupParticipant(confId, uri);
    } catch (...) {}
}
