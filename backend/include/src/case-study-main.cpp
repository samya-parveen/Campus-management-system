#include "../Store.h"

#include <iostream>

int main() {
  oop::Store store;
  if (!store.addProduct(101, "C++ Textbook", study::Category::Book, 500.0) ||
      !store.addProduct(202, "USB Mouse", study::Category::Accessory, 800.0) ||
      !store.addProduct(303, "Pen Set", study::Category::Stationery, 200.0)) {
    std::cerr << "Could not load the reference products.\n";
    return 1;
  }
  std::cout << store.demoOutput();
  std::cout << store.audit(101) << '\n';
  std::cout << "Live Product objects: " << oop::Product::getProductCount() << '\n';
  return 0;
}
