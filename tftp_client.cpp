#include<iostream>     
#include<sys/socket.h> 
#include<netinet/in.h> 
#include<unistd.h>    
#include<string.h>     
#include<sys/sendfile.h>
#include<string>
#include<map>
#include<vector>
#include<fcntl.h>
#include <arpa/inet.h>
#include<sys/types.h>    
#include<sys/stat.h>
#include<sys/epoll.h>
#include<pthread.h>
#include<errno.h>
#include<cstdlib>
#include<cstdio>
#include "tftp_client.h"
#include "tftp_packt.h"
TFTPClient::TFTPClient(char* ip, int port=69){
    TFTP_Packet packt;
    this->server_ipv4 = ip;
    this->server_port = port; 
    this->socket_descriptor = -1;
    this->resend_max_count = TFTP_RESEND_DEFAULT_CNT;
    this->logger = Logger(Logger::terminal, Logger::debug, "./log.txt");
}

TFTPClient::~TFTPClient(){
    close(this->socket_descriptor);
}

int TFTPClient::UDPconnectServer(){
    logger.DEBUG("Connecting to "+ std::string(this->server_ipv4)+ " on port "+ std::to_string(this->server_port));

    this->socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if(this->socket_descriptor == -1){
        logger.ERROR("Socket Create Error");
        return -1;
    }

    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(this->server_port);	
    client_addr.sin_addr.s_addr = inet_addr(this->server_ipv4);

    this->connection = connect(this->socket_descriptor, (const struct sockaddr *)&client_addr,\
                        sizeof(client_addr));

    if( this->connection == 0) {
        logger.DEBUG("Unable to connect to Server");
        return -1;
    }

    logger.DEBUG("Successfully connected");
    return 1;
}

int TFTPClient::sendPacket(TFTP_Packet* packet){
    return write(socket_descriptor, packet->getData(), packet->getSize());
}

bool TFTPClient::getFile(char* filename, char* destination){
    TFTP_Packet packet_rrq, packet_ack;
    std::ofstream file(destination, std::ifstream::binary);

    if( !file ){
        logger.ERROR("error when open output file");
        return false;
    }

    char buffer[TFTP_PACKET_DATA_SIZE];

    packet_rrq.createRRQ(filename);
    sendPacket(&packet_rrq);

    int last_packet_number, wait_status, timeout_count;

    while( 1 ){

        wait_status = waitForPacketData(last_packet_number, TFTP_CLIENT_SERVER_TIMEOUT);

        //收到了ERROR包
        if( wait_status == TFTP_CLIENT_ERROR_PACKET_UNEXPECTED){
            received_packet.dumpData();
            file.close();
            return false;
        }else//发生超时
        if(wait_status == TFTP_CLIENT_ERROR_TIMEOUT){
            timeout_count ++;
            if(timeout_count >= this->resend_max_count ){
                logger.ERROR("There may be connection timeout event");
                file.close();
                return false;
            }
            logger.DEBUG("Timeout "+std::to_string(timeout_count)+" times");
            //first packet timeout， should resend the rrq packet
            if( last_packet_number == 1)
                sendPacket(&packet_rrq);
            else{
                packet_ack.createACK(last_packet_number - 1);
                sendPacket(&packet_ack);
            }  
            continue;
        }
        //收到了失序的包,或其它类型的包
        if(last_packet_number != received_packet.getNumber()){
            logger.ERROR("Unexpected packet number "+std::to_string(received_packet.getNumber())\
                 +" , expected "+std::to_string(last_packet_number));
            packet_ack.createACK(last_packet_number - 1);
            sendPacket(&packet_ack);
            continue;
        }

        received_packet.dumpData(); 
        last_packet_number ++;
        timeout_count = 0;
        //2 bytes opcode + 2 bytes packet number
        if( received_packet.copyData( 2 + 2, buffer, TFTP_PACKET_DATA_SIZE)) {
            file.write(buffer, received_packet.getSize() - 2 - 2); 
            logger.DEBUG("received packet "+std::to_string(received_packet.getNumber()));
            packet_ack.createACK(last_packet_number - 1);
            sendPacket(&packet_ack);
            if(received_packet.isLastPacket()){
                break;
            }
            

        }

    }
    file.close();
    return true;
}
int TFTPClient::waitForPacketData(WORD packet_number, int timeout_ms){
    int wait_status = waitForPacket(&this->received_packet, timeout_ms);
    
    if(wait_status != TFTP_CLIENT_ERROR_NO_ERROR){
        return wait_status;
    }

    if (received_packet.isError()) {
    	int error_code = received_packet.getWord(2);

        logger.DEBUG("Client received error packet"+ std::to_string(error_code)+" : "\
            +std::string(error_message[error_code]));
		return TFTP_CLIENT_ERROR_PACKET_UNEXPECTED;
	}else
    if( this->received_packet.isData()){
        return TFTP_CLIENT_ERROR_NO_ERROR;
    }
    return wait_status;
}

int TFTPClient::waitForPacket(TFTP_Packet* packet, int timeout_ms){
    packet->clear();
    fd_set fd_reader;
    timeval connection_timer;

    connection_timer = timeval{ timeout_ms, 0};
    FD_ZERO(&fd_reader); FD_SET(this->socket_descriptor, &fd_reader);

    int select_ready = select(socket_descriptor+1,&fd_reader,NULL,NULL, &connection_timer);
    if( select_ready < 0){
        logger.ERROR("select error when waiting for packet");
        return TFTP_CLIENT_ERROR_SELECT;
    } else 
    if( select_ready == 0 ){
        logger.ERROR("select timout while waiting for packet");
        return TFTP_CLIENT_ERROR_TIMEOUT;
    } 

    int res = read(this->socket_descriptor,(char*)packet->getData(), TFTP_PACKET_MAX_SIZE);  
    if( res == 0) {
        logger.ERROR("connection was closed by server");
        return TFTP_CLIENT_ERROR_CONNECTION_CLOSED;
    } else
    if(res == -1){
        logger.ERROR("socket error when waiting for packet");
        return TFTP_CLIENT_ERROR_RECEIVE;
    }

    packet->setSize(res);

    return TFTP_CLIENT_ERROR_NO_ERROR;

}
bool TFTPClient::sendFile(char* filename, char* destination){
    
}

