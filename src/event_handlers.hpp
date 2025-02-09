/*
 *
 * This file is part of trackpad-is-too-damn-big utility
 * Copyright (c) https://github.com/tascvh/trackpad-is-too-damn-big
 *
 */
#include "libevdev/libevdev.h"
#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

static int print_event(const struct input_event *const ev) {
  if (ev->type == EV_SYN)
    printf("Event: time %ld.%06ld, ++++++++++++++++++++ %s +++++++++++++++\n",
           ev->input_event_sec, ev->input_event_usec,
           libevdev_event_type_get_name(ev->type));
  else
    printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %d\n",
           ev->input_event_sec, ev->input_event_usec, ev->type,
           libevdev_event_type_get_name(ev->type), ev->code,
           libevdev_event_code_get_name(ev->type, ev->code), ev->value);
  return 0;
}

class PrintEvents {
public:
  bool grab() { return false; }
  void eventReport(const struct input_event &ev) { print_event(&ev); }
  void eventData(const struct input_event &ev) { print_event(&ev); }
  void eventSync(const struct input_event &ev) { print_event(&ev); }
};

template <typename Destination, typename Filter> class ForwardTo {
  std::vector<struct input_event> m_event_buffer;
  Destination m_dest;
  Filter m_filter;

public:
  bool grab() { return true; }

  ForwardTo(Destination dest, Filter filter)
      : m_dest(std::move(dest)), m_filter(std::move(filter)) {
    m_event_buffer.reserve(50);
  }

  void eventSync(const struct input_event &) {}

  constexpr void eventReport(const struct input_event &ev) {
    m_event_buffer.push_back(ev);
    m_filter.processEvents(m_event_buffer);
    for (auto &ev : m_event_buffer) {
      m_dest.writeEvent(ev);
    }
    m_event_buffer.clear();
  }

  constexpr void eventData(const struct input_event &ev) {
    m_event_buffer.push_back(ev);
  }
};
