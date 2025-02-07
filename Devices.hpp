/*
 *
 * This file is part of trackpad-is-too-damn-big utility
 * Copyright (c) https://github.com/tascvh/trackpad-is-too-damn-big
 *
*/
#include "libevdev/libevdev.h"
#include "libevdev/libevdev-uinput.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <format>
#include <exception>
#include <cstring>
#include <cerrno>

static void print_bits(struct libevdev *dev);
static void print_props(struct libevdev *dev);

class Device_Evdev {
	int _fd=0;
	struct libevdev* _dev=NULL;

	public:

	Device_Evdev() = delete;
	Device_Evdev(const Device_Evdev&) = delete;
	Device_Evdev& operator=(const Device_Evdev&) = delete;
	Device_Evdev& operator=(Device_Evdev&&) = delete;

	Device_Evdev(Device_Evdev&& other) noexcept
		: _fd(other._fd), _dev(other._dev){
		other._dev = NULL;
		other._fd = 0;
	}

	Device_Evdev(const std::string& device) {
		_fd = open(device.c_str(), O_RDONLY);
		if (_fd < 0) {
			throw std::runtime_error(std::format("Failed to open device ({}) : {}", device,std::strerror(errno)));
		}

		int rc = libevdev_new_from_fd(_fd, &_dev);
		if (rc < 0) {
			close(_fd);
			throw std::runtime_error("Failed to init libevdev \n");
		}
	}

	~Device_Evdev() {
		if(_dev)
			libevdev_free(_dev);
		if (_fd > 0) {
			close(_fd);
		}
	}

	template <typename T, typename... Args>
		T Spawn(Args... args) {
			return T(_dev, args...);
		}

	void Grab(bool grab) {
		int rc = libevdev_grab(_dev, grab?LIBEVDEV_GRAB:LIBEVDEV_UNGRAB);
		if (0 != rc) {
				throw std::runtime_error(std::format("Failed to {} device", grab?"grab":"ungrab"));
		}
	}

	void Print() {
			printf("Input device ID: bus %#x vendor %#x product %#x\n",
					libevdev_get_id_bustype(_dev),
					libevdev_get_id_vendor(_dev),
					libevdev_get_id_product(_dev));
			printf("Evdev version: %x\n", libevdev_get_driver_version(_dev));
			printf("Input device name: \"%s\"\n", libevdev_get_name(_dev));
			printf("Phys location: %s\n", libevdev_get_phys(_dev));
			printf("Uniq identifier: %s\n", libevdev_get_uniq(_dev));
			print_bits(_dev);
			print_props(_dev);
	}

	template <typename EventHandler>
		auto RunEventLoop(EventHandler handler) {
			if(handler.Grab()) {
				Grab(true);
			}

			int rc=0;
			do {
				struct input_event ev;
				rc = libevdev_next_event(_dev, LIBEVDEV_READ_FLAG_NORMAL|LIBEVDEV_READ_FLAG_BLOCKING, &ev);
				if (rc == LIBEVDEV_READ_STATUS_SYNC) {
					while (rc == LIBEVDEV_READ_STATUS_SYNC) {
						handler.EventSync(ev);
						rc = libevdev_next_event(_dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
					}
				} else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
					if (ev.type == EV_SYN) {
						handler.EventReport(ev);
					}
					else {
						handler.EventData(ev);
					}
				}
			} while (rc == LIBEVDEV_READ_STATUS_SYNC || rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == -EAGAIN);

			if (rc != LIBEVDEV_READ_STATUS_SUCCESS && rc != -EAGAIN) {
				std::cerr << "Failed to handle events: " << strerror(-rc) << std::endl;;
			}

			if(handler.Grab()) {
				Grab(false);
			}
			return rc;
		}
};


class Device_UInput {
	int _fd = 0;
	struct libevdev_uinput *_uinput = NULL;
	public:

	Device_UInput() = delete;
	Device_UInput(const Device_UInput&) = delete;
	Device_UInput& operator=(const Device_UInput&) = delete;
	Device_UInput& operator=(Device_UInput&&) = delete;

	Device_UInput(Device_UInput&& other) noexcept
		: _fd(other._fd), _uinput(other._uinput){
		other._fd = 0;
		other._uinput = NULL;
	}

	Device_UInput(libevdev const * const dev) {
		_fd = open("/dev/uinput", O_RDWR);
		if (_fd < 0) {
			throw std::runtime_error(std::format("Failed to open /dev/uinput {}",  std::strerror(errno)));
		}
		int rc = libevdev_uinput_create_from_device(dev, _fd, &_uinput);
		if (rc != 0) {
			close(_fd);
			throw std::runtime_error("Failed to create uinput");
		}
		// wait for the device to register
		sleep(1);
	}

	~Device_UInput() {
		if(_uinput)
			libevdev_uinput_destroy(_uinput);
		if(_fd > 0)
			close(_fd);
	}

	int WriteEvent(const input_event& ev) {
		int rc = libevdev_uinput_write_event(_uinput, ev.type, ev.code, ev.value);
		if (rc != 0) {
			std::cerr << "Failed to write event";
		}
		return rc;
	}
};


static void print_abs_bits(struct libevdev *dev, int axis)
{
	const struct input_absinfo *abs;

	if (!libevdev_has_event_code(dev, EV_ABS, axis))
		return;

	abs = libevdev_get_abs_info(dev, axis);

	printf("	Value	%6d\n", abs->value);
	printf("	Min	%6d\n", abs->minimum);
	printf("	Max	%6d\n", abs->maximum);
	if (abs->fuzz)
		printf("	Fuzz	%6d\n", abs->fuzz);
	if (abs->flat)
		printf("	Flat	%6d\n", abs->flat);
	if (abs->resolution)
		printf("	Resolution	%6d\n", abs->resolution);
}

static void print_code_bits(struct libevdev *dev, unsigned int type, unsigned int max)
{
	unsigned int i;
	for (i = 0; i <= max; i++) {
		if (!libevdev_has_event_code(dev, type, i))
			continue;

		printf("    Event code %i (%s)\n", i, libevdev_event_code_get_name(type, i));
		if (type == EV_ABS)
			print_abs_bits(dev, i);
	}
}

static void print_bits(struct libevdev *dev)
{
	unsigned int i;
	printf("Supported events:\n");

	for (i = 0; i <= EV_MAX; i++) {
		if (libevdev_has_event_type(dev, i))
			printf("  Event type %d (%s)\n", i, libevdev_event_type_get_name(i));
		switch(i) {
			case EV_KEY:
				print_code_bits(dev, EV_KEY, KEY_MAX);
				break;
			case EV_REL:
				print_code_bits(dev, EV_REL, REL_MAX);
				break;
			case EV_ABS:
				print_code_bits(dev, EV_ABS, ABS_MAX);
				break;
			case EV_LED:
				print_code_bits(dev, EV_LED, LED_MAX);
				break;
		}
	}
}

static void print_props(struct libevdev *dev)
{
	unsigned int i;
	printf("Properties:\n");

	for (i = 0; i <= INPUT_PROP_MAX; i++) {
		if (libevdev_has_property(dev, i))
			printf("  Property type %d (%s)\n", i,
					libevdev_property_get_name(i));
	}
}
