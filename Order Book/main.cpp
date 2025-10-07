//
//  main.cpp
//  Order Book
//
//  Created by Brian Soh on 2025-10-07.
//
#include <iostream>
#include <vector>
#include <list>

// scoped enums
enum class OrderType {
    GoodTillCancel,
    FillAndKill
};

enum class Side {
    Buy,
    Sell
};

// Type alias
// Prices can be negative
using Price = std::int32_t;
// Quantity and OrderId can not be negative
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
    LevelInfo bids_;
    LevelInfo asks_;
     
public:
    // constructor which takes in const references for bids and asks
    OrderBookLevelInfos(const LevelInfo& bids, const LevelInfo& asks)
    // use curly brackets to prevent implicity conversion
    : bids_{bids}
    , asks_{asks}
    { }
    
    // enforces encapsulation for class' bid and ask data by returning them as const references
    // ensures that the function cannot modify the object by stating the member function as const
    const LevelInfo& getBids() const { return bids_; }
    const LevelInfo& getAsks() const { return asks_; }
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
    Side getSize() const { return side_; }
    Price getPrice() const { return price_; }
    Quantity getInitialQuantity() const { return initial_quantity_; }
    Quantity getRemainingQuantity() const { return remaining_quantity_; }
    Quantity getFilledQuantity() const { return getInitialQuantity() - getRemainingQuantity(); }
    
    void Fill(Quantity quantity) {
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



int main(int argc, const char * argv[]) {
    // insert code here...
    std::cout << "Hello, World!\n";
    return 0;
}
