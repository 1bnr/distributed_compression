
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/twi.h>

#include "lufa.h"
#include "leds.h"
#include "i2c_master.h"

#define menuString "\
R/r: read\r\n\
W/w: write\r\n\
S/s: Set address\r\n"

#define F_CPU 16000000UL
#define F_SCL     100000UL // SCL frequency

extern uint16_t bytes_in_buffer;
extern uint8_t* buffer;

void slave_setup() {
  // set the addr to 0x51
  TWAR = (0x51<<1);
  // Set the clock frequency to 100khz
  TWBR = ((((16000000UL / F_SCL) / 1) - 16) / 2);
	TWCR |= (1<<TWEN)|(1<<TWEA); // Enables the TWI Interface.
}


int send_bytes(uint16_t bytes_to_send, uint8_t *buffer) {
  while ((TWCR & (1<<TWINT)) != (1 << TWINT)){
    USB_Mainloop_Handler();
    //printf("Waiting\r\n");
    // This is just a busy wait for the master to send the start signal
    // when the TWINT is set in TWCR it means there is a valid value in
    // the TWSR
  }
  // send the ack back

  // send master the number of bytes to read
  if(TWSR == 0xA8) {

    TWDR = (bytes_to_send>>8);
    TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN);
    while ((TWCR & (1<<TWINT)) != (1 << TWINT)){
      USB_Mainloop_Handler();
      printf("Waiting for master to receive byte\r\n");
    }
  } else {

    printf("Failed to send first byte TWSR:%hhx\r\n", TWSR);
    return -1;
  }

  // send the second byte
  if(TWSR == 0xB8) {
    TWDR = bytes_to_send;
    TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN);
    while ((TWCR & (1<<TWINT)) != (1 << TWINT)){
      USB_Mainloop_Handler();
      printf("Waiting for master to receive byte\r\n");
    }
  } else {

    printf("Failed to send second byte TWSR:%hhx\r\n", TWSR);
    return -1;
  }


  if(TWSR == 0xB8) {
    // This means that it is in the correct state
    // send back the ack
    TWDR = buffer[0];
    printf("SendingFirstBytes:%hhx\r\n",TWDR);
    printf("SendingFirstBytes:%hhx\r\n",buffer[0]);
    TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN);
    int i;
    for(i = 1; i < bytes_to_send; i++) {
      printf("sending byte:%d\r\n",i);
      while ((TWCR & (1<<TWINT)) != (1 << TWINT)){
        USB_Mainloop_Handler();
      }

      if(TWSR == 0xB8) {
        // This means that the last byte was received and is ready to send the next
        // load TWDR
        TWDR = buffer[i];
        printf("SendingNextBytes:%hhx\r\n",TWDR);
        printf("SendingNextBytes:%hhx\r\n",buffer[i]);
        // Send the signal to master to receive the next byte
        TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN);
      } else {
        // This is in a state I don't understand
        printf("Error with sending a byte state\r\n");
        printf("TWSR:%hhx\r\n", TWSR);
        return -1;
      }
    } // end of for loop
    return 0;
  } else {
    // This was in a state that is not sending so return -1 to indicate
    // and error;
    printf("Error with first state\r\n");
    printf("TWSR:%hhx\r\n", TWSR);
    return -1;
  }
}
// Returns the number of bytes read and buffer is byte buffer returned
uint16_t receive_bytes(uint8_t *buffer) {

  // Wait for the start signal
  while ((TWCR & (1<<TWINT)) != (1 << TWINT)){
    USB_Mainloop_Handler();
    printf("Waiting\r\n");
    // This is just a busy wait for the master to send the start signal
    // when the TWINT is set in TWCR it means there is a valid value in
    // the TWSR
  }
  if(TWSR == 0x60) {
    // Send back the ack by setting twint and tew
    TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN);

    while ((TWCR & (1<<TWINT)) != (1 << TWINT)){
      USB_Mainloop_Handler();
      printf("Waiting for byte 1\r\n");
    }
    uint8_t one = 0;
    uint8_t two = 0;
    if(TWSR == 0x80) {
      // This means that there is a byte to read in the twdr
      one = TWDR;
      // Now send back the ack
      TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN);
      // Now wait for byte two
      while ((TWCR & (1<<TWINT)) != (1 << TWINT)){
        USB_Mainloop_Handler();
        printf("Waiting for byte 2\r\n");
      }

      if(TWSR == 0x80) {
        two = TWDR;
        // Let master know that we got the bytes
        TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN);
        while(1) {
          uint16_t read_size = (one << 8) | two;
          buffer = (uint8_t*)malloc(read_size);
          int i;
          for(i = 0; i < read_size; i++) {
            // Wait for the byte to be snt
            while ((TWCR & (1<<TWINT)) != (1 << TWINT)){
              USB_Mainloop_Handler();
              printf("Waiting for data 2\r\n");
            }

            if(TWSR == 0x80) {
              // Here we would want to read the data and move on to the next
              buffer[i] = TWDR;

              // Send the ack
              TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN);
            } else {
              while(1) {
                USB_Mainloop_Handler();
                printf("We got a weird status");
                printf("TWSR:%hxx\r\n",TWSR);
                printf("TWCR:%hxx\r\n",TWCR);
                printf("TWDR:%hxx\r\n",TWDR);
              }
              // I don't know what the rest of the systems are
            }
          } // end of for loop
          // Done reading in the bytes so return the number of bytes read
          return read_size;


        }
      }else {
        while(1) {
          printf("Byte one error\r\n");
          printf("TWSR:%hxx\r\n",TWSR);
          printf("TWCR:%hxx\r\n",TWCR);
          printf("TWDR:%hxx\r\n",TWDR);
        }
    }
    } else {
      while(1) {
        printf("Byte one error\r\n");
        printf("TWSR:%hxx\r\n",TWSR);
        printf("TWCR:%hxx\r\n",TWCR);
        printf("TWDR:%hxx\r\n",TWDR);
      }
    }
  } else {
    // This means that it was told to send bytes, but we wanted to receive so send back the nack
    TWCR = (1<<TWINT) | (1<<TWEN);
    return -1;
  }

}

//extern volatile uint8_t in_ui_mode;
volatile uint8_t ui_stage = 0;
volatile uint8_t mem_location = 0x0;
void handleInput(char c) {
// WARNING: This uses a busy-wait, thus will block main loop until done
    const int COMMAND_BUFF_LEN = 16;
    int i;
    char command[COMMAND_BUFF_LEN + 1];

    USB_Mainloop_Handler();

    // Get chars until end-of-line received or buffer full
    for (i = 0; i < COMMAND_BUFF_LEN; i++) {
        // first char received as input parameter. next char fetched at bottom
        // of for-loop

        // if its backspace, reset index
        if (c == '\b') {
            i -= 2;
            printf("\b \b");
            continue;
        }
        // if newline, go back 1 to write over
        else if (c == '\n') {
            i--;
            continue;
        }
        // if return, we have complete input
        else if (c == '\r') {
            break;
        }

        // otherwise, we got a char - echo it back
        printf("%c", c);
        USB_Mainloop_Handler();
        command[i] = c;

        // in busy-wait until char received
        c = EOF;

        while (c == EOF) {
            USB_Mainloop_Handler();
            c = fgetc(stdin);
        }
    }

    // echo back the return
    printf("\r\n");
    USB_Mainloop_Handler();

    // buffer is full - ignore input
    if (i == COMMAND_BUFF_LEN) {
        printf("Buffer full\r\n");
        command[COMMAND_BUFF_LEN - 1] = '\r';
        USB_Mainloop_Handler();
        return;
    }

    command[i] = '\0';

    if (strlen(command) == 0) {
        return;
    }

    // handle input per ui_stage
    if (!ui_stage) handleCommand(command);
    else {
        // set new goal here
        mem_location = atoi(command);
        printf("new address is %d\r\n\n", mem_location);
        ui_stage = 0;
        return;
    }

    return;
}

void handleCommand(char *command) {
    uint8_t buffer[16];

    // Start with a char - need to change this to strings

    switch (command[0]) {
        case ('R'):
        case ('r'): {
            // READ COMMAND
            memset(&buffer, 0, 16);
            if (i2c_readReg(0x51, mem_location, buffer, 11))
            printf("ERROR in read\r\n");
          else
            for (int i=0;i< 16; i++)
            printf("addr: 0x%04x-> %c\r\n", (mem_location+i), (char)buffer[i]);
            break;
        }

        // W/w write
        case ('W'):
        case ('w'): {
            // WRITE COMMAND
            sprintf((char *)buffer, "hello world");
                if (i2c_writeReg(0x51, mem_location, buffer, 11))
                  printf("ERROR in write\r\n");
            break;
        }

        // S/s: Set read/write address
        case ('S'):
        case ('s'):
            printf("enter new address: \r\n");
            ui_stage = 1;
            break;
    case ('Z'):
    case ('z'):
      printf("waiting\r\n");
      uint8_t* buffer;
      buffer = (uint8_t *)malloc(sizeof(uint8_t) * 5);
      buffer[0]='h';
      buffer[1]='e';
      buffer[2]='l';
      buffer[3]='l';
      buffer[4]='o';
      printf("Buffer0:%hhx\r\n",buffer[0]);
      printf("Buffer1:%hhx\r\n",buffer[1]);
      printf("Buffer2:%hhx\r\n",buffer[2]);
      printf("Buffer3:%hhx\r\n",buffer[3]);
      printf("Buffer4:%hhx\r\n",buffer[4]);
      slave_setup();
      int received_bytes = send_bytes((uint16_t)5,buffer);
      if(received_bytes == -1) {
        printf("Send_bytes returned -1\r\n");
      } else {
        printf("Bytes received:%d\r\n",received_bytes );
      }
      break;
    case('T'):
    case('t'):
      //set the ready pin
      PORTD |= (1<<PORTD6);
      // First send the bytes we have
      // Because this is the first there are no bytes

      send_bytes(bytes_in_buffer,buffer);
        default:
            printf(menuString);
    }

    USB_Mainloop_Handler();
}