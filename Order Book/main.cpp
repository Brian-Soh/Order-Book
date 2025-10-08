//
//  main.cpp
//  Order Book
//
//  Created by Brian Soh on 2025-10-07.
//
#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <memory>
#include <stdexcept>
#include <cstdint>
#include <format>


// scoped enums
enum class OrderType {
    GoodTillCancel,
    FillAndKill
};

enum class Side {
    Buy,
    Sell
};

// Type aliases
// Prices, Quantity and OrderId can not be negative
using Price = std::uint32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

// struct to get information of the OrderBook
struct LevelInfo {
    Price price_;
    // initial quantity, quantity remaining, quantity filled
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

// Use a class to encapsulate the level info object to represent the side

class OrderBookLevelInfos {
private:
    LevelInfos bids_;
    LevelInfos asks_;
     
public:
    // constructor which takes in const references for bids and asks
    OrderBookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
    // use curly brackets to prevent implicity conversion
    : bids_{bids}
    , asks_{asks}
    { }
    
    // enforces encapsulation for class' bid and ask data by returning them as const references
    // ensures that the function cannot modify the object by stating the member function as const
    const LevelInfos& getBids() const { return bids_; }
    const LevelInfos& getAsks() const { return asks_; }
};

// order objects represent things that can be added to the order book
class Order {
private:
    OrderType order_type_;
    OrderId order_id_;
    Side side_;
    Price price_;
    Quantity initial_quantity_;
    Quantity remaining_quantity_;
public:
    Order(OrderType order_type, OrderId order_id, Side side, Price price, Quantity quantity)
    : order_type_{order_type}
    , order_id_{order_id}
    , side_{side}
    , price_{price}
    , initial_quantity_{quantity}
    , remaining_quantity_{quantity}
    { }
    
    OrderType getOrderType() const { return order_type_; }
    OrderId getOrderId() const { return order_id_; }
    Side getSide() const { return side_; }
    Price getPrice() const { return price_; }
    Quantity getInitialQuantity() const { return initial_quantity_; }
    Quantity getRemainingQuantity() const { return remaining_quantity_; }
    Quantity getFilledQuantity() const { return getInitialQuantity() - getRemainingQuantity(); }
    bool isFilled() const { return remaining_quantity_ == 0; }
    
    void fill(Quantity quantity) {
        if (quantity > remaining_quantity_) {
            throw std::logic_error(std::format("Order({}) cannot be filled for more than its remaining quantity.", getOrderId()));
        }
        remaining_quantity_ -= quantity;
    }
};

// reference semantics to keep track of the same object rather than copying
// an order object can be stored in both an orders dictionary as well as a bid or ask based dictionary
using OrderPointer = std::shared_ptr<Order>;

// use a list to hold orders because it gives us access to an iterator
using OrderPointers = std::list<OrderPointer>;

// abstraction for an order that can be modified
class OrderModify {
private:
    OrderId order_id_;
    Side side_;
    Price price_;
    Quantity quantity_;
public:
    OrderModify(OrderId order_id, Side side, Price price, Quantity quantity)
    : order_id_{order_id}
    , side_{side}
    , price_{price}
    , quantity_{quantity}
    { }
    
    OrderId getOrderId() const { return order_id_; }
    Side getSide() const { return side_; }
    Price getPrice() const { return price_; }
    Quantity getQuantity() const { return quantity_; }
    
    // Transform a given order that already exists into a new order
    OrderPointer toOrderPointer(OrderType type) const {
        return std::make_shared<Order>(type, getOrderId(), getSide(), getPrice(), getQuantity());
    }
};

struct TradeInfo {
    OrderId order_id_;
    Price price_;
    Quantity quantity_;
};

class Trade {
private:
    TradeInfo bid_trade_;
    TradeInfo ask_trade_;
    
public:
    Trade(const TradeInfo& bid_trade, const TradeInfo& ask_trade)
    : bid_trade_{bid_trade}
    , ask_trade_{ask_trade}
    { }
    
    const TradeInfo& getBidTrade() const { return bid_trade_; }
    const TradeInfo& getAskTrade() const { return ask_trade_; }
};

using Trades = std::vector<Trade>;

class OrderBook {
private:
    struct OrderEntry {
        OrderPointer order_ { nullptr };
        OrderPointers::iterator location_;
    };
    
    // order in descending for bid prices
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    // order in ascending for asks
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    
    std::unordered_map<OrderId, OrderEntry> orders_;
    
    // discards fill and kill if cannot be matched
    bool canMatch(Side side, Price price) {
        if (side == Side::Buy) {
            if (asks_.empty()) return false;
            
            const auto& [bestAsk, _] = *asks_.begin();
            return price >= bestAsk;
        } else {
            if (bids_.empty()) return false;
            
            const auto& [bestBid, _] = *bids_.begin();
            return price <= bestBid;
        }
    }
    
    Trades matchOrders() {
        Trades trades;
        trades.reserve(orders_.size());

        while (true) {
            if (bids_.empty() || asks_.empty()) break;

            // use iterators to access map node data
            auto bIt = bids_.begin();
            auto aIt = asks_.begin();

            Price bidPrice = bIt->first;
            Price askPrice = aIt->first;

            auto& bids = bIt->second;
            auto& asks = aIt->second;

            //check if lowest ask can be fulfilled by highest bid
            if (bidPrice < askPrice) break;

            while (!bids.empty() && !asks.empty()) {
                auto& bid = bids.front();
                auto& ask = asks.front();

                Quantity quantity = std::min(bid->getRemainingQuantity(), ask->getRemainingQuantity());
                if (quantity == 0) break;

                // record trade
                trades.push_back(Trade{
                    TradeInfo{bid->getOrderId(), bid->getPrice(), quantity},
                    TradeInfo{ask->getOrderId(), ask->getPrice(), quantity}
                });

                // perform fills
                bid->fill(quantity);
                ask->fill(quantity);

                // remove filled orders
                if (bid->isFilled()) {
                    orders_.erase(bid->getOrderId());
                    bids.pop_front();
                }

                if (ask->isFilled()) {
                    orders_.erase(ask->getOrderId());
                    asks.pop_front();
                }
            }

            // erase emptied price levels using iterators
            if (bids.empty()) bids_.erase(bIt);
            if (asks.empty()) asks_.erase(aIt);
        }

        // clean up remaining FAK orders after loop
        // only possible for highest bid and lowest ask to potentially remain partially filled
        // this is because unfillable FAK orders are never added in the first place because canMatch returns false
        if (!bids_.empty()) {
            auto& firstBids = bids_.begin()->second;
            auto& order = firstBids.front();
            if (order->getOrderType() == OrderType::FillAndKill)
                cancelOrder(order->getOrderId());
        }

        if (!asks_.empty()) {
            auto& firstAsks = asks_.begin()->second;
            auto& order = firstAsks.front();
            if (order->getOrderType() == OrderType::FillAndKill)
                cancelOrder(order->getOrderId());
        }

        return trades;
    }


public:
    Trades addOrder(OrderPointer order) {
        if (orders_.contains(order->getOrderId())) return {};
        
        if (order->getOrderType() == OrderType::FillAndKill && !canMatch(order->getSide(), order->getPrice())) return {};
        
        OrderPointers::iterator iterator;
        
        if (order->getSide() == Side::Buy) {
            auto& orders = bids_[order->getPrice()];
            orders.push_back(order);
            iterator = std::prev(orders.end());
        } else {
            auto& orders = asks_[order->getPrice()];
            orders.push_back(order);
            iterator = std::prev(orders.end());
        }
        
        orders_.insert({order->getOrderId(), OrderEntry{ order, iterator }});
        return matchOrders();
    }
    
    void cancelOrder(OrderId order_id) {
        if (!orders_.contains(order_id)) return;
        
        const auto& [order, orderIterator] = orders_.at(order_id);
        
        if (order->getSide() == Side::Buy) {
            Price price = order->getPrice();
            auto& orders = bids_.at(price);
            orders.erase(orderIterator);
            if (orders.empty()) {
                bids_.erase(price);
            }
        } else {
            Price price = order->getPrice();
            auto& orders = asks_.at(price);
            orders.erase(orderIterator);
            if (orders.empty()) {
                asks_.erase(price);
            }
        }
        
        orders_.erase(order_id);
    }

    Trades matchOrder(OrderModify order) {
        if (!orders_.contains(order.getOrderId())) return { };
        
        const auto& [existingOrder, _] = orders_.at(order.getOrderId());
        cancelOrder(order.getOrderId());
        return addOrder(order.toOrderPointer(existingOrder->getOrderType()));
    }
    
    std::size_t Size() const { return orders_.size(); }
    
    OrderBookLevelInfos getOrderInfos() const {
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());
        
        auto CreateLevelInfos = [](Price price, const OrderPointers& orders) {
            return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
                                                      [](Quantity runningSum, const OrderPointer& order)
                                                     { return runningSum += order->getRemainingQuantity();})};
        };
        
        for (const auto& [price, orders] : bids_) {
            bidInfos.push_back(CreateLevelInfos(price, orders));
        }
        
        for (const auto& [price, orders] : asks_) {
            askInfos.push_back(CreateLevelInfos(price, orders));
        }
        
        
        return OrderBookLevelInfos{bidInfos, askInfos};
    }
};


void printCurrentBids(const LevelInfos& bids) {
    std::cout << "Current Bids: " << std::endl;
    for (const LevelInfo& bid: bids) {
        std::cout << "Price: " << bid.price_ << " | " << "Quantity: " << bid.quantity_ << std::endl;
    }
}

void printCurrentAsks(const LevelInfos& asks) {
    LevelInfos reversed = asks;
    sort(reversed.begin(), reversed.end(), [](LevelInfo& a, LevelInfo& b) {
        return a.price_ > b.price_;
    });
    std::cout << "Current Asks: " << std::endl;
    for (const LevelInfo& ask: reversed) {
        std::cout << "Price: " << ask.price_ << " | " << "Quantity: " << ask.quantity_ << std::endl;
    }
}


int main(int argc, const char * argv[]) {
    // instantiate OrderBook object
    OrderBook orderBook;
    
    OrderId currId = 1;
    
    // Exaxmple: GoodTillCancel Mismatch
    std::cout << "******************** GoodTillCancel Resting Orders Example ********************" << std::endl;
    std::cout << "Add GoodTillCancel Sell Order - Price: 101, Quantity: 10" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::GoodTillCancel, currId, Side::Sell, 101, 10));
    currId++;
    std::cout << "Add GoodTillCancel Buy Order - Price: 100, Quantity: 10" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::GoodTillCancel, currId, Side::Buy, 100, 10));
    currId++;
    OrderBookLevelInfos info1 = orderBook.getOrderInfos();
    printCurrentAsks(info1.getAsks());
    printCurrentBids(info1.getBids());

    
    // Exaxmple: GoodTillCancel Merge Levels
    std::cout << "******************** GoodTillCancel Merge Levels Example ********************" << std::endl;
    std::cout << "Add GoodTillCancel Buy Order - Price: 100, Quantity: 10" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::GoodTillCancel, currId, Side::Buy, 100, 10));
    currId++;
    std::cout << "Add GoodTillCancel Sell Order - Price: 101, Quantity: 10" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::GoodTillCancel, currId, Side::Sell, 101, 10));
    currId++;

    OrderBookLevelInfos info2 = orderBook.getOrderInfos();
    printCurrentAsks(info2.getAsks());
    printCurrentBids(info2.getBids());

    
    //Example: GoodTillCancel New Levels
    std::cout << "******************** GoodTillCancel New Levels Example ********************" << std::endl;
    std::cout << "Add GoodTillCancel Buy Order - Price: 99, Quantity: 10" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::GoodTillCancel, currId, Side::Buy, 99, 10));
    currId++;
    std::cout << "Add GoodTillCancel Sell Order - Price: 102, Quantity: 10" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::GoodTillCancel, currId, Side::Sell, 102, 10));
    currId++;
    
    OrderBookLevelInfos info3 = orderBook.getOrderInfos();
    printCurrentAsks(info3.getAsks());
    printCurrentBids(info3.getBids());

    // Example: GoodTillCancel Complete Fill
    std::cout << "******************** GoodTillCancel Complete Fill Example ********************" << std::endl;
    std::cout << "Add GoodTillCancel Sell Order - Price: 100, Quantity: 10" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::GoodTillCancel, currId, Side::Sell, 100, 10));
    currId++;
    
    OrderBookLevelInfos info4 = orderBook.getOrderInfos();
    printCurrentAsks(info4.getAsks());
    printCurrentBids(info4.getBids());
    
    // Example: GoodTillCancel Partial Fill
    std::cout << "******************** GoodTillCancel Partial Fill Example ********************" << std::endl;
    std::cout << "Add GoodTillCancel Sell Order - Price: 100, Quantity: 15" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::GoodTillCancel, currId, Side::Sell, 100, 15));
    currId++;
    
    OrderBookLevelInfos info5 = orderBook.getOrderInfos();
    printCurrentAsks(info5.getAsks());
    printCurrentBids(info5.getBids());
    
    // Example: GoodTillCancel Multilevel Fill
    std::cout << "******************** GoodTillCancel Multilevel Fill Example ********************" << std::endl;
    std::cout << "Add GoodTillCancel Buy Order - Price: 101, Quantity: 15" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::GoodTillCancel, currId, Side::Buy, 101, 15));
    currId++;
    
    OrderBookLevelInfos info6 = orderBook.getOrderInfos();
    printCurrentAsks(info6.getAsks());
    printCurrentBids(info6.getBids());
    
    // Example: FillAndKill Partial Fill
    std::cout << "******************** FillAndKill Partial Fill Example ********************" << std::endl;
    std::cout << "Add FillAndKill Buy Order - Price: 101, Quantity: 15" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::FillAndKill, currId, Side::Buy, 101, 15));
    currId++;
    
    OrderBookLevelInfos info7= orderBook.getOrderInfos();
    printCurrentAsks(info7.getAsks());
    printCurrentBids(info7.getBids());
    
    // Example: FillAndKill Complete Fill
    std::cout << "******************** FillAndKill Complete Fill Example ********************" << std::endl;
    std::cout << "Add FillAndKill Sell Order - Price: 99, Quantity: 5" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::FillAndKill, currId, Side::Sell, 99, 5));
    currId++;
    
    OrderBookLevelInfos info8 = orderBook.getOrderInfos();
    printCurrentAsks(info8.getAsks());
    printCurrentBids(info8.getBids());
    
    // Example: FillAndKill Mismatch
    std::cout << "******************** FillAndKill Mismatch Example ********************" << std::endl;
    std::cout << "Add FillAndKill Buy Order - Price: 100, Quantity: 10" << std::endl;
    orderBook.addOrder(std::make_shared<Order>(OrderType::FillAndKill, currId, Side::Buy, 100, 10));
    currId++;
    
    OrderBookLevelInfos info9 = orderBook.getOrderInfos();
    printCurrentAsks(info9.getAsks());
    printCurrentBids(info9.getBids());
    
    return 0;
}
