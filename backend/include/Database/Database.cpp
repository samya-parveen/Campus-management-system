#include "Database.h"

#include <windows.h>
#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

using Json = nlohmann::json;
const int kPasswordIterations = 120000;

class Statement {
 public:
  Statement(sqlite3* db, const char* sql) : db_(db), statement_(nullptr) {
    if (sqlite3_prepare_v2(db_, sql, -1, &statement_, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
  }
  ~Statement() { sqlite3_finalize(statement_); }
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  sqlite3_stmt* get() const { return statement_; }
  void bind(int index, const std::string& value) {
    if (sqlite3_bind_text(statement_, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
  }
  void bind(int index, int value) {
    if (sqlite3_bind_int(statement_, index, value) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
  }
  void bind(int index, double value) {
    if (sqlite3_bind_double(statement_, index, value) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
  }
  bool row() {
    const int status = sqlite3_step(statement_);
    if (status == SQLITE_ROW) return true;
    if (status == SQLITE_DONE) return false;
    throw std::runtime_error(sqlite3_errmsg(db_));
  }
  void done() {
    if (sqlite3_step(statement_) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
  }

 private:
  sqlite3* db_;
  sqlite3_stmt* statement_;
};

std::string columnText(sqlite3_stmt* statement, int column) {
  const unsigned char* text = sqlite3_column_text(statement, column);
  return text ? reinterpret_cast<const char*>(text) : "";
}

void execute(sqlite3* db, const char* sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error ? error : sqlite3_errmsg(db);
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

std::string timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm value;
  const std::tm* local = std::localtime(&now);
  if (!local) throw std::runtime_error("Could not read the system clock.");
  value = *local;
  std::ostringstream result;
  result << std::put_time(&value, "%Y-%m-%dT%H:%M:%S");
  return result.str();
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

std::string hexEncode(const unsigned char* bytes, size_t count) {
  std::ostringstream result;
  result << std::hex << std::setfill('0');
  for (size_t i = 0; i < count; ++i) result << std::setw(2) << static_cast<int>(bytes[i]);
  return result.str();
}

std::vector<unsigned char> hexDecode(const std::string& value) {
  if (value.size() % 2 != 0) return {};
  std::vector<unsigned char> bytes;
  for (size_t i = 0; i < value.size(); i += 2) {
    char* end = nullptr;
    const unsigned long byte = std::strtoul(value.substr(i, 2).c_str(), &end, 16);
    if (end == nullptr || *end != '\0' || byte > 255) return {};
    bytes.push_back(static_cast<unsigned char>(byte));
  }
  return bytes;
}

std::vector<unsigned char> randomBytes(size_t count) {
  HMODULE module = LoadLibraryW(L"bcrypt.dll");
  if (!module) throw std::runtime_error("Windows cryptographic provider is unavailable.");
  typedef LONG(WINAPI * RandomFn)(void*, unsigned char*, ULONG, ULONG);
  RandomFn random = reinterpret_cast<RandomFn>(GetProcAddress(module, "BCryptGenRandom"));
  std::vector<unsigned char> bytes(count);
  const LONG status = random ? random(nullptr, bytes.data(), static_cast<ULONG>(count), 2) : -1;
  FreeLibrary(module);
  if (status < 0) throw std::runtime_error("Could not generate secure random bytes.");
  return bytes;
}

std::vector<unsigned char> deriveKey(const std::string& password,
                                     const std::vector<unsigned char>& salt,
                                     int iterations) {
  HMODULE module = LoadLibraryW(L"bcrypt.dll");
  if (!module) throw std::runtime_error("Windows cryptographic provider is unavailable.");
  typedef LONG(WINAPI * OpenFn)(void**, const wchar_t*, const wchar_t*, ULONG);
  typedef LONG(WINAPI * Pbkdf2Fn)(void*, unsigned char*, ULONG, unsigned char*, ULONG,
                                  unsigned long long, unsigned char*, ULONG, ULONG);
  typedef LONG(WINAPI * CloseFn)(void*, ULONG);
  OpenFn openProvider = reinterpret_cast<OpenFn>(GetProcAddress(module, "BCryptOpenAlgorithmProvider"));
  Pbkdf2Fn pbkdf2 = reinterpret_cast<Pbkdf2Fn>(GetProcAddress(module, "BCryptDeriveKeyPBKDF2"));
  CloseFn closeProvider = reinterpret_cast<CloseFn>(GetProcAddress(module, "BCryptCloseAlgorithmProvider"));
  void* provider = nullptr;
  LONG status = openProvider ? openProvider(&provider, L"SHA256", nullptr, 8) : -1;
  std::vector<unsigned char> key(32);
  if (status >= 0 && pbkdf2) {
    status = pbkdf2(provider,
                    reinterpret_cast<unsigned char*>(const_cast<char*>(password.data())),
                    static_cast<ULONG>(password.size()),
                    const_cast<unsigned char*>(salt.data()), static_cast<ULONG>(salt.size()),
                    static_cast<unsigned long long>(iterations), key.data(),
                    static_cast<ULONG>(key.size()), 0);
  } else if (status >= 0) {
    status = -1;
  }
  if (provider && closeProvider) closeProvider(provider, 0);
  FreeLibrary(module);
  if (status < 0) throw std::runtime_error("Password hashing failed.");
  return key;
}

std::string sessionHash(const std::string& token) {
  const std::vector<unsigned char> salt = {'C', 'S', 'M', 'S', '-', 's', 'e', 's', 's', 'i', 'o', 'n'};
  const std::vector<unsigned char> digest = deriveKey(token, salt, 1);
  return hexEncode(digest.data(), digest.size());
}

bool constantTimeEqual(const std::vector<unsigned char>& first,
                       const std::vector<unsigned char>& second) {
  if (first.size() != second.size()) return false;
  unsigned char difference = 0;
  for (size_t i = 0; i < first.size(); ++i) difference |= first[i] ^ second[i];
  return difference == 0;
}

bool validCategory(const std::string& category) {
  return category == "Book" || category == "Accessory" || category == "Stationery";
}

}

std::string hashPassword(const std::string& password) {
  const std::vector<unsigned char> salt = randomBytes(16);
  const std::vector<unsigned char> digest = deriveKey(password, salt, kPasswordIterations);
  return hexEncode(salt.data(), salt.size()) + "$" + hexEncode(digest.data(), digest.size());
}

bool verifyPassword(const std::string& password, const std::string& encoded) {
  const size_t separator = encoded.find('$');
  if (separator == std::string::npos) return false;
  const std::vector<unsigned char> salt = hexDecode(encoded.substr(0, separator));
  const std::vector<unsigned char> expected = hexDecode(encoded.substr(separator + 1));
  if (salt.size() != 16 || expected.size() != 32) return false;
  return constantTimeEqual(deriveKey(password, salt, kPasswordIterations), expected);
}

std::string createSessionToken() {
  const std::vector<unsigned char> bytes = randomBytes(32);
  return hexEncode(bytes.data(), bytes.size());
}

Database::Database(const std::string& path) : db_(nullptr) {
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    const std::string message = db_ ? sqlite3_errmsg(db_) : "Could not open database.";
    if (db_) sqlite3_close(db_);
    db_ = nullptr;
    throw std::runtime_error(message);
  }
  sqlite3_busy_timeout(db_, 5000);
  execute(db_, "PRAGMA foreign_keys = ON;");
}

Database::~Database() {
  if (db_) sqlite3_close(db_);
}

void Database::initialize(const std::string& schemaPath) {
  std::ifstream schema(schemaPath.c_str(), std::ios::binary);
  if (!schema) throw std::runtime_error("Could not open database schema: " + schemaPath);
  const std::string sql((std::istreambuf_iterator<char>(schema)), std::istreambuf_iterator<char>());
  if (sql.empty()) throw std::runtime_error("Database schema is empty: " + schemaPath);
  execute(db_, sql.c_str());
  seedLocked();
}

void Database::seedLocked() {
  {
    Statement check(db_, "SELECT COUNT(*) FROM users;");
    if (check.row() && sqlite3_column_int(check.get(), 0) == 0) {
      const char* insert = "INSERT INTO users(email,name,phone,password_hash,role,created_at) VALUES(?,?,?,?,?,?);";
      Statement admin(db_, insert);
      admin.bind(1, "demo@campus.edu"); admin.bind(2, "Store Admin"); admin.bind(3, "");
      admin.bind(4, hashPassword("campus123")); admin.bind(5, "admin"); admin.bind(6, timestamp()); admin.done();
      Statement customer(db_, insert);
      customer.bind(1, "customer@campus.edu"); customer.bind(2, "Campus Customer"); customer.bind(3, "");
      customer.bind(4, hashPassword("customer123")); customer.bind(5, "customer"); customer.bind(6, timestamp()); customer.done();
    }
  }
  {
    Statement check(db_, "SELECT COUNT(*) FROM products;");
    if (check.row() && sqlite3_column_int(check.get(), 0) == 0) {
      const char* insert = "INSERT INTO products(id,name,category,base_price,quantity,updated_at) VALUES(?,?,?,?,?,?);";
      const struct { int id; const char* name; const char* category; double price; int quantity; } seed[] = {
        {101, "Fundamentals of Physics", "Book", 480, 18},
        {102, "Engineering Mathematics", "Book", 560, 12},
        {103, "Campus Hoodie", "Accessory", 899, 4},
        {104, "Canvas Tote Bag", "Accessory", 349, 9},
        {105, "Notebook Set", "Stationery", 160, 31},
        {106, "Gel Pen Pack", "Stationery", 95, 24},
        {107, "USB-C Flash Drive", "Accessory", 599, 3},
        {108, "Organic Chemistry Notes", "Book", 320, 7}
      };
      for (const auto& item : seed) {
        Statement row(db_, insert);
        row.bind(1, item.id); row.bind(2, item.name); row.bind(3, item.category);
        row.bind(4, item.price); row.bind(5, item.quantity); row.bind(6, timestamp()); row.done();
      }
    }
  }
  {
    Statement check(db_, "SELECT COUNT(*) FROM coupons;");
    if (check.row() && sqlite3_column_int(check.get(), 0) == 0) {
      const char* insert = "INSERT INTO coupons(code,type,value,category,minimum_purchase,max_uses,expires_at,active,usage_count) VALUES(?,?,?,?,?,?,?,?,0);";
      Statement campus(db_, insert);
      campus.bind(1, "CAMPUS10"); campus.bind(2, "percentage"); campus.bind(3, 10.0);
      campus.bind(4, "All"); campus.bind(5, 0.0); campus.bind(6, 0);
      campus.bind(7, ""); campus.bind(8, 1); campus.done();
      Statement books(db_, insert);
      books.bind(1, "BOOK15"); books.bind(2, "percentage"); books.bind(3, 15.0);
      books.bind(4, "Book"); books.bind(5, 0.0); books.bind(6, 0);
      books.bind(7, ""); books.bind(8, 1); books.done();
    }
  }
}

nlohmann::json Database::productsLocked() const {
  Json result = Json::array();
  Statement query(db_, "SELECT id,name,category,base_price,quantity FROM products ORDER BY id;");
  while (query.row()) {
    result.push_back({{"id", sqlite3_column_int(query.get(), 0)},
                      {"name", columnText(query.get(), 1)},
                      {"category", columnText(query.get(), 2)},
                      {"basePrice", sqlite3_column_double(query.get(), 3)},
                      {"price", sqlite3_column_double(query.get(), 3)},
                      {"quantity", sqlite3_column_int(query.get(), 4)}});
  }
  return result;
}

nlohmann::json Database::products() const {
  return productsLocked();
}

nlohmann::json Database::saveProduct(const nlohmann::json& product, bool creating) {
  const int id = product.at("id").get<int>();
  const std::string name = product.at("name").get<std::string>();
  const std::string category = product.at("category").get<std::string>();
  const double price = product.value("basePrice", product.value("price", 0.0));
  const int quantity = product.at("quantity").get<int>();
  if (id <= 0 || name.find_first_not_of(" \t\r\n") == std::string::npos ||
      !validCategory(category) || !std::isfinite(price) || price <= 0.0 || quantity < 0) {
    throw std::invalid_argument("Enter a valid product ID, name, category, price, and quantity.");
  }
  if (creating) {
    Statement insert(db_, "INSERT INTO products(id,name,category,base_price,quantity,updated_at) VALUES(?,?,?,?,?,?);");
    insert.bind(1, id); insert.bind(2, name); insert.bind(3, category);
    insert.bind(4, price); insert.bind(5, quantity); insert.bind(6, timestamp()); insert.done();
  } else {
    Statement update(db_, "UPDATE products SET name=?,category=?,base_price=?,quantity=?,updated_at=? WHERE id=?;");
    update.bind(1, name); update.bind(2, category); update.bind(3, price);
    update.bind(4, quantity); update.bind(5, timestamp()); update.bind(6, id); update.done();
    if (sqlite3_changes(db_) == 0) throw std::invalid_argument("Product was not found.");
  }
  for (Json& row : productsLocked()) if (row["id"] == id) return row;
  throw std::runtime_error("Saved product could not be loaded.");
}

bool Database::deleteProduct(int id) {
  Statement remove(db_, "DELETE FROM products WHERE id=?;");
  remove.bind(1, id); remove.done();
  if (sqlite3_changes(db_) == 0) return false;
  return true;
}

nlohmann::json Database::userLocked(const std::string& email) const {
  Statement query(db_, "SELECT name,email,phone,role,created_at FROM users WHERE email=? COLLATE NOCASE;");
  query.bind(1, email);
  if (!query.row()) return nullptr;
  return Json{{"name", columnText(query.get(), 0)}, {"email", columnText(query.get(), 1)},
              {"phone", columnText(query.get(), 2)}, {"role", columnText(query.get(), 3)},
              {"createdAt", columnText(query.get(), 4)}};
}

nlohmann::json Database::registerCustomer(const nlohmann::json& body) {
  const std::string name = body.at("name").get<std::string>();
  const std::string email = lower(body.at("email").get<std::string>());
  const std::string phone = body.value("phone", "");
  const std::string password = body.at("password").get<std::string>();
  if (name.size() < 2 || email.find('@') == std::string::npos ||
      password.size() < 8 || phone.size() > 20) {
    throw std::invalid_argument("Enter a valid name, email, phone, and password of at least 8 characters.");
  }
  Statement insert(db_, "INSERT INTO users(email,name,phone,password_hash,role,created_at) VALUES(?,?,?,?, 'customer', ?);");
  insert.bind(1, email); insert.bind(2, name); insert.bind(3, phone);
  insert.bind(4, hashPassword(password)); insert.bind(5, timestamp()); insert.done();
  Json user = userLocked(email);
  const std::string token = createSessionToken();
  Statement session(db_, "INSERT INTO sessions(token_hash,email,expires_at) VALUES(?,?,?);");
  session.bind(1, sessionHash(token)); session.bind(2, email);
  session.bind(3, static_cast<int>(std::time(nullptr) + 60 * 60 * 24 * 30)); session.done();
  auditLocked(user, "Account created", "Customer account created.");
  return Json{{"user", user}, {"token", token}};
}

nlohmann::json Database::authenticate(const std::string& email, const std::string& password) {
  Statement query(db_, "SELECT password_hash FROM users WHERE email=? COLLATE NOCASE;");
  query.bind(1, lower(email));
  if (!query.row() || !verifyPassword(password, columnText(query.get(), 0))) {
    throw std::invalid_argument("Email or password is incorrect.");
  }
  Json user = userLocked(email);
  const std::string token = createSessionToken();
  Statement session(db_, "INSERT INTO sessions(token_hash,email,expires_at) VALUES(?,?,?);");
  session.bind(1, sessionHash(token)); session.bind(2, user["email"].get<std::string>());
  session.bind(3, static_cast<int>(std::time(nullptr) + 60 * 60 * 24 * 30)); session.done();
  auditLocked(user, "Signed in", "Session started.");
  return Json{{"user", user}, {"token", token}};
}

nlohmann::json Database::userForToken(const std::string& token) const {
  if (token.empty()) return nullptr;
  Statement query(db_,
      "SELECT u.name,u.email,u.phone,u.role,u.created_at FROM sessions s "
      "JOIN users u ON u.email=s.email WHERE s.token_hash=? AND s.expires_at>?;");
  query.bind(1, sessionHash(token)); query.bind(2, static_cast<int>(std::time(nullptr)));
  if (!query.row()) return nullptr;
  return Json{{"name", columnText(query.get(), 0)}, {"email", columnText(query.get(), 1)},
              {"phone", columnText(query.get(), 2)}, {"role", columnText(query.get(), 3)},
              {"createdAt", columnText(query.get(), 4)}};
}

void Database::revokeToken(const std::string& token) {
  Statement remove(db_, "DELETE FROM sessions WHERE token_hash=?;");
  remove.bind(1, sessionHash(token)); remove.done();
}

nlohmann::json Database::updateProfile(const nlohmann::json& user, const nlohmann::json& body) {
  const std::string name = body.at("name").get<std::string>();
  const std::string phone = body.value("phone", "");
  if (name.size() < 2 || name.find_first_not_of(" \t\r\n") == std::string::npos || phone.size() > 20) {
    throw std::invalid_argument("Enter a valid name and phone number.");
  }
  Statement update(db_, "UPDATE users SET name=?,phone=? WHERE email=? COLLATE NOCASE;");
  update.bind(1, name); update.bind(2, phone); update.bind(3, user.at("email").get<std::string>()); update.done();
  Json updated = userLocked(user.at("email").get<std::string>());
  auditLocked(updated, "Profile updated", "Name and contact details updated.");
  return updated;
}

void Database::changePassword(const nlohmann::json& user, const std::string& currentPassword,
                              const std::string& newPassword) {
  if (newPassword.size() < 8) throw std::invalid_argument("Choose a password with at least 8 characters.");
  const std::string email = user.at("email").get<std::string>();
  Statement query(db_, "SELECT password_hash FROM users WHERE email=? COLLATE NOCASE;");
  query.bind(1, email);
  if (!query.row() || !verifyPassword(currentPassword, columnText(query.get(), 0))) {
    throw std::invalid_argument("Your current password is incorrect.");
  }
  Statement update(db_, "UPDATE users SET password_hash=? WHERE email=? COLLATE NOCASE;");
  update.bind(1, hashPassword(newPassword)); update.bind(2, email); update.done();
  auditLocked(user, "Password changed", "Account password updated.");
}

nlohmann::json Database::coupons() const {
  Json result = Json::array();
  Statement query(db_, "SELECT code,type,value,category,minimum_purchase,max_uses,expires_at,active,usage_count FROM coupons ORDER BY code;");
  while (query.row()) result.push_back({{"code", columnText(query.get(), 0)}, {"type", columnText(query.get(), 1)},
      {"value", sqlite3_column_double(query.get(), 2)}, {"category", columnText(query.get(), 3)},
      {"minimumPurchase", sqlite3_column_double(query.get(), 4)}, {"maxUses", sqlite3_column_int(query.get(), 5)},
      {"expiresAt", columnText(query.get(), 6)}, {"active", sqlite3_column_int(query.get(), 7) != 0},
      {"usageCount", sqlite3_column_int(query.get(), 8)}});
  return result;
}

nlohmann::json Database::couponLocked(const std::string& code) const {
  Statement query(db_, "SELECT code,type,value,category,minimum_purchase,max_uses,expires_at,active,usage_count FROM coupons WHERE code=? COLLATE NOCASE;");
  query.bind(1, code);
  if (!query.row()) return nullptr;
  return Json{{"code", columnText(query.get(), 0)}, {"type", columnText(query.get(), 1)},
      {"value", sqlite3_column_double(query.get(), 2)}, {"category", columnText(query.get(), 3)},
      {"minimumPurchase", sqlite3_column_double(query.get(), 4)}, {"maxUses", sqlite3_column_int(query.get(), 5)},
      {"expiresAt", columnText(query.get(), 6)}, {"active", sqlite3_column_int(query.get(), 7) != 0},
      {"usageCount", sqlite3_column_int(query.get(), 8)}};
}

nlohmann::json Database::featuredCoupon() const {
  Json all = Json::array();
  Statement query(db_, "SELECT code,type,value,category,minimum_purchase,max_uses,expires_at,active,usage_count FROM coupons ORDER BY code;");
  while (query.row()) all.push_back({{"code", columnText(query.get(), 0)}, {"type", columnText(query.get(), 1)},
      {"value", sqlite3_column_double(query.get(), 2)}, {"category", columnText(query.get(), 3)},
      {"minimumPurchase", sqlite3_column_double(query.get(), 4)}, {"maxUses", sqlite3_column_int(query.get(), 5)},
      {"expiresAt", columnText(query.get(), 6)}, {"active", sqlite3_column_int(query.get(), 7) != 0},
      {"usageCount", sqlite3_column_int(query.get(), 8)}});
  const std::string today = timestamp().substr(0, 10);
  for (const Json& coupon : all) {
    if (coupon["active"].get<bool>() &&
        (coupon["expiresAt"] == "" || coupon["expiresAt"].get<std::string>() >= today) &&
        (coupon["maxUses"].get<int>() == 0 || coupon["usageCount"].get<int>() < coupon["maxUses"].get<int>())) {
      return coupon;
    }
  }
  return nullptr;
}

nlohmann::json Database::saveCoupon(const nlohmann::json& coupon, bool creating) {
  const std::string code = coupon.at("code").get<std::string>();
  const std::string type = coupon.value("type", "percentage");
  const double value = coupon.at("value").get<double>();
  const std::string category = coupon.value("category", "All");
  const double minimum = coupon.value("minimumPurchase", 0.0);
  const int maxUses = coupon.value("maxUses", 0);
  const std::string expiresAt = coupon.value("expiresAt", "");
  const bool active = coupon.value("active", true);
  if (code.empty() || code.size() > 32 || (type != "fixed" && type != "percentage") ||
      (!validCategory(category) && category != "All") || !std::isfinite(value) || value <= 0.0 ||
      (type == "percentage" && value > 100.0) || minimum < 0.0 || maxUses < 0) {
    throw std::invalid_argument("Enter valid coupon details.");
  }
  const std::string sql = creating
      ? "INSERT INTO coupons(code,type,value,category,minimum_purchase,max_uses,expires_at,active,usage_count) VALUES(?,?,?,?,?,?,?,?,0);"
      : "UPDATE coupons SET type=?,value=?,category=?,minimum_purchase=?,max_uses=?,expires_at=?,active=? WHERE code=? COLLATE NOCASE;";
  Statement statement(db_, sql.c_str());
  if (creating) {
    statement.bind(1, code); statement.bind(2, type); statement.bind(3, value); statement.bind(4, category);
    statement.bind(5, minimum); statement.bind(6, maxUses); statement.bind(7, expiresAt); statement.bind(8, active ? 1 : 0);
  } else {
    statement.bind(1, type); statement.bind(2, value); statement.bind(3, category); statement.bind(4, minimum);
    statement.bind(5, maxUses); statement.bind(6, expiresAt); statement.bind(7, active ? 1 : 0); statement.bind(8, code);
  }
  statement.done();
  if (!creating && sqlite3_changes(db_) == 0) throw std::invalid_argument("Coupon was not found.");
  return couponLocked(code);
}

bool Database::deleteCoupon(const std::string& code) {
  Statement remove(db_, "DELETE FROM coupons WHERE code=? COLLATE NOCASE;");
  remove.bind(1, code); remove.done();
  if (sqlite3_changes(db_) == 0) return false;
  return true;
}

nlohmann::json Database::discounts() const {
  Json result = Json::array();
  Statement query(db_, "SELECT id,name,category,value,starts_at,ends_at,active FROM discounts ORDER BY name;");
  while (query.row()) result.push_back({{"id", columnText(query.get(), 0)}, {"name", columnText(query.get(), 1)},
      {"category", columnText(query.get(), 2)}, {"value", sqlite3_column_double(query.get(), 3)},
      {"startsAt", columnText(query.get(), 4)}, {"endsAt", columnText(query.get(), 5)},
      {"active", sqlite3_column_int(query.get(), 6) != 0}});
  return result;
}

nlohmann::json Database::discountLocked(const std::string& id) const {
  Statement query(db_, "SELECT id,name,category,value,starts_at,ends_at,active FROM discounts WHERE id=?;");
  query.bind(1, id);
  if (!query.row()) return nullptr;
  return Json{{"id", columnText(query.get(), 0)}, {"name", columnText(query.get(), 1)},
      {"category", columnText(query.get(), 2)}, {"value", sqlite3_column_double(query.get(), 3)},
      {"startsAt", columnText(query.get(), 4)}, {"endsAt", columnText(query.get(), 5)},
      {"active", sqlite3_column_int(query.get(), 6) != 0}};
}

nlohmann::json Database::saveDiscount(const nlohmann::json& discount, bool creating) {
  const std::string id = discount.at("id").get<std::string>();
  const std::string name = discount.at("name").get<std::string>();
  const std::string category = discount.value("category", "All");
  const double value = discount.at("value").get<double>();
  const std::string starts = discount.value("startsAt", "");
  const std::string ends = discount.value("endsAt", "");
  const bool active = discount.value("active", true);
  if (id.empty() || name.size() < 2 || (category != "All" && !validCategory(category)) ||
      !std::isfinite(value) || value <= 0.0 || value > 100.0 || (!starts.empty() && !ends.empty() && ends < starts)) {
    throw std::invalid_argument("Enter valid discount details.");
  }
  const char* sql = creating
      ? "INSERT INTO discounts(id,name,category,value,starts_at,ends_at,active) VALUES(?,?,?,?,?,?,?);"
      : "UPDATE discounts SET name=?,category=?,value=?,starts_at=?,ends_at=?,active=? WHERE id=?;";
  Statement statement(db_, sql);
  if (creating) {
    statement.bind(1, id); statement.bind(2, name); statement.bind(3, category); statement.bind(4, value);
    statement.bind(5, starts); statement.bind(6, ends); statement.bind(7, active ? 1 : 0);
  } else {
    statement.bind(1, name); statement.bind(2, category); statement.bind(3, value); statement.bind(4, starts);
    statement.bind(5, ends); statement.bind(6, active ? 1 : 0); statement.bind(7, id);
  }
  statement.done();
  if (!creating && sqlite3_changes(db_) == 0) throw std::invalid_argument("Discount was not found.");
  return discountLocked(id);
}

bool Database::deleteDiscount(const std::string& id) {
  Statement remove(db_, "DELETE FROM discounts WHERE id=?;");
  remove.bind(1, id); remove.done();
  if (sqlite3_changes(db_) == 0) return false;
  return true;
}

nlohmann::json Database::quoteLocked(const nlohmann::json& items, const std::string& couponCode,
                                     const CategoryPriceFunction& categoryPrice) const {
  if (!items.is_array() || items.empty() || items.size() > 100) {
    throw std::invalid_argument("Add between 1 and 100 products.");
  }
  const Json productList = productsLocked();
  Json discountList = Json::array();
  Statement discountQuery(db_, "SELECT id,name,category,value,starts_at,ends_at,active FROM discounts;");
  while (discountQuery.row()) discountList.push_back({{"id", columnText(discountQuery.get(), 0)},
      {"name", columnText(discountQuery.get(), 1)}, {"category", columnText(discountQuery.get(), 2)},
      {"value", sqlite3_column_double(discountQuery.get(), 3)}, {"startsAt", columnText(discountQuery.get(), 4)},
      {"endsAt", columnText(discountQuery.get(), 5)}, {"active", sqlite3_column_int(discountQuery.get(), 6) != 0}});
  Json lines = Json::array();
  std::vector<int> seenProductIds;
  double subtotal = 0.0;
  double promotionTotal = 0.0;
  for (const Json& requested : items) {
    const int id = requested.at("id").get<int>();
    const int quantity = requested.at("quantity").get<int>();
    if (quantity <= 0 || quantity > 10000) throw std::invalid_argument("Product quantities must be positive whole numbers.");
    if (std::find(seenProductIds.begin(), seenProductIds.end(), id) != seenProductIds.end()) {
      throw std::invalid_argument("Each product may appear only once in the basket.");
    }
    seenProductIds.push_back(id);
    auto product = std::find_if(productList.begin(), productList.end(),
        [id](const Json& candidate) { return candidate["id"].get<int>() == id; });
    if (product == productList.end()) throw std::invalid_argument("A product in your basket is no longer available.");
    const double price = categoryPrice((*product)["category"].get<std::string>(),
                                       (*product)["basePrice"].get<double>());
    const double lineSubtotal = std::round(price * quantity * 100.0) / 100.0;
    const std::string category = (*product)["category"].get<std::string>();
    const std::string today = timestamp().substr(0, 10);
    double bestPercent = 0.0;
    std::string promotionName;
    for (const Json& discount : discountList) {
      const bool dateValid = (discount["startsAt"] == "" || discount["startsAt"].get<std::string>() <= today) &&
          (discount["endsAt"] == "" || discount["endsAt"].get<std::string>() >= today);
      if (discount["active"].get<bool>() && dateValid &&
          (discount["category"] == "All" || discount["category"] == category) &&
          discount["value"].get<double>() > bestPercent) {
        bestPercent = discount["value"].get<double>();
        promotionName = discount["name"].get<std::string>();
      }
    }
    const double promotion = std::round(lineSubtotal * bestPercent) / 100.0;
    lines.push_back({{"id", id}, {"name", (*product)["name"]}, {"category", category},
        {"price", price}, {"quantity", quantity}, {"lineSubtotal", lineSubtotal},
        {"promotionDiscount", promotion}, {"promotionName", promotionName}});
    subtotal += lineSubtotal;
    promotionTotal += promotion;
  }
  subtotal = std::round(subtotal * 100.0) / 100.0;
  promotionTotal = std::round(promotionTotal * 100.0) / 100.0;
  double couponDiscount = 0.0;
  std::string couponError;
  Json appliedCoupon = nullptr;
  if (!couponCode.empty()) {
    Json coupon = couponLocked(couponCode);
    if (coupon.is_null() || !coupon["active"].get<bool>()) couponError = "That coupon is not active.";
    else if (!coupon["expiresAt"].get<std::string>().empty() && coupon["expiresAt"].get<std::string>() < timestamp().substr(0, 10)) couponError = "That coupon has expired.";
    else if (coupon["maxUses"].get<int>() > 0 && coupon["usageCount"].get<int>() >= coupon["maxUses"].get<int>()) couponError = "That coupon has reached its usage limit.";
    else {
      double eligibleBefore = 0.0;
      double eligibleAfter = 0.0;
      for (const Json& line : lines) {
        if (coupon["category"] == "All" || coupon["category"] == line["category"]) {
          eligibleBefore += line["lineSubtotal"].get<double>();
          eligibleAfter += line["lineSubtotal"].get<double>() - line["promotionDiscount"].get<double>();
        }
      }
      if (eligibleBefore < coupon["minimumPurchase"].get<double>()) couponError = "Your order does not meet the minimum purchase for this coupon.";
      else if (eligibleAfter <= 0.0) couponError = "This coupon cannot be applied to the selected items.";
      else {
        couponDiscount = coupon["type"] == "fixed"
            ? coupon["value"].get<double>()
            : eligibleAfter * coupon["value"].get<double>() / 100.0;
        couponDiscount = std::round(std::min(couponDiscount, eligibleAfter) * 100.0) / 100.0;
        appliedCoupon = coupon;
      }
    }
  }
  const double totalDiscount = promotionTotal + couponDiscount;
  return Json{{"items", lines}, {"subtotal", subtotal}, {"promotionDiscount", promotionTotal},
      {"couponDiscount", couponDiscount}, {"discount", totalDiscount},
      {"total", std::max(0.0, std::round((subtotal - totalDiscount) * 100.0) / 100.0)},
      {"coupon", appliedCoupon}, {"couponError", couponError}};
}

nlohmann::json Database::quote(const nlohmann::json& items, const std::string& couponCode,
                               const CategoryPriceFunction& categoryPrice) const {
  return quoteLocked(items, couponCode, categoryPrice);
}

nlohmann::json Database::createSale(const nlohmann::json& body, const nlohmann::json& user,
                                    const CategoryPriceFunction& categoryPrice) {
  const std::string channel = body.value("channel", "admin");
  if (channel != "customer" && channel != "admin") {
    throw std::invalid_argument("Choose a valid order channel.");
  }
  if ((user["role"] == "customer" && channel != "customer") ||
      (user["role"] == "admin" && channel == "customer")) {
    throw std::invalid_argument("This account cannot place that type of order.");
  }
  const std::string payment = body.value("payment", "");
  if (payment != "UPI" && payment != "Cash" && payment != "Card" &&
      payment != "Pay at pickup" && payment != "UPI on pickup" && payment != "Demo card") {
    throw std::invalid_argument("Choose a supported payment method.");
  }
  const std::string couponCode = body.value("couponCode", "");
  const Json requested = body.at("items");
  const std::string fulfillment = body.value("fulfillment", "");
  const std::string deliveryAddress = body.value("deliveryAddress", "");
  if (channel == "customer" && fulfillment != "Campus pickup" && fulfillment != "Campus delivery") {
    throw std::invalid_argument("Choose a valid order fulfilment method.");
  }
  if (fulfillment == "Campus delivery" && deliveryAddress.find_first_not_of(" \t\r\n") == std::string::npos) {
    throw std::invalid_argument("Enter a campus delivery location.");
  }
  execute(db_, "BEGIN IMMEDIATE;");
  try {
    Json totals = quoteLocked(requested, couponCode, categoryPrice);
    if (!totals["couponError"].get<std::string>().empty()) throw std::invalid_argument(totals["couponError"].get<std::string>());
    for (const Json& line : totals["items"]) {
      Statement stock(db_, "UPDATE products SET quantity=quantity-?,updated_at=? WHERE id=? AND quantity>=?;");
      stock.bind(1, line["quantity"].get<int>()); stock.bind(2, timestamp());
      stock.bind(3, line["id"].get<int>()); stock.bind(4, line["quantity"].get<int>()); stock.done();
      if (sqlite3_changes(db_) == 0) throw std::invalid_argument(line["name"].get<std::string>() + " does not have enough stock.");
    }
    const std::string id = "CS-" + std::to_string(std::time(nullptr)) + "-" +
        std::to_string(sqlite3_last_insert_rowid(db_) + 1);
    const std::string email = user["email"].get<std::string>();
    const std::string customerName = user["name"].get<std::string>();
    const std::string coupon = totals["coupon"].is_null() ? "" : totals["coupon"]["code"].get<std::string>();
    const char* insertSale = "INSERT INTO sales(id,date,subtotal,discount,promotion_discount,coupon_discount,total,payment,coupon_code,channel,customer_name,customer_email,fulfillment,delivery_address) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
    Statement saleInsert(db_, insertSale);
    saleInsert.bind(1, id); saleInsert.bind(2, timestamp()); saleInsert.bind(3, totals["subtotal"].get<double>());
    saleInsert.bind(4, totals["discount"].get<double>()); saleInsert.bind(5, totals["promotionDiscount"].get<double>());
    saleInsert.bind(6, totals["couponDiscount"].get<double>()); saleInsert.bind(7, totals["total"].get<double>());
    saleInsert.bind(8, payment); saleInsert.bind(9, coupon); saleInsert.bind(10, channel);
    saleInsert.bind(11, customerName); saleInsert.bind(12, email); saleInsert.bind(13, fulfillment);
    saleInsert.bind(14, deliveryAddress); saleInsert.done();
    for (const Json& line : totals["items"]) {
      Statement item(db_, "INSERT INTO sale_items(sale_id,product_id,name,category,price,quantity,line_subtotal,promotion_discount,promotion_name) VALUES(?,?,?,?,?,?,?,?,?);");
      item.bind(1, id); item.bind(2, line["id"].get<int>()); item.bind(3, line["name"].get<std::string>());
      item.bind(4, line["category"].get<std::string>()); item.bind(5, line["price"].get<double>());
      item.bind(6, line["quantity"].get<int>()); item.bind(7, line["lineSubtotal"].get<double>());
      item.bind(8, line["promotionDiscount"].get<double>()); item.bind(9, line["promotionName"].get<std::string>());
      item.done();
    }
    if (!coupon.empty()) {
      Statement use(db_, "UPDATE coupons SET usage_count=usage_count+1 WHERE code=? COLLATE NOCASE;");
      use.bind(1, coupon); use.done();
    }
    auditLocked(user, channel == "customer" ? "Customer order placed" : "In-store sale completed",
                id + " · " + std::to_string(totals["items"].size()) + " items.");
    execute(db_, "COMMIT;");
    return saleLocked(id);
  } catch (...) {
    execute(db_, "ROLLBACK;");
    throw;
  }
}

nlohmann::json Database::saleLocked(const std::string& id) const {
  Statement query(db_, "SELECT id,date,subtotal,discount,promotion_discount,coupon_discount,total,payment,coupon_code,channel,customer_name,customer_email,fulfillment,delivery_address FROM sales WHERE id=?;");
  query.bind(1, id);
  if (!query.row()) return nullptr;
  Json items = Json::array();
  Statement itemQuery(db_, "SELECT product_id,name,category,price,quantity,line_subtotal,promotion_discount,promotion_name FROM sale_items WHERE sale_id=?;");
  itemQuery.bind(1, id);
  while (itemQuery.row()) items.push_back({{"id", sqlite3_column_int(itemQuery.get(), 0)},
      {"name", columnText(itemQuery.get(), 1)}, {"category", columnText(itemQuery.get(), 2)},
      {"price", sqlite3_column_double(itemQuery.get(), 3)}, {"quantity", sqlite3_column_int(itemQuery.get(), 4)},
      {"lineSubtotal", sqlite3_column_double(itemQuery.get(), 5)}, {"promotionDiscount", sqlite3_column_double(itemQuery.get(), 6)},
      {"promotionName", columnText(itemQuery.get(), 7)}});
  return Json{{"id", columnText(query.get(), 0)}, {"date", columnText(query.get(), 1)},
      {"subtotal", sqlite3_column_double(query.get(), 2)}, {"discount", sqlite3_column_double(query.get(), 3)},
      {"promotionDiscount", sqlite3_column_double(query.get(), 4)}, {"couponDiscount", sqlite3_column_double(query.get(), 5)},
      {"total", sqlite3_column_double(query.get(), 6)}, {"payment", columnText(query.get(), 7)},
      {"couponCode", columnText(query.get(), 8)}, {"channel", columnText(query.get(), 9)},
      {"customer", {{"name", columnText(query.get(), 10)}, {"email", columnText(query.get(), 11)}}},
      {"fulfillment", columnText(query.get(), 12)}, {"deliveryAddress", columnText(query.get(), 13)},
      {"items", items}};
}

nlohmann::json Database::salesLocked(const nlohmann::json& user) const {
  Json result = Json::array();
  const bool admin = user["role"] == "admin";
  Statement query(db_, admin
      ? "SELECT id FROM sales ORDER BY date DESC;"
      : "SELECT id FROM sales WHERE customer_email=? COLLATE NOCASE ORDER BY date DESC;");
  if (!admin) query.bind(1, user["email"].get<std::string>());
  while (query.row()) result.push_back(saleLocked(columnText(query.get(), 0)));
  return result;
}

nlohmann::json Database::sales(const nlohmann::json& user) const {
  return salesLocked(user);
}

nlohmann::json Database::sale(const std::string& id, const nlohmann::json& user) const {
  Json result = saleLocked(id);
  if (result.is_null()) return result;
  if (user["role"] != "admin" && lower(result["customer"]["email"].get<std::string>()) != lower(user["email"].get<std::string>())) return nullptr;
  return result;
}

nlohmann::json Database::auditEntries() const {
  Json result = Json::array();
  Statement query(db_, "SELECT id,date,user,email,role,action,details FROM audit_logs ORDER BY id DESC LIMIT 1000;");
  while (query.row()) result.push_back({{"id", "AUD-" + std::to_string(sqlite3_column_int64(query.get(), 0))},
      {"date", columnText(query.get(), 1)}, {"user", columnText(query.get(), 2)},
      {"email", columnText(query.get(), 3)}, {"role", columnText(query.get(), 4)},
      {"action", columnText(query.get(), 5)}, {"details", columnText(query.get(), 6)}});
  return result;
}

void Database::auditLocked(const nlohmann::json& user, const std::string& action,
                           const std::string& details) {
  Statement insert(db_, "INSERT INTO audit_logs(date,user,email,role,action,details) VALUES(?,?,?,?,?,?);");
  insert.bind(1, timestamp()); insert.bind(2, user.value("name", "System"));
  insert.bind(3, user.value("email", "")); insert.bind(4, user.value("role", "system"));
  insert.bind(5, action); insert.bind(6, details); insert.done();
}

void Database::recordAudit(const nlohmann::json& user, const std::string& action,
                           const std::string& details) {
  if (action.empty() || action.size() > 100 || details.size() > 500) {
    throw std::invalid_argument("Audit entry is invalid.");
  }
  auditLocked(user, action, details);
}