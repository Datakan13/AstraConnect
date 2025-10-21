#pragma once
#include <cstring>
#include <AstraLib/Logger/FileTypes.hpp>
struct FileHeader {
    size_t MESSAGE_SIZE;
    FileType TYPE;
    public:
    constexpr FileHeader(FileType type_,size_t messageSize_): MESSAGE_SIZE(messageSize_),TYPE(type_){};
    FileHeader() {};
};