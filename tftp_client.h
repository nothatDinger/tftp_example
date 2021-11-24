#ifndef TFTPCLIENT
#define TFTPCLIENT

#define TFTP_CLIENT_SERVER_TIMEOUT 2
#define TFTP_CLIENT_ERROR_MESSAGE_NUM 8
#define TFTP_RESEND_DEFAULT_CNT 10

#define TFTP_CLIENT_ERROR_TIMEOUT 0
#define TFTP_CLIENT_ERROR_SELECT 1
#define TFTP_CLIENT_ERROR_CONNECTION_CLOSED 2
#define TFTP_CLIENT_ERROR_RECEIVE 3
#define TFTP_CLIENT_ERROR_NO_ERROR 4
#define TFTP_CLIENT_ERROR_PACKET_UNEXPECTED 5

#include<iostream>     
#include<sys/socket.h> 
#include<unistd.h>    
#include<string>
#include <netdb.h> 
#include<sys/stat.h>
#include<sys/time.h>
#include "tftp_packet.h"
#include "logger.h"

static const char *error_message[TFTP_CLIENT_ERROR_MESSAGE_NUM]={
   "Not defined, see error message (if any).",
   "File not found.",
   "Access violation.",
   "Disk full or allocation exceeded.",
   "Illegal TFTP operation.",
   "Unknown transfer ID.",
   "File already exists.",
   "No such user.",
};

void unix2dos(string from);
void dos2unix(string from);
class TFTPClient {
    private: 
        struct sockaddr_in* server_addr;
        string server_ipv4;
        int server_port;
        // struct sockaddr_in client_addr;
        int connection;
        int socket_descriptor;
        TFTP_Packet received_packet;
        int resend_max_count;
        Logger* logger;
    public:
        TFTPClient( Logger *logger, string ip, int port);
        ~TFTPClient(); 

        int sendBuffer(char *);
		int sendPacket(TFTP_Packet* packet);

        int UDPInitSocket();
        bool getFile(char* filename, char* destination, const char* transfer_mode);
        bool sendFile(char *filename, char* destination, const char* transfer_mode);

        int waitForPacket(TFTP_Packet* packet, int tileout_ms = TFTP_CLIENT_SERVER_TIMEOUT);
        bool waitForPacketACK(WORD packet_number, int timeout_ms = TFTP_CLIENT_SERVER_TIMEOUT);
		int waitForPacketData(WORD packet_number, int timeout_ms = TFTP_CLIENT_SERVER_TIMEOUT);

        void errorRecieived(TFTP_Packet* error_packet);
};


#endif