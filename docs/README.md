# Wokwi HDC2010 Chip

Simulates a HDC2010 temperature/humdity sensor chip.

Currently supports: 
* Reading temperature and humidity values in manual measurement mode
* Manufacturer ID and Device ID registers
* DRDY signal in high active mode

Not supported
* Automatic measurement mode
* Interrupts
- Threshold and Offset registers


## Pin names

| Name | Description              |
| ---- | ------------------------ |
| VCC  | Supply voltage  - doesn't need to be connected |
| ADDR   | Address Input - leave open for 0 |
| GND  | Ground - doesn't need to be connected |
| SDA  | I2C SDA |
| SCL  | I2C SCL |
| DRDY | Data Ready - goes high when data is ready to be read |

## Usage

To use this chip in your project, include it as a dependency in your `diagram.json` file:

```json
  "dependencies": {
    "chip-hdc2010": "github:ci4rail/wokwi-hdc2010-chip@1.0.0"
  }
```

Then, add the chip to your circuit by adding a `chip-hdc2010` item to the `parts` section of diagram.json:

```json
  "parts": {
    ...,
    { "type": "chip-hdc2010", "id": "chip1" }
  },
```


