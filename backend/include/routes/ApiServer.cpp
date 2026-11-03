#include "ApiServer.h"

#include "HttpServer.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using Json = nlohmann::json;

void addCors(httplib::Response& response) {
  response.set_header("Access-Control-Allow-Origin", "*");
  response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
  response.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  response.set_header("X-Content-Type-Options", "nosniff");
  response.set_header("Cache-Control", "no-store");
}

void respond(httplib::Response& response, int status, const Json& value) {
  addCors(response);
  response.status = status;
  response.set_content(value.dump(), "application/json; charset=utf-8");
}

Json requestBody(const httplib::Request& request) {
  if (!request.has_header("Content-Type") ||
      request.get_header_value("Content-Type").find("application/json") == std::string::npos) {
    throw std::invalid_argument("Request must use application/json.");
  }
  Json body;
  try {
    body = Json::parse(request.body);
  } catch (const Json::exception&) {
    throw std::invalid_argument("Request body must contain valid JSON.");
  }
  if (!body.is_object()) throw std::invalid_argument("Request body must be a JSON object.");
  return body;
}

std::string bearerToken(const httplib::Request& request) {
  const std::string value = request.get_header_value("Authorization");
  const std::string prefix = "Bearer ";
  if (value.compare(0, prefix.size(), prefix) != 0) return "";
  return value.substr(prefix.size());
}

bool hasRole(const Json& user, const std::string& role) {
  return !user.is_null() && user.value("role", "") == role;
}

Json currentUser(const httplib::Request& request, Database& database) {
  return database.userForToken(bearerToken(request));
}

bool requireUser(const httplib::Request& request, httplib::Response& response,
                 Database& database, Json& user) {
  user = currentUser(request, database);
  if (user.is_null()) {
    respond(response, 401, {{"error", "Please sign in to continue."}});
    return false;
  }
  return true;
}

bool requireAdmin(const httplib::Request& request, httplib::Response& response,
                 Database& database, Json& user) {
  if (!requireUser(request, response, database, user)) return false;
  if (!hasRole(user, "admin")) {
    respond(response, 403, {{"error", "Administrator access is required."}});
    return false;
  }
  return true;
}

int statusFor(const std::exception& error) {
  if (dynamic_cast<const std::invalid_argument*>(&error)) return 400;
  if (dynamic_cast<const Json::exception*>(&error)) return 400;
  const std::string message = error.what();
  if (message.find("UNIQUE constraint failed") != std::string::npos) return 409;
  if (message.find("constraint failed") != std::string::npos) return 400;
  return 500;
}

template <typename Handler>
void handle(httplib::Response& response, Handler&& handler) {
  try {
    const Json result = handler();
    const int successStatus = response.status == 200 ? 200 : response.status;
    respond(response, successStatus, result);
  } catch (const std::exception& error) {
    const int status = statusFor(error);
    if (status == 500) std::cerr << "API error: " << error.what() << '\n';
    respond(response, status, {{"error", status == 500 ? "The server could not complete the request." : error.what()}});
  }
}

std::string pathFor(const std::string& root, const std::string& path) {
  if (root.empty()) return path;
  const char last = root[root.size() - 1];
  return root + ((last == '\\' || last == '/') ? "" : "\\") + path;
}

}

int runApiServer(const PricingEngine& pricing, const std::string& repositoryRoot) {
  const char* portValue = std::getenv("CSMS_PORT");
  const int port = portValue ? std::atoi(portValue) : 8080;
  if (port < 1 || port > 65535) throw std::invalid_argument("CSMS_PORT must be between 1 and 65535.");
  const char* databaseValue = std::getenv("CSMS_DB_PATH");
  Database database(databaseValue ? databaseValue : pathFor(repositoryRoot, "backend\\data\\campus_store.db"));
  database.initialize(pathFor(repositoryRoot, "backend\\include\\Database\\schema.sql"));

  httplib::Server server;
  server.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& response) {
    addCors(response);
    response.status = 204;
  });
  server.Get("/api/health", [](const httplib::Request&, httplib::Response& response) {
    const char* variant = std::getenv("CSMS_API_VARIANT");
    respond(response, 200, {{"status", "ok"}, {"service", "campus-store"},
                            {"backend", variant ? variant : "C++"}});
  });

  server.Post("/api/auth/login", [&](const httplib::Request& request, httplib::Response& response) {
    handle(response, [&] {
      const Json body = requestBody(request);
      return database.authenticate(body.at("email").get<std::string>(),
                                   body.at("password").get<std::string>());
    });
  });
  server.Post("/api/auth/register", [&](const httplib::Request& request, httplib::Response& response) {
    handle(response, [&] { return database.registerCustomer(requestBody(request)); });
  });
  server.Get("/api/auth/me", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    respond(response, 200, {{"user", user}});
  });
  server.Post("/api/auth/logout", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] {
      database.recordAudit(user, "Signed out", "Session ended.");
      database.revokeToken(bearerToken(request));
      return Json{{"success", true}};
    });
  });
  server.Put("/api/profile", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] { return Json{{"user", database.updateProfile(user, requestBody(request))}}; });
  });
  server.Put("/api/profile/password", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] {
      const Json body = requestBody(request);
      database.changePassword(user, body.at("currentPassword").get<std::string>(),
                              body.at("newPassword").get<std::string>());
      return Json{{"success", true}};
    });
  });

  server.Get("/api/products", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] {
      Json result = database.products();
      for (Json& product : result) {
        product["price"] = pricing.categoryPrice(product["category"].get<std::string>(),
                                                  product["basePrice"].get<double>());
      }
      return Json{{"items", result}};
    });
  });
  server.Post("/api/products", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireAdmin(request, response, database, user)) return;
    handle(response, [&] {
      const Json product = database.saveProduct(requestBody(request), true);
      Json output = product;
      output["price"] = pricing.categoryPrice(output["category"].get<std::string>(),
                                               output["basePrice"].get<double>());
      database.recordAudit(user, "Product added", product["name"].get<std::string>() + ".");
      response.status = 201;
      return output;
    });
  });
  server.Put(R"(/api/products/(\d+))", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireAdmin(request, response, database, user)) return;
    handle(response, [&] {
      Json body = requestBody(request);
      body["id"] = std::stoi(request.matches[1]);
      Json product = database.saveProduct(body, false);
      product["price"] = pricing.categoryPrice(product["category"].get<std::string>(), product["basePrice"].get<double>());
      database.recordAudit(user, "Product updated", product["name"].get<std::string>() + ".");
      return product;
    });
  });
  server.Delete(R"(/api/products/(\d+))", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireAdmin(request, response, database, user)) return;
    handle(response, [&] {
      const int id = std::stoi(request.matches[1]);
      if (!database.deleteProduct(id)) throw std::invalid_argument("Product was not found.");
      database.recordAudit(user, "Product deleted", "Product ID " + std::to_string(id) + ".");
      return Json{{"success", true}};
    });
  });

  server.Get("/api/coupons", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] { return Json{{"items", database.coupons()}}; });
  });
  server.Get("/api/coupons/featured", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] { return Json{{"coupon", database.featuredCoupon()}}; });
  });
  server.Post("/api/coupons", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireAdmin(request, response, database, user)) return;
    handle(response, [&] {
      Json coupon = database.saveCoupon(requestBody(request), true);
      database.recordAudit(user, "Coupon created", coupon["code"].get<std::string>() + ".");
      response.status = 201;
      return coupon;
    });
  });
  server.Put(R"(/api/coupons/(.+))", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireAdmin(request, response, database, user)) return;
    handle(response, [&] {
      Json body = requestBody(request);
      body["code"] = request.matches[1].str();
      Json coupon = database.saveCoupon(body, false);
      database.recordAudit(user, "Coupon updated", coupon["code"].get<std::string>() + ".");
      return coupon;
    });
  });
  server.Delete(R"(/api/coupons/(.+))", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireAdmin(request, response, database, user)) return;
    handle(response, [&] {
      const std::string code = request.matches[1].str();
      if (!database.deleteCoupon(code)) throw std::invalid_argument("Coupon was not found.");
      database.recordAudit(user, "Coupon deleted", code + ".");
      return Json{{"success", true}};
    });
  });

  server.Get("/api/discounts", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] { return Json{{"items", database.discounts()}}; });
  });
  server.Post("/api/discounts", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireAdmin(request, response, database, user)) return;
    handle(response, [&] {
      Json discount = database.saveDiscount(requestBody(request), true);
      database.recordAudit(user, "Discount created", discount["name"].get<std::string>() + ".");
      response.status = 201;
      return discount;
    });
  });
  server.Put(R"(/api/discounts/(.+))", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireAdmin(request, response, database, user)) return;
    handle(response, [&] {
      Json body = requestBody(request);
      body["id"] = request.matches[1].str();
      Json discount = database.saveDiscount(body, false);
      database.recordAudit(user, "Discount updated", discount["name"].get<std::string>() + ".");
      return discount;
    });
  });
  server.Delete(R"(/api/discounts/(.+))", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireAdmin(request, response, database, user)) return;
    handle(response, [&] {
      const std::string id = request.matches[1].str();
      if (!database.deleteDiscount(id)) throw std::invalid_argument("Discount was not found.");
      database.recordAudit(user, "Discount deleted", id + ".");
      return Json{{"success", true}};
    });
  });

  server.Post("/api/pricing/quote", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] {
      const Json body = requestBody(request);
      return database.quote(body.at("items"), body.value("couponCode", ""), pricing.categoryPrice);
    });
  });
  server.Post("/api/pricing/case-study", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] {
      const Json body = requestBody(request);
      const int id = body.at("productId").get<int>();
      const double percent = body.at("percent").get<double>();
      const double coupon = body.at("coupon").get<double>();
      const Json productList = database.products();
      const auto found = std::find_if(productList.begin(), productList.end(),
          [id](const Json& item) { return item["id"].get<int>() == id; });
      if (found == productList.end()) throw std::invalid_argument("Product was not found.");
      const std::string category = (*found)["category"].get<std::string>();
      const double basePrice = (*found)["basePrice"].get<double>();
      return Json{{"productId", id}, {"category", category}, {"basePrice", basePrice},
          {"categoryPrice", pricing.categoryPrice(category, basePrice)},
          {"afterPercent", pricing.percentageDiscount(category, basePrice, percent)},
          {"afterCoupon", pricing.couponDiscount(category, basePrice, percent, coupon)}};
    });
  });
  server.Get("/api/sales", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] { return Json{{"items", database.sales(user)}}; });
  });
  server.Post("/api/sales", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] {
      Json sale = database.createSale(requestBody(request), user, pricing.categoryPrice);
      response.status = 201;
      return sale;
    });
  });
  server.Get(R"(/api/sales/(.+))", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireUser(request, response, database, user)) return;
    handle(response, [&] {
      Json result = database.sale(request.matches[1].str(), user);
      if (result.is_null()) throw std::invalid_argument("Order was not found.");
      return result;
    });
  });
  server.Get("/api/audit", [&](const httplib::Request& request, httplib::Response& response) {
    Json user;
    if (!requireAdmin(request, response, database, user)) return;
    handle(response, [&] { return Json{{"items", database.auditEntries()}}; });
  });

  const std::string staticRoot = pathFor(repositoryRoot, "frontend");
  if (!server.set_mount_point("/", staticRoot)) {
    throw std::runtime_error("Could not mount frontend files at " + staticRoot + ".");
  }
  server.set_error_handler([](const httplib::Request&, httplib::Response& response) {
    if (response.status == 404) respond(response, 404, {{"error", "The requested route was not found."}});
  });
  std::cout << "Campus Store API (" << (std::getenv("CSMS_API_VARIANT") ? std::getenv("CSMS_API_VARIANT") : "C++")
            << ") listening at http://127.0.0.1:" << port << "/\n";
  if (!server.listen("127.0.0.1", port)) {
    std::cerr << "Could not start the API server on port " << port << ".\n";
    return 1;
  }
  return 0;
}
