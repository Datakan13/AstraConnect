#pragma once
#include "utils/decimal/fixedPoint.hpp"
#include <string>
class AccountInfoLite{
    public:
    std::string asset;
    // 1e-8 units, see utils/decimal/fixedPoint.hpp
    AstraConnect::Utils::Fixed balance;
    AstraConnect::Utils::Fixed crossWalletBalance;  
    AstraConnect::Utils::Fixed availableBalance;
    int64_t updateTime; 
};