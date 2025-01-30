# Wokwi AMC131M0X Chip

Simulates a AMC131M0X ADC chip in Wokwi.

Currently supports: 
* 24 bit word size
* ID register
* CRC check and generation (always on)
* Reset via Pin
* Simulation of DCDC enable
* One or two channels (via attrs or control)

Not supported
* Writes to configuration registers are supported, but have no effect, except DCDC enable
* DRDY pin
* Multi-register read/write
- Threshold and Offset registers


## Pin names

| Name | Description              |
| ---- | ------------------------ |
| SCK | SPI Clock             |
| MISO | SPI Master In Slave Out |
| MOSI | SPI Master Out Slave In |
| CS | SPI Chip Select |
| GND | Ground (not used)|
| VCC | Power (not used)|
| RST | Reset pin (has internal pullup, may be left open)|

## Usage

To use this chip in your project, include it as a dependency in your `diagram.json` file:

```json
  "dependencies": {
    "chip-hdc2010": "github:ci4rail/wokwi-amc131m0x-chip@1.0.0"
  }
```

Then, add the chip to your circuit by adding a `chip-amc131m0x` item to the `parts` section of diagram.json:

```json
  "parts": {
    ...,
    { "type": "chip-amc131m0x", "id": "chip1", "attrs": { "channels": 1 } }
  },
```


