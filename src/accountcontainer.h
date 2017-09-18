#pragma once

// Lrc
#include <api/account.h>
#include <qdebug.h>

class AccountContainer {

public:
    explicit AccountContainer (const lrc::api::account::Info& accInfo)
    : info(accInfo)
    {qDebug() << "XOXOXOXOXOXOXOXOXOXOXOXOXOXOXO\n\n\n\n";}

    const lrc::api::account::Info& info;
};
