// Link layer protocol implementation

#include <sys/termios.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "link_layer.h"
#include <../cable/cable.c>
#include <stdbool.h>

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
#define FR_FLAG 0x7e // 01111110
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
unsigned char SEND_SEQ = II0;
unsigned char REC_READY = RR1;
unsigned char REC_REJECTED = REJ1;
int alarmFlag = 0;
int verify = 0;
LinkLayerRole role;

////////////////////////////////////////////////
// UTILS
////////////////////////////////////////////////


void alarmHandler(int sig, LinkLayer* connectionParameters ){
    printf( "[ALARM] Timeout\n");
    alarmFlag = 1;
    connectionParameters->nRetransmissions++;
}

void sendSupFrame(int fd, unsigned char addr, unsigned char cmd) {
    unsigned char *frame = malloc(5);
    frame[FLAG_IND] = FR_FLAG;
    frame[ADDR_IND] = addr;
    frame[CTRL_IND] = cmd;
    frame[BCC_IND] = frame[ADDR_IND] ^ frame[CTRL_IND];
    frame[END_FLAG_IND] = FR_FLAG;
    write(fd, frame, 5);
    free(frame);
}

int receiveSupFrame(int fd, unsigned char *frame, unsigned char addr, unsigned char cmd) {
    unsigned char input;
    int res = 1;
    verify = 0; // TODO REMEBEMBER o use this global on de llread
    alarmFlag = 0;
    if ((role ==  'LlTx') && (cmd == UA || cmd == DISC)) {
        alarm(3);
    }
    int fail = 0;
    set_states set_machine = START;
    while (!verify) {
        // printf("Lendo sup frame ...\n");
        if (read(fd, &input, sizeof(unsigned char)) < 0 ) {
            printf("\nTIMEOUT!\tRetrying connection!\n");
            return 0;
        }
        if (alarmFlag == 1) {
            printf("\nAlarm!\tTentando novamente\n");
            alarmFlag = 0;
            alarm(0);
            return 0;
        }
        switch (set_machine) {
            case START:
                frame[FLAG_IND] = input;
                if (input == FR_FLAG)
                    set_machine = FLAG_RCV;
                break;
            case FLAG_RCV:
                frame[ADDR_IND] = input;
                if (input == addr)
                    set_machine = A_RCV;
                else if (input != FR_FLAG)
                    set_machine = START;
                break;
            case A_RCV:
                frame[CTRL_IND] = input;
                if (input == REC_READY) {
                    if (input == cmd) {
                        set_machine = C_RCV;
                        res = 2;
                    } else if (input == FR_FLAG)
                        set_machine = FLAG_RCV;
                    else
                        set_machine = START;
                    break;
                } else if (input == REC_REJECTED) {
                    if (REC_READY == cmd) {
                        set_machine = C_RCV;
                        res = 3;
                    } else if (input == FR_FLAG)
                        set_machine = FLAG_RCV;
                    else
                        set_machine = START;
                    break;
                } else {
                    if (input == cmd)
                        set_machine = C_RCV;
                    else if (input == FR_FLAG)
                        set_machine = FLAG_RCV;
                    else
                        set_machine = START;
                    break;
                }
            case C_RCV:
                frame[BCC_IND] = input;

                if (input == (addr ^ (res == 2 ? cmd : (res == 3 ? REC_REJECTED : cmd)))) {
                    set_machine = BCC_OK;
                    break;
                }
                if (input == FR_FLAG)
                    set_machine = FLAG_RCV;
                else
                    set_machine = START;
                break;

            case BCC_OK:
                frame[END_FLAG_IND] = input;
                if (input == FR_FLAG) {
                    alarm(0);
                    return res;
                } else
                    set_machine = START;
                break;
            default:
                break;
        }
    }
    if (!fail && !verify) {
        alarm(0);
        return res;
    }
    return res;

}





unsigned char *stuffing(unsigned char *frame, unsigned int *size) {
    unsigned char *result;
    unsigned int escapes = 0, newSize;
    for (size_t i = 0; i < *size; i++)
        if (frame[i] == FR_FLAG || frame[i] == ESC_FLAG)
            escapes++;
    newSize = *size + escapes;
    result = malloc(newSize);
    int offset = 0;
    for (size_t i = 0; i < *size; i++) {
        if (frame[i] == FR_FLAG) { // FR_FLAG = 7e
            result[i + offset] = ESC_FLAG;// 0x7d
            result[i + offset + 1] = FR_SUB; //0x5e
            offset += 1;
        } else if (frame[i] == ESC_FLAG) { //esc_flag == 7d
            result[offset + i] = ESC_FLAG;// 0x7d
            result[offset + i + 1] = ESC_SUB;//0x5d
            offset += 1;
        } else {
            result[(i) + offset] = frame[i];
        }
    }
    *size = newSize;
    return result;
}

unsigned char *destuffing(unsigned char *frame, unsigned int *size) {
    unsigned char *result;
    unsigned int escapes = 0, newSize;
    for (int i = 0; i < *size; i++) {
        if (i < (*size - 1)) {
            if (frame[i] == ESC_FLAG && frame[i + 1] == FR_SUB)
                escapes++;

            if (frame[i] == ESC_FLAG && frame[i + 1] == ESC_SUB)
                escapes++;
        }
    }
    newSize = *size - escapes;
    *size = newSize;
    result = malloc(newSize);
    int offset = 0;
    for (size_t i = 0; i < newSize;) {
        if (frame[i + offset] == ESC_FLAG && frame[i + 1 + offset] == FR_SUB) {
            result[i++] = FR_FLAG;
            offset += 1;
        } else if (frame[i + offset] == ESC_FLAG && frame[i + 1 + offset] == ESC_SUB) {
            result[i++] = ESC_FLAG;
            offset += 1;
        } else
            result[i++] = frame[i + offset];
    }
    return result;
}
int setRole(LinkLayerRole aRole){
    role  = aRole;
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
    signal(SIGALRM, (void (*)(int)) alarmHandler);
    unsigned char *receivedFrame = malloc(5);
    setRole(connectionParameters.role);
    if (role ==  'LlTx') {
        unsigned char *receivedFrame = malloc(5);

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
            if (!receiveSupFrame(fd, receivedFrame, EM_CMD, UA)) continue;
            printf("Conexao estabelecida em modo Transmitter\n");
            break;
        }
    } //Else Receiver

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
        /* Envia o DISC, Espera o DISC do receptor e envia a UA e fecha connexion */
        while (TRUE) {
            sendSupFrame(showStatistics, EM_CMD, DISC);
            // Espera DISC
            if (!receiveSupFrame()) continue;
            break;
        }
        sendSupFrame(showStatistics, EM_CMD, UA);
        close(showStatistics);
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
