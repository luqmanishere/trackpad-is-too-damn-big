#include <optional>
#include <sstream>

class SimpleParser {
  int m_argc;
  const char *const *const m_argv;

public:
  std::optional<bool> m_showHelp;
  SimpleParser() = delete;
  SimpleParser(int argc, const char *const *const argv)
      : m_argc(argc), m_argv(argv), m_showHelp(false) {};
  template <typename ArgType>
  void read(std::optional<ArgType> &val, const std::string &prefix,
            const std::string &description) {

    if (*m_showHelp) {
      std::cout << prefix << " " << description;
      if (val) {
        std::cout << " default value : " << *val;
      }
      std::cout << std::endl;
      return;
    }

    for (int i = 1; i < m_argc; ++i) {
      std::string arg(m_argv[i]);
      if (arg == prefix) {
        if constexpr (std::is_same<ArgType, bool>::value) {
          val = true;
          return;
        }
        i++;
        if (i == m_argc) {
          throw std::invalid_argument("cannot parse argument " + prefix);
        }

        std::string value(m_argv[i]);
        std::istringstream iss(value);
        ArgType result;
        if (iss >> result) {
          val = result;
          return;
        } else {
          throw std::invalid_argument("cannot parse argument " + prefix);
        }
      }
    }
  }
};
