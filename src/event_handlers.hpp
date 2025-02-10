/*
 *
 * This file is part of trackpad-is-too-damn-big utility
 * Copyright (c) https://github.com/tascvh/trackpad-is-too-damn-big
 *
 */
#include "libevdev/libevdev.h"
#include <vector>

extern "C" int print_event(const struct input_event *const ev);

class PrintEvents {
public:
  bool grab() { return false; }
  void eventReport(const input_event &ev) { print_event(&ev); }
  void eventData(const input_event &ev) { print_event(&ev); }
  void eventSync(const input_event &ev) { print_event(&ev); }
};

template <typename Destination, typename Filter> class ForwardTo {
  std::vector<input_event> m_event_buffer;
  Destination m_dest;
  Filter m_filter;

public:
  bool grab() { return true; }

  ForwardTo(Destination dest, Filter filter)
      : m_dest(std::move(dest)), m_filter(std::move(filter)) {
    m_event_buffer.reserve(50);
  }

  void eventSync(const input_event &) {}

  constexpr void eventReport(const input_event &ev) {
    m_event_buffer.push_back(ev);
    m_filter.processEvents(m_event_buffer);
    for (auto &ev : m_event_buffer) {
      m_dest.writeEvent(ev);
    }
    m_event_buffer.clear();
  }

  constexpr void eventData(const input_event &ev) {
    m_event_buffer.push_back(ev);
  }
};
