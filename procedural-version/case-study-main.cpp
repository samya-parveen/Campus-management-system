#include "store.hpp"

#include <iostream>

int main() {
  procedural::Store store;
  if (!procedural::addProduct(store, 101, "C++ Textbook",
                              study::Category::Book, 500.0) ||
      !procedural::addProduct(store, 202, "USB Mouse",
                              study::Category::Accessory, 800.0) ||
      !procedural::addProduct(store, 303, "Pen Set",
                              study::Category::Stationery, 200.0)) {
    std::cerr << "Could not load the reference products.\n";
    return 1;
  }
  std::cout << procedural::demoOutput(store) << procedural::audit(store, 101) << '\n';
  return 0;
}
