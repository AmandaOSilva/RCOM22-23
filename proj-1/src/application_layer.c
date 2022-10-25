// Application layer protocol implementation


#include "../include/link_layer.h"
#include "string.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {

    printf("Starting link-layer protocol application\n"
           "  - Serial port: %s\n"
           "  - Role: %s\n"
           "  - Baudrate: %d\n"
           "  - Number of tries: %d\n"
           "  - Timeout: %d\n"
           "  - Filename: %s\n",
           serialPort,
           role,
           baudRate,
           nTries,
           timeout,
           filename);


   // printf("paramentros: %s, %s,%d, %d, %s \n", serialPort, role, baudRate, nTries, timeout, filename);

    LinkLayer connectionParameters;

    strcpy(connectionParameters.serialPort, serialPort);
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    printf("Role param: |%s|\n",role);

    if (strcmp(role,"tx")) {
        printf("RX ==========================\n");
        connectionParameters.role = LlRx;;
        printf("ConnectionParameters\n"
               "  - Serial port: %s\n"
               "  - Role: %d\n"
               "  - Baudrate: %d\n"
               "  - Number of tries: %d\n"
               "  - Timeout: %d\n",
               connectionParameters.serialPort,
               connectionParameters.role,
               connectionParameters.baudRate,
               connectionParameters.nRetransmissions,
               connectionParameters.timeout);
        recept(connectionParameters, filename);
    } else {
        printf("TX ==========================\n");
        connectionParameters.role = LlTx;;
        printf("ConnectionParameters\n"
               "  - Serial port: |%s|\n"
               "  - Role: |%d|\n"
               "  - Baudrate: |%d|\n"
               "  - Number of tries: |%d|\n"
               "  - Timeout: |%d|\n",
               connectionParameters.serialPort,
               connectionParameters.role,
               connectionParameters.baudRate,
               connectionParameters.nRetransmissions,
               connectionParameters.timeout);
        transmit(connectionParameters, filename);
    }

}
