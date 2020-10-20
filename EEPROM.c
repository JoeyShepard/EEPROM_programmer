/**   Template for MSP430
 *    Copyright (C) 2014 Joey Shepard
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

#include <msp430.h>
#include <stdbool.h>
#include <string.h>
#include "fastdebug.h"

#define MHZ_16
#ifdef MHZ_1
  #define BC_CONST CALBC1_1MHZ
  #define DCO_CONST CALDCO_1MHZ
  #define DELAY_TIME 1000
#elif defined MHZ_16
  #define BC_CONST CALBC1_16MHZ
  #define DCO_CONST CALDCO_16MHZ
  #define DELAY_TIME 16000
#endif

#define DBG_TIME            2     //milliseconds per bit
#define DBG_PORT            P2OUT //Port of the data pin
#define DBG_OUT             BIT0  //Data pin

#define SR_OUT_LATCH        BIT0  //P1.0
#define UART_RXD            BIT1  //P1.1
#define UART_TXD            BIT2  //P1.2
#define SR_IN_LATCH         BIT3  //P1.3
#define SR_OUT_OE           BIT4  //P1.4
#define IO_CLOCK            BIT5  //P1.5 IO clock
#define IO_MISO             BIT6  //P1.6 IO data out
#define IO_MOSI             BIT7  //P1.7 IO data in

#define EEPROM_OE           BIT1  //P2.1
#define EEPROM_WE           BIT2  //P2.2

#define XM_SOH              0x01
#define XM_EOT              0x04
#define XM_ACK              0x06
#define XM_NAK              0x15
#define XM_CAN              0x18

unsigned char SPI_Send(unsigned char data);
void UART_Send(unsigned char data);
void UART_Text(const char *data);
unsigned char UART_Receive(int timeout);
void UART_Hex(unsigned char data);
void UART_Hex16(unsigned int data);
void UART_Int(unsigned int data);
void delay_ms(int ms);

unsigned int UART_GetHex(unsigned int bytes);
unsigned int UART_GetInt(unsigned int places, unsigned int maxint);
unsigned char UART_Filter(unsigned char byte);
unsigned char ROM_Read(unsigned int read_address);
bool ROM_Write(unsigned int write_address, unsigned char data, bool poll);

volatile bool UART_Failed;
volatile int UART_Flag;
volatile int UART_Error;

int main(void)
{
  WDTCTL=WDTPW + WDTHOLD;

  BCSCTL1=BC_CONST;
  DCOCTL=DCO_CONST;

  BCSCTL3|=LFXT1S_2;

  TA0CCR0=12000;
  TA0CCTL0=CCIE;
  TA0CTL=MC_0|ID_3|TASSEL_1|TACLR;

  //TA1CCR0=2400;//every 200ms
  //TA1CCTL0=CCIE;
  //TA1CTL=MC_0|ID_0|TASSEL_1|TACLR;

  UCA0CTL1=UCSWRST|UCSSEL_2;
  UCA0CTL0 = 0;

  //230.4k
  UCA0MCTL = UCBRS_4+UCBRF_0;
  UCA0BR0 = 0x45;
  UCA0BR1 = 0x00;

  //57.6k
  //UCA0MCTL = UCBRS_6+UCBRF_0;
  //UCA0BR0 = 0x15;
  //UCA0BR1 = 0x01;

   //9.6k
  //UCA0MCTL = UCBRS_5+UCBRF_0;
  //UCA0BR0 = 0x82;
  //UCA0BR1 = 0x06;

  UCA0CTL1&=~UCSWRST;

  UCB0CTL1=UCSWRST;
  UCB0CTL0=UCCKPH|UCMST|UCSYNC|UCMSB;//mode 0
  UCB0CTL1|=UCSSEL_2;
  //PN2222A
  //=======
  //30 too low
  //40 seems to work. maybe too low though

  //2N2369
  //======
  //1 does not work
  //2 seems to work
  UCB0BR0=2;
  UCB0BR1=0;

  UCB0CTL1&=~UCSWRST;

  P1OUT=DBG_OUT|SR_OUT_OE;
  P1DIR=DBG_OUT|SR_OUT_OE|SR_IN_LATCH|SR_OUT_LATCH;
  P1SEL= IO_CLOCK|IO_MISO|IO_MOSI|UART_RXD|UART_TXD;
  P1SEL2=IO_CLOCK|IO_MISO|IO_MOSI|UART_RXD|UART_TXD;

  //P2OUT=EEPROM_WE;
  P2OUT=EEPROM_OE;
  P2DIR=EEPROM_OE|EEPROM_WE;

  //P2SEL=0;
  //P2SEL2=0;

  //P2OUT=RESET_BUTTON;
  //P2REN=RESET_BUTTON;
  //P2IE|=RESET_BUTTON;
  //P2IES|=RESET_BUTTON;//falling edge?
  //P2IFG&=~RESET_BUTTON;*/

  //here?
  __enable_interrupt();

  //delay_ms(1000);

  int j=0;
  unsigned char chr='A';
  unsigned char result;
  int i;
  unsigned int ROM_address, ROM_address2, ROM_counter;
  unsigned int ROM_data, ROM_check;
  unsigned char ROM_packet[132];
  unsigned int ROM_pkt_counter, ROM_state, ROM_pkt_debug;
  unsigned int ROM_PageCheck;
  bool ROM_done;
  //unsigned char ROM_extra[132];
  unsigned int ROM_repeat;
  unsigned char ROM_disp[16];
  unsigned char ROM_crc, ROM_NAK_tries;
  bool PageEnable=false;
  unsigned int PageSize;
  /*SPI_Send(0xAA);//low byte
  SPI_Send(0x55);//high byte
  SPI_Send(0xF0);//data
  P1OUT&=~SR_OUT_LATCH;
  delay_ms(1);
  P1OUT|=SR_OUT_LATCH;
  while(1);*/

  /*while(1)
  {
    for (i='A';i<='Z';i++) UART_Send(i);
    UART_Text("\r\n");
    delay_ms(1000);
  }*/


  /*while(1)
  {
    UART_Hex(UART_Receive(0));
    UART_Send(' ');
    //delay_ms(1000);
  }*/

  UART_Flag=0;

  /*while(1)
  {
    i=UART_Receive(1000);
    if (UART_Failed) UART_Send('*');
    else UART_Send(i);
  }*/

  /*for (i=0;i<10;i++)
  {
    UART_Send(XM_NAK);
    UART_Receive(1000);
    if (UART_Failed==false) break;
  }
  while(1)
  {
    delay_ms(1000);
    UART_Send(XM_ACK);
  }*/

  /*while(1)
  {
    UART_Send(XM_NAK);
    delay_ms(5000);
    //Doesn't work: 0x10,0x18,0x04
    UART_Send(i);
    delay_ms(1000);
  }*/

  /*unsigned char derpdom[10];
  int derpo=0;
  while(1)
  {
    UART_Text("\r\nData: ");
    for (i=0;i<derpo;i++)
    {
      UART_Hex(derpdom[i]);
      UART_Send(' ');
    }
    delay_ms(1000);
    ROM_data=UART_Receive(1);
    if (UART_Failed==false)
    {
      derpdom[derpo]=ROM_data;
      derpo++;
    }
  }*/

  /*UART_Send(XM_NAK);
  for (i=0;i<132;i++) ROM_packet[i]=UART_Receive(0);
  while(1)
  {
    UART_Text("\r\n\r\n");
    for (i=0;i<132;i++)
    {
      UART_Hex(ROM_packet[i]);
      UART_Send(' ');
    }
    delay_ms(2000);
  }*/

  while(1)
  {
    UART_Text("\r\n\r\nEERPOM Programmer v0.1\r\n");
    UART_Text("======================\r\n");
    UART_Text("R: Read a byte from ROM\r\n");
    UART_Text("W: Write a byte to ROM\r\n");
    UART_Text("L: XMODEM a file from ROM\r\n");
    UART_Text("S: XMODEM a file to ROM\r\n");
    UART_Text("C: XMODEM a file to ROM and confirm\r\n");
    UART_Text("D: Display data in ROM\r\n");
    UART_Text("P: Page write settings\r\n");

    result=UART_Receive(0);
    //UART_Send(result);

    if ((result=='R')||(result=='r'))
    {
      UART_Text("\r\nAddress to read(0-FFFF): ");
      ROM_address=UART_GetHex(2);
      UART_Text("\r\nReading ");
      UART_Hex(ROM_address>>8);
      UART_Hex(ROM_address&0xFF);
      UART_Text("...");
      ROM_data=ROM_Read(ROM_address);
      UART_Text("\r\nData: ");
      UART_Hex(ROM_data);
      UART_Send('(');
      UART_Send(UART_Filter(ROM_data));
      UART_Send(')');
      UART_Text("\r\n\r\nPress any key...");
      UART_Receive(0);
      UART_Text("\r\n");
    }
    else if ((result=='W')||(result=='w'))
    {
      UART_Text("\r\nAddress to write(0-FFFF): ");
      ROM_address=UART_GetHex(2);
      UART_Text("\r\nData to write(0-FF): ");
      ROM_data=UART_GetHex(1);
      UART_Text("\r\nWriting ");
      UART_Hex(ROM_data);
      UART_Text(" to ");
      UART_Hex(ROM_address>>8);
      UART_Hex(ROM_address&0xFF);
      UART_Text("...");
      if (!ROM_Write(ROM_address,ROM_data,true))
      {
        UART_Text("\r\nWRITE TIMEOUT");
      }
      else
      {
        UART_Text("\r\nConfirming...");
        ROM_check=ROM_Read(ROM_address);

        if (ROM_data==ROM_check) UART_Text("SUCCESS");
        else
        {
          UART_Text("FAILED");
          UART_Text("\r\nData: ");
          UART_Hex(ROM_check);
        }
      }
      UART_Text("\r\n\r\nPress any key...");
      UART_Receive(0);
      UART_Text("\r\n");
    }
    else if ((result=='L')||(result=='l'))
    {
      UART_Text("\r\nAddress to start read(0-FFFF): ");
      ROM_address=UART_GetHex(2);
      UART_Text("\r\nAddress to stop read(0-FFFF): ");
      ROM_address2=UART_GetHex(2);

      if (ROM_address2<=ROM_address)
      {
        UART_Text("\r\n\r\nStart address must be greater than end address.");
      }
      else if ((ROM_address2-ROM_address)>32640)
      {
        UART_Text("\r\n\r\nRange must be less than 32,640(7F80) bytes for XMODEM.");
      }
      else
      {
        UART_Text("\r\n\r\nReady to send. Ctrl+Z to cancel.");
        ROM_pkt_counter=0;//packet counter
        ROM_counter=ROM_address;
        ROM_done=false;

        //int dbg_cnt=0,NAK_cnt=0;
        //int dbg_junk[20];

        while(ROM_done==false)
        {
          ROM_data=UART_Receive(0);

          //dbg_junk[dbg_cnt]=ROM_data;
          //dbg_cnt++;

          if (ROM_data==0x1A) ROM_done=true;//Ctrl+Z pressed
          else if (ROM_data==XM_ACK)
          {
            ROM_pkt_counter++;
            ROM_counter+=128;
          }
          //else if (ROM_data==XM_NAK) NAK_cnt++;

          if ((ROM_data==XM_ACK)||(ROM_data==XM_NAK))
          {
            ROM_crc=0;
            UART_Send(XM_SOH);
            UART_Send(ROM_pkt_counter+1);
            UART_Send(254-ROM_pkt_counter);
            for (i=0;i<128;i++)
            {
              if ((ROM_counter+i)<=ROM_address2)
              {
                ROM_check=ROM_Read(ROM_counter+i);
                ROM_crc+=ROM_check;
                UART_Send(ROM_check);
              }
              else
              {
                ROM_crc+=0x1A;
                UART_Send(0x1A);
              }

              if ((ROM_counter+i)>=ROM_address2) ROM_done=true;
            }
            UART_Send(ROM_crc);
            if (ROM_done)
            {
              UART_Send(XM_EOT);
              delay_ms(100);
              UART_Text("\r\n\r\nTransfer complete");
            }
          }//ACK or NAK
        }//while
        /*UART_Text("\r\n\r\nCharacter count: ");
        UART_Hex(dbg_cnt);
        UART_Text(" ACKs: ");
        UART_Hex(ROM_pkt_counter);
        UART_Text(" NAKs: ");
        UART_Hex(NAK_cnt);
        UART_Text("\r\n");
        for (i=0;i<dbg_cnt;i++)
        {
          UART_Hex(dbg_junk[i]);
          UART_Text(" ");
        }*/
      }//if in range
      UART_Text("\r\n\r\nPress any key...");
      UART_Receive(0);
      UART_Text("\r\n");
    }
    else if ((result=='S')||(result=='s')||(result=='C')||(result=='c'))
    {
      UART_Text("\r\nAddress to start write(0-FFFF): ");
      ROM_address=UART_GetHex(2);
      UART_Text("\r\n\r\nReady to receive. Ctrl+Z to cancel.");

      while(1)
      {
        UART_Send(XM_NAK);
        ROM_data=UART_Receive(1000);
        if (ROM_data==0x1A) break;//Ctrl+Z pressed
        else if (ROM_data==XM_SOH) break; //first byte of packet
      }

      if (ROM_data==XM_SOH)
      {
        ROM_state=0; //first time through use already read byte
        ROM_done=false;
        ROM_NAK_tries=0;
        while(ROM_done==false)
        {
          //do twice because tera term resends first packet
          for (ROM_repeat=0;ROM_repeat<10;ROM_repeat++)
          {
            for (ROM_pkt_counter=0;ROM_pkt_counter<132;ROM_pkt_counter++)
            {
              if (ROM_pkt_counter==0)
              {
                if (ROM_state==0) //could leave out actually
                {
                  ROM_packet[ROM_pkt_counter]=ROM_data;
                  ROM_state=1;
                }
                else
                {
                  ROM_packet[ROM_pkt_counter]=UART_Receive(1000); //change timeout?
                  if (UART_Failed)
                  {
                    ROM_done=true;
                    ROM_pkt_debug=ROM_pkt_counter;
                    ROM_pkt_counter=132;
                    ROM_state=2;//timeout
                    //This does not cancel anything!
                    UART_Send(XM_CAN);
                    delay_ms(50);
                    UART_Send(XM_CAN);
                    delay_ms(50);
                    UART_Send(XM_CAN);
                    //UART_Text("\r\nUART receive error(first iteration)");
                  }
                  if (ROM_packet[0]==XM_EOT)
                  {
                    for (i=1;i<10;i++) ROM_packet[i]=0;
                    for (i=1;i<10;i++)
                    {
                      ROM_packet[i]=UART_Receive(10);
                      if (UART_Failed) break;
                    }
                    ROM_done=true;
                    ROM_pkt_debug=ROM_pkt_counter;
                    ROM_pkt_counter=132;
                    ROM_state=3;//finished successfully
                  }
                }
              }
              else
              {
                ROM_packet[ROM_pkt_counter]=UART_Receive(500);
                if (UART_Failed)
                {
                  ROM_done=true;
                  ROM_pkt_debug=ROM_pkt_counter;
                  ROM_pkt_counter=132;
                  ROM_state=4;//timeout
                  UART_Send(XM_CAN);
                  delay_ms(50);
                  UART_Send(XM_CAN);
                  delay_ms(50);
                  UART_Send(XM_CAN);
                  //UART_Text("\r\nUART receive error(later iteration)");
                }
              }
            }
            ROM_data=UART_Receive(10);//This may need adjusting!
            if (UART_Failed) ROM_repeat+=0x10;
            else ROM_state=0;
            //When entire transfer completes this times out
          }
          ROM_counter=0;

          if (UC0IFG&UCA0RXIFG)
          {
            ROM_data=UART_Receive(1);
            while(1)
            {
              //Need to handle overflow errors here

              UART_Text("\r\nUnread character: ");
              UART_Hex(ROM_data);
              UART_Text(" Code: ");
              UART_Hex(ROM_state);
              UART_Text(" Error: ");
              UART_Hex(UART_Flag);
              delay_ms(1000);
            }
          }

          //write data
          if (ROM_packet[0]==XM_SOH)
          {
            ROM_crc=0;
            for (i=0;i<131;i++) ROM_crc+=ROM_packet[i];
            if ((ROM_packet[1]!=(255-ROM_packet[2]))||(ROM_crc!=ROM_packet[131]))
            {
              if (ROM_NAK_tries<9)
              {
                ROM_NAK_tries++;
                UART_Send(XM_NAK);
              }
              else
              {
                ROM_done=true;
                ROM_state=5;//10 NAKs exceeded
                UART_Send(XM_CAN);
                delay_ms(50);
                UART_Send(XM_CAN);
                delay_ms(50);
                UART_Send(XM_CAN);
              }
            }
            else
            {
              ROM_address2=ROM_address+((((unsigned int)ROM_packet[1])-1)*128);
              if (PageEnable)
              {
                unsigned int debug_ints[6],di=0,counters[6];
                unsigned int init_pc;
                ROM_PageCheck=ROM_address2&PageSize;
                init_pc=ROM_PageCheck;
                for (ROM_counter=0;ROM_counter<128;ROM_counter++)
                {
                  if (((ROM_address2+ROM_counter)&PageSize)!=ROM_PageCheck)
                  {
                    ROM_Write(ROM_address2+ROM_counter,ROM_packet[ROM_counter+3],false);
                    //delay_ms(50);
                    ROM_PageCheck=(ROM_address2+ROM_counter)&PageSize;
                    /*if (di<6)
                    {
                      debug_ints[di]=ROM_PageCheck;
                      counters[di]=ROM_address2+ROM_counter;
                      di++;
                    }
                    else
                    {
                      while(1)
                      {
                        UART_Text("\r\n\r\nPageSize: ");
                        UART_Hex16(PageSize);
                        UART_Text("\r\nInit PC: ");
                        UART_Hex16(init_pc);
                        for (i=0;i<6;i++)
                        {
                          UART_Text("\r\n");
                          UART_Int(i+1);
                          UART_Text(". PC: ");
                          UART_Hex16(debug_ints[i]);
                          UART_Text(" - ");
                          UART_Hex16(counters[i]);
                        }
                        delay_ms(3000);
                      }
                    }*/
                  }
                  else ROM_Write(ROM_address2+ROM_counter,ROM_packet[ROM_counter+3],false);
                }
              }
              else
              {
                for (ROM_counter=0;ROM_counter<128;ROM_counter++)
                {
                  ROM_Write(ROM_address2+ROM_counter,ROM_packet[ROM_counter+3],true);
                  //ROM_packet[ROM_counter+3]='.';
                  //UART_Text("\r\nWriting ");
                  //UART_Filter(ROM_packet[ROM_counter+3]);
                  //UART_Text(" to ");
                  //UART_Hex((ROM_address2+ROM_counter)>>8);
                  //UART_Hex((ROM_address2+ROM_counter)&0xFF);
                }
              }
              if ((result=='C')||(result=='c'))
              {
                for (ROM_counter=0;ROM_counter<128;ROM_counter++)
                {
                  ROM_data=ROM_Read(ROM_address2+ROM_counter);
                  if (ROM_data!=ROM_packet[ROM_counter+3])
                  {
                    ROM_done=true;
                    ROM_state=6;//failed confirmation
                    UART_Send(XM_CAN);
                    delay_ms(50);
                    UART_Send(XM_CAN);
                    delay_ms(50);
                    UART_Send(XM_CAN);
                    delay_ms(50);

                    //ROM_counter=128;
                    break;
                  }
                }
              }
              //why? only for display?
              //for (ROM_counter=0;ROM_counter<128;ROM_counter++) ROM_packet[ROM_counter+3]='.';
              UART_Send(XM_ACK);
            }
          }
          else if (ROM_packet[0]!=XM_EOT)
          {
            /*while(1)
            {
              UART_Text("\r\n\r\nFirst character: ");
              UART_Hex(ROM_packet[0]);
              UART_Text(" Tries: ");
              UART_Hex(ROM_NAK_tries);
              UART_Text(" Code: ");
              UART_Hex(ROM_state);
              UART_Text("\r\n");
              for (i=3;i<131;i++)
              {
                UART_Send(UART_Filter(ROM_packet[i]));
                //UART_Send(' ');
              }
              delay_ms(1000);
            }*/
            ROM_done=true;
            ROM_state=7;//Unknown starting character
          }
          else UART_Send(XM_ACK);
        }//while(ROM_done==false)

        if (ROM_state!=3)
        {
          ROM_done=false;
          while(ROM_done==false)
          {
            UART_Text("\r\n\r\nError: Writing to ROM failed.");
            UART_Text("\r\nCode: ");
            UART_Hex(ROM_state);
            if (ROM_state==2) UART_Text(" (UART receive error(first iteration))\r\n");
            else if (ROM_state==4) UART_Text(" (UART receive error(later iteration))\r\n");
            else if (ROM_state==5) UART_Text(" (CRC or header errors exceeded 10)\r\n");
            else if (ROM_state==7)
            {
              UART_Text(" (Unknown first character: ");
              UART_Hex(ROM_packet[0]);
              UART_Text(")\r\n");
            }
            else if (ROM_state==6)
            {
              UART_Text(" Confirmation FAILED.\r\nExpected ");
              UART_Hex(ROM_packet[ROM_counter+3]);
              UART_Send('(');
              UART_Send(UART_Filter(ROM_packet[ROM_counter+3]));
              UART_Text(") at ");
              UART_Hex((ROM_address2+ROM_counter)>>8);
              UART_Hex((ROM_address2+ROM_counter)&0xFF);
              UART_Text(".\r\nReceived ");
              UART_Hex(ROM_data);
              UART_Send('(');
              UART_Send(UART_Filter(ROM_data));
              UART_Text(").\r\n");
            }

            UART_Text("UART Flag: ");
            UART_Hex(UART_Flag);
            UART_Text(" UART Error: ");
            UART_Hex(UART_Error);
            UART_Text(" Count: ");
            UART_Hex(ROM_pkt_debug);

            UART_Text("\r\n\r\nPress Ctrl+Z to return to the menu.\r\n");
            if (UART_Receive(5000)==0x1A) ROM_done=true;
          }
        }
        else
        {
          delay_ms(100);
          UART_Text("\r\n\r\nTransfer complete");

          UART_Text("\r\n\r\nPress any key...");
          UART_Receive(0);
        }
      }
      UART_Text("\r\n");
    }
    else if ((result=='D')||(result=='d'))
    {
      UART_Text("\r\nAddress to start read(0-FFFF): ");
      ROM_address=UART_GetHex(2);
      UART_Text("\r\nAddress to stop read(0-FFFF): ");
      ROM_address2=UART_GetHex(2);
      UART_Text("\r\n\r\n                       Range: ");
      UART_Hex(ROM_address>>8);
      UART_Hex(ROM_address&0xFF);
      UART_Text("-");
      UART_Hex(ROM_address2>>8);
      UART_Hex(ROM_address2&0xFF);
      UART_Text("\r\n     ");
      for (i=0;i<16;i++)
      {
        UART_Hex(i);
        UART_Send(' ');
      }
      UART_Text("\r\n     ");
      for (i=0;i<65;i++) UART_Send('-');

      //UART_Text("\r\n");
      for (i=0;i<16;i++) ROM_disp[i]=' ';
      for (ROM_counter=((ROM_address/16)*16);ROM_counter<=ROM_address2;ROM_counter++)
      {
        if ((ROM_counter%16)==0)
        {
          if (ROM_counter!=((ROM_address/16)*16))
          {
            UART_Send('|');
            for (i=0;i<16;i++)
            {
              UART_Send(UART_Filter(ROM_disp[i]));
              ROM_disp[i]=' ';
            }
            UART_Send('|');
          }
          UART_Text("\r\n");
          UART_Hex(ROM_counter>>8);
          UART_Hex(ROM_counter&0xF0);
          UART_Send('|');
        }
        if (ROM_counter>=ROM_address)
        {
          ROM_data=ROM_Read(ROM_counter);
          ROM_disp[ROM_counter%16]=ROM_data;
          UART_Hex(ROM_data);
        }
        else
        {
          UART_Text("  ");
        }
        if ((ROM_counter%16)!=15) UART_Send(' ');
      }
      if ((ROM_counter%16)!=0)
      {
        for (ROM_counter=(ROM_counter%16);ROM_counter<16;ROM_counter++)
        {
          UART_Text("  ");
          if (ROM_counter!=15) UART_Send(' ');
        }
      }
      UART_Send('|');
      for (i=0;i<16;i++) UART_Send(UART_Filter(ROM_disp[i]));
      UART_Send('|');

      UART_Text("\r\n\r\nPress any key...");
      UART_Receive(0);
      UART_Text("\r\n");
    }
    else if ((result=='P')||(result=='p'))
    {
      UART_Text("\r\nEnable page writing(Y/N): ");
      ROM_done=false;
      while(ROM_done==false)
      {
        result=UART_Receive(0);
        if ((result=='Y')||(result=='y'))
        {
          PageEnable=true;
          UART_Text("Y\r\nPage size(2-256): ");
          PageSize=~(UART_GetInt(3,256)-1);
          ROM_done=true;
        }
        else if ((result=='N')||(result=='n'))
        {
          PageEnable=false;
          UART_Send('N');
          ROM_done=true;
        }
      }

      UART_Text("\r\n\r\nPress any key...");
      UART_Receive(0);
      UART_Text("\r\n");
    }//key
  }//while(1)
}//main

__attribute__((interrupt(TIMER0_A0_VECTOR))) static void TA0_ISR(void)
{
  UART_Failed=true;
  TA0CTL=MC_0|ID_0|TASSEL_1|TACLR;
  //TA0CTL&=~TAIFG;//should be unnecessary
  if (UART_Flag!=1)
  {
    while(1)
    {

    }
  }
}

__attribute__((interrupt(PORT1_VECTOR))) static void P1_ISR(void)
{

}

unsigned int UART_GetHex(unsigned int bytes)
{
  unsigned char inbuff[4];
  unsigned int inptr=0,i;
  unsigned int temp_hex=0;
  unsigned char result;
  inptr=0;
  do
  {
    result=UART_Receive(0);
    if (inptr<(bytes*2))
    {
      if ((result>='a')&&(result<='f')) result-=32;
      if (((result>='A')&&(result<='F'))||((result>='0')&&(result<='9')))
      {
        inbuff[inptr]=result;
        inptr++;
        UART_Send(result);
      }
    }
    if (result==8) //backspace
    {
      if (inptr)
      {
        inptr--;
        UART_Send(8);
        UART_Send(' ');
        UART_Send(8);
      }
    }
  } while(result!=13);
  for (i=0;i<inptr;i++)
  {
    temp_hex<<=4;
    if ((inbuff[i]>='A')&&(inbuff[i]<='F')) temp_hex+=(inbuff[i]-'A'+10);
    else if ((inbuff[i]>='0')&&(inbuff[i]<='9')) temp_hex+=(inbuff[i]-'0');
  }
  return temp_hex;
}

unsigned int UART_GetInt(unsigned int places, unsigned int maxint)
{
  unsigned char inbuff[5];
  unsigned int inptr=0,i;
  unsigned int temp_int,old_temp;
  unsigned char result;
  bool done=false;
  inptr=0;
  while(done==false)
  {
    do
    {
      result=UART_Receive(0);
      if (inptr<(places))
      {
        if ((result>='0')&&(result<='9'))
        {
          inbuff[inptr]=result;
          inptr++;
          UART_Send(result);
        }
      }
      if (result==8) //backspace
      {
        if (inptr)
        {
          inptr--;
          UART_Send(8);
          UART_Send(' ');
          UART_Send(8);
        }
      }
    } while(result!=13);
    done=true;
    temp_int=0;
    for (i=0;i<inptr;i++)
    {
      old_temp=temp_int;
      temp_int*=10;
      if (temp_int<old_temp) //overflow
      {
        done=false;
        break;
      }
      old_temp=temp_int;
      temp_int+=(inbuff[i]-'0');
      if (temp_int<old_temp) //overflow
      {
        done=false;
        break;
      }
    }
    if ((maxint)&&(temp_int>maxint)) done=false;
  }
  return temp_int;
}

unsigned char UART_Filter(unsigned char byte)
{
  if ((byte>=0)&&(byte<=32)) return ' ';
  if ((byte>=127)&&(byte<=159)) return ' ';
  return byte;
}

unsigned char ROM_Read(unsigned int read_address)
{
  SPI_Send(read_address&0xFF);//low byte
  SPI_Send(read_address>>8);//high byte
  SPI_Send(0x00);//data(dummy)
  P1OUT&=~SR_OUT_LATCH;
  //delay_ms(1);
  P1OUT|=SR_OUT_LATCH;
  //delay_ms(100);

  P1OUT&=~SR_IN_LATCH;
  //delay_ms(1);
  P1OUT|=SR_IN_LATCH;
  return SPI_Send(0x00);
}

bool ROM_Write(unsigned int write_address, unsigned char data, bool poll)
{
  unsigned char read_data;
  unsigned int read_count;
  SPI_Send(write_address&0xFF);//low byte
  SPI_Send(write_address>>8);//high byte
  SPI_Send(data);//data

  P1OUT&=~SR_OUT_LATCH;
  P1OUT|=SR_OUT_LATCH;

  P2OUT&=~EEPROM_OE; //Reversed
  P1OUT&=~SR_OUT_OE;

  P2OUT&=~EEPROM_WE; //Reversed
  P2OUT|=EEPROM_WE;  //Reversed
  P2OUT&=~EEPROM_WE; //Reversed

  P1OUT|=SR_OUT_OE;
  P2OUT|=EEPROM_OE;  //Reversed
  //Finished writing

  //Poll for write finish
  if (poll)
  {
    //P1OUT|=SR_OUT_OE;
    //P2OUT|=EEPROM_OE;  //Reversed

    for (read_count=0;read_count<0xFFFF;read_count++) //could be smaller
    {
      P1OUT&=~SR_IN_LATCH;
      P1OUT|=SR_IN_LATCH;
      read_data=SPI_Send(0x00);
      if ((data&0x80)==(read_data&0x80))
      {
        //UART_Text("\r\nTries: ");
        //UART_Hex(read_count);
        return true;
      }
    }
    return false;
  }
  else return true;
}

unsigned char SPI_Send(unsigned char data)
{
  unsigned char buff;
  while(!(UC0IFG&UCB0TXIFG));
  UCB0TXBUF=data;
  while (UCB0STAT & UCBUSY);

  buff=UCB0RXBUF;
  return buff;
}

void UART_Send(unsigned char data)
{
  while(!(UC0IFG&UCA0TXIFG));
  UCA0TXBUF=data;
  while (UCA0STAT & UCBUSY);
}

void UART_Text(const char *data)
{
  int i=0;
  while (data[i]) UART_Send(data[i++]);
}

unsigned char UART_Receive(int timeout)
{
  unsigned int dummy;
  UART_Flag=1;
  UART_Failed=false;

  if (timeout)
  {
    //TA0CTL&=~TAIFG; //shouldnt be necessary
    TA0CTL=MC_0|ID_0|TASSEL_1|TACLR; //also shouldnt be neessary
    TA0CCR0=timeout*12;
    TA0CTL=MC_1|ID_0|TASSEL_1|TACLR;
  }

  if (UCA0STAT&UCRXERR)
  {
    //add more specific error checking here
    UART_Failed=true;
    UART_Error=0;
    if (UCA0STAT&UCRXERR) UART_Error|=1;
    if (UCA0STAT&UCFE) UART_Error|=2;
    if (UCA0STAT&UCOE) UART_Error|=4;
    if (UCA0STAT&UCPE) UART_Error|=8;
    if (UCA0STAT&UCBRK) UART_Error|=16;
    dummy=UCA0RXBUF;
    UART_Flag=2;
    TA0CTL=MC_0|ID_0|TASSEL_1|TACLR;//shouldnt be necessary
    return 0;
  }

  while (!(UC0IFG&UCA0RXIFG))
  {
    if (UART_Failed)//time out
    {
      //debug(1000,1000);
      //debug(1000,1000);
      UART_Flag=3;
      return 0;
    }
  }
  UART_Flag=4;
  if (timeout) TA0CTL=MC_0|ID_0|TASSEL_1|TACLR;
  UART_Flag=5;
  return UCA0RXBUF;
}

void UART_Hex(unsigned char data)
{
  unsigned char buff;
  buff=data/16;
  if (buff>9) buff+=55;
  else buff+='0';
  UART_Send(buff);
  buff=data%16;
  if (buff>9) buff+=55;
  else buff+='0';
  UART_Send(buff);
}

void UART_Hex16(unsigned int data)
{
  UART_Hex(data>>8);
  UART_Hex(data&0xFF);
}

void UART_Int(unsigned int data)
{
  unsigned int divisor=10000;
  bool started=false;
  while (divisor)
  {
    if (data/divisor==0)
    {
      if (started) UART_Send(data/divisor+'0');
    }
    else
    {
      UART_Send(data/divisor+'0');
      started=true;
    }
    data-=((data/divisor)*divisor);
    divisor/=10;
  }
  if (started==false) UART_Send('0');
}

void delay_ms(int ms)
{
  while (ms--) __delay_cycles(DELAY_TIME);
}

//===========================================================================//

void DbgByte(unsigned char data)
{
  DBG_PORT&=~DBG_OUT;
  delay_ms(DBG_TIME);
  DBG_PORT|=DBG_OUT;
  delay_ms(DBG_TIME);
  int i;
  for (i=0;i<8;i++)
  {
    if (data&1) DBG_PORT|=DBG_OUT;
    else DBG_PORT&=~DBG_OUT;
    delay_ms(DBG_TIME);
    data>>=1;
  }
  DBG_PORT|=DBG_OUT;
  delay_ms(DBG_TIME);
}

void DbgNib(unsigned char data)
{
  DbgByte(MODE_NUM|MODE_4);
  DbgByte(data&0xF);
}

void DbgNum(unsigned char data)
{
  DbgByte(MODE_NUM|MODE_8);
  DbgByte(data);
}

void DbgNum(char data)
{
}

void DbgNum(unsigned int data)
{
  DbgByte(MODE_NUM|MODE_16);
  DbgByte(data&0xFF);
  DbgByte(data>>8);
}

void DbgNum(int data)
{

}

void DbgNum(long unsigned int data)
{
  DbgByte(MODE_NUM|MODE_32);
  DbgByte(data&0xFF);
  DbgByte((data>>8)&0xFF);
  DbgByte((data>>16)&0xFF);
  DbgByte((data>>24)&0xFF);
}

void DbgNum(long int data)
{

}

void DbgDotOn()
{
  DbgByte(MODE_STATUS|MODE_DOT_ON);
}

void DbgDotOff()
{
  DbgByte(MODE_STATUS|MODE_DOT_OFF);
}

void DbgDec()
{
  DbgByte(MODE_STATUS|MODE_DEC);
}

void DbgHex()
{
  DbgByte(MODE_STATUS|MODE_HEX);
}

void DbgBlinkOn()
{
  DbgByte(MODE_STATUS|MODE_BLINK_ON);
}

void DbgBlinkOff()
{
  DbgByte(MODE_STATUS|MODE_BLINK_OFF);
}
