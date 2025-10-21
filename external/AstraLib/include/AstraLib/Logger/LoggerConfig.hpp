#pragma once
#include <string>
#include <AstraLib/Logger/FileHeader.hpp>
#include <AstraLib/Logger/FileTypes.hpp>
struct LoggerConfig {
    static constexpr size_t QUEUE_SIZE = 512;
    static constexpr size_t MESSAGE_SIZE = 128;
    static constexpr std::string_view DEFAULT_FILE = "Logs/Logs.bin";
    static constexpr FileType DEFAULT_FILE_TYPE = FileType::Unknown;
    static constexpr FileHeader header{DEFAULT_FILE_TYPE ,MESSAGE_SIZE};
};
