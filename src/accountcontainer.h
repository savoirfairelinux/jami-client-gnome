#pragma once

// Lrc
#include <api/account.h>

class AccountContainer {

public:
    explicit AccountContainer (const lrc::api::account::Info& accInfo)
    : info(accInfo)
    {}

    const lrc::api::account::Info& info;
};
