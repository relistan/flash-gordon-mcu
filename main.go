package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"math"
	"os"

	log "github.com/sirupsen/logrus"
)

const (
	BytesPerLine = 32
)

var (
	twoToThe16th = int(math.Pow(2, 16))
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

func formatRecord(recLen int, addr int, recType byte, rec []byte) string {
	// Get first byte of recLen (it's BytesPerLine or less) and pad with space for
	// the 16bit addr as well. We'll overwrite those 0x0s with addr. Follow
	// with the rest of the record
	allBytes := append([]byte{byte(recLen), 0x0, 0x0, recType}, rec...)

	binary.BigEndian.PutUint16(allBytes[1:], uint16(addr))
	checkSum := checksumFor(allBytes, []byte{})

	return fmt.Sprintf(":%02X%02X", allBytes, checkSum)
}

func main() {
	filename := os.Args[1]

	file, err := os.Open(filename)
	if err != nil {
		log.Fatalf("Unable to open %s: %s", filename, err)
	}

	buf := make([]byte, BytesPerLine)
	addr := 0
	segment := 0

	var shouldStop bool

	// Starting address record
	fmt.Println(formatRecord(2, 0x0, 0x04, []byte{0x0, 0x0}))

	for !shouldStop {
		readLen, err := io.ReadFull(file, buf)
		// This is expected when we hit the end of the file
		if err == io.ErrUnexpectedEOF || err == io.EOF {
			shouldStop = true
		} else if err != nil {
			log.Fatalf("Error on read: %s", err)
		}

		fmt.Println(formatRecord(readLen, addr, 0x0, buf[:readLen]))
		addr += readLen

		// Handle more than 16 bits by emitting a new segment record
		if addr > twoToThe16th - 1 {
			log.Errorf("%d", addr)
			addr -= twoToThe16th - 1
			segment++

			segmentBytes := make([]byte, 2)
			binary.BigEndian.PutUint16(segmentBytes, uint16(segment))
			fmt.Println(formatRecord(0x02, 0x00, 0x02, segmentBytes))
		}
	}

	fmt.Printf(":00000001FF\n")
}
