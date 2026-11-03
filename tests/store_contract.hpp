#pragma once

#include <string>

namespace study {

enum class Category {
  Book,
  Accessory,
  Stationery
};

inline const char* categoryName(Category category) {
  switch (category) {
    case Category::Book:
      return "Book";
    case Category::Accessory:
      return "Accessory";
    case Category::Stationery:
      return "Stationery";
  }
  return "Unknown";
}

}
