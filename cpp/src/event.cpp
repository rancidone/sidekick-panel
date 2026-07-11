#include "magicpanel/event.h"

#include <cctype>
#include <sstream>

namespace magicpanel {
namespace {

std::string trim(std::string const& value) {
  std::size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }
  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(begin, end - begin);
}

std::optional<std::string> extract_string_field(std::string const& line,
                                                std::string const& key) {
  std::string needle = "\"" + key + "\"";
  std::size_t key_pos = line.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  std::size_t colon = line.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t quote = line.find('"', colon + 1);
  if (quote == std::string::npos) {
    return std::nullopt;
  }
  std::string value;
  bool escaped = false;
  for (std::size_t i = quote + 1; i < line.size(); ++i) {
    char ch = line[i];
    if (escaped) {
      value.push_back(ch);
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      return value;
    }
    value.push_back(ch);
  }
  return std::nullopt;
}

}  // namespace

std::string Event::field_or(std::string const& key, std::string fallback) const {
  auto it = fields.find(key);
  return it == fields.end() ? fallback : it->second;
}

std::optional<Event> decode_event_line(std::string const& line) {
  std::string cleaned = trim(line);
  if (cleaned.empty() || cleaned.front() != '{' || cleaned.back() != '}') {
    return std::nullopt;
  }
  auto name = extract_string_field(cleaned, "event");
  if (!name || name->empty()) {
    return std::nullopt;
  }

  Event event;
  event.name = *name;
  if (auto target = extract_string_field(cleaned, "to")) {
    event.fields["to"] = *target;
  }
  return event;
}

std::string encode_event_line(std::string const& event_name) {
  return "{\"event\":\"" + event_name + "\"}\n";
}

}  // namespace magicpanel
