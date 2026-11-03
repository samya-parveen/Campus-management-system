#include "acceptance_suite.hpp"
#include "../procedural-version/store.hpp"

#include <utility>

namespace {

class ProceduralAdapter {
 public:
  bool addProduct(int id, std::string name, study::Category category, double price) {
    return procedural::addProduct(store_, id, std::move(name), category, price);
  }

  bool deleteProduct(int id) { return procedural::deleteProduct(store_, id); }
  int productCount() const { return procedural::productCount(store_); }
  int liveProductCount() const { return procedural::liveProductCount(store_); }
  bool categoryPrice(int id, double& price) const {
    return procedural::categoryPrice(store_, id, price);
  }
  double discount(int id, double percent) const {
    return procedural::discount(store_, id, percent);
  }
  double discountWithCoupon(int id, double percent, double coupon) const {
    return procedural::discountWithCoupon(store_, id, percent, coupon);
  }
  double billTotal(double first, double second) const {
    return procedural::billTotal(first, second);
  }
  std::string billOutput(double first, double second) const {
    return procedural::billOutput(first, second);
  }
  std::string audit(int id) const { return procedural::audit(store_, id); }

 private:
  procedural::Store store_;
};

}

int main() {
  return study::runAcceptanceSuite<ProceduralAdapter>("Procedural");
}