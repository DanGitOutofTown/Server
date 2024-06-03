#pragma once

#include <filesystem>
#include <string_view>
#include <source_location>

namespace ErrorLogger
{
    void Init(const std::filesystem::path& errLogFile, std::string_view clientSktName,
              const std::filesystem::path& iniFile);
    
    void LogError(std::string_view errMsg, std::string_view caption = "Error",
                  const std::source_location loc = std::source_location::current());
    
    void PopupError(std::string_view errMsg, std::string_view caption);
}