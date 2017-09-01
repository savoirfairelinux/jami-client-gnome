#pragma once

#include <lrc.h>
#include <data/account.h>

#include <iostream>

class AccountInfoContainer
{
public:
    explicit AccountInfoContainer(lrc::account::Info& a)
    : accountInfo(a) {std::cout << "######" << std::endl;}

    lrc::account::Info& accountInfo;
    lrc::Lrc lrc;
};
