#pragma once

// Lrc
#include <api/conversation.h>

class ConversationContainer {

public:
    explicit ConversationContainer (const lrc::api::conversation::Info cInfo)
    : info(cInfo)
    {}

    const lrc::api::conversation::Info info;
};
