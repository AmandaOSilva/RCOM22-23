// Link layer protocol implementation

#include <sys/termios.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "link_layer.h"
#include <../cable/cable.c>

#define BYTES_PER_PACKAGE 4 * 1024 // 4KB

#define TRANSMITTER 0
#define RECEIVER 1

#define INFO_LENGTH 4

//ADDRESS CONSTANTS
#define EM_CMD 0x03 //0b00000011

//CONTROL CONSTANTS
#define SET 0x03  //0b00000011 COMMAND
#define DISC 0x0b //0b00001011 COMMAND
#define UA 0x07   //0b00000111 ANSWER
#define RR0 0b00000101 //ANSWER
#define RR1 0b10000101 //ANSWER
#define REJ0 0b00000001 //ANSWER
#define REJ1 0b10000001 //ANSWER
#define II0 0b00000000 //ANSWER
#define II1 0b01000000 //ANSWER

//FRAME FLAG
#define FR_FLAG 0x7e
#define ESC_FLAG 0x7d
#define FR_SUB 0x5e
#define ESC_SUB 0x5d

//INDEXES
#define FLAG_IND 0
#define ADDR_IND 1
#define CTRL_IND 2
#define BCC_IND 3
#define END_FLAG_IND 4


typedef enum SET_STATES {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK
} set_states;

int alarmFlag = 0;

////////////////////////////////////////////////
// UTILS
////////////////////////////////////////////////


void alarmHandler(int sig, LinkLayer connectionParameters ){
    printf( "[ALARM] Timeout\n");
    alarmFlag = 1;
    connectionParameters.nRetransmissions++;
}




void sendSupFrame(int fd, unsigned char addr, unsigned char cmd) {
    unsigned char *frame = malloc(5);
    frame[FLAG_IND] = FR_FLAG;
    frame[CTRL_IND] = cmd;
    frame[ADDR_IND] = addr;
    frame[BCC_IND] = frame[ADDR_IND] ^ frame[CTRL_IND];
    frame[END_FLAG_IND] = FR_FLAG;
    write(fd, frame, 5);
    free(frame);
}


int receiveSupFrame( ) {

    return 1;

}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    struct termios *oldtio;
    struct termios *newtio;
    int fd;
    volatile int STOP = FALSE;
    char port[50] = {};
    openSerialPort(connectionParameters.serialPort, oldtio, newtio);
    signal(SIGALRM, alarmHandler);
    unsigned char *receivedFrame = malloc(5);
    if (connectionParameters.role ==  'LlTx') {
        while (TRUE) {
            /* enviar SET e esperar UA */
            printf("Abrindo em modo Transmitter\n");
            if (connectionParameters.nRetransmissions > 3) {
                printf("Nao foi possivel estabelecer conexao\n");
                return -1;
            }

            //printf("Tentativa: %d\n", numAttempts);
            //Envia SET
            sendSupFrame(fd, EM_CMD, SET);

            /* Wait for the UA */
            if (!receiveSupFrame()) continue;
            printf("Conexao estabelecida em modo Transmitter\n");
            break;
        }
    } //Else  Reciver

    return fd;
}




////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
    // TODO




    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    // TODO

    // ESQUELETO

    unsigned char *receivedFrame = malloc(5);
    if ("Transmiter") {
        printf("Fechando em modo Trasmitter\n");
        /* enviar DISC, esperar DISC e enviar UA e fecha conexao */
        while (TRUE) {
            sendSupFrame(showStatistics, EM_CMD, DISC);
            // Espera DISC
            if (!receiveSupFrame()) continue;
            break;
        }
        sendSupFrame(showStatistics, EM_CMD, UA);
        close(showStatistics);
        printf("Trasmitter fechado com sucesso\n");
    } else { // RECEIVER
        /* espera DISC, envia DISC, espera UA e fecha conexao */
        printf("Fechando em modo Receiver\n");
        while (TRUE) {
            // Espera o DISC
            if (!receiveSupFrame()) continue;
            break;
        }
        while (TRUE) {
            // Envia o DISC
            sendSupFrame(showStatistics, EM_CMD, DISC);
            // Esperar o UA
            if (!receiveSupFrame()) continue;
            break;
        }
        close(showStatistics);
        printf("Receiver fechado com sucesso\n");
    }

    return 1;
}
