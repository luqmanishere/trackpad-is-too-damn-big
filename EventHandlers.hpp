/*
 *
 * This file is part of trackpad-is-too-damn-big utility
 * Copyright (c) https://github.com/tascvh/trackpad-is-too-damn-big
 *
*/
#include "libevdev/libevdev.h"
#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include <exception>
#include <format>

static int print_event(const struct input_event * const ev)
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

class EventHandler_PrintEvents {
	public:
	bool Grab() {
		return false;
	}
	void EventReport(const struct input_event& ev) {
		print_event(&ev);
	}
	void EventData(const struct input_event& ev) {
		print_event(&ev);
	}
	void EventSync(const struct input_event& ev) {
		print_event(&ev);
	}
};


template<typename Destination, typename Filter>
	class EventHandler_ForwardTo {
		std::vector<struct input_event> _event_buffer;
		Destination _dest;
		Filter _filter;
		public:
		bool Grab() {
			return true;
		}

		EventHandler_ForwardTo(Destination dest, Filter filter) :
			_dest(std::move(dest)) , _filter(std::move(filter)) {
			_event_buffer.reserve(50);
		}

		void EventSync(const struct input_event& ev) {
			(void)(ev); // silence unused parameter warning
		}

		constexpr void EventReport(const struct input_event& ev) {
			_event_buffer.push_back(ev);
			_filter.ProcessEvents(_event_buffer);
			for(auto& ev : _event_buffer) {
				_dest.WriteEvent(ev);
			}
			_event_buffer.clear();
		}

		constexpr void EventData(const struct input_event& ev) {
			_event_buffer.push_back(ev);
		}
	};


