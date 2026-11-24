// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/pre1_amiga_save.hpp"
#include "titus/session.hpp"
#include <fstream>
namespace titus::pre1_amiga {
class ManagedHost {
  Json request_;
  fs::path directory_, work_, root_;
  std::ofstream events_;
  unsigned sequence_ = 0, command_sequence_ = 0;
  std::size_t command_offset_ = 0;
  bool ready_ = false, returning_ = false, finished_ = false;
  void emit(const char *, const Json &body = Json::object());

public:
  ManagedHost(const std::vector<std::string> &);
  bool inspection() const { return request_.at("kind") == "inspect_request"; }
  void inspect();
  std::vector<std::string> arguments() const;
  const Json &settings() const { return request_.at("settings"); }
  const Json &action() const { return request_.at("action"); }
  std::string fingerprint() const {
    return request_.at("sources")[0].at("fingerprint");
  }
  fs::path root() const { return root_; }
  void ready(const char *scene);
  bool poll();
  void observe(const Json &);
  void finish();
  void fail(const std::exception &);
};
} // namespace titus::pre1_amiga
