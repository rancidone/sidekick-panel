#include "magicpanel/state_store.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace magicpanel {

AccumulatingStateStore::AccumulatingStateStore(std::string path) : path_(std::move(path)) {
  load();
}

int AccumulatingStateStore::get(std::string const& key) const {
  auto it = data_.find(key);
  return it == data_.end() ? 0 : it->second;
}

int AccumulatingStateStore::increment(std::string const& key, int amount) {
  data_[key] = get(key) + amount;
  save();
  return data_[key];
}

void AccumulatingStateStore::reset(std::string const& key) {
  data_[key] = 0;
  save();
}

bool AccumulatingStateStore::load() {
  data_.clear();
  if (path_.empty()) {
    return true;
  }
  std::ifstream file(path_);
  if (!file) {
    return false;
  }
  std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::size_t pos = 0;
  while (true) {
    std::size_t key_begin = body.find('"', pos);
    if (key_begin == std::string::npos) {
      break;
    }
    std::size_t key_end = body.find('"', key_begin + 1);
    std::size_t colon = body.find(':', key_end);
    if (key_end == std::string::npos || colon == std::string::npos) {
      break;
    }
    std::size_t value_begin = body.find_first_of("-0123456789", colon + 1);
    if (value_begin == std::string::npos) {
      break;
    }
    char* end_ptr = nullptr;
    int value = static_cast<int>(std::strtol(body.c_str() + value_begin, &end_ptr, 10));
    data_[body.substr(key_begin + 1, key_end - key_begin - 1)] = value;
    pos = static_cast<std::size_t>(end_ptr - body.c_str());
  }
  return true;
}

bool AccumulatingStateStore::save() const {
  if (path_.empty()) {
    return true;
  }
  std::ofstream file(path_, std::ios::trunc);
  if (!file) {
    return false;
  }
  file << "{";
  bool first = true;
  for (auto const& [key, value] : data_) {
    if (!first) {
      file << ",";
    }
    first = false;
    file << "\"" << key << "\":" << value;
  }
  file << "}";
  return true;
}

}  // namespace magicpanel
