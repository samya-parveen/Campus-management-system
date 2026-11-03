#pragma once

#include "../tests/store_contract.hpp"

#include <string>
#include <vector>

namespace procedural {

struct Product {
  int id;
  std::string name;
  study::Category category;
  double basePrice;
};

struct Bill {
  double total;
};

struct Store {
  std::vector<Product> products_;
};

bool addProduct(Store& store, int id, std::string name, study::Category category,
                double basePrice);
bool deleteProduct(Store& store, int id);
int productCount(const Store& store);
int liveProductCount(const Store& store);
bool categoryPrice(const Store& store, int id, double& price);
double discount(const Store& store, int id, double percent);
double discountWithCoupon(const Store& store, int id, double percent, double coupon);
double billTotal(double firstAmount, double secondAmount);
std::string billOutput(double firstAmount, double secondAmount);
std::string audit(const Store& store, int id);
std::string demoOutput(const Store& store);

}
