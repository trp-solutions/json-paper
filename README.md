# JSON Paper

See the [GitHub wiki](https://github.com/TRP-Solutions/json-paper/wiki) for project documentation.

## Hardware

| Brand | Model |
|---|---|
| Espressif | ESP32-C5-DevKitC-1 v1.2 with 8 MB PSRAM |
| Waveshare | 5.79inch e-Paper Module (G), SKU 27846 |

## Wiring

| E-paper connection | ESP32-C5 connection |
|---|--------------------:|
| PWR |               GPIO5 |
| BUSY |              GPIO9 |
| RST |              GPIO8 |
| DC |               GPIO4 |
| CS |              GPIO10 |
| CLK |               GPIO6 |
| DIN |               GPIO7 |
| GND |                 GND |
| VCC (Pin 1) |                  5V |


| Accessory connection | ESP32-C5 connection |
|----------------------|--------------------:|
| Setup button         |               GPIO0 |
| Setup button GND     |                 GND |

## Pie charts

Use one `draw_pie_slice` command for each colored section. Every slice accepts:

| Argument | Meaning |
|---|---|
| `x`, `y` | Center of the chart |
| `radius` | Slice radius in pixels |
| `start_angle` | Starting angle in degrees |
| `sweep_angle` | Clockwise size from 1° through 360° |
| `color` | `white`, `black`, `red`, or `yellow` |

Zero degrees points right. Angles increase clockwise: 90° points down, 180°
points left, and 270° points up. Starting angles outside 0–359° are normalized.
Later commands paint over earlier commands when slices overlap.

This example draws a radius-80 chart containing 25% red, 35% yellow, and 40%
black:

```json
{
  "version": "1.0",
  "commands": [
    {
      "cmd": "draw_pie_slice",
      "args": {
        "x": 200,
        "y": 135,
        "radius": 80,
        "start_angle": 270,
        "sweep_angle": 90,
        "color": "red"
      }
    },
    {
      "cmd": "draw_pie_slice",
      "args": {
        "x": 200,
        "y": 135,
        "radius": 80,
        "start_angle": 0,
        "sweep_angle": 126,
        "color": "yellow"
      }
    },
    {
      "cmd": "draw_pie_slice",
      "args": {
        "x": 200,
        "y": 135,
        "radius": 80,
        "start_angle": 126,
        "sweep_angle": 144,
        "color": "black"
      }
    }
  ]
}
```

The PHP helper provides the equivalent method:

```php
$display
    ->pieSlice(200, 135, 80, 270, 90, Color::RED)
    ->pieSlice(200, 135, 80, 0, 126, Color::YELLOW)
    ->pieSlice(200, 135, 80, 126, 144, Color::BLACK);
```
