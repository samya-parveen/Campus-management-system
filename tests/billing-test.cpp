#include "acceptance_suite.hpp"
#include "../backend/include/Store.h"

#include <iostream>

int main() {
  const int acceptanceResult = study::runAcceptanceSuite<oop::Store>("OOP");
  study::AcceptanceResults copyResults;
  copyResults.check("OOP: live-object count remains correct when products are copied", [] {
    const int initial = oop::Product::getProductCount();
    {
      oop::Book original(901, "Copy source", 100.0);
      if (oop::Product::getProductCount() != initial + 1) return false;
      {
        oop::Book copy(original);
        if (oop::Product::getProductCount() != initial + 2) return false;
        copy = original;
        if (oop::Product::getProductCount() != initial + 2) return false;
      }
      if (oop::Product::getProductCount() != initial + 1) return false;
    }
    if (oop::Product::getProductCount() != initial) return false;
    try {
      oop::Book invalid(902, " ", -1.0);
    } catch (const std::invalid_argument&) {
      return oop::Product::getProductCount() == initial;
    }
    return false;
  });
  std::cout << "OOP copy-safety tests: " << copyResults.passed() << '/'
            << copyResults.total() << " passed\n";
  return acceptanceResult == 0 && copyResults.passed() == copyResults.total() ? 0 : 1;
}