// Offline parsing of every Binance payload the SDK models, from the samples in
// doc/payloads/ (cleaned into valid JSON: no comments, no trailing commas).
//
//   1. user data events      — each ORDER_TRADE_UPDATE, ACCOUNT_UPDATE, ... sample
//                              through parseUserDataStream, field by field.
//   2. market streams        — parseTradeEvent and parseMarkPrice.
//   3. ws-api responses      — order.place / modify / cancel and the error envelope.
//   4. error helpers         — getErrorFromDocument, iterateResponse, fromHttpStatus.
//   5. enum mapping          — every exchange string, and what an unknown one does.
//   6. mutated payloads      — every fixture truncated at every byte and with bytes
//                              replaced: the parsers must return or throw a
//                              std::exception, never crash. Meant for the ASan build.
//
// Confirmed library bugs not yet fixed are KNOWN_BUG checks, see testHarness.hpp.
#include "testHarness.hpp"

#include <api/common/factory/userDataStreamFactory.hpp>
#include <api/common/factory/tradeEventFactory.hpp>
#include <api/common/factory/markPriceFactory.hpp>
#include <api/common/error/fetchError.hpp>
#include <api/futures/order/orderResponseEvents.hpp>

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <functional>
#include <new>
#include <string>
#include <vector>

using AstraConnect::Utils::Fixed;
using AstraConnect::Utils::parseFixed;

// Decimal string to 1e-8 units, so expectations read like the payload
static Fixed F(const char* text) {
    Fixed v = 0;
    if (!parseFixed(text, v)) throw std::invalid_argument(text);
    return v;
}

static simdjson::ondemand::parser g_parser;

static UserDataStream parseUser(const std::string& json) {
    simdjson::padded_string padded(json);
    return parseUserDataStream(padded, g_parser);
}

// The payload of a parsed event, read through returnPtr(), which is non-null only
// when the event parsed as the expected type. getUserDataStreamData<T>() would be
// the public way, but it does not compile: the event classes have no default
// constructor, which its Data{} fallback needs.
template <typename Data>
struct Parsed {
    UserDataStream stream;
    const Data* data = nullptr;
    explicit operator bool() const { return data != nullptr; }
    const Data& operator*() const { return *data; }
};

template <typename Data>
static Parsed<Data> userEvent(const std::string& json) {
    Parsed<Data> p{parseUser(json)};
    p.data = static_cast<const Data*>(p.stream.returnPtr());
    CHECK_CTX(p.data, "event did not parse as the expected type");
    return p;
}

template <typename Fn>
static bool throws(Fn&& fn) {
    try { fn(); } catch (std::exception&) { return true; }
    return false;
}

// ---------------------------------------------------------------- fixtures

static const std::string ORDER_UPDATE_TRAILING = R"({
  "e":"ORDER_TRADE_UPDATE","E":1568879465651,"T":1568879465650,
  "o":{"s":"BTCUSDT","c":"TEST","S":"SELL","o":"TRAILING_STOP_MARKET","f":"GTC",
    "q":"0.001","p":"0","ap":"0","sp":"7103.04","x":"NEW","X":"NEW","i":8886774,
    "l":"0","z":"0","L":"0","N":"USDT","n":"0","T":1568879465650,"t":0,
    "b":"0","a":"9.91","m":false,"R":false,"wt":"CONTRACT_PRICE",
    "ot":"TRAILING_STOP_MARKET","ps":"LONG","cp":false,"AP":"7476.89","cr":"5.0",
    "pP":false,"si":0,"ss":0,"rp":"0","V":"EXPIRE_TAKER","pm":"OPPONENT","gtd":0,"er":"0"}
})";

// A filled LIMIT order: no AP / cr, which are only pushed for trailing stops
static const std::string ORDER_UPDATE_LIMIT_FILL = R"({
  "e":"ORDER_TRADE_UPDATE","E":1700000000100,"T":1700000000099,
  "o":{"s":"ETHUSDT","c":"myLimit","S":"BUY","o":"LIMIT","f":"GTX",
    "q":"1.500","p":"2250.37","ap":"2250.37","sp":"0","x":"TRADE","X":"FILLED","i":42,
    "l":"1.500","z":"1.500","L":"2250.37","N":"USDT","n":"0.33755550","T":1700000000098,"t":77,
    "b":"0","a":"0","m":true,"R":true,"wt":"MARK_PRICE",
    "ot":"LIMIT","ps":"BOTH","cp":false,
    "pP":true,"si":0,"ss":0,"rp":"-12.5","V":"NONE","pm":"NONE","gtd":0,"er":"6"}
})";

static const std::string ACCOUNT_UPDATE = R"({
  "e":"ACCOUNT_UPDATE","E":1564745798939,"T":1564745798938,
  "a":{"m":"ORDER",
    "B":[{"a":"USDT","wb":"122624.12345678","cw":"100.12345678","bc":"50.12345678"},
         {"a":"BUSD","wb":"1.00000000","cw":"0.00000000","bc":"-49.12345678"}],
    "P":[{"s":"BTCUSDT","pa":"0","ep":"0.00000","bep":"0","cr":"200","up":"0","mt":"isolated","iw":"0.00000000","ps":"BOTH"},
         {"s":"BTCUSDT","pa":"20","ep":"6563.66500","bep":"0","cr":"0","up":"2850.21200","mt":"isolated","iw":"13200.70726908","ps":"LONG"},
         {"s":"BTCUSDT","pa":"-10","ep":"6563.86000","bep":"6563.6","cr":"-45.04000000","up":"-1423.15600","mt":"cross","iw":"6570.42511771","ps":"SHORT"}]}
})";

// FUNDING FEE on a crossed position: only "B" is pushed
static const std::string ACCOUNT_UPDATE_FUNDING = R"({
  "e":"ACCOUNT_UPDATE","E":1564745798939,"T":1564745798938,
  "a":{"m":"FUNDING_FEE","B":[{"a":"USDT","wb":"10.5","cw":"10.5","bc":"-0.0012"}]}
})";

static const std::string MARGIN_CALL_ISOLATED = R"({
  "e":"MARGIN_CALL","E":1587727187525,"cw":"3.16812045",
  "p":[{"s":"ETHUSDT","ps":"LONG","pa":"1.327","mt":"ISOLATED","iw":"0","mp":"187.17127","up":"-1.166074","mm":"1.614445"}]
})";

// As documented: crossed positions report "CROSSED"
static const std::string MARGIN_CALL_CROSSED = R"({
  "e":"MARGIN_CALL","E":1587727187525,"cw":"3.16812045",
  "p":[{"s":"ETHUSDT","ps":"LONG","pa":"1.327","mt":"CROSSED","iw":"0","mp":"187.17127","up":"-1.166074","mm":"1.614445"}]
})";

static const std::string TRADE_LITE = R"({
  "e":"TRADE_LITE","E":1721895408092,"T":1721895408214,"s":"BTCUSDT","q":"0.001","p":"0",
  "m":false,"c":"z8hcUoOsqEdKMeKPSABslD","S":"BUY","L":"64089.20","l":"0.040","t":109100866,"i":8886774
})";

static const std::string ACCOUNT_CONFIG_LEVERAGE = R"({
  "e":"ACCOUNT_CONFIG_UPDATE","E":1611646737479,"T":1611646737476,"ac":{"s":"BTCUSDT","l":25}
})";

static const std::string ACCOUNT_CONFIG_MULTI_ASSET = R"({
  "e":"ACCOUNT_CONFIG_UPDATE","E":1611646737479,"T":1611646737476,"ai":{"j":true}
})";

static const std::string STRATEGY_UPDATE = R"({
  "e":"STRATEGY_UPDATE","T":1669261797627,"E":1669261797628,
  "su":{"si":176054594,"st":"GRID","ss":"NEW","s":"BTCUSDT","ut":1669261797627,"c":8007}
})";

static const std::string GRID_UPDATE = R"({
  "e":"GRID_UPDATE","T":1669262908216,"E":1669262908218,
  "gu":{"si":176057039,"st":"GRID","ss":"WORKING","s":"BTCUSDT","r":"-0.00300716",
        "up":"16720","uq":"-0.001","uf":"-0.00300716","mp":"0.0","ut":1669262908197}
})";

static const std::string CONDITIONAL_ORDER_REJECT = R"({
  "e":"CONDITIONAL_ORDER_TRIGGER_REJECT","E":1685517224945,"T":1685517224955,
  "or":{"s":"ETHUSDT","i":155618472834,"r":"Due to the order could not be filled immediately, the FOK order has been rejected."}
})";

static const std::string TRADE = R"({
  "e":"trade","E":1672515782136,"s":"BNBBTC","t":12345,"p":"0.001","q":"100","T":1672515782136,"m":true,"M":true
})";

static const std::string MARK_PRICE = R"({
  "e":"markPriceUpdate","E":1562305380000,"s":"BTCUSDT","p":"11794.15000000",
  "i":"11784.62659091","P":"11784.25641265","r":"0.00038167","T":1562306400000
})";

static const std::string RATE_LIMITS = R"(
  "rateLimits":[
    {"rateLimitType":"ORDERS","interval":"SECOND","intervalNum":10,"limit":300,"count":1},
    {"rateLimitType":"ORDERS","interval":"MINUTE","intervalNum":1,"limit":1200,"count":1},
    {"rateLimitType":"REQUEST_WEIGHT","interval":"MINUTE","intervalNum":1,"limit":2400,"count":1}])";

static const std::string ORDER_NEW = R"({
  "id":"3f7df6e3-2df4-44b9-9919-d2f38f90a99a","status":200,
  "result":{"orderId":325078477,"symbol":"BTCUSDT","status":"NEW","clientOrderId":"iCXL1BywlBaf2sesNUrVl3",
    "price":"43187.00","avgPrice":"0.00","origQty":"0.100","executedQty":"0.000","cumQty":"0.000",
    "cumQuote":"0.00000","timeInForce":"GTC","type":"LIMIT","reduceOnly":false,"closePosition":false,
    "side":"BUY","positionSide":"BOTH","stopPrice":"0.00","workingType":"CONTRACT_PRICE",
    "priceProtect":false,"origType":"LIMIT","priceMatch":"NONE","selfTradePreventionMode":"NONE",
    "goodTillDate":0,"updateTime":1702555534435},)" + RATE_LIMITS + "}";

static const std::string ORDER_MODIFY = R"({
  "id":"c8c271ba-de70-479e-870c-e64951c753d9","status":200,
  "result":{"orderId":328971409,"symbol":"BTCUSDT","status":"NEW","clientOrderId":"xGHfltUMExx0TbQstQQfRX",
    "price":"43769.10","avgPrice":"0.00","origQty":"0.110","executedQty":"0.000","cumQty":"0.000",
    "cumQuote":"0.00000","timeInForce":"GTC","type":"LIMIT","reduceOnly":false,"closePosition":false,
    "side":"SELL","positionSide":"SHORT","stopPrice":"0.00","workingType":"CONTRACT_PRICE",
    "priceProtect":false,"origType":"LIMIT","priceMatch":"NONE","selfTradePreventionMode":"NONE",
    "goodTillDate":0,"updateTime":1703426756190},)" + RATE_LIMITS + "}";

static const std::string ORDER_CANCEL_TRAILING = R"({
  "id":"5633b6a2-90a9-4192-83e7-925c90b6a2fd","status":200,
  "result":{"clientOrderId":"myOrder1","cumQty":"0","cumQuote":"0","executedQty":"0","orderId":283194212,
    "origQty":"11","origType":"TRAILING_STOP_MARKET","price":"0","reduceOnly":false,"side":"BUY",
    "positionSide":"SHORT","status":"CANCELED","stopPrice":"9300","closePosition":false,"symbol":"BTCUSDT",
    "timeInForce":"GTC","type":"TRAILING_STOP_MARKET","activatePrice":"9020","priceRate":"0.3",
    "updateTime":1571110484038,"workingType":"CONTRACT_PRICE","priceProtect":false,"priceMatch":"NONE",
    "selfTradePreventionMode":"NONE","goodTillDate":0},
  "rateLimits":[{"rateLimitType":"REQUEST_WEIGHT","interval":"MINUTE","intervalNum":1,"limit":2400,"count":1}]
})";

// Cancelling a plain LIMIT order: activatePrice / priceRate are trailing-stop only
static const std::string ORDER_CANCEL_LIMIT = R"({
  "id":"5633b6a2-90a9-4192-83e7-925c90b6a2fe","status":200,
  "result":{"clientOrderId":"myOrder2","cumQty":"0","cumQuote":"0","executedQty":"0","orderId":283194213,
    "origQty":"0.010","origType":"LIMIT","price":"40000.10","reduceOnly":false,"side":"BUY",
    "positionSide":"BOTH","status":"CANCELED","stopPrice":"0","closePosition":false,"symbol":"BTCUSDT",
    "timeInForce":"GTC","type":"LIMIT","updateTime":1571110484039,"workingType":"CONTRACT_PRICE",
    "priceProtect":false,"priceMatch":"NONE","selfTradePreventionMode":"NONE","goodTillDate":0},
  "rateLimits":[{"rateLimitType":"REQUEST_WEIGHT","interval":"MINUTE","intervalNum":1,"limit":2400,"count":1}]
})";

static const std::string WS_API_ERROR = R"({
  "id":"5633b6a2-90a9-4192-83e7-925c90b6a2ff","status":400,
  "error":{"code":-2011,"msg":"Unknown order sent."},
  "rateLimits":[{"rateLimitType":"REQUEST_WEIGHT","interval":"MINUTE","intervalNum":1,"limit":2400,"count":1}]
})";

// ---------------------------------------------------------------- 1. user data

static void test_order_update_trailing() {
    auto p = userEvent<OrderUpdate>(ORDER_UPDATE_TRAILING);
    if (!p) return;
    const OrderUpdate& o = *p;
    CHECK_CTX(o.pair == "BTCUSDT", o.pair);
    CHECK(o.clientID == "TEST");
    CHECK(o.side == OrderSide::SELL);
    CHECK(o.orderType == OrderType::TRAILING_STOP_MARKET);
    CHECK(o.originalOrderType == OrderType::TRAILING_STOP_MARKET);
    CHECK(o.timeInForce == TimeInForce::GTC);
    CHECK(o.originalQuantity == F("0.001"));
    CHECK(o.stopPrice == F("7103.04"));
    CHECK(o.notitionalValueOfOrderA == F("9.91"));
    CHECK(o.executionType == ExecutionType::NEW);
    CHECK(o.orderStatus == OrderStatus::NEW);
    CHECK(o.orderID == 8886774);
    CHECK(o.commisionAsset == "USDT");
    CHECK(o.tradeTimestamp == 1568879465650);
    CHECK(o.workingType == WorkingType::CONTRACT_PRICE);
    CHECK(o.positionSide == PositionSide::LONG);
    CHECK(o.activationPrice == F("7476.89"));
    CHECK(o.callBackRate == 5.0);
    CHECK(o.stpMode == STPMode::EXPIRE_TAKER);
    CHECK(o.priceMatchMode == PriceMatchMode::OPPONENT);
    CHECK(o.expireReason == ExpireReason::NONE);
    CHECK(!o.maker && !o.reduceOnly && !o.closeAll && !o.priceProtection);
}

static void test_order_update_limit_fill() {
    Parsed<OrderUpdate> p;
    CHECK_CTX(!throws([&] { p = userEvent<OrderUpdate>(ORDER_UPDATE_LIMIT_FILL); }), "AP/cr are absent outside trailing stops");
    if (!p) return;
    const OrderUpdate& o = *p;
    CHECK(o.pair == "ETHUSDT");
    CHECK(o.orderType == OrderType::LIMIT);
    CHECK(o.timeInForce == TimeInForce::GTX);
    CHECK(o.executionType == ExecutionType::TRADE);
    CHECK(o.orderStatus == OrderStatus::FILLED);
    CHECK(o.averagePrice == F("2250.37"));
    CHECK(o.totalFilledQuantity == F("1.5"));
    CHECK(o.commision == F("0.3375555"));
    CHECK(o.realizedProfit == F("-12.5"));
    CHECK(o.activationPrice == 0);
    CHECK(o.callBackRate == 0);
    CHECK(o.maker && o.reduceOnly && o.priceProtection);
    CHECK(o.workingType == WorkingType::MARK_PRICE);
    CHECK(o.expireReason == ExpireReason::GTE_FAILED);
}

static void test_account_update() {
    auto p = userEvent<AccountUpdate>(ACCOUNT_UPDATE);
    if (!p) return;
    const AccountUpdate& a = *p;
    CHECK(a.eventType == AccountUpdateType::ORDER);
    CHECK_CTX(a.assets.size() == 2, a.assets.size());
    CHECK_CTX(a.positions.size() == 3, a.positions.size());
    if (a.assets.size() == 2) {
        CHECK(a.assets[0].asset == "USDT");
        CHECK(a.assets[0].walletBalance == 12262412345678);
        CHECK(a.assets[0].crossWalletBalance == F("100.12345678"));
        CHECK(a.assets[1].asset == "BUSD");
        CHECK(a.assets[1].balanceChange == F("-49.12345678"));
    }
    if (a.positions.size() == 3) {
        CHECK(a.positions[0].positionSide == PositionSide::BOTH);
        CHECK(a.positions[1].positionAmount == F("20"));
        CHECK(a.positions[1].entryPrice == F("6563.665"));
        CHECK(a.positions[1].unrealizedPNL == F("2850.212"));
        CHECK(a.positions[1].marginType == MarginType::ISOLATED);
        CHECK(a.positions[1].positionSide == PositionSide::LONG);
        CHECK(a.positions[2].positionAmount == F("-10"));
        CHECK(a.positions[2].breakEvenPrice == F("6563.6"));
        CHECK(a.positions[2].marginType == MarginType::CROSS);
        CHECK(a.positions[2].positionSide == PositionSide::SHORT);
    }
}

static void test_account_update_without_positions() {
    Parsed<AccountUpdate> p;
    CHECK_CTX(!throws([&] { p = userEvent<AccountUpdate>(ACCOUNT_UPDATE_FUNDING); }), "\"P\" is optional");
    if (!p) return;
    const AccountUpdate& a = *p;
    CHECK(a.eventType == AccountUpdateType::FUNDING_FEE);
    CHECK(a.assets.size() == 1);
    CHECK(a.positions.empty());
}

static void test_margin_call() {
    auto p = userEvent<MarginCall>(MARGIN_CALL_ISOLATED);
    if (!p) return;
    const MarginCall& m = *p;
    CHECK(m.crossWalletBalance == F("3.16812045"));
    CHECK(m.positions.size() == 1);
    if (!m.positions.empty()) {
        CHECK(m.positions[0].pair == "ETHUSDT");
        CHECK(m.positions[0].marginType == MarginType::ISOLATED);
        CHECK(m.positions[0].markPrice == F("187.17127"));
        CHECK(m.positions[0].maintenanceMargin == F("1.614445"));
        CHECK(m.positions[0].unrealizedPNL == F("-1.166074"));
    }
    // Crossed margin calls report "CROSSED", not the "cross" of ACCOUNT_UPDATE
    Parsed<MarginCall> crossed;
    CHECK_CTX(!throws([&] { crossed = userEvent<MarginCall>(MARGIN_CALL_CROSSED); }), "\"mt\":\"CROSSED\"");
    if (crossed && !(*crossed).positions.empty()) CHECK((*crossed).positions[0].marginType == MarginType::CROSS);
}

static void test_trade_lite() {
    auto p = userEvent<TradeLite>(TRADE_LITE);
    if (!p) return;
    const TradeLite& t = *p;
    CHECK(t.pair == "BTCUSDT");
    CHECK(t.clientID == "z8hcUoOsqEdKMeKPSABslD");
    CHECK(t.orderSide == OrderSide::BUY);
    CHECK(t.lastFilledPrice == F("64089.2"));
    CHECK(t.lastFilledQuantity == F("0.04"));
    CHECK(t.tradeID == 109100866);
    CHECK(t.orderID == 8886774);
    CHECK(t.tradeTimestamp == 1721895408214);
}

static void test_account_config_update() {
    auto lev = userEvent<AccountConfigUpdate>(ACCOUNT_CONFIG_LEVERAGE);
    auto multi = userEvent<AccountConfigUpdate>(ACCOUNT_CONFIG_MULTI_ASSET);
    if (!lev || !multi) return;
    CHECK((*lev).pair == "BTCUSDT");
    CHECK((*lev).leverage == 25);
    CHECK(!(*lev).multiAssetMode);
    CHECK((*lev).timestamp == 1611646737476);
    CHECK((*multi).multiAssetMode);
    CHECK((*multi).leverage == 0);
}

static void test_strategy_update() {
    // Every field but the transaction time is inside "su"
    Parsed<StrategyUpdate> p;
    CHECK(!throws([&] { p = userEvent<StrategyUpdate>(STRATEGY_UPDATE); }));
    if (!p) return;
    const StrategyUpdate& s = *p;
    CHECK(s.timestamp == 1669261797627);
    CHECK(s.strategyID == 176054594);
    CHECK(s.strategyType == StrategyType::GRID);
    CHECK(s.strategyStatus == StrategyStatus::NEW);
    CHECK(s.pair == "BTCUSDT");
    CHECK(s.updateTimestamp == 1669261797627);
    CHECK(s.opCode == OpCode::NEW_GRID_ORDER);
}

static void test_grid_update() {
    auto p = userEvent<GridUpdate>(GRID_UPDATE);
    if (!p) return;
    const GridUpdate& g = *p;
    CHECK(g.strategyID == 176057039);
    CHECK(g.strategyType == StrategyType::GRID);
    CHECK(g.strategyStatus == StrategyStatus::WORKING);
    CHECK(g.pair == "BTCUSDT");
    CHECK(g.realizedPNL == F("-0.00300716"));
    CHECK(g.unmatchedAveragePrice == F("16720"));
    CHECK(g.unmatchedQuantity == F("-0.001"));
    CHECK(g.updateTimestamp == 1669262908197);
}

static void test_conditional_order_reject() {
    // Sent as CONDITIONAL_ORDER_TRIGGER_REJECT; the shorter name is still accepted
    std::string shortName = CONDITIONAL_ORDER_REJECT;
    shortName.replace(shortName.find("CONDITIONAL_ORDER_TRIGGER_REJECT"), 32, "CONDITIONAL_ORDER_REJECT");
    for (const std::string& json : {CONDITIONAL_ORDER_REJECT, shortName}) {
        auto p = userEvent<ConditionalOrderReject>(json);
        if (!p) continue;
        CHECK((*p).pair == "ETHUSDT");
        CHECK((*p).orderID == 155618472834);
        CHECK((*p).rejectReason.rfind("Due to the order", 0) == 0);
    }
}

static void test_unknown_and_empty_events() {
    for (const char* json : {R"({"e":"SOMETHING_NEW","E":1})", R"({"e":"ALGO_UPDATE","E":1})",
                             R"({"e":"listenKeyExpired","E":1})", R"({})", ""}) {
        UserDataStream stream;
        CHECK_CTX(!throws([&] { stream = parseUser(json); }), json);
        CHECK_CTX(stream.returnPtr() == nullptr, json);
    }
}

// ---------------------------------------------------------------- 2. market streams

static void test_trade_and_mark_price() {
    simdjson::padded_string trade(TRADE);
    TradeEvent t = parseTradeEvent(trade, g_parser);
    CHECK(t.timestamp == 1672515782136);
    CHECK(t.price == F("0.001"));
    CHECK(t.volume == F("100"));
    CHECK(t.maker);

    simdjson::padded_string mark(MARK_PRICE);
    MarkPrice m = parseMarkPrice(mark, g_parser);
    CHECK(m.timestamp == 1562305380000);
    CHECK(m.markPrice == F("11794.15"));
    CHECK(m.indexPrice == F("11784.62659091"));
    CHECK(m.fundingRate == 0.00038167);
    CHECK(m.fundingTime == 1562306400000);
}

// ---------------------------------------------------------------- 3. ws-api

static void drainRateLimits(RateLimitHolder& holder, std::vector<RateLimit>& out) {
    for (int i = 0; i < holder.limitCount.value.load(); ++i) out.push_back(holder.getRateLimit());
}

static void test_order_new_response() {
    simdjson::padded_string json(ORDER_NEW);
    auto doc = g_parser.iterate(json).value();
    OrderResponsePtrWrapper w(OrderTypeSent::NEW, doc);
    CHECK(w.newOrder && !w.error);
    if (!w.newOrder) return;
    OrderNewResponse& r = *w.newOrder;
    CHECK(r.id == "3f7df6e3-2df4-44b9-9919-d2f38f90a99a");
    CHECK(r.status == 200);
    CHECK(r.orderId == 325078477);
    CHECK(r.symbol == "BTCUSDT");
    CHECK(r.orderStatus == OrderStatus::NEW);
    CHECK(r.price == F("43187"));
    CHECK(r.origQty == F("0.1"));
    CHECK(r.type == OrderType::LIMIT);
    CHECK(r.side == OrderSide::BUY);
    CHECK(r.positionSide == PositionSide::BOTH);
    CHECK(r.workingType == WorkingType::CONTRACT_PRICE);
    CHECK(r.updateTime == 1702555534435);
    CHECK(w.getOrderId(OrderTypeSent::NEW) == 325078477);
    CHECK(w.getPrice(OrderTypeSent::NEW) == F("43187"));

    std::vector<RateLimit> limits;
    drainRateLimits(r.rateLimits, limits);
    CHECK_CTX(limits.size() == 3, limits.size());
    if (limits.size() == 3) {
        CHECK(limits[0].type == RateLimitType::ORDER && limits[0].interval == IntervalRateLimit::SECOND);
        CHECK(limits[0].intervalNum == 10 && limits[0].limit == 300);
        CHECK(limits[2].type == RateLimitType::REQUEST_WEIGHT && limits[2].limit == 2400);
    }
}

static void test_order_modify_response() {
    simdjson::padded_string json(ORDER_MODIFY);
    auto doc = g_parser.iterate(json).value();
    OrderResponsePtrWrapper w(OrderTypeSent::MODIFY, doc);
    CHECK(w.modifyOrder);
    if (!w.modifyOrder) return;
    CHECK(w.modifyOrder->orderId == 328971409);
    CHECK(w.modifyOrder->price == F("43769.1"));
    CHECK(w.modifyOrder->origQty == F("0.11"));
    CHECK(w.modifyOrder->side == OrderSide::SELL);
    CHECK(w.modifyOrder->positionSide == PositionSide::SHORT);
}

static void test_order_cancel_response() {
    {
        simdjson::padded_string json(ORDER_CANCEL_TRAILING);
        auto doc = g_parser.iterate(json).value();
        OrderResponsePtrWrapper w(OrderTypeSent::CANCEL, doc);
        CHECK(w.cancelOrder);
        if (w.cancelOrder) {
            CHECK(w.cancelOrder->orderId == 283194212);
            CHECK(w.cancelOrder->stopPrice == F("9300"));
            CHECK(w.cancelOrder->activatePrice == F("9020"));
            CHECK(w.cancelOrder->priceRate == 0.3);
            CHECK(w.cancelOrder->positionSide == PositionSide::SHORT);
        }
    }
    // activatePrice / priceRate are only sent for trailing stops
    simdjson::padded_string json(ORDER_CANCEL_LIMIT);
    std::unique_ptr<OrderResponsePtrWrapper> w;
    CHECK_CTX(!throws([&] {
                  auto doc = g_parser.iterate(json).value();
                  w = std::make_unique<OrderResponsePtrWrapper>(OrderTypeSent::CANCEL, doc);
              }),
              "cancel response for a LIMIT order");
    if (!w || !w->cancelOrder) return;
    CHECK(w->cancelOrder->orderId == 283194213);
    CHECK(w->cancelOrder->type == OrderType::LIMIT);
    CHECK(w->cancelOrder->price == F("40000.1"));
    CHECK(w->cancelOrder->activatePrice == 0 && w->cancelOrder->priceRate == 0);
}

static void test_ws_api_error_envelope() {
    simdjson::padded_string json(WS_API_ERROR);
    auto doc = g_parser.iterate(json).value();
    OrderResponsePtrWrapper w(OrderTypeSent::CANCEL, doc);
    CHECK(!w.cancelOrder);
    CHECK(w.error);
    if (w.error) {
        CHECK(w.error->errorCode == -2011);
        CHECK(w.error->message == "Unknown order sent.");
    }
    CHECK(w.getOrderId(OrderTypeSent::CANCEL) == -1);
}

// ---------------------------------------------------------------- 4. error helpers

static FetchError iterated(const std::string& body, int httpStatus, simdjson::ondemand::document& doc,
                           simdjson::padded_string& keep) {
    keep = simdjson::padded_string(body);
    return iterateResponse(g_parser, keep, doc, httpStatus);
}

static void test_error_helpers() {
    simdjson::ondemand::document doc;
    simdjson::padded_string keep;

    FetchError e = iterated(R"({"code":-1121,"msg":"Invalid symbol."})", 400, doc, keep);
    CHECK(e.error == APIError::BAD_SYMBOL);
    CHECK(e.getErrorMsg() == "Invalid symbol.");

    // A normal payload passes through and is still readable from the start
    e = iterated(R"({"openInterest":"10659.509","symbol":"BTCUSDT","time":1589437530011})", 200, doc, keep);
    CHECK(!e);
    CHECK(!e && AstraConnect::Utils::readFixed128(doc["openInterest"]) == AstraConnect::Utils::Fixed128(F("10659.509")));

    e = iterated(R"([[1,"2"]])", 200, doc, keep);
    CHECK_CTX(!e, "an array body is not an error");

    // HTML error pages: the status decides
    e = iterated("<html>busy</html>", 503, doc, keep);
    CHECK(e.error == APIError::SERVER_BUSY);
    e = iterated("", 418, doc, keep);
    CHECK(e.error == APIError::TOO_MANY_REQUESTS);
    e = iterated("{}", 403, doc, keep);
    CHECK(e.error == APIError::UNAUTHORIZED);
    // On-demand parsing is lazy: garbage passes iterateResponse and fails on the
    // first field read, which the fetch functions turn into BAD_FIELD
    e = iterated("not json", 200, doc, keep);
    CHECK(e.error == APIError::BAD_FIELD || doc.get_object().error());
    // A Binance body beats the bare status
    e = iterated(R"({"code":-1003,"msg":"Too many requests"})", 429, doc, keep);
    CHECK(e.error == APIError::TOO_MANY_REQUESTS && e.getErrorMsg() == "Too many requests");

    CHECK(fromHttpStatus(401) == APIError::UNAUTHORIZED);
    CHECK(fromHttpStatus(500) == APIError::UNEXPECTED_RESP);
    CHECK(fromErrorCode(-1121) == APIError::BAD_SYMBOL);
    CHECK(fromErrorCode(123456) == APIError::UNKNOWN);
}

// ---------------------------------------------------------------- 5. enums

static void test_enum_mapping() {
    CHECK(returnEventType("ORDER_TRADE_UPDATE") == EventType::ORDER_UPDATE);
    CHECK(returnEventType("listenKeyExpired") == EventType::LISTEN_KEY_EXPIRED);
    CHECK(returnEventType("") == EventType::UNKNOWN);

    for (const char* s : {"DEPOSIT", "WITHDRAW", "ORDER", "FUNDING_FEE", "WITHDRAW_REJECT", "ADJUSTMENT",
                          "INSURANCE_CLEAR", "ADMIN_DEPOSIT", "ADMIN_WITHDRAW", "MARGIN_TRANSFER",
                          "MARGIN_TYPE_CHANGE", "ASSET_TRANSFER", "OPTIONS_PREMIUM_FEE",
                          "OPTIONS_SETTLE_PROFIT", "AUTO_EXCHANGE", "COIN_SWAP_DEPOSIT", "COIN_SWAP_WITHDRAW"})
        CHECK_CTX(!throws([&] { returnAccountUpdateType(s); }), s);
    CHECK(throws([] { returnAccountUpdateType("NEW_REASON"); }));

    CHECK(returnMarginType("isolated") == MarginType::ISOLATED);
    CHECK(returnMarginType("CROSS") == MarginType::CROSS);
    CHECK(returnMarginType("CROSSED") == MarginType::CROSS);
    CHECK(returnEventType("CONDITIONAL_ORDER_TRIGGER_REJECT") == EventType::CONDITIONAL_ORDER_REJECT);
    CHECK(returnPositionSide("short") == PositionSide::SHORT);
    CHECK(returnOrderSide("BUY") == OrderSide::BUY);
    CHECK(returnOrderType("TAKE_PROFIT_MARKET") == OrderType::TAKE_PROFIT_MARKET);
    CHECK(returnOrderType("liquidation") == OrderType::LIQUIDATION);
    CHECK(returnExecutionType("AMENDMENT") == ExecutionType::AMENDMENT);
    CHECK(returnOrderStatus("EXPIRED_IN_MATCH") == OrderStatus::EXPIRED_IN_MATCH);
    CHECK(returnWorkingType("MARK_PRICE") == WorkingType::MARK_PRICE);
    CHECK(returnSTPMode("EXPIRE_BOTH") == STPMode::EXPIRE_BOTH);
    CHECK(returnPriceMatchMode("BIDDER") == PriceMatchMode::BIDDER);
    CHECK(returnStrategyType("TWAP") == StrategyType::TWAP);
    CHECK(returnStrategyStatus("EXPIRED") == StrategyStatus::EXPIRED);

    // Documented defaults instead of throwing
    CHECK(returnTimeInForce("GTD") == TimeInForce::GTC);
    CHECK(returnRateLimitType("RAW_REQUESTS") == RateLimitType::UNKNOWN);
    CHECK(returnIntervalRateLimit("DAY") == IntervalRateLimit::UNKNOWN);

    for (int code = 0; code <= 9; ++code) CHECK_CTX(!throws([&] { returnExpireReason(code); }), code);
    CHECK(throws([] { returnExpireReason(10); }));
    for (int code = 8001; code <= 8015; ++code) CHECK_CTX(int(returnOpCode(code)) == code, code);
    CHECK(throws([] { returnOpCode(8016); }));

    for (const char* bad : {"", "Buy", "SIDEWAYS"}) {
        CHECK_CTX(throws([&] { returnOrderSide(bad); }), bad);
        CHECK_CTX(throws([&] { returnOrderType(bad); }), bad);
        CHECK_CTX(throws([&] { returnOrderStatus(bad); }), bad);
    }
}

// ---------------------------------------------------------------- 6. mutations

struct Target {
    const char* name;
    const std::string* json;
    std::function<void(simdjson::padded_string&)> parse;
    // Crashes on some mutation are a confirmed bug in this parser: reported as a
    // known bug while they happen, a failure once they stop
    bool knownCrash = false;
};

struct Mutation {
    std::string text;
    const char* how;
    size_t at;
};

// Shared with the child that runs the mutations, so a crash can be pinned to the
// input that caused it
struct Progress {
    std::atomic<uint64_t> current;
    std::atomic<uint64_t> threw;
};

static std::string printable(const std::string& s, size_t at) {
    const size_t from = at > 30 ? at - 30 : 0;
    std::string out;
    for (char c : s.substr(from, 60)) out += (c == '\n') ? ' ' : (c == '\0') ? '@' : c;
    return out;
}

// Runs every mutation of one target in a child process. A child that dies (signal,
// sanitizer abort) is replaced by a fresh one starting after the input that killed it.
// Returns the indexes of the crashing inputs; exit code 3 marks a non-std exception.
static std::vector<std::pair<uint64_t, int>> runIsolated(const Target& t, const std::vector<Mutation>& inputs,
                                                         Progress& progress) {
    std::vector<std::pair<uint64_t, int>> crashes;
    uint64_t start = 0;
    while (start < inputs.size()) {
        progress.current.store(start);
        std::cout.flush();
        std::cerr.flush();
        const pid_t pid = fork();
        if (pid == 0) {
            for (uint64_t i = start; i < inputs.size(); ++i) {
                progress.current.store(i);
                simdjson::padded_string padded(inputs[i].text);
                try {
                    t.parse(padded);
                } catch (std::exception&) {
                    progress.threw.fetch_add(1);
                } catch (...) {
                    _exit(3);
                }
            }
            _exit(0);
        }
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) break;
        const uint64_t at = progress.current.load();
        crashes.push_back({at, WIFSIGNALED(status) ? -WTERMSIG(status) : WEXITSTATUS(status)});
        start = at + 1;
    }
    return crashes;
}

static void test_mutated_payloads_never_crash() {
    auto user = [](simdjson::padded_string& s) { (void)parseUserDataStream(s, g_parser); };
    auto trade = [](simdjson::padded_string& s) { (void)parseTradeEvent(s, g_parser); };
    auto mark = [](simdjson::padded_string& s) { (void)parseMarkPrice(s, g_parser); };
    auto wsApi = [](OrderTypeSent type) {
        return [type](simdjson::padded_string& s) {
            auto doc = g_parser.iterate(s).value();
            OrderResponsePtrWrapper w(type, doc);
        };
    };
    const std::vector<Target> targets = {
        {"order update trailing", &ORDER_UPDATE_TRAILING, user},
        {"order update limit", &ORDER_UPDATE_LIMIT_FILL, user},
        {"account update", &ACCOUNT_UPDATE, user},
        // AccountUpdate ignores an iteration error in its "B" loop and then looks up
        // "P" on the broken iterator: "...,"bc":"-0.0012"x]}" segfaults (asserts in
        // a debug build)
        {"account update funding", &ACCOUNT_UPDATE_FUNDING, user, true},
        {"margin call", &MARGIN_CALL_ISOLATED, user},
        {"margin call crossed", &MARGIN_CALL_CROSSED, user},
        {"conditional order reject", &CONDITIONAL_ORDER_REJECT, user},
        {"trade lite", &TRADE_LITE, user},
        {"account config", &ACCOUNT_CONFIG_LEVERAGE, user},
        {"account config multi", &ACCOUNT_CONFIG_MULTI_ASSET, user},
        {"strategy update", &STRATEGY_UPDATE, user},
        {"grid update", &GRID_UPDATE, user},
        {"trade", &TRADE, trade},
        {"mark price", &MARK_PRICE, mark},
        {"order new", &ORDER_NEW, wsApi(OrderTypeSent::NEW)},
        {"order modify", &ORDER_MODIFY, wsApi(OrderTypeSent::MODIFY)},
        {"order cancel", &ORDER_CANCEL_TRAILING, wsApi(OrderTypeSent::CANCEL)},
        {"order cancel limit", &ORDER_CANCEL_LIMIT, wsApi(OrderTypeSent::CANCEL)},
        {"ws-api error", &WS_API_ERROR, wsApi(OrderTypeSent::NEW)},
    };
    const char replacements[] = {'"', '}', ']', ',', '0', '-', 'x', ' ', '\0'};
    auto* progress = static_cast<Progress*>(
        mmap(nullptr, sizeof(Progress), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    CHECK(progress != MAP_FAILED);
    if (progress == MAP_FAILED) return;
    new (progress) Progress{};
    uint64_t runs = 0;

    for (const Target& t : targets) {
        const std::string& json = *t.json;
        std::vector<Mutation> inputs;
        for (size_t len = 0; len < json.size(); ++len) inputs.push_back({json.substr(0, len), "truncated", len});
        for (size_t at = 0; at < json.size(); ++at) {
            for (char c : replacements) {
                if (json[at] == c) continue;
                std::string mutated = json;
                mutated[at] = c;
                inputs.push_back({mutated, "replaced", at});
            }
        }
        runs += inputs.size();

        const auto crashes = runIsolated(t, inputs, *progress);
        for (const auto& [index, code] : crashes) {
            const Mutation& m = inputs[index];
            const char* kind = code == 3 ? "non-std exception" : code < 0 ? "killed by a signal" : "sanitizer abort";
            if (!t.knownCrash) {
                CHECK_CTX(false, t.name << ": " << kind << ", " << m.how << " at " << m.at << ": "
                                        << printable(m.text, m.at));
            } else {
                AstraLib::Atomic::SpinlockGuard guard(TestHarness::outLock);
                std::cout << "         crash: " << t.name << ", " << m.how << " at " << m.at << ": "
                          << printable(m.text, m.at) << "\n";
            }
        }
        if (t.knownCrash) {
            KNOWN_BUG(crashes.empty(), t.name << ": " << crashes.size() << " of " << inputs.size()
                                              << " mutated inputs crash the parser");
        }
    }
    std::cout << "         " << runs << " mutated inputs, " << progress->threw.load()
              << " rejected with an exception" << std::endl;
    munmap(progress, sizeof(Progress));
}

int main() {
    return TestHarness::run({
        {"order_update_trailing",          test_order_update_trailing},
        {"order_update_limit_fill",        test_order_update_limit_fill},
        {"account_update",                 test_account_update},
        {"account_update_without_positions", test_account_update_without_positions},
        {"margin_call",                    test_margin_call},
        {"trade_lite",                     test_trade_lite},
        {"account_config_update",          test_account_config_update},
        {"strategy_update",                test_strategy_update},
        {"grid_update",                    test_grid_update},
        {"conditional_order_reject",       test_conditional_order_reject},
        {"unknown_and_empty_events",       test_unknown_and_empty_events},
        {"trade_and_mark_price",           test_trade_and_mark_price},
        {"order_new_response",             test_order_new_response},
        {"order_modify_response",          test_order_modify_response},
        {"order_cancel_response",          test_order_cancel_response},
        {"ws_api_error_envelope",          test_ws_api_error_envelope},
        {"error_helpers",                  test_error_helpers},
        {"enum_mapping",                   test_enum_mapping},
        {"mutated_payloads_never_crash",   test_mutated_payloads_never_crash},
    });
}
