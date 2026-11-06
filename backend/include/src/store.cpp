#include "Store.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace oop {
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

int Product::productCount_ = 0;

Product::Product(int id, std::string name, double basePrice)
    : id_(id), name_(std::move(name)), basePrice_(basePrice) {
  validate(id_, name_, basePrice_);
  ++productCount_;
}

Product::Product(const Product& other)
    : id_(other.id_), name_(other.name_), basePrice_(other.basePrice_) {
  ++productCount_;
}

Product& Product::operator=(const Product& other) {
  if (this != &other) {
    id_ = other.id_;
    name_ = other.name_;
    basePrice_ = other.basePrice_;
  }
  return *this;
}

Product::~Product() {
  --productCount_;
}

double Product::discount(double percent) const {
  if (!std::isfinite(percent) || percent < 0.0 || percent > 100.0) {
    throw std::invalid_argument("Discount percentage must be between 0 and 100.");
  }
  return roundMoney(finalPrice() * (1.0 - percent / 100.0));
}

double Product::discount(double percent, double coupon) const {
  if (!std::isfinite(coupon) || coupon < 0.0) {
    throw std::invalid_argument("Coupon amount must be finite and non-negative.");
  }
  return std::max(0.0, roundMoney(discount(percent) - coupon));
}

int Product::getProductCount() noexcept {
  return productCount_;
}

void Product::validate(int id, const std::string& name, double basePrice) {
  if (id <= 0 || name.find_first_not_of(" \t\r\n") == std::string::npos ||
      !std::isfinite(basePrice) || basePrice < 0.0) {
    throw std::invalid_argument(
        "Product needs a positive ID, name, and finite non-negative price.");
  }
}

double Book::finalPrice() const {
  return roundMoney(getBasePrice() * 0.90);
}

double Accessory::finalPrice() const {
  return roundMoney(getBasePrice() * 1.05);
}

double Stationery::finalPrice() const {
  return roundMoney(getBasePrice() * 0.95);
}

Bill::Bill(double total) : total_(total) {
  if (!std::isfinite(total_) || total_ < 0.0) {
    throw std::invalid_argument("Bill total must be finite and non-negative.");
  }
  total_ = roundMoney(total_);
}

double Bill::total() const noexcept {
  return total_;
}

Bill Bill::operator+(const Bill& other) const {
  return Bill(total_ + other.total_);
}

std::ostream& operator<<(std::ostream& output, const Bill& bill) {
  output << std::fixed << std::setprecision(2) << "Grand total (Rs): " << bill.total_;
  return output;
}

void auditProduct(const Product& product, std::ostream& output) {
  output << "Audit: ID=" << product.id_ << ", Name=" << product.name_;
}

bool Store::addProduct(int id, std::string name, study::Category category,
                       double basePrice) {
  if (!validCategory(category)) {
    return false;
  }

  for (const auto& product : products_) {
    if (product->getId() == id) {
      return false;
    }
  }

  try {
    switch (category) {
      case study::Category::Book:
        products_.push_back(std::make_unique<Book>(id, std::move(name), basePrice));
        break;
      case study::Category::Accessory:
        products_.push_back(std::make_unique<Accessory>(id, std::move(name), basePrice));
        break;
      case study::Category::Stationery:
        products_.push_back(std::make_unique<Stationery>(id, std::move(name), basePrice));
        break;
    }
  } catch (const std::invalid_argument&) {
    return false;
  }
  return true;
}

bool Store::deleteProduct(int id) {
  for (auto product = products_.begin(); product != products_.end(); ++product) {
    if ((*product)->getId() == id) {
      products_.erase(product);
      return true;
    }
  }

  return false;
}

int Store::productCount() const {
  return static_cast<int>(products_.size());
}

int Store::liveProductCount() const {
  return Product::getProductCount();
}

bool Store::categoryPrice(int id, double& price) const {
  const Product* product = findProduct(id);
  if (product == nullptr) {
    return false;
  }

  price = product->finalPrice();
  return true;
}

double Store::discount(int id, double percent) const {
  const Product* product = findProduct(id);
  if (product == nullptr) {
    throw std::invalid_argument("Product ID was not found.");
  }

  return product->discount(percent);
}

double Store::discountWithCoupon(int id, double percent, double coupon) const {
  const Product* product = findProduct(id);
  if (product == nullptr) {
    throw std::invalid_argument("Product ID was not found.");
  }

  return product->discount(percent, coupon);
}

double Store::billTotal(double firstAmount, double secondAmount) const {
  return (Bill(firstAmount) + Bill(secondAmount)).total();
}

std::string Store::billOutput(double firstAmount, double secondAmount) const {
  std::ostringstream output;
  output << Bill(firstAmount) + Bill(secondAmount);
  return output.str();
}

std::string Store::audit(int id) const {
  const Product* product = findProduct(id);
  if (product == nullptr) {
    return "Audit: product not found.";
  }

  std::ostringstream entry;
  auditProduct(*product, entry);
  return entry.str();
}

std::string Store::demoOutput() const {
  std::ostringstream output;
  output << "Products registered: " << productCount() << '\n';
  for (const auto& product : products_) {
    output << product->getName() << " | Category price: Rs "
           << std::fixed << std::setprecision(2) << product->finalPrice() << '\n';
  }
  output << "Book after 10% discount: Rs " << std::fixed << std::setprecision(2)
         << discount(101, 10.0) << '\n';
  output << "Mouse after 10% and Rs 20 coupon: Rs " << std::fixed
         << std::setprecision(2) << discountWithCoupon(202, 10.0, 20.0) << '\n';
  output << billOutput(discount(101, 10.0),
                       discountWithCoupon(202, 10.0, 20.0)) << '\n';
  return output.str();
}

const Product* Store::findProduct(int id) const {
  for (const auto& product : products_) {
    if (product->getId() == id) {
      return product.get();
    }
  }

  return nullptr;
}

}