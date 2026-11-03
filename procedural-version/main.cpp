#include "store.hpp"

#include "../backend/include/routes/ApiServer.h"

#include <iostream>

namespace {

study::Category parseCategory(const std::string& category) {
  if (category == "Book") return study::Category::Book;
  if (category == "Accessory") return study::Category::Accessory;
  if (category == "Stationery") return study::Category::Stationery;
  throw std::invalid_argument("Unknown product category.");
}

double calculateCategoryPrice(const std::string& category, double price) {
  procedural::Store store;
  if (!procedural::addProduct(store, 1, "API product", parseCategory(category), price)) {
    throw std::invalid_argument("Could not calculate the product category price.");
  }
  double result = 0.0;
  if (!procedural::categoryPrice(store, 1, result)) {
    throw std::invalid_argument("Could not calculate the product category price.");
  }
  return result;
}

double calculatePercentDiscount(const std::string& category, double price, double percent) {
  procedural::Store store;
  if (!procedural::addProduct(store, 1, "API product", parseCategory(category), price)) {
    throw std::invalid_argument("Could not calculate the product discount.");
  }
  return procedural::discount(store, 1, percent);
}

double calculateCouponDiscount(const std::string& category, double price, double percent, double coupon) {
  procedural::Store store;
  if (!procedural::addProduct(store, 1, "API product", parseCategory(category), price)) {
    throw std::invalid_argument("Could not calculate the coupon discount.");
  }
  return procedural::discountWithCoupon(store, 1, percent, coupon);
}

}

int main(int argc, char** argv) {
  PricingEngine pricing;
  pricing.categoryPrice = calculateCategoryPrice;
  pricing.percentageDiscount = calculatePercentDiscount;
  pricing.couponDiscount = calculateCouponDiscount;
  try {
    return runApiServer(pricing, argc > 1 ? argv[1] : ".");
  } catch (const std::exception& error) {
    std::cerr << "Could not start procedural backend: " << error.what() << '\n';
    return 1;
  }
}