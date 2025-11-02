#pragma once
#include <string>
class AccountInfoLite{
    public:
    std::string asset;
    double balance;
    double crossWalletBalance;  
    double availableBalance;
    int64_t updateTime; 
};