// Application layer protocol implementation

#include "link_layer.h"
#include "string.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {
    printf("paramentros: %s, %s", serialPort, role);

    LinkLayer connectionParameters;
//            {
//            serialPort,
//            role,
//            baudRate,
//            nTries,
//            timeout
//    };

    // int x = strlen(serialPort);
    //  printf(x);
    strcpy(connectionParameters.serialPort, &serialPort);
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    //if (role == "tx") {
    connectionParameters.role = LlTx;;
    transmit(connectionParameters, filename);
    //}



//    // TODO
//
//    typedef struct
//    {
//        char serialPort[50];
//        LinkLayerRole role;
//        int baudRate;
//        int nRetransmissions;
//        int timeout;
//    } LinkLayer;


}
