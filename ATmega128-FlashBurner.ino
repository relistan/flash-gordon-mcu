// ATmega128 Parallel Flash Burner
// Karl Matthias -- June 2020

// Assumes frequency of 8Mhz

// PINOUT
// ----------------------------------------
// PB0-7 --> Address 0-7
// PF0-7 --> Address 8-15
// PE5-7 --> Address 16-18
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

#define HIGHEST_BIT PE5

// The port the control pins go on, corresponding to the pins below
#define CONTROL_PORT PORTD

// These will need to be inverted
#define CE PD0
#define OE PD1
#define WE PD2

#define DEBUG 0

void setup() {
  // Init the chip control pins
  setWriteDisable();
  setOutputDisable();
  setChipEnable();
  DDRD |= (1 << CE) | (1 << WE) | (1 << OE);

  turnOnLED();
  
  Serial.begin(57600);
  Serial.println("--------------------------------");
  delay(10); // Give serial time to settle
  
  for(int i = 0; i < 60; i++) {
    readData(i);
  }
}

// The AVR board has LEDs on PA0 and PA1, while the Flash Gordon board
// has an LED on PA0.
void turnOnLED() {
  PORTA |= (1 << PA0) | (1 << PA1);
  DDRA |= (1 << PA0) | (1 << PA1);
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

// sectorErase supports up to a 12-bit number. It erases a single sector in the Flash.
// The chip has 4KB sectors. Sector erase takes about 18ms.
void sectorErase(uint16_t sector) {
  sendByte(0x5555, 0xaa);
  sendByte(0x2aaa, 0x55);
  sendByte(0x5555, 0x80);
  
  sendByte(0x5555, 0xaa);
  sendByte(0x2aaa, 0x55);
  sendByte(0x5555, 0x10);
  
  sendByte(sector, 0x30); // 12bit sector on the address lines
  delay(19);
}

void sendByte(uint32_t address, byte data) {
  setAddress(address);
  setData(data);
  setWriteEnable();
  asm("nop; nop; nop; nop;"); // ~50 nanoseconds
  setWriteDisable();
  clearData();
}

void writeByte(uint32_t address, byte data, char type) {
//  Serial.printf("a 0x%05x ", address);
//  Serial.printf("W 0x%02x\n", data);

  sendByte(address, data);
  setChipDisable();
  switch(type) {
    case 'f': delayMicroseconds(20); // Internal flash byte program time
    case 'e': delayMicroseconds(30); // Internal EEPROM byte program time
  }
  setChipEnable();
}

byte readData(uint32_t address) {
  setAddress(address);
  PINC = 0x00;
  DDRC = 0x00;
  
  setOutputEnable();
  asm("nop; nop; nop; nop;"); // ~37.5 nanoseconds
  byte data = PINC;
  if(DEBUG) {
    Serial.printf("a 0x%05x ", address);
    Serial.printf("r 0x%02x\n", data);
  }
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
void setAddress(uint32_t address) {
  // Disable output
  DDRB = 0x00;
  DDRF = 0x00;
  DDRE &= ~(1 << HIGHEST_BIT);
  DDRE &= ~(1 << PE6);
  DDRE &= ~(1 << PE7);
  
  // Set ports
  HIGH_ADDR = highByte(address);
  LOW_ADDR  = lowByte(address);

  // Handle the 17th bit
  if(address > 0xffff) {
    HIGHEST_ADDR |= (1 << HIGHEST_BIT);
    HIGHEST_ADDR &= ~(1 << PE6);
    HIGHEST_ADDR &= ~(1 << PE7);
  } else {
    HIGHEST_ADDR &= ~(1 << HIGHEST_BIT);
    HIGHEST_ADDR &= ~(1 << PE6);
    HIGHEST_ADDR &= ~(1 << PE7);
  }

  // Enable address pins as output
  DDRB = 0xff;
  DDRF = 0xff;
  DDRE |= (1 << HIGHEST_BIT);
  DDRE |= (1 << PE6);
  DDRE |= (1 << PE7);
}

// dumpAll reads a range of bytes from the flash/EEPROM and prints them
// to the serial port as Intel hex.
void dumpAll(uint32_t start, uint32_t len) {
  byte checksum;
  
  // TODO only handles 16bit addressing right now
  
  for(uint32_t i = start; i < len; i += 32) {

    // Each byte of 2 bytes in the 16bit addr + hard-coded length
    checksum = highByte(i) + lowByte(i) + 0x20;
    
    // This is :<length><16bit-address><recType>
    Serial.printf(":%02X%04X00", 32, i); 
    
    for(byte j = 0; j < 32; j++) {
      byte b = readData(i+j);
      checksum += b;
      Serial.printf("%02X", b);
    }
    
    checksum = (checksum ^ 0xFF) + 1;
    Serial.printf("%02X\n", checksum);
  }
}

void loop() {
  processSerialCmd();
}
