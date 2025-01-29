#include <SPI.h>

#define CS 10

void spiTransfer(uint8_t *buffer, size_t size) {
  digitalWrite(CS, LOW);
  SPI.begin();
  SPI.transfer(buffer, size);
  SPI.end();
  digitalWrite(CS, HIGH);
}

void dumpBuffer(uint8_t *buffer, size_t size) {
  for (size_t i = 0; i < size; i++) {
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void setup() {
  uint8_t buffer[9];

  Serial.begin(115200);
  pinMode(CS, OUTPUT);

  // SPI Transaction: sends the contents of buffer, and overwrites it with the received data.
  buffer[0] = 0x01;
  buffer[1] = 0x02;

  spiTransfer(buffer, 9);
  dumpBuffer(buffer, 9);

  buffer[0] = 0x03;
  buffer[1] = 0x04;

  spiTransfer(buffer, 9);
  dumpBuffer(buffer, 9);

}

void loop() {
}
