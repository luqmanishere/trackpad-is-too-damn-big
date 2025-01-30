#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "libevdev/libevdev.h"
#include "libevdev/libevdev-uinput.h"

#include <string>
#include <iostream>
#include <vector>
#include <utility> 
#include <exception> 
#include <format> 

static int print_event(struct input_event *ev);
static void print_bits(struct libevdev *dev);
static void print_props(struct libevdev *dev);

class Evdev {
	int _fd=0;
	struct libevdev* _dev=NULL;

	public:

	Evdev(const Evdev&) = delete;
	Evdev& operator=(const Evdev&) = delete;
	Evdev(Evdev&&) = delete ; 
	Evdev& operator=(Evdev&&) = delete;

	void Grab(bool grab) {
		int rc = libevdev_grab(_dev, grab?LIBEVDEV_GRAB:LIBEVDEV_UNGRAB);
		if (0 != rc) {
				throw std::runtime_error(std::format("Failed to %s device", grab?"grab":"ungrab"));
		}
	}

	Evdev(std::string device) {
		_fd = open(device.c_str(), O_RDONLY);
		if (_fd < 0) {
			throw std::runtime_error(std::format("Failed to open device (%s)", device));
		}

		int rc = libevdev_new_from_fd(_fd, &_dev);
		if (rc < 0) {
			if (_fd > 0) {
				close(_fd);
			}
			throw std::runtime_error(std::format("Failed to init libevdev (%s)\n", strerror(-rc)));
		}
	}

	~Evdev() {
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
	auto RunEventLoop(EventHandler& handler) {
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
			std::cerr << "Failed to handle events: %s" << strerror(-rc) << std::endl;;
		}

		if(handler.Grab()) {
			Grab(false);
		}
		return rc;
	}

};


class UInput {
	int _fd = 0;
	struct libevdev_uinput *_uinput = NULL;
	public:

	UInput(const UInput&) = delete;
	UInput& operator=(const UInput&) = delete;
	UInput(UInput&&) = delete ; 
	UInput& operator=(UInput&&) = delete;

	UInput(libevdev *dev) {
		_fd = open("/dev/uinput", O_RDWR);
		if (_fd < 0) {
			throw std::runtime_error("Failed to open /dev/uinput");
		}
		int rc = libevdev_uinput_create_from_device(dev, _fd, &_uinput);
		if (rc != 0) {
			close(_fd);
			throw std::runtime_error("Failed to create uinput");
		}
		// wait for the device to register
		sleep(1);
	}

	~UInput() {
		if(_uinput)
			libevdev_uinput_destroy(_uinput);
		if(_fd > 0)
			close(_fd);
	}

	int WriteEvent(input_event& ev) {
		int rc = libevdev_uinput_write_event(_uinput, ev.type, ev.code, ev.value);
		if (rc != 0) {
			std::cerr << "Failed to write event";
		}
		return rc;
	}
};

class EventHandler_PrintEvents {
	public:
	bool Grab() {
		return false;
	}
	void EventReport(struct input_event& ev) {
		print_event(&ev);
	}
	void EventData(struct input_event& ev) {
		print_event(&ev);
	}
	void EventSync(struct input_event& ev) {
		print_event(&ev);
	}
};


template<typename Destination, typename Filter>
class EventHandler_ForwardTo {
	std::vector<struct input_event> _event_buffer;
	Destination& _dest;
	Filter& _filter;
	public:
	bool Grab() {
		return true;
	}

	EventHandler_ForwardTo(Destination& dest, Filter& filter) : _dest(dest) , _filter(filter){
		_event_buffer.reserve(50);
	}

	void EventSync(struct input_event& ev) {
	}

	constexpr void EventReport(struct input_event& ev) {
		_event_buffer.push_back(ev);
		_filter.ProcessEvents(_event_buffer);
		for(auto& ev : _event_buffer) {
			_dest.WriteEvent(ev);
		}
		_event_buffer.clear();
	}

	constexpr void EventData(struct input_event& ev) {
		_event_buffer.push_back(ev);
	}
};

class EventFilter_PassAll  {
	public:
	constexpr void ProcessEvents(std::vector<struct input_event>& event_buffer) {
	}
};

class EventFilter_CropRect {
	// defines current valid area
	int _left;
	int _right;
	int _top;
	int _bottom;

	int _current_slot = 0;
	struct Point { int x; int y; };
	std::array<Point, 15> _slot_coordinates = {0};

	constexpr bool inRect(int x,int y) {
		return false;
	}

	public:
	EventFilter_CropRect(libevdev *dev, int perc_left, int perc_right, int perc_top, int perc_bottom) {
			const struct input_absinfo *ai;
			if ((ai = libevdev_get_abs_info(dev, ABS_X)) == NULL) {
				throw std::runtime_error("Failed to get abs info");
			}
			_left = ai->minimum;
			_right = ai->maximum;
			if ((ai = libevdev_get_abs_info(dev, ABS_Y)) == NULL) {
				throw std::runtime_error("Failed to get abs info");
			}
			// y axis is flipped
			_top = -ai->minimum;
			_bottom = -ai->maximum;

			size_t range_x = _right - _left;
			size_t range_y = _top - _bottom;

			_left += perc_left * range_x / 100;
			_right -= perc_right * range_x / 100;
			_bottom += perc_bottom * range_y / 100;
			_top -= perc_top * range_y / 100;
	}

	constexpr void ProcessEvents(std::vector<struct input_event>& event_buffer) {

	  int slot = _current_slot;

		for(auto& ev : event_buffer) {
			switch(ev.code) {
				case ABS_MT_SLOT: 
					_current_slot = ev.value;
					break;
				case ABS_MT_POSITION_X:
					_slot_coordinates[_current_slot].x = ev.value;
					break;
				case ABS_MT_POSITION_Y:
					_slot_coordinates[_current_slot].y = ev.value;
					break;
			}
		}

		for(auto& ev : event_buffer) {
			switch(ev.code) {
				case ABS_MT_SLOT: 
					slot = ev.value;
						break;
				case ABS_MT_TOUCH_MAJOR:
				case ABS_MT_TOUCH_MINOR:
				case ABS_MT_WIDTH_MAJOR:
				case ABS_MT_WIDTH_MINOR:
				case ABS_MT_PRESSURE:
						auto x =_slot_coordinates[slot].x;
						auto y =_slot_coordinates[slot].y;
						y = -y; // y axis is flipped
						if (false == (x >= _left && x <= _right && y >= _bottom && y <= _top)) {
							ev.value = 0;
						}
						break;
			}
		}
	}
};


int main(int argc, char **argv)
{
	try {
		std::string device("/dev/input/event0");
		// crop percentages
		int left = 10;
		int right = 10;
		int top = 0;
		int bottom = 15;

		Evdev evdev(device);

		// prints evdev details
		// evdev.Print();

		// // Only print incoming events
		// EventHandler_PrintEvents printHandler;
		// return evdev.RunEventLoop(printHandler);
		
		// Create virtual device 
		// auto virtual_device = evdev.Spawn<UInput>();
		// EventFilter_PassAll no_filter;
		// auto forwardHandler = EventHandler_ForwardTo(virtual_device, no_filter);
		// return evdev.RunEventLoop(forwardHandler);
		
		auto virtual_device = evdev.Spawn<UInput>();
		auto event_filter = evdev.Spawn<EventFilter_CropRect>(left,right,top,bottom);
		auto event_handler = EventHandler_ForwardTo(virtual_device, event_filter);
		return evdev.RunEventLoop(event_handler);

	}catch (const std::runtime_error& e) {
		std::cerr << "runtime error: " << e.what() << std::endl;
	} catch (...) {
		std::cerr << "Caught unknown exception" << std::endl;
	}
}

static int print_event(struct input_event *ev)
{
	if (ev->type == EV_SYN)
		printf("Event: time %ld.%06ld, ++++++++++++++++++++ %s +++++++++++++++\n",
				ev->input_event_sec,
				ev->input_event_usec,
				libevdev_event_type_get_name(ev->type));
	else
		printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %d\n",
			ev->input_event_sec,
			ev->input_event_usec,
			ev->type,
			libevdev_event_type_get_name(ev->type),
			ev->code,
			libevdev_event_code_get_name(ev->type, ev->code),
			ev->value);
	return 0;
}

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
