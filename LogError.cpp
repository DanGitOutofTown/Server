#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <format>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <ctime>

#include "windows.h"

#include "LogError.h"
#include "ClientToSrvrMsgBuf.h"

namespace ErrorLogger
{
    namespace
    {
        //***** Begin default configuration *****//
        // To be overridden only through ParseConfig()
 
        bool enableLogging = false;
        bool enablePopups = true;
        bool enableUniqueMsgs = true;

        enum class PopupLocation
        {
            Local,
            Remote
        };
        PopupLocation location = PopupLocation::Local;

        int maxErrMsgs{10};
        int maxPopups{5}; // must be <= maxErrMsgs
        int srvrPort{0};
        int clientPort{0};

        std::string srvrSktName;
    
        //***** End default configuration *****//

        // passed into Init()
        std::filesystem::path errFile;
        std::string clientSktName;

        std::vector<std::string> errMsgs;
        int numErrMsgs{0};
        int numPopups{0};

        std::mutex logLock;
        std::mutex popupLock;

        ClientToSrvrMsgBuf srvrMsgBuf;
        SrvrResponse srvrResp;
        int srvrSkt{0};

        bool logErrorFirstPass{true};

        void ParseConfig(const std::filesystem::path& iniFile)
        {
            if (!iniFile.empty())
            {
                ; // parse and override defaults
            }
       }
    }

    inline bool MsgLogged(std::string_view msg)
    {
        return std::find(errMsgs.begin(), errMsgs.end(), msg) != errMsgs.end();
    }

    void Init(const std::filesystem::path& errLogFile, std::string_view clientSktName,
              const std::filesystem::path& iniFile)
    {
        if (!errLogFile.empty())
        {
            // Have a log file so enable logging and popups
            // in case we don't have an iniFile
            enableLogging = true;
            enablePopups = true;
            errFile = errLogFile;
        }

        ParseConfig(iniFile);

        if (!enablePopups)
            return;

        if (location == PopupLocation::Remote)
        {
            if (!srvrSktName.empty() && srvrPort > 0 && clientPort > 0)
            {
                // assign srvrSkt by opening socket to server
                // preset clientPort

                clientSktName.copy(srvrMsgBuf.clientSktName.data(), clientSktName.size()+1, 0);
            }
            else
            {
                enablePopups = false;
                LogError("Communication with remote Popup Server not setup correctly, popups disabled.");
            }
        }
    }

    void PopupError(std::string_view errMsg, std::string_view caption)
    {
        if (!enablePopups || numPopups >= maxPopups)
            return;
            
        const std::lock_guard<std::mutex> lock(popupLock);

        std::string instructions =
            "\nError is now logged, but if possible please leave this"
            "\nmessage up and contact a software engineer to debug the issue."
            "\n\nHit Abort to kill client process to allow for trainer reset."
            "\n\nHit Ignore to continue. Training may be compromised."
            "\n\nHit Retry to disable popups and continue. Training may be compromised.";

        if (enableLogging)
        {
            instructions += "\n\nError is logged to " + std::filesystem::absolute(errFile).string() +
                " and will not be overwritten when trainer is reset or new errors occur. "
                "Please report error at earliest convenience. Thank you.";
        }

        if (location == PopupLocation::Local)
        {
            auto result = MessageBoxA(NULL, (std::string(errMsg) + instructions).data(),
                                      caption.data(), MB_ICONERROR | MB_ABORTRETRYIGNORE | MB_DEFBUTTON3);
            
            switch (result)
            {
                case IDABORT:
                    std::terminate();
                case IDRETRY:
                    enablePopups = false;
                    break;
                case IDIGNORE:
                    break;
                default:
                    break;
            }
            
            numPopups++;
        }
        else if (location == PopupLocation::Remote)
        {
            caption.copy(srvrMsgBuf.caption.data(), caption.size()+1, 0);
            errMsg.copy(srvrMsgBuf.errMsg.data(), errMsg.size()+1, 0);
            instructions.copy(srvrMsgBuf.instructions.data(), instructions.size()+1, 0);

            // send msgBuf to server app using UMP
            // wait for response from server app
            
            switch (srvrResp)
            {
                case SrvrResponse::Ignore:
                    break;
                case SrvrResponse::DisablePopups:
                    enablePopups = false;
                    break;
                case SrvrResponse::TerminateProcess:
                    std::terminate();
                default:
                    break;
            }
            
            numPopups++;
        }
    }

    void LogError(std::string_view errMsg, std::string_view caption, const std::source_location loc)
    {
        if (!(enableLogging || enablePopups))
            return;

        const std::string locStr = std::format("{}:|{}| {}: ", loc.file_name(), loc.line(), loc.function_name());

        std::ostringstream oss;
        oss << locStr << caption << ": " << errMsg << std::endl;

        PopupError(oss.str(), caption);

        if (!enableLogging || (enableUniqueMsgs && MsgLogged(locStr)))
        {
            return;
        }

        const std::lock_guard<std::mutex> lock(logLock);

        std::ofstream ofs;

        if (numErrMsgs < maxErrMsgs)
        {
            if (logErrorFirstPass)
            {
                if (std::filesystem::exists(errFile))
                {
                    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

                    constexpr int maxDateTimeLen{20};
                    char dateTimeBuf[maxDateTimeLen] = {0};

                    struct tm newtime{0};
                    localtime_s(&newtime, &now);

                    std::strftime(dateTimeBuf, sizeof(dateTimeBuf), "_%y-%m-%d_%H-%M-%S", &newtime);

                    std::filesystem::path parent = errFile.parent_path().string();
                    if (!parent.empty())
                    {
                        parent += "/";
                    }
                    std::filesystem::path dest = parent.string() + errFile.stem().string() +
                                                 dateTimeBuf + errFile.extension().string();

                    std::filesystem::rename(errFile, dest);
                }
                else if (enablePopups && !std::filesystem::exists(errFile.parent_path()) &&
                         !std::filesystem::create_directories(errFile.parent_path()))
                {
                    PopupError(locStr + ": " + "Can't create directory " +
                               errFile.parent_path().string() + ", logging stopped.", "Error");
                    enableLogging = false;
                }

                ofs.open(errFile);
                logErrorFirstPass = false;
            }
            else
            {
                ofs.open(errFile, std::ios::app);
            }

            if (ofs)
            {
                ofs << oss.str();
                ofs.close();
            }
            else
            {
                PopupError(locStr + ": " + "Can't open " + errFile.string() +
                           " for write, logging stopped.", "Error");
                enableLogging = false;
            }

            errMsgs.push_back(locStr);
            numErrMsgs++;
        }
        else if (numErrMsgs == maxErrMsgs)
        {
            ofs.open(errFile, std::ios::app);

            std::string msg = "Max messages of " + std::to_string(maxErrMsgs) +
                              " exceeded, logging stopped";

            if (ofs)
            {
                ofs << msg << std::endl;
                ofs.close();
            }
            else
            {
                PopupError(locStr + ": " + "Can't open " + errFile.string() +
                           " for append, logging stopped.", "Error");
            }

            enableLogging = false;
            enablePopups = false;
        }
    }
}
