#pragma once

#include <string>
#include <unordered_map>

namespace magicpanel {

class AccumulatingStateStore {
 public:
  AccumulatingStateStore() = default;
  explicit AccumulatingStateStore(std::string path);

  int get(std::string const& key) const;
  int increment(std::string const& key, int amount = 1);
  void reset(std::string const& key);
  bool load();
  bool save() const;

 private:
  std::string path_;
  std::unordered_map<std::string, int> data_;
};

}  // namespace magicpanel
