/** @file mesh.ino
 *  @brief Demonstrates mesh networking and XBee triangulation.
 *
 *  This file contains the main setup and program loop along with helper
 *  functions.
 *
 *  IMPORTANT: The program must read packets faster than it is receiving them or
 *             it may lose packets. Stated otherwise, the program must Rx at a
 *             rate (ex. every 50ms) faster than another instance is
 *             transferring (ex. every 100ms).
 *
 *  @author Russell Taber (rwtaber)
 *  @modified 5/27/15
 *  @modified by Russell Taber (rwtaber)
 *  @TODO Complete triangulation distance
 *        Add rx data size to the beginning of rx message.
 **/
#include <SoftwareSerial.h>

/* XBee Arduino library from here: http://code.google.com/p/xbee-arduino/
Located in working directory instead of main Arduino library directory for
easier maintenance. */
#include "XBee.h"

/* List of XBee names and associated MAC addresses. */
#include "macs.h"

/* Initialize XBee library object. */
XBee xbee = XBee();

//Set software serial when music shield is in the way
SoftwareSerial XBeeSerial(8, 9); // RX, TX

/* Union created to access SH & SL as byte array and an int. */
typedef union {
  uint32_t uint;
  byte array[4];
} byteAndInt;

/* Global Serial High (SH) of *this* XBee */
byteAndInt sh;
/* Global Serial Low (SL) of *this* XBee */
byteAndInt sl;

void setup() {

  Serial.begin(9600);
  XBeeSerial.begin(9600);
  /* Set which serial port the library will use. */
  xbee.setSerial(XBeeSerial);

  /* AT commands for SH and SL. AT commands are two chars stored in a 2 byte
  array. The temp variable stores the the AT command result (a byte array).
  The setup will infinite loop if the AT commands fail! */

  byte shcmd[] = {'S', 'H'};
  byte slcmd[] = {'S', 'L'};
  byte *temp;

  // Keep executing the SH AT command until it is successful.
  while(1) {
    temp = AtCommand(shcmd);
    if (temp != NULL) {            // Wait for success.
      for(int i = 0; i < 4; i++) { // SH is 4 bytes long.
        sh.array[3-i] = temp[i];   // Store the SH in reverse order on account
      }                            // of byte endianness.
      break;
    }
  }

  // Same as above but for the SL.
  while(1) {
    temp = AtCommand(slcmd);
    if (temp != NULL) {
      for(int i = 0; i < 4; i++) {
        sl.array[3-i] = temp[i];
      }
      break;
    }
  }

  Serial.println();
}

void loop() {

  // EXAMPLE: Sending message to router1. Must be a byte array.
  //byte message[] = {'h', 'e', 'l', 'l', 'o', '\0'};
  //tx(message, sizeof(message), router1);
  //delay(100);

  /* EXAMPLE: Receiving a message. */

  byte *message = rx();
  while(1) {
    if (message != NULL) { // Check is message was actually received.
      int datasize = message[0] | message[1] << 8;
      Serial.println(datasize);
      for(int i = 2; i < datasize; i++) {
        //Serial.write(message[i]);
      }
    }

    free(message); // MUST free the message memory.
    break;
  }
  delay(50);



  /*
  byte *message = rx();
  while(1) {
    if (message != NULL) { // Check is message was actually received.
      Serial.println((char *) message); // Print the message to the console.
      free(message); // MUST free the message memory.
      break;
    }
    delay(50);
  }
*/
  //         |R1|
  //        /  | \
  // dC_R1 /   |  \ dR2_R1
  //      /    |   \
  //     |C|-------|R2|
  //      *  dC_R2

  //int rssi_dC_R1 = (int) RemoteAtCommand(atcmd, router1)[0];
  //delay(50);
  //int rssi_dC_R2 = (int) RemoteAtCommand(atcmd, router2)[0];
  //delay(50);
  //int rssi dR2_R1 = (int) DistanceFrom(
  //delay(100);

  /*
  byte atcmd[] = {'D', 'B'};

  Serial.print("R1 RSSI: ");
  Serial.println((int) RemoteAtCommand(atcmd, router1)[0]);
  Serial.println();
  Serial.print("R2 RSSI: ");
  Serial.println((int) RemoteAtCommand(atcmd, router2)[0]);
  Serial.println();
  */
}

// Takes a byte array, the byte array size, and the address (or name) of the
// XBee to transfer to.
void tx(byte *message, byte msg_size, XBeeAddress64 destination) {
  // Create a request packet
  ZBTxRequest zbTx = ZBTxRequest(destination, message, msg_size);
  // Create a response
  ZBTxStatusResponse txStatus = ZBTxStatusResponse();

  //Send the packet here!
  xbee.send(zbTx);

  // Response timeout after 500ms
  if (xbee.readPacket(500)) {
    // Look specifically for a status response
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);

      if (txStatus.getDeliveryStatus() == SUCCESS) {
        Serial.println(F("PACKET RECEIVED"));
      } else {
        Serial.println(F("PACKET NOT RECEIVED"));
      }
    }
  // Error occurred reading the packet.
  } else if (xbee.getResponse().isError()) {
    Serial.print(F("ERROR READING PACKET, ERROR CODE: "));
    Serial.println(xbee.getResponse().getErrorCode());
  // No packet to read
  } else {
    Serial.println(F("TX RESPONSE NOT RECEIVED"));
  }
}

// Returns either NULL or a pointer to a byte array.
// The first two bytes correspond to the remaining size
// of the array.
byte *rx() {
  // Response packet
  ZBRxResponse zbRx = ZBRxResponse();
  // Modem status response packet. Returns whether the XBee got associated
  // Happens when a router or endpoint joins the coordinators network
  ModemStatusResponse msr = ModemStatusResponse();
  // Read the packet from the buffer
  xbee.readPacket();
  // Allocate memory for the received message
  byte *message;
  message = (byte *) malloc(zbRx.getDataLength() + 2);

  // Check if Rx packet is well formed.
  if (xbee.getResponse().isAvailable()) {
    // Check if Rx packet is data.
    if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
      xbee.getResponse().getZBRxResponse(zbRx);

      message[0] = zbRx.getDataLength() & 0xFF;
      message[1] = (zbRx.getDataLength()>>8) & 0xFF;

      for (int i = 2; i < zbRx.getDataLength() + 2; i++) {
          message [i] = zbRx.getData(i);
      }

      if (zbRx.getOption() == ZB_PACKET_ACKNOWLEDGED) {
        //Serial.println(F("ACK RECIEVED"));
      } else {
        //Serial.println(F("ACK NOT RECIEVED"));
      }

      return message;
    // Check if Rx packet is modem status response.
    } else if (xbee.getResponse().getApiId() == MODEM_STATUS_RESPONSE) {
      xbee.getResponse().getModemStatusResponse(msr);

      if (msr.getStatus() == ASSOCIATED) {
        Serial.println(F("ASSOCIATED"));
      } else if (msr.getStatus() == DISASSOCIATED) {
        Serial.println(F("DISASSOCIATED"));
      } else {
        Serial.println(F("ASSOCIATION ERROR?"));
      }
    // Some other well formed Rx packet not being dealt with
    } else {
      Serial.print(F("ERROR UNEXPECTED PACKET, GOT: "));
      Serial.println(xbee.getResponse().getApiId(), HEX);
    }
  // Malformed Rx packet
  } else {
    //Serial.print(F("ERROR READING PACKET, ERROR CODE: "));
    //Serial.println(xbee.getResponse().getErrorCode());
  }

  // Remember to free the message memory if not returned!
  free(message);
  return NULL;
}

// Executes an AT command on the current XBee.
byte *AtCommand(byte *atcmd) {
  // Set up the AT command request packet with the given AT command
  AtCommandRequest atCmdReq = AtCommandRequest(atcmd);
  // Response of the AT command
  AtCommandResponse atResp = AtCommandResponse();

  // Send the AT command
  xbee.send(atCmdReq);

  // Response timeout after 5 seconds
  if (xbee.readPacket(5000)) {
    // Look for expected, well formed, AT command response. 
    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE) {
      xbee.getResponse().getAtCommandResponse(atResp);

      if (atResp.isOk()) {
        return atResp.getValue();
      } else {
        Serial.print(F("ERROR EXPECTED AT RESPONSE, GOT: "));
        Serial.println(xbee.getResponse().getApiId(), HEX);
      }
    /* Well formed response, but not expected */
    } else {
      Serial.print(F("ERROR UNEXPECTED PACKET, GOT: "));
      Serial.println(xbee.getResponse().getApiId(), HEX);
    }
  /* Malformed response */
  } else {
    Serial.print(F("ERROR READING PACKET, ERROR CODE: "));
    Serial.println(xbee.getResponse().getErrorCode());
  }

  return NULL;
}

byte *RemoteAtCommand(byte *atcmd, XBeeAddress64 destination) {
  RemoteAtCommandRequest atCmdReq = RemoteAtCommandRequest(destination, atcmd);
  RemoteAtCommandResponse atResp = RemoteAtCommandResponse();

  xbee.send(atCmdReq);

  if (xbee.readPacket(5000)) {
    if (xbee.getResponse().getApiId() == REMOTE_AT_COMMAND_RESPONSE) {
      xbee.getResponse().getAtCommandResponse(atResp);

      if (atResp.isOk()) {
        return atResp.getValue();
      } else {
        Serial.print(F("ERROR EXPECTED REMOTE AT RESPONSE, GOT: "));
        Serial.println(xbee.getResponse().getApiId(), HEX);
      }
    } else {
      Serial.print(F("ERROR UNEXPECTED PACKET, GOT: "));
      Serial.println(xbee.getResponse().getApiId(), HEX);
    }
  } else {
    Serial.print(F("ERROR READING PACKET, ERROR CODE: "));
    Serial.println(xbee.getResponse().getErrorCode());
  }

  return NULL;
}

int GetRSSITo(XBeeAddress64 to, XBeeAddress64 from) {
  byte atcmd[] = {'D', 'B'};
  if (from.getMsb() == sh.uint && from.getLsb() == sl.uint) {
    return (int) RemoteAtCommand(atcmd, from)[0];
  } else if (to.getMsb() == sh.uint && to.getLsb() == sl.uint) {
    return (int) RemoteAtCommand(atcmd, to)[0];
  } else {
  }
}

float distance(int rssi, float R, float ple) {
  return pow(10.0,((rssi - R)/(10.0*ple)));
}
