package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"math"
	"os"

	log "github.com/sirupsen/logrus"
)

func checksumFor(record []byte, buf []byte) byte {
	var sum uint16
	for _, j := range record {
		sum += uint16(j)
	}

	for _, j := range buf {
		sum += uint16(j)
	}

	sum = (sum ^ 0xFF) + 1

	result := byte(sum) // truncate to 8 bits
	return result
}

func main() {
	filename := os.Args[1]

	file, err := os.Open(filename)
	if err != nil {
		log.Fatalf("Unable to open %s: %s", filename, err)
	}

	buf := make([]byte, 32)
	addr := 0
	segment := 0

	var shouldStop bool

	for !shouldStop {
		readLen, err := io.ReadFull(file, buf)
		// This is expected when we hit the end of the file
		if err == io.ErrUnexpectedEOF || err == io.EOF {
			shouldStop = true
		} else if err != nil {
			log.Fatalf("Error on read: %s", err)
		}

		bufBytes := buf[:readLen]
		// Get first byte of readLen (it's 32 or less) and pad with space for
		// the 16bit addr as well. We'll overwrite those 0x0s with addr.
		allBytes := []byte{byte(readLen), 0x0, 0x0}

		binary.BigEndian.PutUint16(allBytes[1:], uint16(addr))
		allBytes = append(allBytes, bufBytes...)
		checkSum := checksumFor(allBytes, []byte{})

		fmt.Printf(":%02X%02X%02X%02X%02X%02X\r\n", allBytes[0], allBytes[1], allBytes[2], 0x0, allBytes[3:], checkSum)

		addr += readLen

		// Handle more than 16 bits by emitting a new segment record
		if addr > int(math.Pow(2, 16) - 1) {
			log.Errorf("%d", addr)
			addr -= int(math.Pow(2, 16) - 1)
			segment++

			recordBytes := []byte{0x02, 0x00, 0x00, 0x02}
			fmt.Printf(":02000002%04X%02X\r\n", segment, checksumFor(recordBytes, buf[:readLen]))
		}
	}

	fmt.Printf(":00000001FF\r\n")
}
