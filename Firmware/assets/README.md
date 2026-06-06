Place your project logo here as:

- TIME_logo.png

Then run:

- python Firmware/tools/png_to_lvgl_image.py --input Firmware/assets/TIME_logo.png --name time_logo --max-width 420 --max-height 120

This will regenerate:

- Firmware/src/assets/time_logo.h
- Firmware/src/assets/time_logo.c

The home screen already displays `time_logo`.
