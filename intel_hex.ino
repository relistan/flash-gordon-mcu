// Docs on the format: http://www.keil.com/support/docs/1584/

/*
Example:
:10001300AC12AD13AE10AF1112002F8E0E8F0F2244
:10000300E50B250DF509E50A350CF5081200132259
:03000000020023D8
:0C002300787FE4F6D8FD7581130200031D
:10002F00EFF88DF0A4FFEDC5F0CEA42EFEEC88F016
:04003F00A42EFE22CB
:00000001FF
*/

// BUFFER_SIZE defines how big the buffer is that we'll use for each line.
// This will end up 
#define BUFFER_SIZE 100

#define REC_TYPE_DATA 0
#define REC_TYPE_EOF 1     // end-of-file record
#define REC_TYPE_EXTSEG 2  // extended segment address record
#define REC_TYPE_EXTADDR 4 // extended linear address record (for 32bit records)

const char* const errCodes[] = {
  "Encountered NULL before end of line",
  "Mismatched checksum",
  "Unrecognized starting character"
};

// A DecodeResult contains all the values decoded from a single line of the hex
// file.
struct DecodeResult {
  uint8_t  len;
  uint8_t  *output;
  uint16_t addr;
  uint8_t  recType;
};

// hexToByte converts a single hex pair into a byte of data.
uint8_t hexToByte(char *input) {
  char partA = input[0] > 57 ? input[0] - 55 : input[0] - 48;
  char partB = input[1] > 57 ? input[1] - 55 : input[1] - 48;
  return (partA << 4) | partB;
}

// hexToUint32 converts 8 bytes of hex text to a uint32_t. Assumes
// correct buffer length!
uint32_t hexToUint32(char *buf) {
  return (hexToByte(buf) << 24) | (hexToByte(&buf[2]) << 16) | (hexToByte(&buf[4]) << 8) | hexToByte(&buf[6]);
}

// checksumFor calculates the checksum for an array of bytes
uint8_t checksumFor(char *line, uint8_t len) {
  uint8_t sum = 0;
  for(uint8_t i = 0; i < 8+len; i += 2) {
    // Exit early if we hit the end of string or EOL
    if(line[i] == '\0' || line[i] == '\n') {
            Serial.printf("ERROR: Bailing out early from checksum: '%c'", line[i]);
            break;
    }
        sum += hexToByte(&line[i]);
  }

  return (sum ^ 0xFF) + 1;
}

// (("1C126000097D0A0A097075747328225C6E436F6D706C6574652E22293B0A7D0A".split(//).each_slice(2).map { |x| eval("0x#{x.join}") }.inject(0) { |memo, x| memo += x } & 0xFF) ^ 0xFF) + 1

// decodeLine decodes a line of Intel Hex file and populates the resulting data
// into the `result` struct that is passed in. The `output` field must already
// have been allocated before it is passed as a member of the struct. If a
// non-zero value is returned, none of the values in the output struct can be
// relied upon.
int decodeLine(char *line, struct DecodeResult *result) {
  if(DEBUG) {
    if(line[0] != ':') {
      Serial.printf("decodeLine Got: '0x0%x 0x0%x'\n", line[0], line[1]);
      return -3;
    }
  }

  result->len = hexToByte(&line[1]);
  result->addr = (hexToByte(&line[3]) << 8) | hexToByte(&line[5]);
  result->recType = hexToByte(&line[7]);

  result->len = result->len << 1; // Double the length because it's hex encoded

  // Loop through all the remaining data and decode the bytes (start from byte 9).
  uint8_t outCount = 0;
  for(uint8_t i = 9; i < 9 + result->len; i += 2, outCount++) {
    // End at the null terminator just in case we hit it before
    // the correct line length in an badly-encoded file.
    if(line[i] == '\0') {
      return -1; // null before end of line
    }

    result->output[outCount] = hexToByte(&line[i]);
  }
  result->output[outCount+1] = '\0';

  // Last two characters are the checksum byte
  uint8_t checkSum = hexToByte(&line[9+result->len]);
  uint8_t calculated = checksumFor(&line[1], result->len);

  result->len = result->len >> 1; // divide back to actual length

  if(DEBUG) {
    Serial.println(line);
    Serial.printf("Len: %d\n", result->len);
    Serial.printf("Addr: %d\n", result->addr);
    Serial.printf("RecType: %d\n", result->recType);
    Serial.printf("Checksum: %0X\n", checkSum);
  }

  // Make sure the checksum is legit or return an error. In that case
  // it was most likely corrupted in the serial upload.
  if(calculated != checkSum) {
    if(DEBUG) {
      Serial.printf("ERROR: Calculated: 0x%0X, Got: 0x%0X\n",
        (uint8_t)calculated, checkSum
      );
    }
    return -2; // mismatched checksum
  }

  return 0;
}

// hexDump prints out a set of bytes as hex values, separated by spaces.
void hexDump(DecodeResult *result) {
  switch(result->recType) {
  case REC_TYPE_DATA:
    Serial.printf("D ");
    break;
  case REC_TYPE_EOF:
    Serial.printf("E ");
    break;
  case REC_TYPE_EXTADDR:
    Serial.printf("A ");
    break;
  case REC_TYPE_EXTSEG:
    Serial.printf("S ");
    break;
  default:
    Serial.printf("ERROR");
  }

  for(uint8_t i = 0; i < result->len; i++) {
    Serial.printf("%02X ", result->output[i]);
  }
  Serial.println(".");
}

// writeRecord writes a record to the flash chip with the specified base
// address.
void writeRecord(uint16_t baseAddr, struct DecodeResult *result, char type) {
  for(uint8_t i = 0; i < result->len; i++) {
    
    // Flash chips have to be told to write, EEPROMs are good to go
    if (type == 'f') { // It's a flash chip
      sendByte(0x5555, 0xaa);
      sendByte(0x2aaa, 0x55);
      sendByte(0x5555, 0xa0);
    } /*else { // Disable write protection on EEPROM if accidentally enabled
      sendByte(0x1555, 0xaa);
      sendByte(0x0aaa, 0x55);
      sendByte(0x1555, 0x80);
      sendByte(0x1555, 0xaa);
      sendByte(0x0aaa, 0x55);
      sendByte(0x1555, 0x20);
    }*/
    
    writeByte(baseAddr+i, result->output[i], type);
  }
}

void processHexFile(char type) {
  Serial.println("Processing Hex File.\n");

  char buf[BUFFER_SIZE];  
  uint8_t output[BUFFER_SIZE];
  char *line; 

  DecodeResult result;
  result.output = output;

  uint16_t baseAddr = 0;

  while(1) {
    line = serialReadLine(buf);
    if(line == NULL) {
      Serial.printf("ERROR: got end of file without EOF record. Suspect corrupt data.\n");
      break;
    }

    int code = decodeLine(line, &result);
    if(code < 0) {
      Serial.printf("ERROR: Failed when decoding: %s\n", errCodes[0 - code - 1]);
      break;
    }

    hexDump(&result);

    // Take one parsed record and handle it according to the record type.
    // Modify baseAddr based on the instructions from the EXTADDR and
    // EXTSEG records. Set receivedEOF if we got the EOF record.
    switch(result.recType) {
    case REC_TYPE_EOF:
      goto COMPLETED; // Yes, I used a goto
      break;
    case REC_TYPE_EXTSEG:
      // Shift over 4 bits because this represents the high bits of the address
      baseAddr = ((uint16_t)result.output[0]) << 4;
      if(DEBUG) {
        Serial.printf("New base addr: 0x%X\n", baseAddr);
      }
      break;
    case REC_TYPE_DATA:
      // Do the work: write to the flash
      writeRecord(baseAddr, &result, type);
      baseAddr += result.len;
      break;
    case REC_TYPE_EXTADDR:
      // Extended linear address record (for 32bit records)
      baseAddr = ((uint16_t)result.output[0]);
      if(DEBUG) {
        Serial.printf("New base addr: 0x%X\n", baseAddr);
      }
      break;
    default:
      Serial.printf("ERROR: Unknown record type: %0d\n", result.recType);
    }
  }
COMPLETED:

  Serial.println("\nComplete.");
}
