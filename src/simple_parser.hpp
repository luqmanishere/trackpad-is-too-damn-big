/*
 *
 * This file is part of SimpleCmdParser
 * Copyright (c) https://github.com/tascvh/SimpleCmdParser
 *
 */
#include <iostream>
#include <optional>
#include <sstream>

class SimpleParser {
protected:
  int m_argc;
  char **m_argv;

public:
  bool m_showHelp;
  // SimpleParser() = delete;
  SimpleParser(int argc, char **argv)
      : m_argc(argc), m_argv(argv), m_showHelp(false) {};

  auto programName() { return std::string(m_argv[0]); }

  template <typename T> std::string getTypeName(const std::optional<T> &) {
    if (std::is_same<T, int>::value)
      return "int";
    if (std::is_same<T, long>::value)
      return "long";
    if (std::is_same<T, long long>::value)
      return "long long";
    if (std::is_same<T, short>::value)
      return "short";
    if (std::is_same<T, float>::value)
      return "float";
    if (std::is_same<T, double>::value)
      return "double";
    if (std::is_same<T, long double>::value)
      return "long double";
    if (std::is_same<T, char>::value)
      return "char";
    if (std::is_same<T, std::string>::value)
      return "string";
    if (std::is_same<T, bool>::value)
      return "bool";
    return "";
  }

  template <typename ArgType>
  void
  printHelp(std::optional<ArgType> arg, const std::string &prefix,
            const std::string &description,
            std::pair<std::optional<ArgType>, std::optional<ArgType>> minmax) {

    auto &o1 = std::cout;
    auto tn = getTypeName(arg);
    o1 << prefix + " : (" + tn + ") " + description;
    if (arg)
      o1 << "\n\tDefault value : " << *arg;

    if (minmax.first) {
      o1 << "\n\tmin value : " << minmax.first.value();
    }

    if (minmax.second) {
      o1 << "\n\tmax value : " << minmax.second.value();
    }

    o1 << std::endl;
  }

  template <typename ArgType>
  void
  read(ArgType &val, const std::string &prefix,
       const std::string &description = "",
       std::pair<std::optional<ArgType>, std::optional<ArgType>> minmax = {}) {
    std::optional<ArgType> opt(std::move(val));
    read(opt, prefix, description, minmax);
    val = std::move(*opt);
  }

  template <typename ArgType>
  void
  read(std::optional<ArgType> &val, const std::string &prefix,
       const std::string &description = "",
       std::pair<std::optional<ArgType>, std::optional<ArgType>> minmax = {}) {

    if (m_showHelp) {
      printHelp(val, prefix, description, minmax);
      return;
    }

    for (int i = 1; i < m_argc; ++i) {
      std::string arg(m_argv[i]);
      if (arg == prefix) {
        if constexpr (std::is_same<ArgType, bool>::value) {
          val.emplace(true);
          return;
        }
        i++;
        if (i == m_argc) {
          auto s = "No value supplied for argument " + prefix;
          throw std::invalid_argument(s);
        }

        std::string value(m_argv[i]);
        std::istringstream iss(value);
        ArgType result;
        if (iss >> result) {
          if (minmax.first && result < minmax.first) {
            std::ostringstream oss;
            oss << "Argument " << prefix << " cannot be smaller than "
                << minmax.first.value();
            throw std::invalid_argument(oss.str());
          }
          if (minmax.second && result > minmax.second) {
            std::ostringstream oss;
            oss << "Argument " << prefix << " cannot be greater than "
                << minmax.second.value();
            throw std::invalid_argument(oss.str());
          }
          val.emplace(std::move(result));
          return;
        } else {
          auto s = "Cannot read (" + value + ") as " + getTypeName(val) +
                   " for argument " + prefix;
          throw std::invalid_argument(s);
        }
      }
    }
  }
};
