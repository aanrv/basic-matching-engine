#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <cassert>

enum class Type_t { MarketOrder, LimitOrder };
enum class Side_t { BUY, SELL };
struct Order {
    Type_t type;
    Side_t side;
    int quantity;
    long price;
};

std::ostream& operator<<(std::ostream& os, const Order& o) {
    os <<
        (o.type == Type_t::MarketOrder ? "Market" : "Limit") << " " <<
        (o.side == Side_t::BUY ? "Buy" : "Sell") << " " <<
        o.quantity << " " <<
        o.price;
    return os;
}

class OrderReader {
public:
    OrderReader(std::string filename) : tradesFile(filename) {}
    bool readNext(Order& o) {
        if (tradesFile.eof()) return false;
        std::string line;
        if (!getline(tradesFile, line)) return false;
        std::stringstream ss;
        ss << line;
        char delim, nl;
        char type;
        char side;
        int quantity;
        long price;
        ss >> type >> delim >> side >> delim >> quantity >> delim >> price;

        o.type = type == 'M' ? Type_t::MarketOrder : Type_t::LimitOrder;
        o.side = side == 'B' ? Side_t::BUY : Side_t::SELL;
        o.quantity = quantity;
        o.price = price;
        return true;
    }

private:
    std::ifstream tradesFile;
};

class OrderBook {
public:
    void handleOrder(Order o) {
        std::cout << "Handling order: " << o << std::endl;
        // exec as many shares as possible
        while (execOrder(o))
            ;
        // if limit order, add to book
        if (o.quantity && isLimitOrder(o)) addOrder(o);
        std::cout << "-----------------------" << std::endl;
    }

    ~OrderBook() {
        for (auto& [_, ordersVector] : priceBids) {
            for (Order* o : ordersVector) {
                delete o;
            }
            ordersVector.clear();
        }
        for (auto& [_, ordersVector] : priceAsks) {
            for (Order* o : ordersVector) {
                delete o;
            }
            ordersVector.clear();
        }
    }

    friend std::ostream& operator<<(std::ostream&, const OrderBook&);

private:
    void addOrder(const Order& o) {
        std::cout << "adding order " << o << std::endl;
        Order no = o;
        Order* newOrder = new Order(o);
        if (o.side == Side_t::BUY) priceBids[o.price].push_back(newOrder);
        if (o.side == Side_t::SELL) priceAsks[o.price].push_back(newOrder);
    }
    bool removeOrder(Order const * o) {
        std::cout << "removing order " << *o << std::endl;
        if (o->side == Side_t::BUY) {
            auto& sideOrders = priceBids;
            auto priceSearch = sideOrders.find(o->price);
            if (priceSearch == sideOrders.end()) return false;
            std::vector<Order*>& ordersList = priceSearch->second;
            auto orderSearch = find(ordersList.begin(), ordersList.end(), o);
            if (orderSearch == ordersList.end()) return false;
            delete o;
            ordersList.erase(orderSearch);
            if (ordersList.size() == 0) priceBids.erase(priceSearch);
        } else {
            auto& sideOrders = priceAsks;
            auto priceSearch = sideOrders.find(o->price);
            if (priceSearch == sideOrders.end()) return false;
            std::vector<Order*>& ordersList = priceSearch->second;
            auto orderSearch = find(ordersList.begin(), ordersList.end(), o);
            if (orderSearch == ordersList.end()) return false;
            delete o;
            ordersList.erase(orderSearch);
            if (ordersList.size() == 0) priceAsks.erase(priceSearch);
        }
        return true;
    }
    bool execOrder(Order& o) {
        if (!o.quantity) return false;
        if (isMarketOrder(o)) {
            Order* target = getTop(otherSide(o.side));
            if (!target) return false;
            int execAmt = std::min(o.quantity, target->quantity);
            o.quantity -= execAmt;
            target->quantity -= execAmt;
            std::cout << "filled " << execAmt << " shares" << std::endl;
            if (target->quantity == 0) removeOrder(target);
            assert(o.quantity >= 0);
        } else {
            if (!limitPriceMatch(o)) return false;
            Order* target = getTop(otherSide(o.side));
            if (!target) return false;
            int execAmt = std::min(o.quantity, target->quantity);
            o.quantity -= execAmt;
            target->quantity -= execAmt;
            std::cout << "filled " << execAmt << " shares" << std::endl;
            if (target->quantity == 0) removeOrder(target);
        }
        return true;
    }

    std::map<long, std::vector<Order*>, std::greater<long>> priceBids;
    std::map<long, std::vector<Order*>> priceAsks;

    inline Side_t otherSide(Side_t s) const {
        return s == Side_t::BUY ? Side_t::SELL : Side_t::BUY;
    }

    bool isMarketOrder(const Order& o) const {
        return o.type == Type_t::MarketOrder;
    }

    bool isLimitOrder(const Order& o) const {
        return o.type == Type_t::LimitOrder;
    }

    Order* getTop(Side_t side) const {
        if (side == Side_t::BUY) {
            if (priceBids.empty()) return nullptr;
            assert(priceBids.begin()->second.size());
            return priceBids.begin()->second[0];
        } else {
            if (priceAsks.empty()) return nullptr;
            assert(priceAsks.begin()->second.size());
            return priceAsks.begin()->second[0];
        }
    }

    bool limitPriceMatch(const Order& o) const {
        Order* potentialExec = getTop(otherSide(o.side));
        if (!potentialExec) return false;
        int potentialPrice = potentialExec->price;

        if (o.side == Side_t::BUY) {
            return potentialPrice <= o.price;
        } else {
            return potentialPrice >= o.price;
        }
    }
};

std::ostream& operator<<(std::ostream& os, const OrderBook& b) {
    os << "Bids: ";
    for (auto const& [price, orders] : b.priceBids) {
        for (Order* o : orders) {
            os << *o << " | ";
        }
    }
    os << "\nAsks: ";
    for (auto const& [price, orders] : b.priceAsks) {
        for (Order* o : orders) {
            os << *o << " | ";
        }
    }
    return os;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " tradesFileCSV\n";
        std::cout << "\nFile Format:\n\n\t<M|L>,<B|S>,quantity,price\n\n";
        std::cout << "\twhere M = Market, L = Limit, B = Buy, S = Sell, typeof(quantity) = int, typeof(price) = long" << std::endl;
        return EXIT_SUCCESS;
    }
    char const * tradesFileCSV = argv[1];
    OrderReader reader(tradesFileCSV);
    OrderBook book;
    Order o;
    while (reader.readNext(o)) {
        book.handleOrder(o);
    }
    std::cout << '\n';
    std::cout << "EOD Book" << std::endl;
    std::cout << book << std::endl;
}

