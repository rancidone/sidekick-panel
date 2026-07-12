#include "magicpanel/event.h"

#include <cctype>
#include <string>
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

std::optional<std::string> extract_scalar(std::string const& line, std::size_t& pos) {
  while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
    ++pos;
  }
  if (pos >= line.size()) {
    return std::nullopt;
  }
  if (line[pos] == '"') {
    std::string value;
    bool escaped = false;
    for (++pos; pos < line.size(); ++pos) {
      char ch = line[pos];
      if (escaped) {
        value.push_back(ch);
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == '"') {
        ++pos;
        return value;
      } else {
        value.push_back(ch);
      }
    }
    return std::nullopt;
  }
  std::size_t begin = pos;
  while (pos < line.size() && line[pos] != ',' && line[pos] != '}') {
    ++pos;
  }
  return trim(line.substr(begin, pos - begin));
}

void extract_top_level_fields(std::string const& line, Event& event) {
  std::size_t pos = 1;
  while (pos < line.size()) {
    std::size_t quote = line.find('"', pos);
    if (quote == std::string::npos) {
      return;
    }
    std::size_t end_quote = line.find('"', quote + 1);
    if (end_quote == std::string::npos) {
      return;
    }
    std::string key = line.substr(quote + 1, end_quote - quote - 1);
    std::size_t colon = line.find(':', end_quote + 1);
    if (colon == std::string::npos) {
      return;
    }
    pos = colon + 1;
    auto value = extract_scalar(line, pos);
    if (!value) {
      return;
    }
    if (key != "event") {
      event.fields[key] = *value;
    }
    std::size_t comma = line.find(',', pos);
    if (comma == std::string::npos) {
      return;
    }
    pos = comma + 1;
  }
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
  extract_top_level_fields(cleaned, event);
  return event;
}

std::string encode_event_line(std::string const& event_name) {
  return "{\"event\":\"" + event_name + "\"}\n";
}

}  // namespace magicpanel
