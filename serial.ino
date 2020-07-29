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
      Serial.println("ERROR: line too long!");
      return buf;
    }
   
    buf[count] = readByte;
    count++;
  }

  return &buf[0];
}

void processSerialCmd() {
  Serial.printf("\nEnter Command: ");
  Serial.setTimeout(120000);
  
  char readByte;
  Serial.readBytes(&readByte, 1);
  
  switch(readByte) {
  case 'u':
    processHexFile();
    break;
  case 'e':
    Serial.println("Chip erase...");
    chipErase();
    delay(2000);
    break;
  case 'd':
    Serial.println("Dumping data...");
    dumpAll(0, 5918);
    break;
  }
}
