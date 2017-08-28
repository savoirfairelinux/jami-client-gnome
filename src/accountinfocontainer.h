#pragma once

#include <lrc.h>
#include <data/account.h>

class AccountInfoContainer
{
public:
    explicit AccountInfoContainer(lrc::account::Info& a)
    : accountInfo(a) {}

    lrc::account::Info& accountInfo;
    lrc::Lrc lrc;
};
