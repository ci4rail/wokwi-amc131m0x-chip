#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  pin_t    cs_pin;
  uint32_t spi;
  uint8_t  spi_buffer[9];
  uint16_t regs[0x40];
} chip_state_t;

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value);
static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count);

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  
  chip->cs_pin = pin_init("CS", INPUT_PULLUP);

  const pin_watch_config_t watch_config = {
    .edge = BOTH,
    .pin_change = chip_pin_change,
    .user_data = chip,
  };
  pin_watch(chip->cs_pin, &watch_config);

  const spi_config_t spi_config = {
    .sck = pin_init("SCK", INPUT),
    .miso = pin_init("MISO", INPUT),
    .mosi = pin_init("MOSI", INPUT),
    .done = chip_spi_done,
    .user_data = chip,
  };
  chip->spi = spi_init(&spi_config);
  
  printf("SPI Chip initialized!\n");

}


void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t*)user_data;
  // Handle CS pin logic
  if (pin == chip->cs_pin) {
    if (value == LOW) {
      printf("SPI chip selected\n");
      spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));

    } else {
      printf("SPI chip deselected\n");
      spi_stop(chip->spi);
    }
  }
}

void set_response(chip_state_t *chip, uint16_t res)
{
  chip->spi_buffer[0] = (res >> 8) & 0xFF;
  chip->spi_buffer[1] = res & 0xFF;
}

void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (!count) {
    // This means that we got here from spi_stop, and no data was received
    return;
  }
  uint16_t cmd = (buffer[0] << 8) | buffer[1];
  printf("Command: 0x%04X\n", cmd);

  if( cmd == 0x0000){
    // null command, response is status reg
    set_response(chip, chip->regs[0x01]);
  } else if ((cmd & 0xe000) == 0x6000){
    // read register
    uint16_t reg = cmd & 0x3f;
    set_response(chip, chip->regs[reg]);  
  } else if ((cmd & 0xe000) == 0x3000){
    // write register
    uint16_t reg = cmd & 0x3f;
    chip->regs[reg] = (buffer[2] << 8) | buffer[3];
    set_response(chip, 0x2000 | (cmd & 0x1FFF));
  } else {
    printf("Unknown command: 0x%04X\n", cmd);
    // unknown command
    set_response(chip, 0x0000);
  }
  // set measurements in response


  if (pin_read(chip->cs_pin) == LOW) {
    // Continue with the next character
    spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
  }
}
