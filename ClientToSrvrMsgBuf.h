#pragma once

#include <array>

namespace ErrorLogger
{
    constexpr int captionBufSz{32};
    constexpr int clientSktNameBufSz{32};
    constexpr int errMsgBufSz{80};
    constexpr int errFileNameBufSz{256};
    constexpr int instructionsBufSz{1024};

    struct ClientToSrvrMsgBuf
    {
        std::array<char, captionBufSz> caption;
        std::array<char, clientSktNameBufSz> clientSktName;
        std::array<char, errMsgBufSz> errMsg;
        std::array<char, errFileNameBufSz> errFileName;
        std::array<char, instructionsBufSz> instructions;
    };

    enum class SrvrResponse : int
    {
        Ignore,
        DisablePopups,
        TerminateProcess
    };
}
