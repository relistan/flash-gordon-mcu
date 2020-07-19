#include <stdio.h>
#include <stdint.h>

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

#define DEBUG 1

#define REC_TYPE_DATA 0
#define REC_TYPE_EOF 1     // end-of-file record
#define REC_TYPE_EXTSEG 2  // extended segment address record
#define REC_TYPE_EXTADDR 4 // extended linear address record (for 32bit records)

const char* const errCodes[] = {
	"Encountered NULL before end of line",
	"Mismatched checksum",
	"Unrecognized starting character"
};

// hexToByte converts a single hex pair into a byte of data.
uint8_t hexToByte(char *input) {
	char partA = input[0] > 57 ? input[0] - 55 : input[0] - 48;
	char partB = input[1] > 57 ? input[1] - 55 : input[1] - 48;
	return (partA << 4) | partB;
}

// A DecodeResult contains all the values decoded from a single line of the hex
// file.
typedef struct {
	uint8_t len;
	uint8_t  *output;
	uint16_t addr;
	uint8_t  recType;
} DecodeResult;

// checksumFor calculates the checksum for an array of bytes
uint8_t checksumFor(char *line, uint8_t len) {
    uint8_t sum

    // Start from 1, to skip ':', run to full len
	for(uint8_t i = 1; i <= len; i++) {
        // Exit early if we hit the end of string or EOL
		if(line[i] == '\0' || line[i] == '\n') {
            break;
		}
        sum += line[i];
	}

    return (sum ^ 0xFF) + 1;
}

// decodeLine decodes a line of Intel Hex file and populates the resulting data
// into the `result` struct that is passed in. The `output` field must already
// have been allocated before it is passed as a member of the struct. If a
// non-zero value is returned, none of the values in the output struct can be
// relied upon.
int decodeLine(char *line, DecodeResult *result) {
	if(line[0] != ':') {
		return -3;
	}

	result->len = hexToByte(&line[1]);
	result->addr = (hexToByte(&line[3]) << 8) | hexToByte(&line[5]);
	result->recType = hexToByte(&line[7]);

	uint8_t checkTotal = result->len + result->addr + result->recType;

	result->len = result->len << 1; // Double the length because it's hex encoded

	// Loop through all the remaining data and decode the bytes (start from byte 9).
	uint8_t outCount = 0;
	for(uint8_t i = 9; i < 9 + result->len; i += 2, outCount++) {
		// End at the null terminator just in case we hit it before
		// the correct line length in an badly-encoded file.
		if(line+i == '\0') {
			return -1; // null before end of line
		}

		result->output[outCount] = hexToByte(&line[i]);
		checkTotal += (uint8_t)result->output[outCount];
	}
	result->output[outCount+1] = '\0';

	// Last two characters are the checksum byte
	uint8_t checkSum = hexToByte(&line[9+result->len]);

	result->len = result->len >> 1; // divide back to actual length

	if(DEBUG) {
		puts(line);
		printf("Len: %d\n", result->len);
		printf("Addr: %d\n", result->addr);
		printf("RecType: %d\n", result->recType);
		printf("Checksum: %0X\n", checkSum);
	}

	// Make sure the checksum is legit or return an error. In that case
	// it was most likely corrupted in the serial upload.
	uint16_t calculated = (checkTotal ^ 0xFF) + 1;
	if((uint8_t)calculated != checkSum) {
		if(DEBUG) {
			fprintf(stderr, "Calculated: 0x%0X, Got: 0x%0X\n",
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
		printf("D ");
		break;
	case REC_TYPE_EOF:
		printf("E ");
		break;
	case REC_TYPE_EXTADDR:
		printf("A ");
		break;
	case REC_TYPE_EXTSEG:
		printf("S ");
		break;
	default:
		printf("ERROR");
	}

	for(uint8_t i = 0; i < result->len; i++) {
		printf("%02X ", result->output[i]);
	}
	puts("");
}

// writeRecord writes a record to the flash chip with the specified base
// address.
void writeRecord(uint16_t baseAddr, DecodeResult *result) {

}

int main(int argc, char **argv) {
	puts("Starting up.\n");

	//FILE *fp = fopen("/dev/stdin", "r");//argv[1], "r");
	FILE *fp = fopen(argv[1], "r");

	char buf[BUFFER_SIZE];	
	uint8_t output[BUFFER_SIZE];
	char *line; 

	DecodeResult result;
	result.output = output;

	uint16_t baseAddr = 0;
	uint8_t receivedEOF = 0;

	while(1) {
		line = fgets(buf, BUFFER_SIZE, fp);
		if(line == NULL) {
			if(!receivedEOF) {
				fprintf(stderr, "Error: got end of file without EOF record. Suspect corrupt data.\n");
			}
			break;
		}

		int code = decodeLine(line, &result);
		if(code < 0) {
			fprintf(stderr, "Error decoding: %s\n", errCodes[0 - code - 1]);
			continue;
		}

		hexDump(&result);

		// Take one parsed record and handle it according to the record type.
		// Modify baseAddr based on the instructions from the EXTADDR and
		// EXTSEG records. Set receivedEOF if we got the EOF record.
		switch(result.recType) {
		case REC_TYPE_EOF:
			receivedEOF = 1;
			break;
		case REC_TYPE_EXTSEG:
 			// Shift over 4 bits because this represents the high bits of the address
			baseAddr = ((uint16_t)result.output[0]) << 4;
			if(DEBUG) {
				printf("New base addr: 0x%X\n", baseAddr);
			}
			break;
		case REC_TYPE_DATA:
			// Do the work: write to the flash
			writeRecord(baseAddr, &result);
			break;
		case REC_TYPE_EXTADDR:
			// Extended linear address record (for 32bit records)
			baseAddr = ((uint16_t)result.output[0]);
			if(DEBUG) {
				printf("New base addr: 0x%X\n", baseAddr);
			}
			break;
		default:
			fprintf(stderr, "Unknown record type: %0d\n", result.recType);
		}
	}

	puts("\nComplete.");
}
