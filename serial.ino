char *serialReadLine(char *buf) {
  char readByte;
  byte count = 0;
  
  while (1) { 
    if(Serial.readBytes(&readByte, 1) == 0) {
      return NULL;
    }
    
    // XXX do we need to handle \r?
    switch(readByte) {
    case '\n':
      buf[count] = '\0';
      return buf;
    case '\0':
      return buf;
    }

    if(count >= BUFFER_SIZE) {
      Serial.printf("ERROR: line too long! Buffer size '%d', got, '%d'", BUFFER_SIZE, count);
      return buf;
    }
   
    buf[count] = readByte;
    count++;
  }

  return &buf[0];
}

void processSerialCmd() {
  Serial.printf("\n>");
  Serial.setTimeout(120000);
  
  char readByte;
  char buf[8];
  uint32_t baseAddr, len = 0;

  Serial.readBytes(&readByte, 1);
  
  switch(readByte) {
  case 'u':
    Serial.readBytes(&readByte, 1);
    switch(readByte) {
      case 'f':
        processHexFile('f');
        break;
      case 'e':
        processHexFile('e');
        break;
      default:
        Serial.printf("ERROR: unknown chip type '%c'", readByte);
        break;
    }
    break;
  case 'e':
    Serial.println("Chip erase...");
    chipErase();
    delay(2000);
    break;
  case 'd':
    Serial.readBytes(buf, 8);
    baseAddr = hexToUint32(buf);
    Serial.readBytes(buf, 8);
    len = hexToUint32(buf);    
    dumpAll(baseAddr, len);
    break;
  }
}
