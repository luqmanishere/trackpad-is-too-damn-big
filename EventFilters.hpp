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
#include <unordered_set>

class EventFilter_PassAll  {
	public:
	constexpr void ProcessEvents(std::vector<struct input_event>& event_buffer) {
		(void)(event_buffer); // silence unused parameter warning
	}
};

class EventFilter_CropRect {
	protected:
		// original device area
		int _dev_left;
		int _dev_right;
		int _dev_top;
		int _dev_bottom;

		int _num_slot_max;

		// current valid area
		int _left;
		int _right;
		int _top;
		int _bottom;

		int _current_slot = 0;
		struct Point { int x = 0; int y = 0; };
		std::vector<Point> _slot_coordinates ;

	public:
		EventFilter_CropRect(libevdev *dev, int perc_left, int perc_right, int perc_top, int perc_bottom) {

			if (!libevdev_has_event_code(dev, EV_ABS, ABS_X) ||
					!libevdev_has_event_code(dev, EV_ABS, ABS_Y) ||
					!libevdev_has_event_code(dev, EV_ABS, ABS_MT_SLOT)
					)  {
				throw std::runtime_error("Device does not appear to be a Trackpad");
			}

			const struct input_absinfo *ai;

			if ((ai = libevdev_get_abs_info(dev, ABS_MT_SLOT)) == NULL) {
				throw std::runtime_error("Failed to get slot info");
			}
			_num_slot_max = ai->maximum;
			_slot_coordinates.reserve(_num_slot_max);

			if ((ai = libevdev_get_abs_info(dev, ABS_X)) == NULL) {
				throw std::runtime_error("Failed to get abs x info");
			}
			_dev_left = ai->minimum;
			_dev_right = ai->maximum;

			if ((ai = libevdev_get_abs_info(dev, ABS_Y)) == NULL) {
				throw std::runtime_error("Failed to get abs y info");
			}
			_dev_top = ai->maximum;
			_dev_bottom = ai->minimum;

			_left = _dev_left;
			_right = _dev_right;
			// y axis is flipped
			_top = -_dev_bottom;
			_bottom = -_dev_top;

			size_t range_x = _right - _left;
			size_t range_y = _top - _bottom;

			_left += perc_left * range_x / 100;
			_right -= perc_right * range_x / 100;
			_bottom += perc_bottom * range_y / 100;
			_top -= perc_top * range_y / 100;
		}

		constexpr bool IsInValidArea(int x, int y) noexcept {
			y = -y; // y axis is flipped
			if (x >= _left && x <= _right && y >= _bottom && y <= _top) {
				return true;
			}
			return false;
		}

		constexpr void ProcessEvents(std::vector<struct input_event>& event_buffer) noexcept {

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
						if (false == IsInValidArea(_slot_coordinates[slot].x,_slot_coordinates[slot].y)) {
							ev.value = 0;
						}
						break;
				}
			}
		}
};

class EventFilter_CropRectFlex : EventFilter_CropRect {

	std::unordered_set<int> _slots_active;
	std::vector<bool> _slot_valid;

	int _diagonal_sq;

	public:
	EventFilter_CropRectFlex(libevdev *dev, int perc_left, int perc_right, int perc_top, int perc_bottom)
		: EventFilter_CropRect(dev, perc_left, perc_right, perc_top, perc_bottom) {
				auto delta_x = _dev_right-_dev_left;
				auto delta_y = _dev_top-_dev_bottom;
				_diagonal_sq = delta_x*delta_x + delta_y*delta_y;
				_slots_active.reserve(_num_slot_max);
				_slot_valid.reserve(_num_slot_max);
	}

	void ProcessEvents(std::vector<struct input_event>& event_buffer) noexcept {

		// we will save the last reported slot
		// before processing the events
	  int slot = _current_slot;

		// gather latest state from the events
		for(const auto& ev : event_buffer) {
			switch(ev.code) {
				case ABS_MT_TRACKING_ID :
					_slot_valid[_current_slot] = false;
					if(ev.value == -1) {
						_slots_active.erase(_current_slot);
					}
					else {
						_slots_active.insert(_current_slot);
					}
					break;
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

		// determine which slots are active
		for(const auto& s: _slots_active) {
			if (IsInValidArea(_slot_coordinates[s].x,_slot_coordinates[s].y)) {
				_slot_valid[s] = true;
			}
		}

		// proximity check for multitouch gestures
		for(const auto& s1:_slots_active) {
			if (_slot_valid[s1]) {
				continue;
			}
			for(const auto& s2:_slots_active) {
				if (s1 == s2) {
					continue;
				}
				auto delta_x = _slot_coordinates[s1].x - _slot_coordinates[s2].x;
				auto delta_y = _slot_coordinates[s1].y - _slot_coordinates[s2].y;
				auto dist_sq = delta_x*delta_x + delta_y*delta_y;

				// sqrt(dist_sq) < sqrt(_diagonal_sq)/4;
				if (dist_sq < (_diagonal_sq/16)) {
					_slot_valid[s1] = true;
					_slot_valid[s2] = true;
				}
			}
		}

		// modify the events based on active or not
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
					if(false == _slot_valid[slot]) {
						ev.value = 0;
					}
			}
		}
	}
};

