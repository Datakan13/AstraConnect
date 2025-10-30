#include "apiManager.hpp"
#include "boost_include_helpers/includeBoost.hpp"
#include "includeIndicators.hpp"
int main() {
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    APIManager apiManager(ioc,ctx);

}