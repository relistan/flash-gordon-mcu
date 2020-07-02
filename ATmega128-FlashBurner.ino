// ATmega128 Parallel Flash Burner
// Karl Matthias -- June 2020

// Assumes frequency of 8Mhz

// PINOUT
// ----------------------------------------
// PB0-7 --> Address 0-7
// PF0-7 --> Address 8-15
// PE7   --> Address 16
// PC0-7 --> Data 0-7
// PD0 --> CE#
// PD1 --> OE#
// PD2 --> WE#
// ----------------------------------------

#define DATA_REG_OUT PORTC
#define DATA_REG_IN PINC
#define LOW_ADDR PORTB
#define HIGH_ADDR PORTF
#define HIGHEST_ADDR PORTE

#define HIGHEST_BIT PE7

// The port the control pins go on, corresponding to the pins below
#define CONTROL_PORT PORTD

// These will need to be inverted
#define CE PD0
#define OE PD1
#define WE PD2

void setup() {
  // Init the chip control pins
  setWriteDisable();
  setOutputDisable();
  setChipEnable();
  DDRD |= (1 << CE) | (1 << WE) | (1 << OE);
  
  Serial.begin(57600);
  uint8_t startingConfig = PORTD;
  Serial.println("--------------------------------");
  Serial.printf("Starting control pins: 0x%02X\n", startingConfig);
  
  chipErase();

/*
  for(int i = 0; i < 50; i++) {
    if (i % 2 == 0) {
      writeByte(i, 0xaa);
    }
  }
  */
  
  delay(10);
  for(int i = 0; i < 60; i++) {
    byte data = readData(i);
  }
  //setChipDisable();
}

void setWriteEnable() {
  CONTROL_PORT &= ~(1 << WE);
}

void setWriteDisable() {
  CONTROL_PORT |= (1 << WE);
}

void setChipEnable() {
  CONTROL_PORT &= ~(1 << CE);
}

void setChipDisable() {
  CONTROL_PORT |= (1 << CE);
}

void setOutputEnable() {
  CONTROL_PORT &= ~(1 << OE); 
}

void setOutputDisable() {
  CONTROL_PORT |= (1 << OE); 
}

void chipErase() {
  sendByte(0x5555, 0xaa);
  sendByte(0x2aaa, 0x55);
  sendByte(0x5555, 0x80);
  
  sendByte(0x5555, 0xaa);
  sendByte(0x2aaa, 0x55);
  sendByte(0x5555, 0x10);

  delay(100);
}

void sendByte(uint32_t address, byte data) {
  setAddress(address);
  setData(data);
  setWriteEnable();
  asm("nop; nop; nop; nop;"); // ~50 nanoseconds
  setWriteDisable();
  clearData();
}

void writeByte(uint32_t address, byte data) {
  Serial.printf("a 0x%05x ", address);
  Serial.printf("W 0x%02x\n", data);
  sendByte(0x5555, 0xaa);
  sendByte(0x2aaa, 0x55);
  sendByte(0x5555, 0xa0);

  sendByte(address, data);
  setChipDisable();
  delayMicroseconds(20); // Internal flash byte program time
  setChipEnable();
}

byte readData(uint32_t address) {
  setAddress(address);
  PINC = 0x00;
  DDRC = 0x00;
  
  setOutputEnable();
  asm("nop; nop; nop; nop;"); // ~37.5 nanoseconds
  byte data = PINC;
  Serial.printf("a 0x%05x ", address);
  Serial.printf("r 0x%02x\n", data);
  setOutputDisable();
 
  return data;
}

void setData(uint8_t data) {
  PINC = 0x00; // Make sure we don't get a stale read
  DDRC = 0x00;
  PORTC = data;
  DDRC = 0xff;
}

void clearData() {
  DDRC = 0x00;
  PORTC = 0x00;
}

// setAddress supports a 17bit number for the 010A variant of the flash EEPROM
byte setAddress(uint32_t address) {
  // Disable output
  DDRB = 0x00;
  DDRF = 0x00;
  DDRE &= ~(1 << HIGHEST_BIT);
  
  // Set ports
  HIGH_ADDR = highByte(address);
  LOW_ADDR  = lowByte(address);

  // Handle the 17th bit
  if(address > 0xffff) {
    HIGHEST_ADDR |= (1 << HIGHEST_BIT);
  } else {
    HIGHEST_ADDR &= ~(1 << HIGHEST_BIT);
  }

  // Enable address pins as output
  DDRB = 0xff;
  DDRF = 0xff;
  DDRE |= (1 << HIGHEST_BIT);
}

void loop() {
  // put your main code here, to run repeatedly:
}
