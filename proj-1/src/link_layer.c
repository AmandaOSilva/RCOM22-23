// Link layer protocol implementation

#include <termios.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/link_layer.h"
#include <stdbool.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <termios.h>
#include <unistd.h>

#define BYTES_PER_PACKAGE 1 * 1024 // 4KB

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

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400


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
int fd;


////////////////////////////////////////////////
// UTILS
////////////////////////////////////////////////

// Returns: serial port file descriptor (fd).
int openSerialPort(const char *serialPort, struct termios *oldtio, struct termios *newtio)
{
    int fd = open(serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0)
        return -1;

    // Save current port settings
    if (tcgetattr(fd, oldtio) == -1)
        return -1;
    memset(newtio, 0, sizeof(*newtio));

    newtio->c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio->c_iflag = IGNPAR;
    newtio->c_oflag = 0;
    newtio->c_lflag = 0;
    newtio->c_cc[VTIME] = 1; // Inter-character timer unused
    newtio->c_cc[VMIN] = 0;  // Read without blocking

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, newtio) == -1)
        return -1;

    return fd;
}


unsigned char calculateBCC(const unsigned char *buf, int size) {
    unsigned char BCC = 0;
    for (int i = 0; i < size; i++) {
        BCC ^= buf[i]; //Bitwise exclusive OR and assignment
    }
    return BCC;
}

void alarmHandler(int sig, LinkLayer *connectionParameters) {
    printf("[ALARM] Timeout\n");
    alarmFlag = 1;
    connectionParameters->nRetransmissions++;
}

void sendSupFrame(unsigned char addr, unsigned char cmd) {
    unsigned char *frame = malloc(5);
    frame[FLAG_IND] = FR_FLAG;
    frame[ADDR_IND] = addr;
    frame[CTRL_IND] = cmd;
    frame[BCC_IND] = frame[ADDR_IND] ^ frame[CTRL_IND];
    frame[END_FLAG_IND] = FR_FLAG;
    write(fd, frame, 5);
    free(frame);
}

int receiveSupFrame(unsigned char *frame, unsigned char addr, unsigned char cmd) {
    unsigned char input;
    int res = 1;
    verify = 0;
    alarmFlag = 0;
    if ((role == LlTx) && (cmd == UA || cmd == DISC)) {
        alarm(3);
    }
    int fail = 0;
    set_states set_machine = START;
    while (!verify) {
        // printf("Lendo sup frame ...\n");
        if (read(fd, &input, sizeof(unsigned char)) < 0) {
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

int setRole(LinkLayerRole aRole) {
    role = aRole;
    return 1;
}

int receiveInfoFrame(unsigned char *frame, unsigned int *totalSize) {
    set_states setMachine = START;
    int res;
    verify = 0;
    int fail = 0;
    alarmFlag = 0;
    unsigned char input;
    unsigned int ind = 4, acc = 0, current = 0;
    while (!verify) {
        // printf("Lendo info frame ...\n");
        if (read(fd, &input, sizeof(unsigned char)) == -1) {
            printf("\nTIMEOUT!\tTentando reconectar!\n");
            return 0;
        }
        if (alarmFlag == 1) {
            printf("\nAlarm!\tTentando novamente\n");
            alarmFlag = 0;
            alarm(0);
            return 0;
        }
        frame[setMachine] = input;
        switch (setMachine) {
            case START:
                //printf("START STATE: %02x\n", input);
                if (input == FR_FLAG) {
                    setMachine = FLAG_RCV;
                }
                break;

            case FLAG_RCV:
                //printf("FLAG STATE: %02x\n", input);
                if (input == EM_CMD)
                    setMachine = A_RCV;
                else if (input != FR_FLAG)
                    setMachine = START;
                break;

            case A_RCV:
                //printf("A STATE: %02x\n", input);
                if (input == SEND_SEQ)
                    setMachine = C_RCV;
                else if (input == FR_FLAG)
                    setMachine = FLAG_RCV;
                else
                    setMachine = START;
                break;

            case C_RCV:
                //printf("C STATE: %02x\n", input);
                if (input == (EM_CMD ^ SEND_SEQ)) {
                    setMachine = BCC_OK;
                    break;
                }
                if (input == FR_FLAG)
                    setMachine = FLAG_RCV;
                else
                    setMachine = START;
                break;


            default:
                //  printf("INFO: %02x\n", input);
                if (input == FR_FLAG) {
                    alarm(0);
                    verify = 1;
                    break;
                }
                setMachine++;
                acc = setMachine;
                break;
        }

    }
    *totalSize = acc + 1;
    return 1;
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    struct termios oldtio;
    struct termios newtio;
    volatile int STOP = FALSE;
    char port[50] = {};
    fd = openSerialPort(connectionParameters.serialPort, &oldtio, &newtio);
    signal(SIGALRM, (void (*)(int)) alarmHandler);
    unsigned char *receivedFrame = malloc(5);
    setRole(connectionParameters.role);
    if (role == LlTx) {
        unsigned char *receivedFrame = malloc(5);

        while (TRUE) {
            /* enviar SET e esperar UA */
            printf("Abrindo em modo Transmitter\n");
            if (connectionParameters.nRetransmissions > 3) {
                printf("Nao foi possivel estabelecer conexao\n");
                return -1;
            }

            //Envia SET
            sendSupFrame(EM_CMD, SET);

            /* Wait for the UA */
            if (!receiveSupFrame(receivedFrame, EM_CMD, UA)) continue;
            printf("Conexao estabelecida em modo Transmitter\n");
            break;
        }
    } else { // RECEIVER
        printf("Abrindo em modo Receiver\n");
        /* espera o SET e devolve o UA */
        while (TRUE) {
            if (!receiveSupFrame(receivedFrame, EM_CMD, SET)) continue;
            sendSupFrame(EM_CMD, UA);
            printf("Conexao estabelecida em modo Receiver\n");
            break;
        }
    }

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
    int res;
    unsigned char bcc2 = calculateBCC(buf, bufSize);
    int bcc2Size = 1;
    int infoLength = bufSize + bcc2Size;
    unsigned char *info = malloc((infoLength));
    memcpy(info, buf, bufSize);
    memcpy(&info[bufSize], &bcc2, bcc2Size);
    unsigned char *stuffedData = stuffing(info, &infoLength);
    unsigned char *frameData = malloc(5);
    int sent = 0;
    int received_status;
    while (!sent) {
        // Send Info frame
        // | F | A | C | BBC1 | Data | BBC2 | F |
        unsigned char *frame = malloc((infoLength + 5));
        frame[FLAG_IND] = FR_FLAG;
        frame[ADDR_IND] = EM_CMD;
        frame[CTRL_IND] = SEND_SEQ;
        frame[BCC_IND] = frame[ADDR_IND] ^ frame[CTRL_IND];
        memcpy(&frame[4], stuffedData, infoLength);
        frame[4 + infoLength] = FR_FLAG;
        res = write(fd, frame, infoLength + 5);//
        free(frame);
        if (res < 0) {
            perror("Erro ao enviar frame\n");
        }
        alarm(10);
        received_status = receiveSupFrame(frameData, EM_CMD, REC_READY);
        alarm(0);
        if (received_status == 3) {
            printf("\nFrame rejeitado. Reenviando...\n");
            continue;
        } else if (received_status == 0) {
            continue;
        }
        if (frameData[CTRL_IND] != REC_READY)
            continue;
        sent = 1;
        break;


    }
    free(stuffedData);
    free(frameData);
    free(info);
    //new values of freq
    if (SEND_SEQ == II0) {
        SEND_SEQ = II1;
        REC_READY = RR0;
        REC_REJECTED = REJ0;
    } else {
        SEND_SEQ = II0;
        REC_READY = RR1;
        REC_REJECTED = REJ1;
    }
    return res;
}


////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    /* read() informacao (I) */
    unsigned char *frameData = malloc(BYTES_PER_PACKAGE * 2);
    unsigned char *stuffed_data;
    unsigned char *destuffed_data;

    int rejected = 0;
    int real_size = 0, data_size;
    // reset();
    int numAttempts = 0;
    while (numAttempts < 3) {
        rejected = 0;
        alarm(10);
        if (!receiveInfoFrame(frameData, &real_size)) {
            alarm(0);
            sendSupFrame(EM_CMD, REC_REJECTED);
            rejected = 1;
            continue;
        }
        alarm(0);
        unsigned char *final_frame = malloc(real_size);
        memcpy(final_frame, frameData, real_size);
        *&data_size = real_size - 5;
        unsigned char *data = (unsigned char *) malloc(*&data_size);
        stuffed_data = memcpy(data, &final_frame[4], *&data_size);
        destuffed_data = destuffing(stuffed_data, &data_size);
        for (int i = 0; i < 10; ++i) {
            printf(" %02x", destuffed_data[i]);
        }
        unsigned char received_bcc2 = destuffed_data[data_size - 1];

        unsigned char calculated_bcc2 = calculateBCC(destuffed_data, data_size - 1);
        printf(" Bcc2   |%d|, |%d|", (int)received_bcc2, (int)calculated_bcc2);
        if (received_bcc2 != calculated_bcc2) {
            printf("\nBCC2 not recognized\n");
            rejected = 1;
            sendSupFrame(EM_CMD, REC_REJECTED);
            continue;
        }
        sendSupFrame(EM_CMD, REC_READY);
        if (!rejected) {
            numAttempts = 0;
            memcpy(packet, destuffed_data, data_size - 1);
            free(frameData);
            free(final_frame);
            free(stuffed_data);
            free(destuffed_data);
            //new values of freq
            if (SEND_SEQ == II0) {
                SEND_SEQ = II1;
                REC_READY = RR0;
                REC_REJECTED = REJ0;
            } else {
                SEND_SEQ = II0;
                REC_READY = RR1;
                REC_REJECTED = REJ1;
            }
            return data_size - 1;
        }
        return -2;
    }
    return -1;
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    // TODO show statistics !!

    unsigned char *receivedFrame = malloc(5);
    if (role == LlTx) {
        printf("Fechando em modo Trasmitter\n");
        /* Envia o DISC, Espera o DISC do receptor e envia a UA e fecha connexion */
        while (TRUE) {
            sendSupFrame( EM_CMD, DISC);
            // Espera DISC
            if (!receiveSupFrame( receivedFrame, EM_CMD, DISC)) continue;
            break;
        }
        sendSupFrame(EM_CMD, UA);
        close(fd);
    } else { // RECEIVER
        /* espera DISC, envia DISC, espera UA e fecha conexao */
        printf("Fechando em modo Receiver\n");
        while (TRUE) {
            // Espera o DISC
            if (!receiveSupFrame(receivedFrame, EM_CMD, DISC)) continue;
            break;
        }
        while (TRUE) {
            // Envia o DISC
            sendSupFrame(EM_CMD, DISC);
            // A Esperar o UA
            if (!receiveSupFrame( receivedFrame, EM_CMD, UA)) continue;
            break;
        }
        close(fd);
        printf("Receiver fechado com sucesso\n");
    }

    return 1;
}
