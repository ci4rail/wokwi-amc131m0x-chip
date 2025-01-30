#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NUM_CH 2
#define WORD_SIZE 3 // support only 24 bits per word

#define REG_ID 0x00
#define REG_STATUS 0x01
#define REG_DCDC_CTRL 0x31

#define STATUS_SEC_FAIL 0x0040

typedef struct
{
  int num_ch; // number of channels in the ADC
  pin_t cs_pin;
  pin_t rst_pin;
  uint32_t spi;
  uint8_t spi_buffer[(2 + MAX_NUM_CH) * WORD_SIZE];
  size_t spi_buffer_len;
  uint16_t regs[0x40];
  timer_t dcdc_timer;
  uint32_t ana_attr[MAX_NUM_CH];
} chip_state_t;

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value);
static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count);
static uint16_t calc_crc(uint8_t *data, size_t len);
static void rst_pin_change(void *user_data, pin_t pin, uint32_t value);

static void dump_buffer(uint8_t *buffer, size_t size)
{
  for (size_t i = 0; i < size; i++)
  {
    printf("%02X ", buffer[i]);
  }
  printf("\n");
}

static void reset(chip_state_t *chip)
{
  memset(chip->regs, 0, sizeof(chip->regs));
  chip->regs[REG_ID] = chip->num_ch;
  chip->regs[REG_STATUS] = STATUS_SEC_FAIL;
}

static void write_reg(chip_state_t *chip, uint16_t reg, uint16_t value)
{
  switch (reg)
  {
  case 0x02:
  case 0x03:
  case 0x04:
  case 0x06:
  case 0x09:
  case 0x0a:
  case 0x0b:
  case 0x0c:
  case 0x0d:
  case 0x0e:
  case 0x0f:
  case 0x10:
  case 0x11:
  case 0x12:
    break;

  case REG_DCDC_CTRL:
    printf("DCDC_CTRL: 0x%04X %04X\n", value, chip->regs[REG_DCDC_CTRL]);
    if ((value & 0x01) && !(chip->regs[REG_DCDC_CTRL] & 0x1))
    {
      printf("starting to enable DCDC\n");
      timer_start(chip->dcdc_timer, 200 * 1000, false); // 200ms
    }
    break;
  default:
    printf("Write to unsupported register: 0x%02X\n", reg);
    return;
  }

  chip->regs[reg] = value;
}

static void on_dcdc_timer(void *user_data)
{
  chip_state_t *chip = user_data;
  printf("Enabled DCDC\n");
  chip->regs[REG_STATUS] &= ~STATUS_SEC_FAIL;
}

void chip_init(void)
{
  chip_state_t *chip = malloc(sizeof(chip_state_t));

  memset(chip->regs, 0, sizeof(chip->regs));
  uint32_t num_ch_attr = attr_init("channels", 1);
  chip->num_ch = attr_read(num_ch_attr);
  printf("Running with %d channels\n", chip->num_ch);

  chip->spi_buffer_len = (2 + chip->num_ch) * WORD_SIZE;
  chip->cs_pin = pin_init("CS", INPUT_PULLUP);
  chip->rst_pin = pin_init("RST", INPUT_PULLUP);
  chip->regs[REG_ID] = chip->num_ch;

  chip->dcdc_timer = timer_init(&(timer_config_t){
      .user_data = chip,
      .callback = on_dcdc_timer,
  });

  const pin_watch_config_t watch_config = {
      .edge = BOTH,
      .pin_change = chip_pin_change,
      .user_data = chip,
  };
  pin_watch(chip->cs_pin, &watch_config);

  const pin_watch_config_t rst_watch_config = {
      .edge = BOTH,
      .pin_change = rst_pin_change,
      .user_data = chip,
  };
  pin_watch(chip->rst_pin, &rst_watch_config);


  const spi_config_t spi_config = {
      .sck = pin_init("SCK", INPUT),
      .miso = pin_init("MISO", INPUT),
      .mosi = pin_init("MOSI", INPUT),
      .done = chip_spi_done,
      .user_data = chip,
  };
  chip->spi = spi_init(&spi_config);

  for(int ch=0; ch<MAX_NUM_CH; ch++){
    char attr_name[10];
    sprintf(attr_name, "ana%d", ch);
    chip->ana_attr[ch] = attr_init(attr_name, 0);
  }
}

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value)
{
  chip_state_t *chip = (chip_state_t *)user_data;
  // Handle CS pin logic
  if (pin == chip->cs_pin)
  {
    if (value == LOW)
    {
      printf("SPI chip selected\n");
      // set measurements 
      for(int ch=0; ch<chip->num_ch; ch++){
        float ana_val = attr_read_float(chip->ana_attr[ch]);
        int32_t ana_raw = ana_val * (1<<23);

        printf("Channel %d: %f %d\n", ch, ana_val, ana_raw);

        chip->spi_buffer[3 + ch * WORD_SIZE] = (ana_raw >> 16) & 0xFF;
        chip->spi_buffer[3 + ch * WORD_SIZE + 1] = (ana_raw >> 8) & 0xFF;
        chip->spi_buffer[3 + ch * WORD_SIZE + 2] = ana_raw & 0xFF;
      }

      // set CRC 
      uint16_t crc = calc_crc(chip->spi_buffer, chip->spi_buffer_len - WORD_SIZE);
      chip->spi_buffer[chip->spi_buffer_len - 3] = (crc >> 8) & 0xFF;
      chip->spi_buffer[chip->spi_buffer_len - 2] = crc & 0xFF;

      spi_start(chip->spi, chip->spi_buffer, chip->spi_buffer_len);
    }
    else
    {
      printf("SPI chip deselected\n");
      spi_stop(chip->spi);
    }
  }
}


static void rst_pin_change(void *user_data, pin_t pin, uint32_t value){
  chip_state_t *chip = (chip_state_t *)user_data;
  if (value == LOW)
  {
    printf("hard resetting chip\n");
    reset(chip);
  }
}

void set_response(chip_state_t *chip, uint16_t res)
{
  chip->spi_buffer[0] = (res >> 8) & 0xFF;
  chip->spi_buffer[1] = res & 0xFF;
}

void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count)
{
  chip_state_t *chip = (chip_state_t *)user_data;
  if (!count)
  {
    // This means that we got here from spi_stop, and no data was received
    return;
  }
  uint16_t cmd = (buffer[0] << 8) | buffer[1];
  printf("Command: 0x%04X count=%d\n", cmd, count);

  // check CRC
  uint16_t crc = calc_crc(buffer, count - WORD_SIZE);
  uint16_t actual_crc = (buffer[count - 3] << 8) | buffer[count - 2];
  if (crc != actual_crc)
  {
    printf("CRC mismatch: 0x%04X != 0x%04X\n", crc, actual_crc);
    dump_buffer(buffer, count);
    return;
  }

  if (cmd == 0x0000)
  {
    // null command, response is status reg
    set_response(chip, chip->regs[0x01]);
  }
  else if ((cmd & 0xe000) == 0xa000)
  {
    // read register
    uint16_t reg = (cmd & 0x1f80) >> 7;
    set_response(chip, chip->regs[reg]);
  }
  else if ((cmd & 0xe000) == 0x6000)
  {
    // write register
    uint16_t reg = (cmd & 0x1f80) >> 7;
    uint16_t val = (buffer[3] << 8) | buffer[4];
    write_reg(chip, reg, val);
    set_response(chip, 0x2000 | (cmd & 0x1FFF));
  }
  else
  {
    printf("Unknown command: 0x%04X\n", cmd);
    // unknown command
    set_response(chip, 0x0000);
  }


  if (pin_read(chip->cs_pin) == LOW)
  {
    // Continue with the next character
    spi_start(chip->spi, chip->spi_buffer, sizeof(chip->spi_buffer));
  }
}

static uint16_t calc_crc(uint8_t *data, size_t len)
{
  int bit_index, byte_index;
  bool data_msb;          /* Most significant bit of data byte   */
  bool crc_msb;           /* Most significant bit of crc byte    */
  uint16_t poly = 0x1021; /* CCITT CRC polynomial = x^16 + x^15 + x^2 + 1 */
  uint16_t crc = 0xffff;

  for (byte_index = 0; byte_index < len; byte_index++)
  {
    bit_index = 0x80;

    /* Loop through all bits in the current byte    */
    while (bit_index > 0)
    {
      data_msb = (bool)(data[byte_index] & bit_index);
      crc_msb = (bool)(crc & 0x8000u);

      crc <<= 1;

      if (data_msb ^ crc_msb)
      {
        crc ^= poly;
      }
      bit_index >>= 1;
    }
  }
  return (crc);
}
