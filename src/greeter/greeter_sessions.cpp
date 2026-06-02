#include "greeter/greeter_sessions.h"

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

std::string trim(std::string value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return {};
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string sanitizeDesktopExec(const std::string &exec) {
  std::istringstream stream(exec);
  std::string token;
  std::string out;
  while (stream >> token) {
    if (!token.empty() && token[0] == '%') {
      continue;
    }
    if (!out.empty()) {
      out.push_back(' ');
    }
    out += token;
  }
  return trim(out);
}

[[nodiscard]] bool equalsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

} // namespace

namespace greeter {

std::vector<SessionOption> discoverSessions() {
  std::vector<SessionOption> sessions;
  const std::array<std::filesystem::path, 2> dirs = {
      "/usr/share/wayland-sessions",
      "/usr/local/share/wayland-sessions",
  };

  for (const auto &dir : dirs) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || ec) {
      continue;
    }
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
      if (ec || !entry.is_regular_file()) {
        continue;
      }
      if (entry.path().extension() != ".desktop") {
        continue;
      }

      std::ifstream in(entry.path());
      std::string line;
      std::string name;
      std::string exec;
      while (std::getline(in, line)) {
        if (line.rfind("Name=", 0) == 0) {
          name = trim(line.substr(5));
        } else if (line.rfind("Exec=", 0) == 0) {
          exec = sanitizeDesktopExec(line.substr(5));
        }
      }

      if (!name.empty() && !exec.empty()) {
        sessions.push_back(SessionOption{.name = name, .command = exec});
      }
    }
  }

  if (sessions.empty()) {
    sessions.push_back(SessionOption{.name = "Shell", .command = "/bin/sh"});
  }
  return sessions;
}

std::optional<std::size_t>
findSessionIndex(const std::vector<SessionOption> &sessions,
                 std::string_view name) {
  if (name.empty()) {
    return std::nullopt;
  }
  for (std::size_t i = 0; i < sessions.size(); ++i) {
    if (equalsIgnoreCase(sessions[i].name, name)) {
      return i;
    }
  }
  return std::nullopt;
}

} // namespace greeter
