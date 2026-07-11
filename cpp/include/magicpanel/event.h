#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace magicpanel {

struct Event {
  std::string name;
  std::unordered_map<std::string, std::string> fields;

  std::string field_or(std::string const& key, std::string fallback = "") const;
};

std::optional<Event> decode_event_line(std::string const& line);
std::string encode_event_line(std::string const& event_name);

}  // namespace magicpanel
