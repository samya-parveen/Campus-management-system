#include "store.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace procedural {
namespace {

double roundMoney(double amount) {
  return std::round(amount * 100.0) / 100.0;
}

bool validCategory(study::Category category) {
  return category == study::Category::Book ||
         category == study::Category::Accessory ||
         category == study::Category::Stationery;
}

}

const Product* findProduct(const Store& store, int id) {
  for (const Product& product : store.products_) {
    if (product.id == id) return &product;
  }
  return nullptr;
}

double calculateFinalPrice(const Product& product) {
  double multiplier = 1.0;
  if (product.category == study::Category::Book) multiplier = 0.90;
  if (product.category == study::Category::Accessory) multiplier = 1.05;
  if (product.category == study::Category::Stationery) multiplier = 0.95;
  return roundMoney(product.basePrice * multiplier);
}

double applyPercentage(double amount, double percent) {
  return roundMoney(amount * (1.0 - percent / 100.0));
}

void validateDiscount(double percent, double coupon) {
  if (!std::isfinite(percent) || percent < 0.0 || percent > 100.0 ||
      !std::isfinite(coupon) || coupon < 0.0) {
    throw std::invalid_argument("Discount must be 0-100% and coupon must be non-negative.");
  }
}

bool addProduct(Store& store, int id, std::string name, study::Category category,
                double basePrice) {
  if (id <= 0 || name.find_first_not_of(" \t\r\n") == std::string::npos ||
      !std::isfinite(basePrice) || basePrice < 0.0 || !validCategory(category) ||
      findProduct(store, id) != nullptr) {
    return false;
  }
  store.products_.push_back(Product{id, std::move(name), category, basePrice});
  return true;
}

bool deleteProduct(Store& store, int id) {
  for (auto product = store.products_.begin(); product != store.products_.end(); ++product) {
    if (product->id == id) {
      store.products_.erase(product);
      return true;
    }
  }
  return false;
}

int productCount(const Store& store) {
  return static_cast<int>(store.products_.size());
}

int liveProductCount(const Store& store) {
  return productCount(store);
}

bool categoryPrice(const Store& store, int id, double& price) {
  const Product* product = findProduct(store, id);
  if (product == nullptr) return false;
  price = calculateFinalPrice(*product);
  return true;
}

double discount(const Store& store, int id, double percent) {
  validateDiscount(percent, 0.0);
  const Product* product = findProduct(store, id);
  if (product == nullptr) throw std::invalid_argument("Product ID was not found.");
  return applyPercentage(calculateFinalPrice(*product), percent);
}

double discountWithCoupon(const Store& store, int id, double percent, double coupon) {
  validateDiscount(percent, coupon);
  const Product* product = findProduct(store, id);
  if (product == nullptr) throw std::invalid_argument("Product ID was not found.");
  return std::max(0.0, roundMoney(applyPercentage(calculateFinalPrice(*product), percent) -
                                  coupon));
}

double billTotal(double firstAmount, double secondAmount) {
  if (!std::isfinite(firstAmount) || !std::isfinite(secondAmount) ||
      firstAmount < 0.0 || secondAmount < 0.0) {
    throw std::invalid_argument("Bill amounts must be finite and non-negative.");
  }
  const double total = firstAmount + secondAmount;
  if (!std::isfinite(total)) throw std::invalid_argument("Bill total is too large.");
  return roundMoney(total);
}

std::string billOutput(double firstAmount, double secondAmount) {
  std::ostringstream output;
  output << std::fixed << std::setprecision(2)
         << "Grand total (Rs): " << procedural::billTotal(firstAmount, secondAmount);
  return output.str();
}

std::string audit(const Store& store, int id) {
  const Product* product = findProduct(store, id);
  if (product == nullptr) return "Audit: product not found.";
  std::ostringstream entry;
  entry << "Audit: ID=" << product->id << ", Name=" << product->name;
  return entry.str();
}

std::string demoOutput(const Store& store) {
  std::ostringstream output;
  output << "Products registered: " << productCount(store) << '\n';
  for (const Product& product : store.products_) {
    output << product.name << " | Category price: Rs "
           << std::fixed << std::setprecision(2) << calculateFinalPrice(product) << '\n';
  }
  output << "Book after 10% discount: Rs " << std::fixed << std::setprecision(2)
         << discount(store, 101, 10.0) << '\n';
  output << "Mouse after 10% and Rs 20 coupon: Rs " << std::fixed
         << std::setprecision(2) << discountWithCoupon(store, 202, 10.0, 20.0) << '\n';
  output << billOutput(discount(store, 101, 10.0),
                       discountWithCoupon(store, 202, 10.0, 20.0)) << '\n';
  return output.str();
}

}
