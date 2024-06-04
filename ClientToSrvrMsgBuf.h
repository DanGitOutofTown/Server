#pragma once

#include <array>

namespace ErrorLogger
{
    constexpr int srvrPort{8888};
    constexpr int srvrRunningPort{8889};

    constexpr int captionBufSz{64};
    constexpr int errMsgBufSz{128};
    constexpr int instructionsBufSz{1024};

    struct ClientToSrvrMsgBuf
    {
        std::array<char, captionBufSz> caption;
        std::array<char, errMsgBufSz> errMsg;
        std::array<char, instructionsBufSz> instructions;
    };

    enum class SrvrResponse : int
    {
        Ignore,
        DisablePopups,
        TerminateProcess
    };
}
