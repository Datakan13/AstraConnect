#include <api/orderbook/topBook.hpp>
#include <vector>
#include <AstraLib/AstraLib.hpp>
int main() {
    PriceLevel priceLevel(1000.0,0.1);
    std::vector<double> vec;
    vec.reserve(6);
    double priceFirst= 1000.3;
    double priceScnd = 1000.4;
    double priceThrd = 1000.1;
    double priceSxth = 1000.0;
    double priceFth = 1001.0;
    AstraLib::Time::Timer timer;
    timer.start();
    priceLevel.modifyPriceLevels(std::pair<double,bool>(priceFirst,true),std::pair<double,bool>(priceScnd,true),
    std::pair<double,bool>(priceThrd,true),std::pair<double,bool>(priceFth,true),std::pair<double,bool>(priceSxth,true));
    timer.write();
    timer.start();
    priceLevel.findTopOfLevel(6,vec);
    timer.write();
    for(double a : vec) {
        std::cout << a << std::endl;
    }
}