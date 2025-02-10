#include "libevdev/libevdev-uinput.h"
#include "libevdev/libevdev.h"
#include <stdio.h>

int print_event(const struct input_event *const ev) {
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

void print_abs_bits(struct libevdev *dev, int axis) {
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

void print_code_bits(struct libevdev *dev, unsigned int type,
                     unsigned int max) {
  unsigned int i;
  for (i = 0; i <= max; i++) {
    if (!libevdev_has_event_code(dev, type, i))
      continue;

    printf("    Event code %i (%s)\n", i,
           libevdev_event_code_get_name(type, i));
    if (type == EV_ABS)
      print_abs_bits(dev, i);
  }
}

void print_bits(struct libevdev *dev) {
  unsigned int i;
  printf("Supported events:\n");

  for (i = 0; i <= EV_MAX; i++) {
    if (libevdev_has_event_type(dev, i))
      printf("  Event type %d (%s)\n", i, libevdev_event_type_get_name(i));
    switch (i) {
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

void print_props(struct libevdev *dev) {
  unsigned int i;
  printf("Properties:\n");

  for (i = 0; i <= INPUT_PROP_MAX; i++) {
    if (libevdev_has_property(dev, i))
      printf("  Property type %d (%s)\n", i, libevdev_property_get_name(i));
  }
}

void print_evdev(struct libevdev *dev) {
  printf("Input device ID: bus %#x vendor %#x product %#x\n",
         libevdev_get_id_bustype(dev), libevdev_get_id_vendor(dev),
         libevdev_get_id_product(dev));
  printf("Evdev version: %x\n", libevdev_get_driver_version(dev));
  printf("Input device name: \"%s\"\n", libevdev_get_name(dev));
  printf("Phys location: %s\n", libevdev_get_phys(dev));
  printf("Uniq identifier: %s\n", libevdev_get_uniq(dev));
  print_bits(dev);
  print_props(dev);
}
