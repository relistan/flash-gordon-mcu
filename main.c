#include <stdio.h>
#include <stdint.h>

/*
:10001300AC12AD13AE10AF1112002F8E0E8F0F2244
:10000300E50B250DF509E50A350CF5081200132259
:03000000020023D8
:0C002300787FE4F6D8FD7581130200031D
:10002F00EFF88DF0A4FFEDC5F0CEA42EFEEC88F016
:04003F00A42EFE22CB
:00000001FF
*/

#define BUFFER_SIZE 100

const char* const errCodes[] = {
	"Encountered NULL before end of line",
	"Mismatched checksum"
};

// hexToByte converts a single hex pair into a byte of data.
uint8_t hexToByte(char *input) {
	char partA = input[0] > 57 ? input[0] - 55 : input[0] - 48;
	char partB = input[1] > 57 ? input[1] - 55 : input[1] - 48;
	return (partA << 4) + partB;
}

// A DecodeResult contains all the values decoded from a single line of
// the hex file.
typedef struct {
	uint8_t len;
	uint8_t  *output;
	uint16_t addr;
	uint8_t  recType;
} DecodeResult;

// decodeLine decodes a line of Intel Hex file and populates the resulting
// data into the `result` struct that is passed in. The `output` field
// must already have been allocated before it is passed as a member of the
// struct.
int decodeLine(char *line, DecodeResult *result) {
	if(line[0] != ':') {
		puts("ERROR, unrecognized starting character");
	}

	result->len = hexToByte(line+1);
	result->addr = (hexToByte(line+3) << 8) + hexToByte(line+5);
	result->recType = hexToByte(line+7);

	uint8_t checkTotal = result->len + result->addr + result->recType;

	result->len = result->len << 1; // Double the length because it's hex encoded

	// Loop through all the remaining data and decode the bytes
	uint8_t outCount = 0;
	for(uint8_t i = 9; i < 9 + result->len; i += 2, outCount++) {
		// End at the null terminator just in case we hit it before
		// in an badly-encoded file.
		if(line+i == '\0') {
			return -1; // null before end of line
		}

		result->output[outCount] = hexToByte(line+i);
		checkTotal += result->output[outCount];
	}
	result->output[outCount+1] = '\0';

	// Last two characters are the checksum byte
	uint8_t checkSum = hexToByte(&line[9+result->len]);

	result->len = result->len >> 1; // divide back to actual length

	uint8_t calculated = (checkTotal ^ 0xFF) + 1;
	if(calculated != checkSum) {
		return -2; // mismatched checksum
	}

	return 0;
}

// hexDump prints out a set of bytes as hex values, separated by spaces.
void hexDump(uint8_t *input, uint8_t len) {
	for(uint8_t i = 0; i < len; i++) {
		printf("%02X ", input[i]);
	}
	puts("");
}

int main(int argc, char **argv) {
	FILE *fp = fopen(argv[1], "r");

	char buf[BUFFER_SIZE];	
	uint8_t output[BUFFER_SIZE];
	char *line; 

	DecodeResult result;
	result.output = output;

	while(1) {
		line = fgets(buf, BUFFER_SIZE, fp);
		if(line == NULL) {
			break;
		}

		int len = decodeLine(line, &result);
		if(len < 0) {
			fprintf(stderr, "Error decoding: %s\n", errCodes[0 - len - 1]);
			continue;
		}

		printf("Len: %d\n", result.len);
		printf("Addr: %d\n", result.addr);
		printf("RecType: %d\n", result.recType);

		hexDump(result.output, result.len);
		puts(line);
	}
}
