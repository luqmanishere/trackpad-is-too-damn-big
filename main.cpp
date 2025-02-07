/*
 *
 * This file is part of trackpad-is-too-damn-big utility
 * Copyright (c) https://github.com/tascvh/trackpad-is-too-damn-big
 *
*/
#include <iostream>
#include <sstream>
#include "Devices.hpp"
#include "EventHandlers.hpp"
#include "EventFilters.hpp"
#include "cmdparser.hpp"

int main(int argc, char **argv)
{
	try {
		std::stringstream output { };
		std::stringstream errors { };

		cli::Parser parser(argc, argv);

		parser.set_required<std::string>("d", "device", "Device filename");
		parser.set_optional<std::string>("m", "mode", "f" ,  "Running mode. Accepted values : f (Flex) , s (Strict) , p (Only Print Events) ");
		parser.set_optional<int>("l", "left", 10,  "Crop percentage from left side");
		parser.set_optional<int>("r", "right", 10,  "Crop percentage from right side");
		parser.set_optional<int>("t", "top", 0, "Crop percentage from top");
		parser.set_optional<int>("b", "bottom", 15,  "Crop percentage from bottom");

		parser.run_and_exit_if_error();

		const auto device = parser.get<std::string>("d");
		const auto mode = parser.get<std::string>("m") ;

		const auto left = parser.get<int>("l") ;
		const auto right = parser.get<int>("r") ;
		const auto top = parser.get<int>("t") ;
		const auto bottom = parser.get<int>("b") ;

		std::cout << "device : " << device << std::endl;
		std::cout << "mode : " << mode << std::endl;
		std::cout << "left side crop percentage : " << left << std::endl;
		std::cout << "right side crop percentage : " << right << std::endl;
		std::cout << "top crop percentage : " << top << std::endl;
		std::cout << "bottom crop percentage : " << bottom << std::endl;
		std::cout << "bottom crop percentage : " << bottom << std::endl;

		auto evdev = Device_Evdev(device);

		if(mode == "p") {
			// print mode
			// Only print incoming events to the device
			evdev.Print();
			return evdev.RunEventLoop(EventHandler_PrintEvents());
		}
		else if(mode == "s") {
			// strict mode
			// strictly disable cropped out areas
			auto event_filter = evdev.Spawn<EventFilter_CropRect>(left,right,top,bottom);
			auto virtual_device = evdev.Spawn<Device_UInput>();
			return evdev.RunEventLoop(EventHandler_ForwardTo(std::move(virtual_device), std::move(event_filter)));
		}
		else if(mode == "f") {
			// flex mode
			// disable lone clicks but permit re-entry to and gestures in cropped out areas
			auto event_filter = evdev.Spawn<EventFilter_CropRectFlex>(left,right,top,bottom);
			auto virtual_device = evdev.Spawn<Device_UInput>();
			return evdev.RunEventLoop(EventHandler_ForwardTo(std::move(virtual_device), std::move(event_filter)));
		}
		else {
			std::cerr << "unknown mode" << std::endl;
		}

	}catch (const std::runtime_error& e) {
		std::cerr << "runtime error: " << e.what() << std::endl;
	} catch (...) {
		std::cerr << "Caught unknown exception" << std::endl;
	}
}

