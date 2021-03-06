/*
 * sf_gps_settings.c
 *
 * Authors: Tristan Lennertz and Alex Grazela
 */


#include "sf_coms.h"
#include "sf_gps_settings.h"
#include <stdio.h>

/* From the Data Sheet:
0 NMEA_SEN_GLL, // GPGLL interval - Geographic Position - Latitude longitude
1 NMEA_SEN_RMC, // GPRMC interval - Recommended Minimum Specific GNSS Sentence
2 NMEA_SEN_VTG, // GPVTG interval - Course over Ground and Ground Speed
3 NMEA_SEN_GGA, // GPGGA interval - GPS Fix Data
4 NMEA_SEN_GSA, // GPGSA interval - GNSS DOPS and Active Satellites
5 NMEA_SEN_GSV, // GPGSV interval - GNSS Satellites in View
6 //Reserved
7 //Reserved
13 //Reserved
14 //Reserved
15 //Reserved
16 //Reserved
17 //Reserved
18 NMEA_SEN_MCHN, // PMTKCHN interval � GPS channel status

*/

/*
The Datasheet for commands to send to this memer is availble at https://cdn-shop.adafruit.com/datasheets/PMTK_A11.pdf
Also note, I'm not sure if the <CR><LF> is supposed to be two certain bytes or something?
Another note, the GPS is always spitting out it's data without the need to query it, or is that not right?
*/

#define messagePreambleLength 9
#define baudRateExtraCharacters 13
#define StartMessageLength 20
#define nmeaRateMessageLength 58
#define checksumLength 3
#define nmeaRateReservedLength 20
#define zyboFIFOBufferLength 64

//This should restart it to full factory default, clearing anything we have set
void sf_gps_full_cold_start() {
    char fullColdStartMessage[StartMessageLength];
    sprintf(fullColdStartMessage, "$PMTK104*37\r\n");
    sf_uart_send((u8 *)fullColdStartMessage, StartMessageLength);
}

void sf_gps_cold_start() {
    char coldStartMessage[StartMessageLength];
    sprintf(coldStartMessage, "$PMTK103*30\r\n");
    sf_uart_send((u8 *)coldStartMessage, StartMessageLength);
}

void sf_gps_warm_start() {
    char warmStartMessage[StartMessageLength];
    sprintf(warmStartMessage, "$PMTK102*31\r\n");
    sf_uart_send((u8 *)warmStartMessage, StartMessageLength);
}

void sf_gps_hot_start() {
    char hotStartMessage[StartMessageLength];
    sprintf(hotStartMessage, "$PMTK101*32\r\n");
    sf_uart_send((u8 *)hotStartMessage, StartMessageLength);
}



void sf_gps_set_baud(u32 baudRate, u8 baudByteLen) {
    char baudRateBuf[baudByteLen];
    char baudRateMessage[messagePreambleLength + baudByteLen + baudRateExtraCharacters];
    char preamble[messagePreambleLength + 1];
    char checksumBuf[checksumLength];
    
    sprintf(baudRateBuf, "%ld", baudRate);
    sprintf(preamble, "$PMKT251,");

    sprintf(baudRateMessage, "%s%s*", preamble, baudRateBuf);
    sf_gps_checksum_calc(checksumBuf, baudRateMessage);
    sprintf(baudRateMessage, "%s\r\n", checksumBuf);
    
    sf_uart_send((u8 *)baudRateMessage, baudByteLen + messagePreambleLength + baudRateExtraCharacters);
}

void sf_gps_set_fix_interval(u32 interval, u32 intervalByteLength) {
    char intervalBuf[intervalByteLength];
    char preamble[messagePreambleLength + 1];
    char intervalMessage[intervalByteLength + messagePreambleLength + 8];
    char checksumBuf[2];
    
    sprintf(intervalBuf, "%ld", interval);
    sprintf(preamble, "$PMKT220,");
    sprintf(intervalMessage, "%s%s*%s\r\n", preamble, intervalBuf, checksumBuf);
    
    //This 4 comes from the comma, the *, and 2 from <CR><LF>
    sf_uart_send((u8 *)intervalMessage, messagePreambleLength + intervalByteLength + checksumLength + 4);
}

//This will construct and send a message that sets each valid nmea sentence recieve rate based on 1 - 5 fixes
//Of course this means that each u8 passed in should only be in values 0 (invalid) or 1 - 5
//nmea Rate example $PMTK314,1,1,1,1,1,5,0,0,0,0,0,0,0,0,0,0,0,0,0*2C<CR><LF>

void sf_gps_set_nmea_rate(u8 GLL, u8 RMC, u8 VTG, u8 GGA, u8 GSA, u8 GSV) {
    char nmeaRateMessage[nmeaRateMessageLength];
    char GLLBuf[2];
    char RMCBuf[2];
    char VTGBuf[2];
    char GSABuf[2];
    char GSVBuf[2];
    char CRLFBuf[2];
    char GGABuf[2];
    char checksumBuf[2];
    char ReservedBuf[25];
    char preamble[messagePreambleLength];
    
    sprintf(CRLFBuf, "\r\n");
    sprintf(GLLBuf, "%c", GLL);
    sprintf(RMCBuf, "%c", RMC);
    sprintf(VTGBuf, "%c", VTG);
    sprintf(GSABuf, "%c", GSA);
    sprintf(GSVBuf, "%c", GSV);
    sprintf(GGABuf, "%c", GGA);
        
    sprintf(GLLBuf + 1, ",");
    sprintf(RMCBuf + 1, ",");
    sprintf(RMCBuf + 1, ",");
    sprintf(VTGBuf + 1, ",");
    sprintf(GSABuf + 1, ",");
    sprintf(GSVBuf + 1, ",");
    sprintf(GGABuf + 1, ",");

    sprintf(ReservedBuf, "0,0,0,0,0,0,0,0,0,0,0,0,0");
    sprintf(preamble, "$PMTK314,");
    
    sprintf(nmeaRateMessage, "%s%s%s%s%s%s%s%s*", preamble, GLLBuf, RMCBuf, VTGBuf, GGABuf, GSABuf, GSVBuf, ReservedBuf);
    sf_gps_checksum_calc(checksumBuf, nmeaRateMessage);

    sprintf(nmeaRateMessage + (messagePreambleLength + 10 + nmeaRateReservedLength), "%s%s", checksumBuf, CRLFBuf); 

    sf_uart_send((u8 *)nmeaRateMessage, nmeaRateMessageLength);
}

void sf_gps_checksum_calc(char *checksumBuf, char *messageString) {
   char *next = messageString + 2;
   u8 checksum = messageString[1];
   
   while (*next != '*') {
      checksum ^= (*next);
      next++;
   }

   sprintf(checksumBuf, "%*X", 2, checksum);
}

//In the case of an NMEA sentence, it will end with *<checksum>
//the ack ends with *<checksum>\r\n
void sf_gps_recieve(char *readBuf, u32 numBytes) {
   sf_uart_receive((u8 *)readBuf, numBytes);
}
