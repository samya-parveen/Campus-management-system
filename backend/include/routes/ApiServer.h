#pragma once

#include "../Database/Database.h"

#include <functional>
#include <string>

struct PricingEngine {
  CategoryPriceFunction categoryPrice;
  std::function<double(const std::string&, double, double)> percentageDiscount;
  std::function<double(const std::string&, double, double, double)> couponDiscount;
};

int runApiServer(const PricingEngine& pricing, const std::string& repositoryRoot);
