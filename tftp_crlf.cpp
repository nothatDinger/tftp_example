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
#include <netdb.h>
#include<sys/types.h>    
#include<sys/stat.h>
#include<sys/epoll.h>
#include<pthread.h>
#include<errno.h>
#include<cstdlib>
#include<cstdio>
#include "tftp_client.h"
struct sockaddr_in server_addr_;
TFTPClient::TFTPClient(Logger* logger_,string ip, int port=69){
    TFTP_Packet packt;
    this->server_ipv4 = ip;
    this->server_port = port; 
    this->socket_descriptor = -1;
    this->resend_max_count = TFTP_RESEND_DEFAULT_CNT;
    //this->logger = Logger(Logger::terminal, Logger::debug, "./log.txt");
    this->logger = logger_;
}

TFTPClient::~TFTPClient(){
    close(this->socket_descriptor);
}

int TFTPClient::UDPconnectServer(){
    logger->DEBUG("Connecting to "+ std::string(this->server_ipv4)+ " on port "+ std::to_string(this->server_port));

    this->socket_descriptor = socket(PF_INET, SOCK_DGRAM, 0);
    if(this->socket_descriptor == -1){
        logger->ERROR("Socket Create Error");
        return -1;
    }
    int len = sizeof(server_addr_);
    bzero(&server_addr_, sizeof( server_addr_));
    if(bind(socket_descriptor, (sockaddr*)&server_addr_, sizeof(server_addr_)) < 0 ){
        logger->ERROR("Unable to bind local port");
        return -1;
    }
    bzero(&server_addr_, sizeof(server_addr_));
    
	if (getsockname(socket_descriptor, (struct sockaddr *)&server_addr_, (socklen_t *)&len) == -1) {
		printf("Get local port failed (%s)\n", strerror(errno));
		return -1;
	}

    struct hostent *host = gethostbyname(this->server_ipv4.c_str());

    bzero(&server_addr_, sizeof(server_addr_));
	server_addr_.sin_family = host->h_addrtype;
	server_addr_.sin_port = htons(this->server_port);
	memcpy(&server_addr_.sin_addr, (struct in_addr *) host->h_addr, sizeof(server_addr_.sin_addr));
    this->client_addr = &server_addr_;
    // bzero(client_addr, sizeof(* client_addr));
    // client_addr->sin_family = AF_INET;
	// client_addr->sin_port = htons(this->server_port);	
    // client_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    //client_addr->sin_addr.s_addr = inet_addr(this->server_ipv4.c_str());
    
    //client_addr->sin_addr.s_addr = inet_addr("127.0.0.1");
    // this->connection = connect(this->socket_descriptor, (const struct sockaddr *)&client_addr,\
    //                     sizeof(client_addr));
    // if( this->connection == 0) {
    //     logger->DEBUG("Unable to connect to Server");
    //     return -1;
    // }

    //logger->DEBUG("Successfully connected");
    return 1;
}

int TFTPClient::sendPacket(TFTP_Packet* packet){
    // return write(socket_descriptor, packet->getData(), packet->getSize());
    //return send(socket_descriptor, (char*)packet->getData(), packet->getSize(), 0);
    return sendto(socket_descriptor, (char*)packet->getData(), packet->getSize(), 0,(const sockaddr *)client_addr, sizeof(*client_addr));
}

bool TFTPClient::sendFile(char* filename, char* destination, const char* transfer_mode){
    logger->DEBUG("sending file, filename: "+std::string(filename)+", destination: "+std::string(destination)+", mode: "+std::string(transfer_mode));

    TFTP_Packet packet_wrq, packet_data;
    std::ifstream file(filename, std::ifstream::binary);
    char memBlock[TFTP_PACKET_DATA_SIZE];

    if(!file){
        logger->ERROR("ERROR to open file when send file");
        return false;
    }

    packet_wrq.createWRQ(filename, transfer_mode);
    packet_wrq.dumpData();

    sendPacket(&packet_wrq);
    int last_packet_number = 0, wait_status, timeout_count=0;

    while(1){
        wait_status = waitForPacketACK(last_packet_number, TFTP_CLIENT_SERVER_TIMEOUT);

        //timeout
        if( !wait_status){
            timeout_count ++;
            logger->DEBUG("resend "+std::to_string(timeout_count)+" times when sending file");
            if( timeout_count >= this->resend_max_count){
                logger->ERROR("Failed to resend file");
                return false;
            }
            if( last_packet_number == 0)
                sendPacket(&packet_wrq);
            else
                sendPacket(&packet_data);
            continue;
        }
        //得接到ACK再关闭连接
        if (file.eof()) {
			break;
		}
        //循环分块读入
        file.read(memBlock, TFTP_PACKET_DATA_SIZE);
        last_packet_number++;
        packet_data.createData(last_packet_number, memBlock, file.gcount());
        packet_data.dumpData();
        sendPacket(&packet_data);
    }
    return true;

}
bool TFTPClient::getFile(char* filename, char* destination, const char* transfer_mode){
    logger->DEBUG("getting file, filename: "+std::string(filename)+", destination: "+std::string(destination)+", mode: "+std::string(transfer_mode));
    TFTP_Packet packet_rrq, packet_ack;
    //打开接收用的文件
    std::ofstream file;
    
    if( ! strcmp((char*)transfer_mode, TFTP_DEFAULT_TRANSFER_MODE)){
        file.open(destination, std::ofstream::binary|std::ofstream::trunc);
    } else 
    if( ! strcmp((char*)transfer_mode, TFTP_ASCII_TRANSFER_MODE)){
        file.open(destination, std::ofstream::out|std::ofstream::trunc);
    } else{
        logger->ERROR("unknown transfer mode: "+ std::string(transfer_mode));
        return false;
    }
    
    if( !file ){
        logger->ERROR("error when open output file");
        return false;
    }

    char buffer[TFTP_PACKET_DATA_SIZE];
    //发送RRQ请求
    packet_rrq.createRRQ(filename, transfer_mode);

    logger->DEBUG("send rrq request");
    packet_rrq.dumpData();
    sendPacket(&packet_rrq);
    
    
    int last_packet_number= 1 , wait_status, timeout_count;

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
                logger->ERROR("There may be connection timeout event");
                file.close();
                return false;
            }
            logger->DEBUG("Timeout "+std::to_string(timeout_count)+" times");
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
            logger->ERROR("Unexpected packet number "+std::to_string(received_packet.getNumber())\
                 +" , expected "+std::to_string(last_packet_number));
            packet_ack.createACK(last_packet_number - 1);
            sendPacket(&packet_ack);
            continue;
        }
        //收到正常的包
        received_packet.dumpData(); 
        last_packet_number ++;
        timeout_count = 0;
        //2 bytes opcode + 2 bytes packet number
        if( received_packet.copyData( 2 + 2, buffer, TFTP_PACKET_DATA_SIZE)) {
            file.write(buffer, received_packet.getSize() - 2 - 2); 

            logger->DEBUG("received packet "+std::to_string(received_packet.getNumber()));

            packet_ack.createACK(last_packet_number - 1);
            sendPacket(&packet_ack);
            if(received_packet.isLastPacket()){//最后一个包
                break;
            }
            

        }

    }
    file.close();
    return true;
}
bool TFTPClient::waitForPacketACK(WORD packet_number, int timeout_ms){
    int wait_status = waitForPacket(&this->received_packet, timeout_ms);
    
    if(wait_status != TFTP_CLIENT_ERROR_NO_ERROR){
        return false;
    }

    if (received_packet.isError()) {
    	int error_code = received_packet.getWord(2);

        logger->DEBUG("Client received error packet"+ std::to_string(error_code)+" : "\
            +std::string(error_message[error_code]));
		return false;
	}else
    if( this->received_packet.isACK()){
        logger->DEBUG("ACK for packet"+std::to_string(received_packet.getNumber())+" , expected: " +std::to_string(packet_number));
        if( received_packet.getNumber()!=packet_number)
            return false;
        return true;
    }
    if( this->received_packet.isData()){
        logger->DEBUG("DATA ACK for packet"+std::to_string(received_packet.getNumber())+" , expected: " +std::to_string(packet_number));
        return true;
    }
    return true;
    
}
int TFTPClient::waitForPacketData(WORD packet_number, int timeout_ms){
    int wait_status = waitForPacket(&this->received_packet, timeout_ms);
    
    if(wait_status != TFTP_CLIENT_ERROR_NO_ERROR){
        return wait_status;
    }

    if (received_packet.isError()) {
    	int error_code = received_packet.getWord(2);

        logger->DEBUG("Client received error packet"+ std::to_string(error_code)+" : "\
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
    struct sockaddr_in from;
    memset(&from, 0, sizeof(from));
	socklen_t fromlen = sizeof(from);
    connection_timer = timeval{ timeout_ms, 0};
    FD_ZERO(&fd_reader); FD_SET(this->socket_descriptor, &fd_reader);

    int select_ready = select(FD_SETSIZE,&fd_reader,NULL,NULL, &connection_timer);
    //int select_ready = select(socket_descriptor+1,&fd_reader,NULL,NULL, &connection_timer);
    if( select_ready < 0){
        logger->ERROR("select error when waiting for packet");
        return TFTP_CLIENT_ERROR_SELECT;
    } else 
    if( select_ready == 0 ){
        logger->ERROR("select timout while waiting for packet");
        return TFTP_CLIENT_ERROR_TIMEOUT;
    } 

    int res = recvfrom(this->socket_descriptor,(char*)packet->getData(0), TFTP_PACKET_MAX_SIZE, 0, (sockaddr *)&from, &fromlen);  
    if( res == 0) {
        logger->ERROR("connection was closed by server");
        return TFTP_CLIENT_ERROR_CONNECTION_CLOSED;
    } else
    if(res == -1){
        logger->ERROR("socket error when waiting for packet");
        return TFTP_CLIENT_ERROR_RECEIVE;
    }
    logger->DEBUG(" server port: "+std::to_string(from.sin_port)+"; my port: "+std::to_string(client_addr->sin_port));
    if( client_addr->sin_port == htons(this->server_port)){
        client_addr->sin_port = from.sin_port;
    }
    packet->setSize(res);
    packet->dumpData();

    return TFTP_CLIENT_ERROR_NO_ERROR;

}