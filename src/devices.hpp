/*
 *
 * This file is part of trackpad-is-too-damn-big utility
 * Copyright (c) https://github.com/tascvh/trackpad-is-too-damn-big
 *
 */
#include "libevdev/libevdev-uinput.h"
#include "libevdev/libevdev.h"
#include <cerrno>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <format>
#include <iostream>
#include <unistd.h>

extern "C" void print_evdev(struct libevdev *dev);

class Evdev final {
  int m_fd = 0;
  struct libevdev *m_dev = NULL;

public:
  Evdev() = delete;
  Evdev(const Evdev &) = delete;
  Evdev &operator=(const Evdev &) = delete;
  Evdev &operator=(Evdev &&) = delete;

  Evdev(Evdev &&other) noexcept : m_fd(other.m_fd), m_dev(other.m_dev) {
    other.m_dev = NULL;
    other.m_fd = 0;
  }

  Evdev(const std::string &device) {
    m_fd = open(device.c_str(), O_RDONLY);
    if (m_fd < 0) {
      throw std::runtime_error(std::format("Failed to open device ({}) : {}",
                                           device, std::strerror(errno)));
    }

    int rc = libevdev_new_from_fd(m_fd, &m_dev);
    if (rc < 0) {
      close(m_fd);
      throw std::runtime_error("Failed to init libevdev \n");
    }
  }

  ~Evdev() {
    if (m_dev)
      libevdev_free(m_dev);
    if (m_fd > 0)
      close(m_fd);
  }

  template <typename T, typename... Args> T Spawn(Args... args) {
    return T(m_dev, args...);
  }

  void grab(bool grab) {
    int rc = libevdev_grab(m_dev, grab ? LIBEVDEV_GRAB : LIBEVDEV_UNGRAB);
    if (0 != rc) {
      std::string s = std::format("Failed to {}grab device", grab ? "" : "un");
      throw std::runtime_error(s);
    }
  }

  void print() { print_evdev(m_dev); }

  template <typename EventHandler> auto runEventLoop(EventHandler handler) {

    if (handler.grab())
      grab(true);

    int rc = 0;
    do {
      input_event ev;
      rc = libevdev_next_event(
          m_dev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING, &ev);
      if (rc == LIBEVDEV_READ_STATUS_SYNC) {
        while (rc == LIBEVDEV_READ_STATUS_SYNC) {
          handler.eventSync(ev);
          rc = libevdev_next_event(m_dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
        }
      } else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
        if (ev.type == EV_SYN) {
          handler.eventReport(ev);
        } else {
          handler.eventData(ev);
        }
      }
    } while (rc == LIBEVDEV_READ_STATUS_SYNC ||
             rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == -EAGAIN);

    if (rc != LIBEVDEV_READ_STATUS_SUCCESS && rc != -EAGAIN) {
      std::cerr << "Failed to handle events: " << strerror(-rc) << std::endl;
    }

    if (handler.grab())
      grab(false);

    return rc;
  }
};

class UInput final {
  int m_fd = 0;
  libevdev_uinput *m_uinput = NULL;

public:
  UInput() = delete;
  UInput(const UInput &) = delete;
  UInput &operator=(const UInput &) = delete;
  UInput &operator=(UInput &&) = delete;

  UInput(UInput &&other) noexcept : m_fd(other.m_fd), m_uinput(other.m_uinput) {
    other.m_fd = 0;
    other.m_uinput = NULL;
  }

  UInput(libevdev const *const dev) {
    m_fd = open("/dev/uinput", O_RDWR);
    if (m_fd < 0) {
      throw std::runtime_error(
          std::format("Failed to open /dev/uinput {}", std::strerror(errno)));
    }
    int rc = libevdev_uinput_create_from_device(dev, m_fd, &m_uinput);
    if (rc != 0) {
      close(m_fd);
      throw std::runtime_error("Failed to create uinput");
    }
    // wait for the device to register
    sleep(1);
  }

  ~UInput() {
    if (m_uinput)
      libevdev_uinput_destroy(m_uinput);
    if (m_fd > 0)
      close(m_fd);
  }

  int writeEvent(const input_event &ev) {
    int rc = libevdev_uinput_write_event(m_uinput, ev.type, ev.code, ev.value);
    if (rc != 0)
      std::cerr << "Failed to write event";
    return rc;
  }
};
