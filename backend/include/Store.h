#pragma once

#include "../../tests/store_contract.hpp"

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace oop {

class Product {
 public:
  Product(int id, std::string name, double basePrice);
  Product(const Product& other);
  Product& operator=(const Product& other);
  virtual ~Product();

  inline const std::string& getName() const noexcept { return name_; }
  inline int getId() const noexcept { return id_; }
  inline double getBasePrice() const noexcept { return basePrice_; }

  virtual double finalPrice() const = 0;
  double discount(double percent) const;
  double discount(double percent, double coupon) const;
  static int getProductCount() noexcept;

  friend void auditProduct(const Product& product, std::ostream& output);

 private:
  static void validate(int id, const std::string& name, double basePrice);
  static int productCount_;

  int id_;
  std::string name_;
  double basePrice_;
};

class Book final : public Product {
 public:
  using Product::Product;
  double finalPrice() const override;
};

class Accessory final : public Product {
 public:
  using Product::Product;
  double finalPrice() const override;
};

class Stationery final : public Product {
 public:
  using Product::Product;
  double finalPrice() const override;
};

class Bill {
 public:
  explicit Bill(double total = 0.0);
  double total() const noexcept;
  Bill operator+(const Bill& other) const;
  friend std::ostream& operator<<(std::ostream& output, const Bill& bill);

 private:
  double total_;
};

class Store {
 public:
  bool addProduct(int id, std::string name, study::Category category, double basePrice);
  bool deleteProduct(int id);
  int productCount() const;
  int liveProductCount() const;
  bool categoryPrice(int id, double& price) const;
  double discount(int id, double percent) const;
  double discountWithCoupon(int id, double percent, double coupon) const;
  double billTotal(double firstAmount, double secondAmount) const;
  std::string billOutput(double firstAmount, double secondAmount) const;
  std::string audit(int id) const;
  std::string demoOutput() const;

 private:
  const Product* findProduct(int id) const;
  std::vector<std::unique_ptr<Product>> products_;
};

void auditProduct(const Product& product, std::ostream& output);

}