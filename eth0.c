// ETH0 Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <eth0.h>
#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "wait.h"
#include "gpio.h"
#include "spi0.h"
#include "eeprom.h"
#include "str.h"

// Pins
#define CS PORTA,3
#define WOL PORTB,3
#define INT PORTC,6

// Ether registers
#define ERDPTL      0x00
#define ERDPTH      0x01
#define EWRPTL      0x02
#define EWRPTH      0x03
#define ETXSTL      0x04
#define ETXSTH      0x05
#define ETXNDL      0x06
#define ETXNDH      0x07
#define ERXSTL      0x08
#define ERXSTH      0x09
#define ERXNDL      0x0A
#define ERXNDH      0x0B
#define ERXRDPTL    0x0C
#define ERXRDPTH    0x0D
#define ERXWRPTL    0x0E
#define ERXWRPTH    0x0F
#define EIE         0x1B
#define EIR         0x1C
#define RXERIF  0x01
#define TXERIF  0x02
#define TXIF    0x08
#define PKTIF   0x40
#define ESTAT       0x1D
#define CLKRDY  0x01
#define TXABORT 0x02
#define ECON2       0x1E
#define PKTDEC  0x40
#define ECON1       0x1F
#define RXEN    0x04
#define TXRTS   0x08
#define ERXFCON     0x38
#define EPKTCNT     0x39
#define MACON1      0x40
#define MARXEN  0x01
#define RXPAUS  0x04
#define TXPAUS  0x08
#define MACON2      0x41
#define MARST   0x80
#define MACON3      0x42
#define FULDPX  0x01
#define FRMLNEN 0x02
#define TXCRCEN 0x10
#define PAD60   0x20
#define MACON4      0x43
#define MABBIPG     0x44
#define MAIPGL      0x46
#define MAIPGH      0x47
#define MACLCON1    0x48
#define MACLCON2    0x49
#define MAMXFLL     0x4A
#define MAMXFLH     0x4B
#define MICMD       0x52
#define MIIRD   0x01
#define MIREGADR    0x54
#define MIWRL       0x56
#define MIWRH       0x57
#define MIRDL       0x58
#define MIRDH       0x59
#define MAADR1      0x60
#define MAADR0      0x61
#define MAADR3      0x62
#define MAADR2      0x63
#define MAADR5      0x64
#define MAADR4      0x65
#define MISTAT      0x6A
#define MIBUSY  0x01
#define ECOCON      0x75

// Ether phy registers
#define PHCON1      0x00
#define PDPXMD 0x0100
#define PHSTAT1     0x01
#define LSTAT  0x0400
#define PHCON2      0x10
#define HDLDIS 0x0100
#define PHLCON      0x14

// Packets
#define IP_ADD_LENGTH 4
#define HW_ADD_LENGTH 6


// Ethernet defines and globals
#define IPv4_frame 0x0800
uint8_t macDhcpServer[HW_ADD_LENGTH] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
const uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// IP defines and globals
uint8_t no_ip[]        = {0, 0, 0, 0};
uint8_t ip_udp         = 0x11;
uint8_t ip_tcp         = 0x06;

// dhcp defines and globals

#define TEN_Mb_ETHERNET 1
#define SIX_BYTES 6
#define FIRST_BLOCK_NO_OFFSET 0x0000
#define DHCP_ENABLED  0xFFFFFFFF
#define DHCP_DISABLED 0x0F0F0F0F
#define SN_MASK_CODE 1
#define GW_CODE      3
#define DNS_CODE     6
#define REQ_IP_MSG 50
#define IP_LEASE_CODE 51
#define DHCPMESSAGE 53
#define SERVERID 54
#define PARAMETER_REQUEST 55
#define END 255

//
// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
uint8_t nextPacketLsb = 0x00;
uint8_t nextPacketMsb = 0x00;
uint8_t sequenceId = 1;
uint32_t sum;
uint8_t macAddress[HW_ADD_LENGTH] = {2,3,4,5,6,7};
uint8_t ipAddress[IP_ADD_LENGTH] = {0,0,0,0};
uint8_t ipSubnetMask[IP_ADD_LENGTH] = {255,255,255,0};
uint8_t ipGwAddress[IP_ADD_LENGTH] = {0,0,0,0};
uint8_t ipDnsServer[IP_ADD_LENGTH] = {0, 0, 0, 0};
uint8_t ipDhcpServer[IP_ADD_LENGTH] = {0, 0, 0, 0};
uint32_t transaction_id = 0x10101010; //can be randomly generated; constant is chosen
uint8_t siaddr[4] = {0,0,0,0};
uint8_t yiaddr[4] = {0,0,0,0};
bool si_yi_clear = true;
uint32_t lease_time;
uint16_t port_num = 5;
uint8_t tcp_flags;
uint32_t seq_num = 0;
uint32_t ack_num = 0;

char telnet_command[80];
bool command_pending = false;
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

// This M4F is little endian (TI hardwired it this way)
// Network byte order is big endian
// Must interpret uint16_t in reverse order

typedef struct _enc28j60Frame // 4-bytes
{
  uint16_t size;
  uint16_t status;
  uint8_t data;
} enc28j60Frame;

typedef struct _etherFrame // 14-bytes
{
  uint8_t destAddress[6];
  uint8_t sourceAddress[6];
  uint16_t frameType;
  uint8_t data;
} etherFrame;

typedef struct _ipFrame // minimum 20 bytes
{
  uint8_t revSize;
  uint8_t typeOfService;
  uint16_t length;
  uint16_t id;
  uint16_t flagsAndOffset;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t headerChecksum;
  uint8_t sourceIp[4];
  uint8_t destIp[4];
} ipFrame;

typedef struct _icmpFrame
{
  uint8_t type;
  uint8_t code;
  uint16_t check;
  uint16_t id;
  uint16_t seq_no;
  uint8_t data;
} icmpFrame;

typedef struct _arpFrame
{
  uint16_t hardwareType;
  uint16_t protocolType;
  uint8_t hardwareSize;
  uint8_t protocolSize;
  uint16_t op;
  uint8_t sourceAddress[6];
  uint8_t sourceIp[4];
  uint8_t destAddress[6];
  uint8_t destIp[4];
} arpFrame;

typedef struct _udpFrame // 8 bytes
{
  uint16_t sourcePort;
  uint16_t destPort;
  uint16_t length;
  uint16_t check;
  uint8_t  data;
} udpFrame;
typedef struct _dhcpFrame
{
  uint8_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;
  uint32_t  xid;
  uint16_t secs;
  uint16_t flags;
  uint8_t ciaddr[4];
  uint8_t yiaddr[4];
  uint8_t siaddr[4];
  uint8_t giaddr[4];
  uint8_t chaddr[16];
  uint8_t data[192];
  uint32_t magicCookie;
  uint8_t options [0];
} dhcpFrame;
typedef struct _tcpFrame //20 bytes
{
    uint16_t sourcePort;
    uint16_t destPort;
    uint32_t sequenceNum;
    uint32_t ackNum;
    uint16_t offsetAndFlags;
    uint16_t windowSize;
    uint16_t check;
    uint16_t urgentPointer;
    uint8_t  optionsPaddingData[0];

}tcpFrame;
const uint8_t dhcpSize = sizeof(dhcpFrame);
const uint8_t ipHeaderLength = 20;
const uint8_t  udpHeaderLength = 8;
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Buffer is configured as follows
// Receive buffer starts at 0x0000 (bottom 6666 bytes of 8K space)
// Transmit buffer at 01A0A (top 1526 bytes of 8K space)

void etherCsOn()
{
    setPinValue(CS, 0);
    __asm (" NOP");                    // allow line to settle
    __asm (" NOP");
    __asm (" NOP");
    __asm (" NOP");
}

void etherCsOff()
{
    setPinValue(CS, 1);
}

void etherWriteReg(uint8_t reg, uint8_t data)
{
    etherCsOn();
    writeSpi0Data(0x40 | (reg & 0x1F));
    readSpi0Data();
    writeSpi0Data(data);
    readSpi0Data();
    etherCsOff();
}

uint8_t etherReadReg(uint8_t reg)
{
    uint8_t data;
    etherCsOn();
    writeSpi0Data(0x00 | (reg & 0x1F));
    readSpi0Data();
    writeSpi0Data(0);
    data = readSpi0Data();
    etherCsOff();
    return data;
}

void etherSetReg(uint8_t reg, uint8_t mask)
{
    etherCsOn();
    writeSpi0Data(0x80 | (reg & 0x1F));
    readSpi0Data();
    writeSpi0Data(mask);
    readSpi0Data();
    etherCsOff();
}

void etherClearReg(uint8_t reg, uint8_t mask)
{
    etherCsOn();
    writeSpi0Data(0xA0 | (reg & 0x1F));
    readSpi0Data();
    writeSpi0Data(mask);
    readSpi0Data();
    etherCsOff();
}

void etherSetBank(uint8_t reg)
{
    etherClearReg(ECON1, 0x03);
    etherSetReg(ECON1, reg >> 5);
}

void etherWritePhy(uint8_t reg, uint16_t data)
{
    etherSetBank(MIREGADR);
    etherWriteReg(MIREGADR, reg);
    etherWriteReg(MIWRL, data & 0xFF);
    etherWriteReg(MIWRH, (data >> 8) & 0xFF);
}

uint16_t etherReadPhy(uint8_t reg)
{
    uint16_t data, dataH;
    etherSetBank(MIREGADR);
    etherWriteReg(MIREGADR, reg);
    etherWriteReg(MICMD, MIIRD);
    waitMicrosecond(11);
    etherSetBank(MISTAT);
    while ((etherReadReg(MISTAT) & MIBUSY) != 0);
    etherSetBank(MICMD);
    etherWriteReg(MICMD, 0);
    data = etherReadReg(MIRDL);
    dataH = etherReadReg(MIRDH);
    data |= (dataH << 8);
    return data;
}

void etherWriteMemStart()
{
    etherCsOn();
    writeSpi0Data(0x7A);
    readSpi0Data();
}

void etherWriteMem(uint8_t data)
{
    writeSpi0Data(data);
    readSpi0Data();
}

void etherWriteMemStop()
{
    etherCsOff();
}

void etherReadMemStart()
{
    etherCsOn();
    writeSpi0Data(0x3A);
    readSpi0Data();
}

uint8_t etherReadMem()
{
    writeSpi0Data(0);
    return readSpi0Data();
}

void etherReadMemStop()
{
    etherCsOff();
}

// Initializes ethernet device
// Uses order suggested in Chapter 6 of datasheet except 6.4 OST which is first here
void etherInit(uint16_t mode)
{
    // Initialize SPI0
    initSpi0(USE_SSI0_RX);
    setSpi0BaudRate(4e6, 40e6);
    setSpi0Mode(0, 0);

    // Enable clocks
    enablePort(PORTA);
    enablePort(PORTB);
    enablePort(PORTC);

    // Configure pins for ethernet module
    selectPinPushPullOutput(CS);
    selectPinDigitalInput(WOL);
    selectPinDigitalInput(INT);

    // make sure that oscillator start-up timer has expired
    while ((etherReadReg(ESTAT) & CLKRDY) == 0) {}

    // disable transmission and reception of packets
    etherClearReg(ECON1, RXEN);
    etherClearReg(ECON1, TXRTS);

    // initialize receive buffer space
    etherSetBank(ERXSTL);
    etherWriteReg(ERXSTL, LOBYTE(0x0000));
    etherWriteReg(ERXSTH, HIBYTE(0x0000));
    etherWriteReg(ERXNDL, LOBYTE(0x1A09));
    etherWriteReg(ERXNDH, HIBYTE(0x1A09));

    // initialize receiver write and read ptrs
    // at startup, will write from 0 to 1A08 only and will not overwrite rd ptr
    etherWriteReg(ERXWRPTL, LOBYTE(0x0000));
    etherWriteReg(ERXWRPTH, HIBYTE(0x0000));
    etherWriteReg(ERXRDPTL, LOBYTE(0x1A09));
    etherWriteReg(ERXRDPTH, HIBYTE(0x1A09));
    etherWriteReg(ERDPTL, LOBYTE(0x0000));
    etherWriteReg(ERDPTH, HIBYTE(0x0000));

    // setup receive filter
    // always check CRC, use OR mode
    etherSetBank(ERXFCON);
    etherWriteReg(ERXFCON, (mode | ETHER_CHECKCRC) & 0xFF);

    // bring mac out of reset
    etherSetBank(MACON2);
    etherWriteReg(MACON2, 0);

    // enable mac rx, enable pause control for full duplex
    etherWriteReg(MACON1, TXPAUS | RXPAUS | MARXEN);

    // enable padding to 60 bytes (no runt packets)
    // add crc to tx packets, set full or half duplex
    if ((mode & ETHER_FULLDUPLEX) != 0)
        etherWriteReg(MACON3, FULDPX | FRMLNEN | TXCRCEN | PAD60);
    else
        etherWriteReg(MACON3, FRMLNEN | TXCRCEN | PAD60);

    // leave MACON4 as reset

    // set maximum rx packet size
    etherWriteReg(MAMXFLL, LOBYTE(1518));
    etherWriteReg(MAMXFLH, HIBYTE(1518));

    // set back-to-back inter-packet gap to 9.6us
    if ((mode & ETHER_FULLDUPLEX) != 0)
        etherWriteReg(MABBIPG, 0x15);
    else
        etherWriteReg(MABBIPG, 0x12);

    // set non-back-to-back inter-packet gap registers
    etherWriteReg(MAIPGL, 0x12);
    etherWriteReg(MAIPGH, 0x0C);

    // leave collision window MACLCON2 as reset

    // setup mac address
    etherSetBank(MAADR0);
    etherWriteReg(MAADR5, macAddress[0]);
    etherWriteReg(MAADR4, macAddress[1]);
    etherWriteReg(MAADR3, macAddress[2]);
    etherWriteReg(MAADR2, macAddress[3]);
    etherWriteReg(MAADR1, macAddress[4]);
    etherWriteReg(MAADR0, macAddress[5]);

    // initialize phy duplex
    if ((mode & ETHER_FULLDUPLEX) != 0)
        etherWritePhy(PHCON1, PDPXMD);
    else
        etherWritePhy(PHCON1, 0);

    // disable phy loopback if in half-duplex mode
    etherWritePhy(PHCON2, HDLDIS);

    // Flash LEDA and LEDB
    etherWritePhy(PHLCON, 0x0880);
    waitMicrosecond(100000);

    // set LEDA (link status) and LEDB (tx/rx activity)
    // stretch LED on to 40ms (default)
    etherWritePhy(PHLCON, 0x0472);
    // enable reception
    etherSetReg(ECON1, RXEN);
}

// Returns true if link is up
bool etherIsLinkUp()
{
    return (etherReadPhy(PHSTAT1) & LSTAT) != 0;
}

// Returns TRUE if packet received
bool etherIsDataAvailable()
{
    return ((etherReadReg(EIR) & PKTIF) != 0);
}

// Returns true if rx buffer overflowed after correcting the problem
bool etherIsOverflow()
{
    bool err;
    err = (etherReadReg(EIR) & RXERIF) != 0;
    if (err)
        etherClearReg(EIR, RXERIF);
    return err;
}

// Returns up to max_size characters in data buffer
// Returns number of bytes copied to buffer
// Contents written are 16-bit size, 16-bit status, payload excl crc
uint16_t etherGetPacket(uint8_t packet[], uint16_t maxSize)
{
    uint16_t i = 0, size, tmp16, status;

    // enable read from FIFO buffers
    etherReadMemStart();

    // get next packet information
    nextPacketLsb = etherReadMem();
    nextPacketMsb = etherReadMem();

    // calc size
    // don't return crc, instead return size + status, so size is correct
    size = etherReadMem();
    tmp16 = etherReadMem();
    size |= (tmp16 << 8);

    // get status (currently unused)
    status = etherReadMem();
    tmp16 = etherReadMem();
    status |= (tmp16 << 8);

    // copy data
    if (size > maxSize)
        size = maxSize;
    while (i < size)
        packet[i++] = etherReadMem();

    // end read from FIFO buffers
    etherReadMemStop();

    // advance read pointer
    etherSetBank(ERXRDPTL);
    etherWriteReg(ERXRDPTL, nextPacketLsb); // hw ptr
    etherWriteReg(ERXRDPTH, nextPacketMsb);
    etherWriteReg(ERDPTL, nextPacketLsb);   // dma rd ptr
    etherWriteReg(ERDPTH, nextPacketMsb);

    // decrement packet counter so that PKTIF is maintained correctly
    etherSetReg(ECON2, PKTDEC);

    return size;
}

// Writes a packet
bool etherPutPacket(uint8_t packet[], uint16_t size)
{
    uint16_t i;

    // clear out any tx errors
    if ((etherReadReg(EIR) & TXERIF) != 0)
    {
        etherClearReg(EIR, TXERIF);
        etherSetReg(ECON1, TXRTS);
        etherClearReg(ECON1, TXRTS);
    }

    // set DMA start address
    etherSetBank(EWRPTL);
    etherWriteReg(EWRPTL, LOBYTE(0x1A0A));
    etherWriteReg(EWRPTH, HIBYTE(0x1A0A));

    // start FIFO buffer write
    etherWriteMemStart();

    // write control byte
    etherWriteMem(0);

    // write data
    for (i = 0; i < size; i++)
        etherWriteMem(packet[i]);

    // stop write
    etherWriteMemStop();

    // request transmit
    etherWriteReg(ETXSTL, LOBYTE(0x1A0A));
    etherWriteReg(ETXSTH, HIBYTE(0x1A0A));
    etherWriteReg(ETXNDL, LOBYTE(0x1A0A+size));
    etherWriteReg(ETXNDH, HIBYTE(0x1A0A+size));
    etherClearReg(EIR, TXIF);
    etherSetReg(ECON1, TXRTS);

    // wait for completion
    while ((etherReadReg(ECON1) & TXRTS) != 0);

    // determine success
    return ((etherReadReg(ESTAT) & TXABORT) == 0);
}

// Calculate sum of words
// Must use getEtherChecksum to complete 1's compliment addition
void etherSumWords(void* data, uint16_t sizeInBytes)
{
	uint8_t* pData = (uint8_t*)data;
    uint16_t i;
    uint8_t phase = 0;
    uint16_t data_temp;
    for (i = 0; i < sizeInBytes; i++)
    {
        if (phase)
        {
            data_temp = *pData;
            sum += data_temp << 8;
        }
        else
          sum += *pData;
        phase = 1 - phase;
        pData++;
    }
}

// Completes 1's compliment addition by folding carries back into field
uint16_t getEtherChecksum()
{
    uint16_t result;
    // this is based on rfc1071
    while ((sum >> 16) > 0)
      sum = (sum & 0xFFFF) + (sum >> 16);
    result = sum & 0xFFFF;
    return ~result;
}

void etherCalcIpChecksum(ipFrame* ip)
{
    // 32-bit sum over ip header
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
}

// Converts from host to network order and vice versa
uint16_t htons(const uint16_t value)
{
    return ((value & 0xFF00) >> 8) + ((value & 0x00FF) << 8);
}
#define ntohs htons

// Determines whether packet is IP datagram
bool etherIsIp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    bool ok;
    ok = (ether->frameType == htons(0x0800));
    if (ok)
    {
        sum = 0;
        etherSumWords(&ip->revSize, (ip->revSize & 0xF) * 4);
        ok = (getEtherChecksum() == 0);
    }
    return ok;
}

// Determines whether packet is unicast to this ip
// Must be an IP packet
bool etherIsIpUnicast(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    uint8_t i = 0;
    bool ok = true;
    while (ok & (i < IP_ADD_LENGTH))
    {
        ok = (ip->destIp[i] == ipAddress[i]);
        i++;
    }
    return ok;
}

// Determines whether packet is ping request
// Must be an IP packet
bool etherIsPingRequest(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    icmpFrame* icmp = (icmpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    return (ip->protocol == 0x01 & icmp->type == 8);
}

// Sends a ping response given the request data
void etherSendPingResponse(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    icmpFrame* icmp = (icmpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    uint8_t i, tmp;
    uint16_t icmp_size;
    // swap source and destination fields
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        tmp = ether->destAddress[i];
        ether->destAddress[i] = ether->sourceAddress[i];
        ether->sourceAddress[i] = tmp;
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)

    {
        tmp = ip->destIp[i];
        ip->destIp[i] = ip ->sourceIp[i];
        ip->sourceIp[i] = tmp;
    }
    // this is a response
    icmp->type = 0;
    // calc icmp checksum
    sum = 0;
    etherSumWords(&icmp->type, 2);
    icmp_size = ntohs(ip->length);
    icmp_size -= 24; // sub ip header and icmp code, type, and check
    etherSumWords(&icmp->id, icmp_size);
    icmp->check = getEtherChecksum();
    // send packet
    etherPutPacket(ether, 14 + ntohs(ip->length));
}

// Determines whether packet is ARP
bool etherIsArpRequest(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    arpFrame* arp = (arpFrame*)&ether->data;
    bool ok;
    uint8_t i = 0;
    ok = (ether->frameType == htons(0x0806));
    while (ok & (i < IP_ADD_LENGTH))
    {
        ok = (arp->destIp[i] == ipAddress[i]);
        i++;
    }
    if (ok)
        ok = (arp->op == htons(1));
    return ok;
}
bool etherIsArpResponse(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    arpFrame* arp = (arpFrame*)&ether->data;
    bool ok;
    uint8_t i = 0;
    ok = (ether->frameType == htons(0x0806));
    while (ok & (i < IP_ADD_LENGTH))
    {
        ok = (arp->destIp[i] == ipAddress[i]);
        i++;
    }
    if (ok)
        ok = (arp->op == htons(2));
    return ok;
}
// Sends an ARP response given the request data
void etherSendArpResponse(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    arpFrame* arp = (arpFrame*)&ether->data;
    uint8_t i, tmp;
    // set op to response
    arp->op = htons(2);
    // swap source and destination fields
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        arp->destAddress[i] = arp->sourceAddress[i];
        ether->destAddress[i] = ether->sourceAddress[i];
        ether->sourceAddress[i] = arp->sourceAddress[i] = macAddress[i];
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        tmp = arp->destIp[i];
        arp->destIp[i] = arp->sourceIp[i];
        arp->sourceIp[i] = tmp;
    }
    // send packet
    etherPutPacket(ether, 42);
}
void etherSendGratuitousArpResponse(uint8_t packet[], uint8_t ip[])
{
    uint8_t i;
    etherFrame* ether = (etherFrame*)packet;
    arpFrame* arp = (arpFrame*)&ether->data;
    // set op to response
    ether->frameType = 0x0608;
    arp->hardwareType = htons(1);
    arp->protocolType = htons(0x0800);
    arp->hardwareSize = HW_ADD_LENGTH;
    arp->protocolSize = IP_ADD_LENGTH;
    arp->op = htons(2);
    for (i = 0; i < IP_ADD_LENGTH; i++)
        arp->destIp[i] = arp->sourceIp[i] = ip[i];

    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = arp->destAddress[i] = 0xFF;
        ether->sourceAddress[i] = arp->sourceAddress[i] = macAddress[i];
    }
    etherPutPacket(ether, 42);
}
// Sends an ARP request
void etherSendArpRequest(uint8_t packet[], uint8_t ip[])
{
    etherFrame* ether = (etherFrame*)packet;
    arpFrame* arp = (arpFrame*)&ether->data;
    uint8_t i;
    // fill ethernet frame
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = 0xFF;
        ether->sourceAddress[i] = macAddress[i];
    }
    ether->frameType = 0x0608;
    // fill arp frame
    arp->hardwareType = htons(1);
    arp->protocolType = htons(0x0800);
    arp->hardwareSize = HW_ADD_LENGTH;
    arp->protocolSize = IP_ADD_LENGTH;
    arp->op = htons(1);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        arp->sourceAddress[i] = macAddress[i];
        arp->destAddress[i] = 0xFF;
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        arp->sourceIp[i] = ipAddress[i];
        arp->destIp[i] = ip[i];
    }
    // send packet
    etherPutPacket(ether, 42);
}

// Determines whether packet is UDP datagram
// Must be an IP packet
bool etherIsUdp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    bool ok;
    uint16_t tmp16;
    ok = (ip->protocol == 0x11);
    if (ok)
    {
        // 32-bit sum over pseudo-header
        sum = 0;
        etherSumWords(ip->sourceIp, 8);
        tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;
        etherSumWords(&udp->length, 2);
        // add udp header and data
        etherSumWords(udp, ntohs(udp->length));
        ok = (getEtherChecksum() == 0);
    }
    return ok;
}
bool etherIsDhcp(uint8_t packet[])
{
  //only checks recv case
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    bool ok;
    // client always sends/recvs on 68 and server always sends/recvs on 67
    ok = ((htons(udp->sourcePort) == 67) && (htons(udp->destPort) == 68));
    ok &= matchesXid(packet);
    return ok;
}
uint8_t getDhcpMsgNumber(uint8_t packet[])
{
  etherFrame* ether = (etherFrame*)packet;
  ipFrame* ip = (ipFrame*)&ether->data;
  udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
  dhcpFrame* dhcp = (dhcpFrame*)&udp->data;
  return dhcp->options[2];
}
// Gets pointer to UDP payload of frame
uint8_t* etherGetUdpData(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    return &udp->data;
}

// Send responses to a udp datagram
// destination port, ip, and hardware address are extracted from provided data
// uses destination port of received packet as destination of this packet
void etherSendUdpResponse(uint8_t packet[], uint8_t* udpData, uint8_t udpSize)
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    uint8_t *copyData;
    uint8_t i, tmp8;
    uint16_t tmp16;
    // swap source and destination fields
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        tmp8 = ether->destAddress[i];
        ether->destAddress[i] = ether->sourceAddress[i];
        ether->sourceAddress[i] = tmp8;
    }
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        tmp8 = ip->destIp[i];
        ip->destIp[i] = ip->sourceIp[i];
        ip->sourceIp[i] = tmp8;
    }
    // set source port of resp will be dest port of req
    // dest port of resp will be left at source port of req
    // unusual nomenclature, but this allows a different tx
    // and rx port on other machine
    udp->sourcePort = udp->destPort;
    // adjust lengths
    ip->length = htons(((ip->revSize & 0xF) * 4) + 8 + udpSize);
    // 32-bit sum over ip header
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    udp->length = htons(8 + udpSize);
    // copy data
    copyData = &udp->data;
    for (i = 0; i < udpSize; i++)
        copyData[i] = udpData[i];
    // 32-bit sum over pseudo-header
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    etherSumWords(&udp->length, 2);
    // add udp header except crc
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, udpSize);
    udp->check = getEtherChecksum();

    // send packet with size = ether + udp hdr + ip header + udp_size
    etherPutPacket(ether, 22 + ((ip->revSize & 0xF) * 4) + udpSize);
}

uint16_t etherGetId()
{
    return htons(sequenceId);
}

void etherIncId()
{
    sequenceId++;
}

// Enable or disable DHCP mode
void etherEnableDhcpMode()
{
    writeEeprom(0, DHCP_ENABLED);
    etherSetIpAddress(0,0,0,0);
    etherSetIpSubnetMask(0,0,0, 0);
    etherSetIpGatewayAddress(0,0,0,0);
    etherSetIpDnsServer(0,0,0,0);
}

void etherDisableDhcpMode()
{
    writeEeprom(0, DHCP_DISABLED);
    etherSetIpAddress(0,0,0,0);
    etherSetIpSubnetMask(0,0,0, 0);
    etherSetIpGatewayAddress(0,0,0,0);
    etherSetIpDnsServer(0,0,0,0);
}

bool etherIsDhcpEnabled()
{
    //read eeprom and check for either 0xFFFFFFFF
    uint32_t result;
    result = readEeprom(FIRST_BLOCK_NO_OFFSET);

    if (result == DHCP_ENABLED)
        return true;
    else
        return false;
}
// Determines if the IP address is valid
bool etherIsIpValid()
{
    return ipAddress[0] || ipAddress[1] || ipAddress[2] || ipAddress[3];
}

// Sets IP address
void etherSetIpAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3)
{
    ipAddress[0] = ip0;
    ipAddress[1] = ip1;
    ipAddress[2] = ip2;
    ipAddress[3] = ip3;
}

// Gets IP address
void etherGetIpAddress(uint8_t ip[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        ip[i] = ipAddress[i];
}

// Sets IP subnet mask
void etherSetIpSubnetMask(uint8_t mask0, uint8_t mask1, uint8_t mask2, uint8_t mask3)
{
    ipSubnetMask[0] = mask0;
    ipSubnetMask[1] = mask1;
    ipSubnetMask[2] = mask2;
    ipSubnetMask[3] = mask3;
}

// Gets IP subnet mask
void etherGetIpSubnetMask(uint8_t mask[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        mask[i] = ipSubnetMask[i];
}
void etherSetIpDnsServer(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3)
{
    ipDnsServer[0] = ip0;
    ipDnsServer[1] = ip1;
    ipDnsServer[2] = ip2;
    ipDnsServer[3] = ip3;
}
void etherGetIpDnsServer(uint8_t ip[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        ip[i] = ipDnsServer[i];
}
// Sets IP gateway address
void etherSetIpGatewayAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3)
{
    ipGwAddress[0] = ip0;
    ipGwAddress[1] = ip1;
    ipGwAddress[2] = ip2;
    ipGwAddress[3] = ip3;
}

// Gets IP gateway address
void etherGetIpGatewayAddress(uint8_t ip[4])
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        ip[i] = ipGwAddress[i];
}

// Sets MAC address
void etherSetMacAddress(uint8_t mac0, uint8_t mac1, uint8_t mac2, uint8_t mac3, uint8_t mac4, uint8_t mac5)
{
    macAddress[0] = mac0;
    macAddress[1] = mac1;
    macAddress[2] = mac2;
    macAddress[3] = mac3;
    macAddress[4] = mac4;
    macAddress[5] = mac5;
}

// Gets MAC address
void etherGetMacAddress(uint8_t mac[6])
{
    uint8_t i;
    for (i = 0; i < 6; i++)
        mac[i] = macAddress[i];
}
void dhcpSendMessage(uint8_t packet[], uint8_t type, uint8_t ipAdd[])
{
    uint8_t i = 0;
    uint16_t initial_id = 0x7147;
    /*initialize pointers*/
    //ether frame
    etherFrame* ether = (etherFrame*)packet;
//    if (type == DHCPREQUEST && ipAdd[0] == 255)
//    {
//        for (i = 0; i < HW_ADD_LENGTH; i++)
//            macDhcpServer[i] = ether->sourceAddress[i];
//
//    }
    while ( i < HW_ADD_LENGTH )
    {
        ether->destAddress[i] = broadcast_mac[i];
        ether->sourceAddress[i] = macAddress[i];
        i++;
    }
    i = 0;
    ether->frameType = htons(IPv4_frame);


    //ip packet
    ipFrame* ip = (ipFrame*)&ether->data;
    ip->revSize = 0x45 ;
    ip->typeOfService = 0x00;
    ip->id = htons(initial_id++);
    ip->flagsAndOffset = 0x0000;
    ip->ttl = 64;
    ip->protocol = ip_udp;
    ip->headerChecksum = 0;
    while ( i < IP_ADD_LENGTH )
    {
        ip->sourceIp[i] = ipAddress[i];
        ip->destIp[i] = ipAdd[i];
        i++;
    }

    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    //udp segment
    udp->sourcePort = htons(68);
    udp->destPort = htons(67);
    udp->check = 0;


    dhcpFrame* dhcp = (dhcpFrame*)&udp->data;


    //dhcp datafields
    dhcp->op = 1;
    dhcp->htype = TEN_Mb_ETHERNET;
    dhcp->hlen = SIX_BYTES;
    dhcp->hops = 0; //no hops since network is local
    dhcp->xid = transaction_id; //number i got from wireshark
    dhcp->secs = 0; //seconds since client requested an ip address
    dhcp->flags = 0;


    i = 0;

    while ( i < sizeof(dhcp->chaddr)/sizeof(uint8_t))
    {
        if (i < HW_ADD_LENGTH)
            dhcp->chaddr[i] = macAddress[i];
        else
            dhcp->chaddr[i] = 0;
        i++;
    }
    i = 0;
    while ( i < sizeof(dhcp->data)/sizeof(uint8_t))
        dhcp->data[i++] = 0;
    dhcp->magicCookie = 0x63538263; //taken from wireshark
    uint8_t lenOpts;
    i = 0;
    switch (type)
    {
      case DHCPDISCOVER:
        while ( i < IP_ADD_LENGTH)
        {
            dhcp->ciaddr[i] = dhcp->yiaddr[i] = dhcp->siaddr[i] = dhcp->giaddr[i] = 0;
            i++;
        }
          dhcp->flags = htons(0x8000);
          dhcp->options[0] = DHCPMESSAGE;
          dhcp->options[1] = 1;
          dhcp->options[2] = type;
          dhcp->options[3] = END;
          lenOpts = 4;
          break;
      case DHCPREQUEST:
        while ( i < IP_ADD_LENGTH)
        {
            dhcp->ciaddr[i] = dhcp->giaddr[i] = 0;
            if (si_yi_clear)
            {
                yiaddr[i] = dhcp->yiaddr[i];
                siaddr[i] = dhcp->siaddr[i];
            }
            i++;
        }
        si_yi_clear = false;
          dhcp->options[0] = DHCPMESSAGE;
          dhcp->options[1] = 1; //length
          dhcp->options[2] = type;
          dhcp->options[3] = REQ_IP_MSG;
          dhcp->options[4] = 4; //length
          dhcp->options[5] = yiaddr[0];
          dhcp->options[6] = yiaddr[1];
          dhcp->options[7] = yiaddr[2];
          dhcp->options[8] = yiaddr[3];
          dhcp->options[9] = SERVERID;
          dhcp->options[10] = 4;
          dhcp->options[11] = siaddr[0];
          dhcp->options[12] = siaddr[1];
          dhcp->options[13] = siaddr[2];
          dhcp->options[14] = siaddr[3];
          dhcp->options[15] = PARAMETER_REQUEST;
          dhcp->options[16] = 3;
          dhcp->options[17] = SN_MASK_CODE;//subnet mask
          dhcp->options[18] = GW_CODE;//router
          dhcp->options[19] = DNS_CODE;//dns
          dhcp->options[20] = END;
          dhcp->options[21] = END;
          lenOpts = 22;
          break;
      case DHCPDECLINE:

          break;
      case DHCPACK:
          break;
      case DHCPNAK:
          break;
      case DHCPRELEASE:
            dhcp->options[0] = DHCPMESSAGE;
            dhcp->options[1] = 1;
            dhcp->options[2] = type;
            dhcp->options[3] = END;
            lenOpts = 4;
          break;
      case DHCPINFORM:
          break;
      default:
          break;
    }
    ip->length = htons( ipHeaderLength + udpHeaderLength + dhcpSize + lenOpts ); /*20 + 8 + dhcpSize + options*/;
    udp->length = htons( udpHeaderLength + dhcpSize + lenOpts );
    //options all populated -> make checksums

    etherCalcIpChecksum(ip);
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    uint16_t tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    etherSumWords(&udp->length, 2);
    // add udp header except crc
    etherSumWords(udp, 6);
    etherSumWords(&udp->data, dhcpSize + lenOpts);
    udp->check = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 8 + dhcpSize + lenOpts);
}
bool matchesXid(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*)&udp->data;
    return dhcp->xid == transaction_id;
}
void dhcpStoreVars(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    udpFrame* udp = (udpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*)&udp->data;

    etherSetIpAddress(dhcp->yiaddr[0], dhcp->yiaddr[1], dhcp->yiaddr[2], dhcp->yiaddr[3]);
    uint8_t length = htons(ip->length) - ipHeaderLength - udpHeaderLength - dhcpSize;
    uint8_t i;
    bool msg_accounted, si_accounted, lease_accounted, sn_accounted, router_accounted, dns_accounted;
    msg_accounted = si_accounted = lease_accounted = sn_accounted = router_accounted = dns_accounted = false;
    for (i = 0; i < length; i++)
    {
        if (dhcp->options[i] == DHCPMESSAGE && msg_accounted == false)
        {
            i+=dhcp->options[i+1] + 1; msg_accounted = true; continue;
        }
        if (dhcp->options[i] == SERVERID && si_accounted == false)
        {
            i+=dhcp->options[i+1] + 1; si_accounted = true; continue;
        }
        if (dhcp->options[i] == IP_LEASE_CODE && lease_accounted == false)
        {
            lease_time = dhcp->options[i+2] << 24 | dhcp->options[i+3] << 16 | dhcp->options[i+4] << 8 | dhcp->options[i+5] << 0;
            i+=dhcp->options[i+1] + 1; lease_accounted = true;
            continue;
        }
        if (dhcp->options[i] == SN_MASK_CODE && sn_accounted == false)
        {
            etherSetIpSubnetMask(dhcp->options[i+2],dhcp->options[i+3],dhcp->options[i+4],dhcp->options[i+5]);
            i+=dhcp->options[i+1] + 1; sn_accounted = true; continue;
        }
        if (dhcp->options[i] == GW_CODE && router_accounted == false)
        {
            etherSetIpGatewayAddress(dhcp->options[i+2],dhcp->options[i+3],dhcp->options[i+4],dhcp->options[i+5]);
            i+=dhcp->options[i+1] + 1; router_accounted = true; continue;
        }
        if (dhcp->options[i] == DNS_CODE && dns_accounted == false)
        {
            etherSetIpDnsServer(dhcp->options[i+2],dhcp->options[i+3],dhcp->options[i+4],dhcp->options[i+5]);
            i+=dhcp->options[i+1] + 1; dns_accounted = true; continue;
        }
    }
}
uint32_t getLeaseTime()
{
    return lease_time;
}

bool etherIsTcp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*)packet;
    ipFrame* ip = (ipFrame*)&ether->data;
    tcpFrame* tcp = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));
    port_num = htons(tcp->destPort);
    tcp_flags = htons(tcp->offsetAndFlags) & 0x00FF;
    if (ip->protocol == 0x06 && port_num == 23)
        return true;
    else
        return false;
}
uint16_t getPortNum(uint8_t packet[])
{
    return port_num;
}
void get_siaddr(uint8_t temp_ip[4])
{
    uint8_t i;
    for (i = 0; i < IP_ADD_LENGTH; i++)
        temp_ip[i] = siaddr[i];
}
uint8_t get_tcp_flags()
{
    return tcp_flags;
}
uint32_t htonl(const uint32_t value)
{
    return htons(value >> 16) | (htons((uint16_t) value) << 16);
}
void sendTcpMsg(uint8_t packet[], uint8_t flag, uint8_t payload[], bool payload_empty)
{
    uint8_t i;
    uint16_t data_length = 0;

    uint16_t initial_id = 0x0;
    etherFrame* ether = (etherFrame*)packet;
    i = 0;
    while ( i < HW_ADD_LENGTH )
    {
        ether->destAddress[i] = ether->sourceAddress[i];
        ether->sourceAddress[i] = macAddress[i];
        i++;
    }
    ether->frameType = htons(IPv4_frame);

    i = 0;
    ipFrame* ip = (ipFrame*)&ether->data;
    while ( i < IP_ADD_LENGTH)
    {
        ip->destIp[i] = ip->sourceIp[i];
        ip->sourceIp[i] = ipAddress[i];
        i++;
    }

    data_length = htons(ip->length) - ((ip->revSize & 0xF) * 4); //tcp header + data length
    ip->revSize = 0x45 ;
    ip->typeOfService = 0x00;
    ip->id = htons(initial_id++);
    ip->flagsAndOffset = 0x0000;
    ip->ttl = 64;
    ip->protocol = 0x06;
    ip->headerChecksum = 0;

    tcpFrame* tcp = (tcpFrame*)((uint8_t*)ip + ((ip->revSize & 0xF) * 4));

    data_length -= ((htons(tcp->offsetAndFlags)) >> 12) * 4; // shift right by 8 to get appropriate
                                                          // 0-indexed value. Each LSb indicates
                                                          // 32-bit word (4 bytes).

    tcp->destPort = tcp->sourcePort;
    tcp->sourcePort = htons(23);
    tcp->windowSize = htons(0x05b4);
    tcp->urgentPointer = htons(0);
    tcp->check = 0;
    uint8_t tcpSize = sizeof(tcpFrame);
    uint16_t lenOpts;
    uint32_t packet_seq = htonl(tcp->sequenceNum);
    switch (flag)
    {
    case 0x01:
        tcp->ackNum = htonl(packet_seq + 1) ;
        tcp->sequenceNum = htonl(seq_num);
        tcp->offsetAndFlags = htons(0b0101000000010000);
        lenOpts = 0;
        break;
    case 0x02:
        tcp->ackNum = htonl(0);
        tcp->sequenceNum = htonl(seq_num++);
        tcp->offsetAndFlags = htons(0b0101000000010000);
        lenOpts = 0;
        break;
    case 0x10: // ack
        tcp->ackNum = htonl(packet_seq + data_length) ;
        tcp->sequenceNum = htonl(seq_num);
        tcp->offsetAndFlags = htons(0b0101000000010000);
        lenOpts = 0;
        break;
    case 0x12: // syn/ack
        tcp->ackNum = htonl(packet_seq + 1) ;
        tcp->sequenceNum = htonl(seq_num++);
        tcp->offsetAndFlags = htons(0b0110000000010010);
        tcp->optionsPaddingData[0] = 2;
        tcp->optionsPaddingData[1] = 4;
        tcp->optionsPaddingData[2] = 0x05;
        tcp->optionsPaddingData[3] = 0xb4;
        lenOpts = 4;
        break;
    case 0x18: //push ack
        /*telnet processing: parse tcp->optionsPaddingData. If tcp->optionsPaddingData[i] == 255
         * you should interpret the next two bytes as a command. Otherwise, interpret as text.*/
        tcp->ackNum = htonl(packet_seq + data_length) ;
        tcp->sequenceNum = htonl(seq_num);
        tcp->offsetAndFlags = htons(0b0101000000010000);
        lenOpts = 0;
        ip->length = htons( ipHeaderLength + tcpSize + lenOpts ); /*20 + 8 + dhcpSize + options*/;
        //options all populated -> make checksums

        etherCalcIpChecksum(ip);
        sum = 0;
        etherSumWords(ip->sourceIp, 8);
        uint16_t tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;
        uint16_t tmp_len = htons(lenOpts + tcpSize);
        etherSumWords(&tmp_len, 2);
        // add udp header except crc
        etherSumWords(tcp, 18);
        etherSumWords(tcp->optionsPaddingData, lenOpts);
        tcp->check = getEtherChecksum();
        etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize + lenOpts);

        ip->headerChecksum = 0;
        tcp->offsetAndFlags = htons(0b0101000000011000);
        tcp->check = 0;
        i = 0;
        while (i < data_length)
        {
            if (tcp->optionsPaddingData[i] == 0xFF) // interpret as command
            {
                if (tcp->optionsPaddingData[i + 1] == 0xFA) // suboption ; handle on the side
                {
                    //iterate through until you locate sequence 0xff 0xf0
                }
                else
                {
                    switch(tcp->optionsPaddingData[i + 2])
                    {
                    case 0x03: // suppress go ahead option
                        /*
                         * WILL = 0xfb
                         * WONT = 0xfc
                         * DO   = 0xfd
                         * DONT = 0xfe
                         */
                        if (will_wont(tcp->optionsPaddingData[i+1]))
                            tcp->optionsPaddingData[i + 1] = 0xfe;
                        else
                            tcp->optionsPaddingData[i + 1] = 0xfc;
                        break;
                    case 0x05: // status option
                        if (will_wont(tcp->optionsPaddingData[i+1]))
                            tcp->optionsPaddingData[i + 1] = 0xfe;
                        else
                            tcp->optionsPaddingData[i + 1] = 0xfc;
                        break;
                    case 0x18: // terminal type option
                        if (will_wont(tcp->optionsPaddingData[i+1]))
                            tcp->optionsPaddingData[i + 1] = 0xfe;
                        else
                            tcp->optionsPaddingData[i + 1] = 0xfc;
                        break;
                    case 0x1f: // negotiate window size
                        if (will_wont(tcp->optionsPaddingData[i+1]))
                            tcp->optionsPaddingData[i + 1] = 0xfe;
                        else
                            tcp->optionsPaddingData[i + 1] = 0xfc;
                        break;
                    case 0x20: // terminal speed
                        if (will_wont(tcp->optionsPaddingData[i+1]))
                            tcp->optionsPaddingData[i + 1] = 0xfe;
                        else
                            tcp->optionsPaddingData[i + 1] = 0xfc;
                        break;
                    case 0x21: // remote flow control
                        if (will_wont(tcp->optionsPaddingData[i+1]))
                            tcp->optionsPaddingData[i + 1] = 0xfe;
                        else
                            tcp->optionsPaddingData[i + 1] = 0xfc;
                        break;
                    case 0x22: // linemode option
                        if (will_wont(tcp->optionsPaddingData[i+1]))
                            tcp->optionsPaddingData[i + 1] = 0xfd;
                        else
                            tcp->optionsPaddingData[i + 1] = 0xfb;
                        break;
                    case 0x25: // authentication option
                        if (will_wont(tcp->optionsPaddingData[i+1]))
                            tcp->optionsPaddingData[i + 1] = 0xfe;
                        else
                            tcp->optionsPaddingData[i + 1] = 0xfc;
                        break;
                    case 0x27: // new environment option
                        if (will_wont(tcp->optionsPaddingData[i+1]))
                            tcp->optionsPaddingData[i + 1] = 0xfe;
                        else
                            tcp->optionsPaddingData[i + 1] = 0xfc;
                        break;
                    default:
                        break;
                    }
                    i += 3;
                }
            }
            else
                telnet_command[i] = tcp->optionsPaddingData[i++];
        }
        telnet_command[data_length] = '\0';
        lenOpts = data_length;
        ip->length = htons( ipHeaderLength + tcpSize + lenOpts ); /*20 + 8 + dhcpSize + options*/;
        //options all populated -> make checksums
        etherCalcIpChecksum(ip);
        sum = 0;
        etherSumWords(ip->sourceIp, 8);
        tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;
        tmp_len = htons(lenOpts + tcpSize);
        etherSumWords(&tmp_len, 2);
        // add udp header except crc
        etherSumWords(tcp, 18);
        etherSumWords(tcp->optionsPaddingData, lenOpts);
        tcp->check = getEtherChecksum();
        etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize + lenOpts);
        return;
    default:
        break;
    }
    if (!payload_empty)
    {
        uint8_t len = strlen((char*) payload);
        for (i = 0; i < len; i++)
            tcp->optionsPaddingData[lenOpts++] = payload[i];
        seq_num += lenOpts;
    }
    ip->length = htons( ipHeaderLength + tcpSize + lenOpts ); /*20 + 8 + dhcpSize + options*/;
    //options all populated -> make checksums
    etherCalcIpChecksum(ip);
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    uint16_t tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    uint16_t tmp_len = htons(lenOpts + tcpSize);
    etherSumWords(&tmp_len, 2);
    // add udp header except crc
    etherSumWords(tcp, 18);
    etherSumWords(tcp->optionsPaddingData, lenOpts);
    tcp->check = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + tcpSize + lenOpts);
}
bool telnet_command_recv()
{
    return command_pending;
}
void clear_command_recv()
{
    command_pending = false;
}
void copy_command(char* strInput)
{
    strcpy(telnet_command, strInput);
}
bool will_wont(uint8_t command)
{
    return command == 0xfb || command == 0xfc;
}
