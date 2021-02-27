# BLEinky
An arduino blinky program that's controllable with BLE.

## Hardware
- Adafruit Feather nRF52 Bluefruit LE
- Adafruit NeoPixel Ring - 12 x 5050 RGB LED

## Usage
Connect and send commands using the Adafruit Bluefruit LE app.

Put the BLEinky outside your office and use the light color to signal your availability.

### Commands
- Busy: `busy` | `b` | `red` - Send a 'busy' command to turn the lights red.
- Free: `free` | `f` | `green`- Send a 'free' command to turn the lights green.
- Wave: `wave` | `meow` | `cycle` - Send a 'wave' command to cycle a rainbow of colors to attract attention.

## Notes
This was just a weekend project and is full of bad code that was fun to hack together.
