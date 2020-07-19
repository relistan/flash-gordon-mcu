package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"math"
	"os"

	log "github.com/sirupsen/logrus"
	"gopkg.in/alecthomas/kingpin.v2"
)

const (
	// BytesPerLine is the number of binary bytes we'll encode per line of the
	// hex file.
	BytesPerLine = 32
)

var (
	// twoToThe16th is used in checking address lengths
	twoToThe16th = int(math.Pow(2, 16))
)

type Config struct {
	BaseAddr  *int
	InputFile *string
}

// checksumFor calculates the checksum byte for the byteslice we pass in
func checksumFor(record []byte) byte {
	var sum byte
	for _, j := range record {
		sum += j
	}

	return (sum ^ 0xFF) + 1
}

// formatRecord prints out a record in the correctly encoded format.
func formatRecord(addr int, recType byte, rec []byte) string {
	// Get first byte of rec len (it's BytesPerLine or less) and pad with space for
	// the 16bit addr as well. We'll overwrite those 0x0s with addr. Follow
	// with the rest of the record.
	allBytes := append([]byte{byte(len(rec)), 0x0, 0x0, recType}, rec...)

	binary.BigEndian.PutUint16(allBytes[1:], uint16(addr))
	checkSum := checksumFor(allBytes)

	return fmt.Sprintf(":%02X%02X", allBytes, checkSum)
}

// parseConfig parse the command line using Kingpin and returns a Config
// struct that has been populated.
func parseConfig() *Config {
	config := &Config{
		InputFile: kingpin.Arg("input-file", "The file to take input from").String(),
		BaseAddr: kingpin.Arg("base-addr", "Base Address to use as starting address").Default("0").Int(),
	}
	kingpin.Parse()
	return config
}

func main() {
	config := parseConfig()

	var (
		file *os.File
		err error
	)

	// If we got a filename use it. Otherwise use stdin.
	if *config.InputFile == "" {
		file = os.Stdin
	} else {
		file, err = os.Open(*config.InputFile)
		if err != nil {
			log.Fatalf("Unable to open %s: %s", *config.InputFile, err)
		}
	}

	var (
		addr       int = *config.BaseAddr
		segment    int
		shouldStop bool
		buf        []byte = make([]byte, BytesPerLine)
	)

	// Starting address record
	fmt.Println(formatRecord(0x0, 0x04, []byte{0x0, 0x0}))

	for !shouldStop {
		readLen, err := io.ReadFull(file, buf)
		// This is expected when we hit the end of the file
		if err == io.ErrUnexpectedEOF || err == io.EOF {
			shouldStop = true
		} else if err != nil {
			log.Fatalf("Error on read: %s", err)
		}

		fmt.Println(formatRecord(addr, 0x0, buf[:readLen]))
		addr += readLen

		// Handle more than 16 bits by emitting a new segment record when
		// we have an address bigger than 16bits.
		if addr > twoToThe16th-1 {
			addr -= twoToThe16th - 1
			segment++

			segmentBytes := make([]byte, 2)
			binary.BigEndian.PutUint16(segmentBytes, uint16(segment))
			fmt.Println(formatRecord(0x00, 0x02, segmentBytes))
		}
	}

	// EOF Record
	fmt.Printf(":00000001FF\n")
}
