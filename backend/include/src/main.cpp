#include "../routes/ApiServer.h"
#include "../Store.h"

#include <iostream>
#include <memory>

namespace {

std::unique_ptr<oop::Product> makeProduct(const std::string& category, double price) {
  if (category == "Book") return std::unique_ptr<oop::Product>(new oop::Book(1, "API product", price));
  if (category == "Accessory") return std::unique_ptr<oop::Product>(new oop::Accessory(1, "API product", price));
  if (category == "Stationery") return std::unique_ptr<oop::Product>(new oop::Stationery(1, "API product", price));
  throw std::invalid_argument("Unknown product category.");
}

}

int main(int argc, char** argv) {
  PricingEngine pricing;
  pricing.categoryPrice = [](const std::string& category, double price) {
    return makeProduct(category, price)->finalPrice();
  };
  pricing.percentageDiscount = [](const std::string& category, double price, double percent) {
    return makeProduct(category, price)->discount(percent);
  };
  pricing.couponDiscount = [](const std::string& category, double price, double percent, double coupon) {
    return makeProduct(category, price)->discount(percent, coupon);
  };
  try {
    return runApiServer(pricing, argc > 1 ? argv[1] : ".");
  } catch (const std::exception& error) {
    std::cerr << "Could not start OOP backend: " << error.what() << '\n';
    return 1;
  }
}