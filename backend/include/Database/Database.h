#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <string>

struct sqlite3;
using CategoryPriceFunction = std::function<double(const std::string&, double)>;

class Database {
 public:
  explicit Database(const std::string& path);
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  void initialize(const std::string& schemaPath);

  nlohmann::json products() const;
  nlohmann::json saveProduct(const nlohmann::json& product, bool creating);
  bool deleteProduct(int id);

  nlohmann::json registerCustomer(const nlohmann::json& body);
  nlohmann::json authenticate(const std::string& email, const std::string& password);
  nlohmann::json userForToken(const std::string& token) const;
  void revokeToken(const std::string& token);
  nlohmann::json updateProfile(const nlohmann::json& user, const nlohmann::json& body);
  void changePassword(const nlohmann::json& user, const std::string& currentPassword,
                      const std::string& newPassword);

  nlohmann::json coupons() const;
  nlohmann::json featuredCoupon() const;
  nlohmann::json saveCoupon(const nlohmann::json& coupon, bool creating);
  bool deleteCoupon(const std::string& code);

  nlohmann::json discounts() const;
  nlohmann::json saveDiscount(const nlohmann::json& discount, bool creating);
  bool deleteDiscount(const std::string& id);

  nlohmann::json quote(const nlohmann::json& items, const std::string& couponCode,
                       const CategoryPriceFunction& categoryPrice) const;
  nlohmann::json createSale(const nlohmann::json& body, const nlohmann::json& user,
                            const CategoryPriceFunction& categoryPrice);
  nlohmann::json sales(const nlohmann::json& user) const;
  nlohmann::json sale(const std::string& id, const nlohmann::json& user) const;
  nlohmann::json auditEntries() const;
  void recordAudit(const nlohmann::json& user, const std::string& action,
                   const std::string& details);

 private:
  nlohmann::json quoteLocked(const nlohmann::json& items, const std::string& couponCode,
                             const CategoryPriceFunction& categoryPrice) const;
  nlohmann::json productsLocked() const;
  nlohmann::json salesLocked(const nlohmann::json& user) const;
  nlohmann::json saleLocked(const std::string& id) const;
  void seedLocked();
  void auditLocked(const nlohmann::json& user, const std::string& action,
                   const std::string& details);
  nlohmann::json userLocked(const std::string& email) const;
  nlohmann::json couponLocked(const std::string& code) const;
  nlohmann::json discountLocked(const std::string& id) const;

  sqlite3* db_;
};

std::string hashPassword(const std::string& password);
bool verifyPassword(const std::string& password, const std::string& encoded);
std::string createSessionToken();