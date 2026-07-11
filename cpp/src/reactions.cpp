#include "magicpanel/reactions.h"

#include <algorithm>

namespace magicpanel {

ReactionEngine::ReactionEngine(std::vector<ReactionRule> rules,
                               AccumulatingStateStore* accumulator)
    : rules_(std::move(rules)), accumulator_(accumulator) {
  for (std::size_t i = 0; i < rules_.size(); ++i) {
    index_rule(i);
  }
}

void ReactionEngine::index_rule(std::size_t i) {
  auto const& rule = rules_[i];
  by_trigger_[rule.trigger_event].push_back(i);
  if (!rule.resolve_event.empty()) {
    by_resolve_[rule.resolve_event].push_back(i);
  }
  if (!rule.end_event.empty()) {
    by_end_[rule.end_event].push_back(i);
  }
  if (!rule.reset_event.empty()) {
    by_reset_[rule.reset_event].push_back(i);
  }
}

void ReactionEngine::clear_active(std::string const& name) {
  active_.erase(name);
  transient_expiry_.erase(name);
}

void ReactionEngine::handle_event(std::string const& event_name) {
  if (auto it = by_resolve_.find(event_name); it != by_resolve_.end()) {
    for (std::size_t index : it->second) {
      clear_active(rules_[index].name);
    }
  }

  if (auto it = by_end_.find(event_name); it != by_end_.end()) {
    for (std::size_t index : it->second) {
      clear_active(rules_[index].name);
    }
  }

  if (accumulator_ != nullptr) {
    if (auto it = by_reset_.find(event_name); it != by_reset_.end()) {
      for (std::size_t index : it->second) {
        accumulator_->reset(rules_[index].name);
      }
    }
  }

  auto triggered = by_trigger_.find(event_name);
  if (triggered == by_trigger_.end()) {
    return;
  }

  for (std::size_t index : triggered->second) {
    auto const& rule = rules_[index];
    switch (rule.kind) {
      case ReactionKind::Transient:
        active_.insert(rule.name);
        transient_expiry_[rule.name] = clock_ + rule.duration_seconds;
        break;
      case ReactionKind::Sticky:
      case ReactionKind::DurationBound:
        active_.insert(rule.name);
        break;
      case ReactionKind::Accumulating:
        if (accumulator_ != nullptr) {
          accumulator_->increment(rule.name);
        }
        break;
    }
  }
}

void ReactionEngine::tick(float dt) {
  clock_ += dt;
  std::vector<std::string> expired;
  for (auto const& [name, expiry] : transient_expiry_) {
    if (expiry <= clock_) {
      expired.push_back(name);
    }
  }
  for (auto const& name : expired) {
    clear_active(name);
  }
}

bool ReactionEngine::is_active(std::string const& name) const {
  return active_.find(name) != active_.end();
}

int ReactionEngine::accumulated(std::string const& name) const {
  return accumulator_ == nullptr ? 0 : accumulator_->get(name);
}

}  // namespace magicpanel
