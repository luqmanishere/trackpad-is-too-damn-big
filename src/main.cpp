/*
 *
 * This file is part of trackpad-is-too-damn-big utility
 * Copyright (c) https://github.com/tascvh/trackpad-is-too-damn-big
 *
 */
#include "devices.hpp"
#include "event_filters.hpp"
#include "event_handlers.hpp"
#include "simple_parser.hpp"
#include <iostream>
#include <optional>

int main(int argc, char **argv) {

  struct RunningMode {
    enum class Type { Print, Strict, Flex, Invalid };
    static Type FromString(const std::string &mode) {
      if (mode == "p")
        return Type::Print;
      if (mode == "s")
        return Type::Strict;
      if (mode == "f")
        return Type::Flex;
      return Type::Invalid;
    }
    static std::string Usage() { return "Running mode options : p/s/f "; }
  };

  try {
    // cmd arguments
    std::optional<std::string> device;
    std::string mode("f");
    int left(10);
    int right(10);
    int top(0);
    int bottom(15);

    {
      SimpleParser sp(argc, argv);

      sp.read(sp.m_showHelp, "-h");
      sp.read(device, "-d", "trackpad device filename (mandatory)");
      sp.read(mode, "-m", RunningMode::Usage());
      sp.read(left, "-l", "left percentage", {0, 100});
      sp.read(right, "-r", "right percentage", {0, 100});
      sp.read(top, "-t", "top percentage", {0, 100});
      sp.read(bottom, "-b", "bottom percentage", {0, 100});

      if (sp.m_showHelp) {
        std::cout << std::endl << "Example usage :" << std::endl;
        std::cout << "\t" << sp.programName() << " -d /dev/input/event0 -m f"
                  << std::endl;
        return EXIT_SUCCESS;
      }
    }

    if (!device)
      throw std::invalid_argument("device argument is mandatory");

    auto running_mode = RunningMode::FromString(mode);
    if (running_mode == RunningMode::Type::Invalid)
      throw std::invalid_argument("Invalid running mode: " + mode);

    // All parameters are valid

    auto evdev = Evdev(*device);

    switch (running_mode) {
    case RunningMode::Type::Print: {
      evdev.print();
      return evdev.runEventLoop(PrintEvents());
    }
    case RunningMode::Type::Strict: {
      return evdev.runEventLoop(
          ForwardTo(evdev.Spawn<UInput>(),
                    evdev.Spawn<CropRect>(left, right, top, bottom)));
    }
    case RunningMode::Type::Flex: {
      return evdev.runEventLoop(
          ForwardTo(evdev.Spawn<UInput>(),
                    evdev.Spawn<CropRectFlex>(left, right, top, bottom)));
    }
    case RunningMode::Type::Invalid: {
      // unreachable
    }
    }

  } catch (const std::invalid_argument &e) {
    std::cerr << "invalid argument: " << e.what() << std::endl;
    std::cerr << "use -h for help" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Caught unknown exception" << std::endl;
  }

  return EXIT_FAILURE;
}
