#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "magicpanel/state_store.h"

namespace magicpanel {

enum class ReactionKind {
  Transient,
  Sticky,
  DurationBound,
  Accumulating,
};

struct ReactionRule {
  std::string name;
  ReactionKind kind;
  std::string trigger_event;
  float duration_seconds = 0.0f;
  std::string resolve_event;
  std::string end_event;
  std::string reset_event;
};

class ReactionEngine {
 public:
  ReactionEngine(std::vector<ReactionRule> rules, AccumulatingStateStore* accumulator);

  void handle_event(std::string const& event_name);
  void tick(float dt);
  bool is_active(std::string const& name) const;
  int accumulated(std::string const& name) const;

 private:
  using RuleIndex = std::vector<std::size_t>;

  void index_rule(std::size_t i);
  void clear_active(std::string const& name);

  std::vector<ReactionRule> rules_;
  AccumulatingStateStore* accumulator_;
  std::unordered_map<std::string, RuleIndex> by_trigger_;
  std::unordered_map<std::string, RuleIndex> by_resolve_;
  std::unordered_map<std::string, RuleIndex> by_end_;
  std::unordered_map<std::string, RuleIndex> by_reset_;
  std::unordered_map<std::string, float> transient_expiry_;
  std::unordered_set<std::string> active_;
  float clock_ = 0.0f;
};

}  // namespace magicpanel
