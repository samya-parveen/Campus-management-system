#pragma once

#include "store_contract.hpp"

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace study {

class AcceptanceResults {
 public:
  void check(const std::string& name, const std::function<bool()>& test) {
    ++total_;
    try {
      if (test()) {
        ++passed_;
        std::cout << "[PASS] " << name << '\n';
      } else {
        std::cout << "[FAIL] " << name << '\n';
      }
    } catch (const std::exception& error) {
      std::cout << "[FAIL] " << name << " - " << error.what() << '\n';
    } catch (...) {
      std::cout << "[FAIL] " << name << " - unexpected error\n";
    }
  }

  int total() const { return total_; }
  int passed() const { return passed_; }

 private:
  int total_ = 0;
  int passed_ = 0;
};

inline bool near(double actual, double expected) {
  return std::abs(actual - expected) < 0.005;
}

template <typename Store>
int runAcceptanceSuite(const std::string& implementation) {
  AcceptanceResults results;

  results.check("FR1: accepts a named product with a non-negative base price", [] {
    Store store;
    return store.addProduct(101, "C++ Textbook", Category::Book, 500.0) &&
           store.productCount() == 1;
  });

  results.check("FR1/FR2/CR1: rejects duplicate IDs and invalid product data", [] {
    Store store;
    const bool first = store.addProduct(101, "C++ Textbook", Category::Book, 500.0);
    const bool duplicate = store.addProduct(101, "Duplicate", Category::Book, 80.0);
    const bool emptyName = store.addProduct(102, " ", Category::Book, 10.0);
    const bool negativePrice = store.addProduct(103, "Bad price", Category::Book, -1.0);
    const bool nonFinitePrice = store.addProduct(
        104, "Infinite price", Category::Book,
        std::numeric_limits<double>::infinity());
    const bool invalidCategory =
        store.addProduct(105, "Unknown", static_cast<Category>(99), 5.0);
    return first && !duplicate && !emptyName && !negativePrice && !nonFinitePrice &&
           !invalidCategory && store.productCount() == 1;
  });

  results.check("FR2: applies Book and Accessory category pricing", [] {
    Store store;
    double bookPrice = 0.0;
    double accessoryPrice = 0.0;
    return store.addProduct(101, "C++ Textbook", Category::Book, 500.0) &&
           store.addProduct(202, "USB Mouse", Category::Accessory, 800.0) &&
           store.categoryPrice(101, bookPrice) &&
           store.categoryPrice(202, accessoryPrice) &&
           near(bookPrice, 450.0) && near(accessoryPrice, 840.0);
  });

  results.check("FR3: applies the percentage discount to category price", [] {
    Store store;
    if (!store.addProduct(101, "C++ Textbook", Category::Book, 500.0)) return false;
    return near(store.discount(101, 10.0), 405.0);
  });

  results.check("CR2: applies coupon after percentage and floors at zero", [] {
    Store store;
    if (!store.addProduct(202, "USB Mouse", Category::Accessory, 800.0)) return false;
    return near(store.discountWithCoupon(202, 10.0, 20.0), 736.0) &&
           near(store.discountWithCoupon(202, 10.0, 1000.0), 0.0);
  });

  results.check("FR8: rejects invalid discount and coupon values", [] {
    Store store;
    if (!store.addProduct(101, "C++ Textbook", Category::Book, 500.0)) return false;
    const auto rejects = [&store](const std::function<double()>& calculation) {
      try {
        calculation();
        return false;
      } catch (const std::invalid_argument&) {
        return true;
      }
    };
    return rejects([&store] { return store.discount(101, -1.0); }) &&
           rejects([&store] { return store.discount(101, 101.0); }) &&
           rejects([&store] { return store.discountWithCoupon(101, 10.0, -1.0); });
  });

  results.check("FR4: combines bill amounts and prints a two-decimal total", [] {
    Store store;
    const double total = store.billTotal(405.0, 736.0);
    return near(total, 1141.0) &&
           store.billOutput(405.0, 736.0).find("1141.00") != std::string::npos;
  });

  results.check("FR6: audit output includes the product identifier and name", [] {
    Store store;
    if (!store.addProduct(101, "C++ Textbook", Category::Book, 500.0)) return false;
    const std::string entry = store.audit(101);
    return entry.find("101") != std::string::npos &&
           entry.find("C++ Textbook") != std::string::npos;
  });

  results.check("CR1: prices Stationery at a 5% category reduction", [] {
    Store store;
    double price = 0.0;
    return store.addProduct(303, "Pen Set", Category::Stationery, 200.0) &&
           store.categoryPrice(303, price) && near(price, 190.0) &&
           near(store.discount(303, 10.0), 171.0);
  });

  results.check("FR5: product count follows currently stored products", [] {
    Store store;
    if (!store.addProduct(101, "C++ Textbook", Category::Book, 500.0) ||
        !store.addProduct(202, "USB Mouse", Category::Accessory, 800.0) ||
        store.productCount() != 2 || store.liveProductCount() != 2) {
      return false;
    }
    return store.deleteProduct(202) && store.productCount() == 1 &&
           store.liveProductCount() == 1 && !store.deleteProduct(999);
  });

  std::cout << implementation << " acceptance tests: " << results.passed()
            << '/' << results.total() << " passed\n";
  return results.passed() == results.total() ? 0 : 1;
}

}
