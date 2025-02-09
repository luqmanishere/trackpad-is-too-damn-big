/*
 *
 * This file is part of trackpad-is-too-damn-big utility
 * Copyright (c) https://github.com/tascvh/trackpad-is-too-damn-big
 *
 */
#include "cmdparser.hpp"
#include "devices.hpp"
#include "event_filters.hpp"
#include "event_handlers.hpp"
#include <iostream>
#include <sstream>

int main(int argc, char **argv) {
  try {
    std::stringstream output{};
    std::stringstream errors{};

    cli::Parser parser(argc, argv);

    parser.set_required<std::string>("d", "device", "Device filename");
    parser.set_optional<std::string>("m", "mode", "f",
                                     "Running mode:\n"
                                     "\tf (Flex)\n"
                                     "\ts (Strict)\n"
                                     "\tp (Only Print Events)\n");
    parser.set_optional<int>("l", "left", 10, "Crop perc. from left side");
    parser.set_optional<int>("r", "right", 10, "Crop perc. from right side");
    parser.set_optional<int>("t", "top", 0, "Crop perc. from top");
    parser.set_optional<int>("b", "bottom", 15, "Crop perc. from bottom");

    parser.run_and_exit_if_error();

    const auto device = parser.get<std::string>("d");
    const auto mode = parser.get<std::string>("m");

    const auto left = parser.get<int>("l");
    const auto right = parser.get<int>("r");
    const auto top = parser.get<int>("t");
    const auto bottom = parser.get<int>("b");

    auto evdev = Evdev(device);

    if (mode == "p") {
      // print mode
      // Only print incoming events to the device
      evdev.print();
      return evdev.runEventLoop(PrintEvents());
    } else if (mode == "s") {
      // strict mode
      // strictly disable cropped out areas
      return evdev.runEventLoop(
          ForwardTo(evdev.Spawn<UInput>(),
                    evdev.Spawn<CropRect>(left, right, top, bottom)));
    } else if (mode == "f") {
      // flex mode
      // permit re-entry to and gestures in cropped out areas
      return evdev.runEventLoop(
          ForwardTo(evdev.Spawn<UInput>(),
                    evdev.Spawn<CropRectFlex>(left, right, top, bottom)));
    } else {
      std::cerr << "unknown mode" << std::endl;
    }

  } catch (const std::runtime_error &e) {
    std::cerr << "runtime error: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Caught unknown exception" << std::endl;
  }
}
