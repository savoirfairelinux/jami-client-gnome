#pragma once

// Lrc
#include <data/account.h>

class AccountContainer {

public:
    explicit AccountContainer (const lrc::account::Info& info)
    : accInfo(info)
    {}

    const lrc::account::Info& accInfo;
};
