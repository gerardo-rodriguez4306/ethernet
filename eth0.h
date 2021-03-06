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

#ifndef ETH0_H_
#define ETH0_H_

#include <stdint.h>
#include <stdbool.h>

#define ETHER_UNICAST        0x80
#define ETHER_BROADCAST      0x01
#define ETHER_MULTICAST      0x02
#define ETHER_HASHTABLE      0x04
#define ETHER_MAGICPACKET    0x08
#define ETHER_PATTERNMATCH   0x10
#define ETHER_CHECKCRC       0x20

#define ETHER_HALFDUPLEX     0x00
#define ETHER_FULLDUPLEX     0x100
#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPDECLINE  4
#define DHCPACK      5
#define DHCPNAK      6
#define DHCPRELEASE  7
#define DHCPINFORM   8
#define LOBYTE(x) ((x) & 0xFF)
#define HIBYTE(x) (((x) >> 8) & 0xFF)

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void etherInit(uint16_t mode);
bool etherIsLinkUp();

bool etherIsDataAvailable();
bool etherIsOverflow();
uint16_t etherGetPacket(uint8_t packet[], uint16_t maxSize);
bool etherPutPacket(uint8_t packet[], uint16_t size);

bool etherIsIp(uint8_t packet[]);
bool etherIsIpUnicast(uint8_t packet[]);

bool etherIsPingRequest(uint8_t packet[]);
void etherSendPingResponse(uint8_t packet[]);

bool etherIsArpRequest(uint8_t packet[]);
void etherSendArpResponse(uint8_t packet[]);
void etherSendGratuitousArpResponse(uint8_t packet[], uint8_t ip[]);
void etherSendArpRequest(uint8_t packet[], uint8_t ip[]);

bool etherIsUdp(uint8_t packet[]);
bool etherIsTcp(uint8_t packet[]);
uint8_t* etherGetUdpData(uint8_t packet[]);
void etherSendUdpResponse(uint8_t packet[], uint8_t* udpData, uint8_t udpSize);

void etherEnableDhcpMode();
void etherDisableDhcpMode();
bool etherIsDhcpEnabled();
bool etherIsIpValid();
void etherSetIpAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void etherGetIpAddress(uint8_t ip[4]);
void etherSetIpGatewayAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void etherGetIpGatewayAddress(uint8_t ip[4]);
void etherSetIpSubnetMask(uint8_t mask0, uint8_t mask1, uint8_t mask2, uint8_t mask3);
void etherGetIpSubnetMask(uint8_t mask[4]);
void etherSetIpDnsServer(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void etherGetIpDnsServer(uint8_t ip[4]);
void etherSetMacAddress(uint8_t mac0, uint8_t mac1, uint8_t mac2, uint8_t mac3, uint8_t mac4, uint8_t mac5);
void etherGetMacAddress(uint8_t mac[6]);
void dhcpSendMessage(uint8_t packet[], uint8_t type, uint8_t ipAdd[]);
bool etherIsDhcp(uint8_t packet[]);
uint8_t getDhcpMsgNumber(uint8_t packet[]);
uint16_t htons(const uint16_t value);
bool matchesXid(uint8_t packet[]);
void dhcpStoreVars();
void setLeaseTime(uint8_t arg1, uint8_t arg2, uint8_t arg3, uint8_t arg4);
uint32_t getLeaseTime();
uint16_t getPortNum();
bool etherIsArpResponse(uint8_t packet[]);
void get_siaddr(uint8_t temp_ip[4]);
uint8_t get_tcp_flags();
void sendTcpMsg(uint8_t packet[], uint8_t flag, uint8_t payload[], bool payload_empty);
uint32_t htonl(const uint32_t value);
bool telnet_command_recv();
void clear_command_recv();
void copy_command(char* strInput);
bool will_wont(uint8_t command);
#define ntohs htons

#endif
