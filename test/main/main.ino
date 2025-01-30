#include <SPI.h>
#include <string.h>

#define CS 10
#define NUM_CH 2
#define SPI_LEN ((2 + NUM_CH) * 3)

void dumpBuffer(uint8_t *buffer, size_t size)
{
  for (size_t i = 0; i < size; i++)
  {
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
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

void spiTransfer(uint8_t *buffer, uint16_t opcode)
{
  buffer[0] = (opcode & 0xff00) >> 8;
  buffer[1] = (opcode & 0xff);

  uint16_t crc = calc_crc(buffer, SPI_LEN - 3);
  buffer[SPI_LEN - 3] = (crc & 0xff00) >> 8;
  buffer[SPI_LEN - 2] = (crc & 0xff);

  digitalWrite(CS, LOW);
  SPI.begin();
  SPI.setDataMode(SPI_MODE1);
  SPI.transfer(buffer, SPI_LEN);
  SPI.end();
  digitalWrite(CS, HIGH);

  uint16_t expected_crc = calc_crc(buffer, SPI_LEN - 3);
  uint16_t actual_crc = (buffer[SPI_LEN - 3] << 8) | buffer[SPI_LEN - 2];

  if (expected_crc != actual_crc)
  {
    Serial.println("CRC mismatch");
    Serial.print("Expected: ");
    Serial.println(expected_crc, HEX);
    Serial.print("Actual: ");
    Serial.println(actual_crc, HEX);
  }
}

int32_t sign_extend(uint8_t dataBytes[])
{
  return ((int32_t)((((uint32_t)dataBytes[0] << 24) | ((uint32_t)dataBytes[1] << 16) | ((uint32_t)dataBytes[2] << 8))) >> 8);
}

int32_t get_measurement(uint8_t *buffer, int ch)
{
  return sign_extend(&buffer[3 + (ch * 3)]);
}

void null_cmd(uint8_t *buffer)
{
  spiTransfer(buffer, 0x0000);
}

void write_reg(uint8_t reg, uint16_t value)
{
  uint8_t buffer[SPI_LEN] = {0};
  uint16_t opcode = 0x6000 | (reg << 7);
  buffer[3] = (value & 0xff00) >> 8;
  buffer[4] = (value & 0xff);
  spiTransfer(buffer, opcode);
}

uint16_t read_reg(uint8_t reg)
{
  uint8_t buffer[SPI_LEN] = {0};
  uint16_t opcode = 0xa000 | (reg << 7);
  spiTransfer(buffer, opcode);
  memset(buffer, 0, SPI_LEN);

  null_cmd(buffer);
  return (buffer[0] << 8) | buffer[1];
}

void setup()
{
  Serial.begin(115200);
  pinMode(CS, OUTPUT);

  write_reg(0x31, 0x0001); // enable DCDC

  // wait until SEC_FAIL disappears
  int tout = 10;
  while (1)
  {
    uint16_t v;
    v = read_reg(0x01);
    if (!(v & 0x0040))
    {
      break;
    }
    delay(100);
    if (--tout == 0)
    {
      Serial.println("SEC_FAIL timeout");
      return;
    }
  }
  Serial.println("SEC_FAIL cleared");
}

void loop()
{
  uint8_t buffer[SPI_LEN] = {0};
  null_cmd(buffer);
  int32_t v = get_measurement(buffer, 0);
  Serial.print("Measurement CH0: ");
  Serial.println(v);
#if NUM_CH > 1
  v = get_measurement(buffer, 1);
  Serial.print("Measurement CH1: ");
  Serial.println(v);
#endif

  delay(500);
}
