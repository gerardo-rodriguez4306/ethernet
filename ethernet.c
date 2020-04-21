// Ethernet Example
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

// Pinning for IoT projects with wireless modules:
// N24L01+ RF transceiver
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS on PE0
//   INT on PB2
// Xbee module
//   DIN (UART1TX) on PC5
//   DOUT (UART1RX) on PC4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "eth0.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "eeprom.h"
#include "str.h"

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4
#define MAX_CHARS 80
#define MAX_ARGS 6
uint8_t broadcast_ip[] = {255, 255, 255, 255};
uint8_t no_payload[0];
char tcp_ifconfig_buffer[128];
//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------


typedef struct user_input
{
    char strInput[MAX_CHARS];
    char *temp_command;
    char *temp_arg[MAX_ARGS];
    uint8_t argCount;

}user_input;
// Initialize Hardware
extern void ResetISR(void);
void initHw()
{
	// Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
    initEeprom();
}
bool is_alphanumeric(char c)
{
    /*determines whether input is alphanumeric or not based on ASCII values*/
    uint8_t is_a_n[128] = { 0,0,0,0,0,0,0,0,0,0, //0-9
                            0,0,0,0,0,0,0,0,0,0, //10-19
                            0,0,0,0,0,0,0,0,0,0, //20-29
                            0,0,0,1,0,0,0,0,1,0, //30-39
                            0,0,0,0,0,0,0,0,1,1, //40-49
                            1,1,1,1,1,1,1,1,0,0, //50-59
                            0,0,0,0,0,1,1,1,1,1, //60-69
                            1,1,1,1,1,1,1,1,1,1, //70-79
                            1,1,1,1,1,1,1,1,1,1, //80-89
                            0,0,0,0,0,0,0,1,1,1, //90-99
                            1,1,1,1,1,1,1,1,1,1, //100-109
                            1,1,1,1,1,1,1,1,1,1, //110-119
                            1,1,1,0,0,0,0,0, //120-127
                            };
    return is_a_n[c];
}
void tokenize_string(user_input *temp)
{
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t length = strlen(temp->strInput);
    if (is_alphanumeric(temp->strInput[0]))
    {
        temp->temp_command = temp->temp_arg[0] = &(temp->strInput[0]);
        j++;
    }

    for (i = 0; i < length; i++)
    {
        if ( !(is_alphanumeric(temp->strInput[i]) ) )
                temp->strInput[i] = '\0';
        if ( is_alphanumeric(temp->strInput[i]) && !(is_alphanumeric(temp->strInput[i - 1])) && i > 0)
                temp->temp_arg[j++] = &(temp->strInput[i]);
    }
    if (j == 0) temp->temp_command = temp->temp_arg[0] = &(temp->strInput[0]);
    temp->temp_command = temp->temp_arg[0];
    temp->argCount = j - 1;
}

/* This function determines if command is valid. Cases must be coded separately. Thus, this function must
                      be modified according to the commands available to the user.                    */
bool isCommand(char* cmd, user_input temp)
{
  /*commands: reboot, menu*/
  if (strcmp(temp.temp_command, cmd) == 0 && temp.argCount == 0)
      return true;
  else if(strcmp(temp.temp_command, cmd) == 0 && temp.argCount == 1)
      return true;
  else if(strcmp(temp.temp_command, cmd) == 0 && temp.argCount == 2)
      return true;
  else if(strcmp(temp.temp_command, cmd) == 0 && temp.argCount == 5)
        return true;
  else
    return false;
}
void getsUart0(user_input *temp, uint8_t maxChars)
{
    uint8_t count = 0;
    char c;
    while (count < maxChars)
    {
        c = getcUart0();
        //if you've reached the max amount of characters, break (80 chars max)
        if (count == maxChars)
        {
            temp->strInput[count] = '\0';
            break;
        }
        if (c == 8 || c == 127)
        {
            if (count < 1) continue;
            count--; continue;
        }
        //if you've pressed enter, add a newline delimiter, a carriage return, and a null terminator
        if (c == 13)
        {
            temp->strInput[count] = '\0';
            break;
        }
        //if an input is an uppercase letter, make it lowercase
        if (c >= 'A' && c <= 'Z')
        {
            c+=32;
            temp->strInput[count] = c;
        }
        //put whatever is in the buffer onto the screen
        else
            temp->strInput[count] = c;
        count++;
    }
}
void putIpUart0(uint8_t ip[])
{
    char int_buf[6]; int i;
    for (i = 0; i < 4; i++)
    {
        itoa(ip[i], int_buf);
        putsUart0(int_buf);
        if (i < 3)
            putsUart0(".");
    }
    putcUart0('\n');
}
void displayConnectionInfo()
{
    uint8_t i;
    char buf_hex[3];
    uint8_t mac[6];
    uint8_t ip[4];
    etherGetMacAddress(mac);
    putsUart0("\nHW: ");
    for (i = 0; i < 6; i++)
    {

        htoa(mac[i], buf_hex);
        putsUart0(buf_hex);
        if (i < 5)
            putsUart0(":");
    }

    putcUart0('\n');
    etherGetIpAddress(ip);
    putsUart0("IP: ");
    putIpUart0(ip);

    if (etherIsDhcpEnabled())
        putsUart0(" (dhcp)");
    else
        putsUart0(" (static)");
    putcUart0('\n');
    etherGetIpSubnetMask(ip);
    putsUart0("SN: ");
    putIpUart0(ip);

    etherGetIpGatewayAddress(ip);
    putsUart0("GW: ");
    putIpUart0(ip);

    etherGetIpDnsServer(ip);
    putsUart0("DNS: ");
    putIpUart0(ip);

    if (etherIsLinkUp())
        putsUart0("Link is up\n");
    else
        putsUart0("Link is down\n");
}
void putMenu(char* menu)
{
    int i;
    for (i = 0; menu[i] != '\0'; i++)
        putcUart0(menu[i]);
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522

int main(void)
{
    uint8_t data[MAX_PACKET_SIZE];

    // Init controller
    initHw(); //eeprom is initialized here as well

    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    // Init ethernet interface (eth0)
    putsUart0("\nStarting eth0-en9\n");
    etherSetMacAddress(2, 3, 4, 5, 6, 123);
    etherDisableDhcpMode();
    etherInit(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    etherSetIpAddress(192,168,2,123);
    etherSetIpSubnetMask(255, 255, 255, 0);
    etherSetIpGatewayAddress(192, 168, 2, 1);
    waitMicrosecond(100000);
    displayConnectionInfo();
    char prompt[] = "\nIoT-shell-0.1:~ ";
    putcUart0('\n');
    putsUart0(prompt);
    user_input current_user_input;
    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);


    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    // bool telnet_data_recv = false;
    uint8_t i = 0;
    char *menu  =  "\n\thelp menu: \n"
                   "help:\t\t displays help menu\n"
                   "reboot:\t\t reboots the microcontroller.\n"
                   "ifconfig:\t dumps current IP, SN, GW, DNS, and DHCP mode\n"
                   "dhcp:\t\t must be supplied with on|off or refresh|release argument\n"
                   "\t\t examples: dhcp on OR dhcp dhcp release\n"
                   "set:\t\t primary arg ip, gw, dns, sn, dns and secondary arg ip address\n"
                   "\t\t example: set ip 192.168.1.1\n"
                   "\t\t if going from (dhcp) to (static), all addresses must be set\n";
    while (true)
    {

        uint8_t temp_ip[4] = {0,0,0,0};
        // Put terminal processing here
        if (kbhitUart0())
        {
            getsUart0(&current_user_input, MAX_CHARS);
            tokenize_string(&current_user_input);
            //
            /*tokenizing string, setting argCount, getting arguments, and determining command*/
            if (isCommand("help", current_user_input))
                putMenu(menu);
            else if (isCommand("reboot", current_user_input))
            {
                putsUart0("System rebooting...\n");
                ResetISR();
            }

            else if (isCommand("dhcp", current_user_input))
            {
                if (current_user_input.argCount == 1)
                {
                    if (strcmp(current_user_input.temp_arg[1],"on") == 0)
                    {
                        putsUart0("dhcp on\n");
                    }
                    else if (strcmp(current_user_input.temp_arg[1],"off") == 0)
                    {
                        putsUart0("dhcp off\n");
                    }
                    else
                        putsUart0("invalid dhcp command");
                }
                else if (current_user_input.argCount == 2)
                {
                    if (strcmp(current_user_input.temp_arg[2], "refresh") == 0)
                    {
                        putsUart0("dhcp refresh\n");
                    }
                    else if (strcmp(current_user_input.temp_arg[2], "release") == 0)
                    {
                        putsUart0("dhcp release\n");
                    }
                    else
                        putsUart0("invalid dhcp command");
                }
                else
                    putsUart0("invalid dhcp command");
            }
            else if (isCommand("set", current_user_input))
            {
                if (etherIsDhcpEnabled())
                    putsUart0("dhcp must be disabled to set this variable");
                else
                {
                    if (strcmp(current_user_input.temp_arg[1],"ip") == 0)
                    {
                        for (i = 0; i < 4; i++)
                            temp_ip[i] = atoi(current_user_input.temp_arg[i+2]);
                        etherSetIpAddress(temp_ip[0],temp_ip[1],temp_ip[2],temp_ip[3]);
                    }
                    else if (strcmp(current_user_input.temp_arg[1],"gw") == 0)
                    {
                        for (i = 0; i < 4; i++)
                            temp_ip[i] = atoi(current_user_input.temp_arg[i+2]);
                        etherSetIpGatewayAddress(temp_ip[0],temp_ip[1],temp_ip[2],temp_ip[3]);
                    }
                    else if (strcmp(current_user_input.temp_arg[1],"dns") == 0)
                    {
                        for (i = 0; i < 4; i++)
                            temp_ip[i] = atoi(current_user_input.temp_arg[i+2]);
                        etherSetIpDnsServer(temp_ip[0],temp_ip[1],temp_ip[2],temp_ip[3]);
                    }
                    else if (strcmp(current_user_input.temp_arg[1],"sn") == 0)
                    {
                        for (i = 0; i < 4; i++)
                            temp_ip[i] = atoi(current_user_input.temp_arg[i+2]);
                        etherSetIpSubnetMask(temp_ip[0],temp_ip[1],temp_ip[2],temp_ip[3]);
                    }
                    else
                        putsUart0("ip config cannot be set. try \'ip\',\'gw\',\'dns\', or \'sn\'");

                }
            }
            else if (isCommand("ifconfig", current_user_input))
                displayConnectionInfo();
            else
            {
                putsUart0(current_user_input.temp_command);
                putsUart0(" is not specified. You might be missing arguments.\n");
            }

            current_user_input.argCount = 0;

            putsUart0(prompt);
        }
        if (telnet_command_recv())
        {
            putsUart0("recvd command\n");
            // put commands into current_user_input to save space; you can do this upon a PSH/ACK
            copy_command(current_user_input.strInput);
            putsUart0(current_user_input.strInput);
            // support limited number of commands as to not lose connection
            if (isCommand("help", current_user_input))
                sendTcpMsg(data, 0x18, (uint8_t*)menu, false);
            else if (isCommand("reboot", current_user_input))
            {
                sendTcpMsg(data, 0x18, "System rebooting...\n", false);
                ResetISR();
            }
            else
            {
                sendTcpMsg(data, 0x18, "that command is either not specified or supported for telnet use.\n", true);
            }
            clear_command_recv();
        }

        // Packet processing
        if (etherIsDataAvailable())
        {
            if (etherIsOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
            }

            // Get packet
            etherGetPacket(data, MAX_PACKET_SIZE);
            // Handle ARP request
            if (etherIsArpRequest(data))
            {
                etherSendArpResponse(data);
            }

            // Handle IP datagram
            if (etherIsIp(data))
            {
            	if (etherIsIpUnicast(data))
            	{
            		// handle icmp ping request
					if (etherIsPingRequest(data))
					{
					  etherSendPingResponse(data);
                      setPinValue(RED_LED, 1);
                      waitMicrosecond(100000);
                      setPinValue(RED_LED, 0);
                      waitMicrosecond(100000);
					}
					if (etherIsTcp(data)) //since we're only supporting port 23, fn
					{                     //returns false if port != 23
					    uint8_t flags = get_tcp_flags();
					    switch (flags)
                        {
                        case 0x01: // fin
                            // ack
                            sendTcpMsg(data, 0x10, no_payload, true);
                            // fin
                            sendTcpMsg(data, 0x01, no_payload, true);
                            break;
					    case 0x02: //syn
					        sendTcpMsg(data, 0x12, no_payload, true);
					        break;
					    case 0x10: //ack ---> for the time being, don't take action on receiving ack's
					               //         System actually has to take action if messages aren't being ack'ed
					               //         like re-transmissions
					        // sendTcpMsg(data, 0x12, no_payload);
					        // if (state == TIME_WAIT) putsUart0("connection closed");
					        break;
					    case 0x12: //syn/ack
					        sendTcpMsg(data, 0x10, no_payload, true);
					        break;
					    case 0x18: // push/ack ---> reply with data
					        // send response -> payload is messenger-given not sender generated
					        sendTcpMsg(data, 0x18, no_payload, true);
					        break;
					    case 0xc2:
					        sendTcpMsg(data, 0x12, no_payload, true);
					        break;
					    default:
					        break;
                        }
					}
                }
            }
        }
    }
}
