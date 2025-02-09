/*
 *
 * This file is part of trackpad-is-too-damn-big utility
 * Copyright (c) https://github.com/tascvh/trackpad-is-too-damn-big
 *
 */
#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "libevdev/libevdev.h"

class PassAll {
public:
  constexpr void processEvents(std::vector<struct input_event> &) {}
};

class CropRect {
protected:
  // original device area
  int m_dev_left;
  int m_dev_right;
  int m_dev_top;
  int m_dev_bottom;

  int m_num_slot_max;

  // current valid area
  int m_left;
  int m_right;
  int m_top;
  int m_bottom;

  int m_current_slot = 0;
  struct Point {
    int x = 0;
    int y = 0;
  };
  std::vector<Point> m_slot_coordinates;

public:
  CropRect() = delete;
  CropRect(libevdev const *const dev, int perc_left, int perc_right,
           int perc_top, int perc_bottom) {
    if (!libevdev_has_event_code(dev, EV_ABS, ABS_X) ||
        !libevdev_has_event_code(dev, EV_ABS, ABS_Y) ||
        !libevdev_has_event_code(dev, EV_ABS, ABS_MT_SLOT)) {
      throw std::runtime_error("Device does not appear to be a Trackpad");
    }

    const struct input_absinfo *ai;

    if ((ai = libevdev_get_abs_info(dev, ABS_MT_SLOT)) == NULL) {
      throw std::runtime_error("Failed to get slot info");
    }
    m_num_slot_max = ai->maximum;
    m_slot_coordinates.reserve(m_num_slot_max);

    if ((ai = libevdev_get_abs_info(dev, ABS_X)) == NULL) {
      throw std::runtime_error("Failed to get abs x info");
    }
    m_dev_left = ai->minimum;
    m_dev_right = ai->maximum;

    if ((ai = libevdev_get_abs_info(dev, ABS_Y)) == NULL) {
      throw std::runtime_error("Failed to get abs y info");
    }
    m_dev_top = ai->maximum;
    m_dev_bottom = ai->minimum;

    m_left = m_dev_left;
    m_right = m_dev_right;
    // y axis is flipped
    m_top = -m_dev_bottom;
    m_bottom = -m_dev_top;

    size_t range_x = m_right - m_left;
    size_t range_y = m_top - m_bottom;

    m_left += perc_left * range_x / 100;
    m_right -= perc_right * range_x / 100;
    m_bottom += perc_bottom * range_y / 100;
    m_top -= perc_top * range_y / 100;
  }

  constexpr bool insideValidArea(int x, int y) noexcept {
    y = -y; // y axis is flipped
    if (x >= m_left && x <= m_right && y >= m_bottom && y <= m_top) {
      return true;
    }
    return false;
  }

  constexpr void
  processEvents(std::vector<struct input_event> &event_buffer) noexcept {
    int slot = m_current_slot;

    for (const auto &ev : event_buffer) {
      switch (ev.code) {
      case ABS_MT_SLOT:
        m_current_slot = ev.value;
        break;
      case ABS_MT_POSITION_X:
        m_slot_coordinates[m_current_slot].x = ev.value;
        break;
      case ABS_MT_POSITION_Y:
        m_slot_coordinates[m_current_slot].y = ev.value;
        break;
      }
    }

    for (auto &ev : event_buffer) {
      switch (ev.code) {
      case ABS_MT_SLOT:
        slot = ev.value;
        break;
      case ABS_MT_TOUCH_MAJOR:
      case ABS_MT_TOUCH_MINOR:
      case ABS_MT_WIDTH_MAJOR:
      case ABS_MT_WIDTH_MINOR:
      case ABS_MT_PRESSURE:
        if (!insideValidArea(m_slot_coordinates[slot].x,
                             m_slot_coordinates[slot].y)) {
          ev.value = 0;
        }
        break;
      }
    }
  }
};

class CropRectFlex : CropRect {
  std::unordered_set<int> m_set_slots;
  std::vector<bool> m_slot_valid;

  int m_diagonal_sq;

public:
  CropRectFlex(libevdev *dev, int perc_left, int perc_right, int perc_top,
               int perc_bottom)
      : CropRect(dev, perc_left, perc_right, perc_top, perc_bottom) {
    auto delta_x = m_dev_right - m_dev_left;
    auto delta_y = m_dev_top - m_dev_bottom;
    m_diagonal_sq = delta_x * delta_x + delta_y * delta_y;
    m_set_slots.reserve(m_num_slot_max);
    m_slot_valid.reserve(m_num_slot_max);
  }

  void processEvents(std::vector<struct input_event> &event_buffer) noexcept {
    // we will save the last reported slot
    // before processing the events
    int slot = m_current_slot;

    // gather latest state from the events
    for (const auto &ev : event_buffer) {
      switch (ev.code) {
      case ABS_MT_TRACKING_ID:
        m_slot_valid[m_current_slot] = false;
        if (ev.value == -1) {
          m_set_slots.erase(m_current_slot);
        } else {
          m_set_slots.insert(m_current_slot);
        }
        break;
      case ABS_MT_SLOT:
        m_current_slot = ev.value;
        break;
      case ABS_MT_POSITION_X:
        m_slot_coordinates[m_current_slot].x = ev.value;
        break;
      case ABS_MT_POSITION_Y:
        m_slot_coordinates[m_current_slot].y = ev.value;
        break;
      }
    }

    // determine which slots are active
    for (const auto &s : m_set_slots) {
      if (insideValidArea(m_slot_coordinates[s].x, m_slot_coordinates[s].y)) {
        m_slot_valid[s] = true;
      }
    }

    // proximity check for multitouch gestures
    for (const auto &s1 : m_set_slots) {
      if (m_slot_valid[s1]) {
        continue;
      }
      for (const auto &s2 : m_set_slots) {
        if (s1 == s2) {
          continue;
        }
        auto delta_x = m_slot_coordinates[s1].x - m_slot_coordinates[s2].x;
        auto delta_y = m_slot_coordinates[s1].y - m_slot_coordinates[s2].y;
        auto dist_sq = delta_x * delta_x + delta_y * delta_y;

        // sqrt(dist_sq) < sqrt(m_diagonal_sq)/4
        if (dist_sq < (m_diagonal_sq / 16)) {
          m_slot_valid[s1] = true;
          m_slot_valid[s2] = true;
        }
      }
    }

    // modify the events based on active or not
    for (auto &ev : event_buffer) {
      switch (ev.code) {
      case ABS_MT_SLOT:
        slot = ev.value;
        break;
      case ABS_MT_TOUCH_MAJOR:
      case ABS_MT_TOUCH_MINOR:
      case ABS_MT_WIDTH_MAJOR:
      case ABS_MT_WIDTH_MINOR:
      case ABS_MT_PRESSURE:
        if (false == m_slot_valid[slot]) {
          ev.value = 0;
        }
      }
    }
  }
};
