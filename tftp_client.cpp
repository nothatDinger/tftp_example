#include "tftp_client.h"

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

struct sockaddr_in server_addr_;
TFTPClient::TFTPClient(Logger* logger_,string ip, int port=69){
    TFTP_Packet packt;
    this->server_ipv4 = ip;
    this->server_port = port; 
    this->socket_descriptor = -1;
    this->resend_max_count = TFTP_RESEND_DEFAULT_CNT;
    //this->logger = Logger(Logger::terminal, Logger::debug, "./log.txt");
    this->logger = logger_;
    this->loss = this->ack = 0;
}

TFTPClient::~TFTPClient(){

    printf("loss rate: %.2lf%%\n",1.0*loss/(loss+ack)*100);
    close(this->socket_descriptor);
}

int TFTPClient::UDPInitSocket(){
    std::cout<< "Connecting to "+ std::string(this->server_ipv4)+ " on port "+ std::to_string(this->server_port) << std::endl;

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

    struct hostent *host = gethostbyname(this->server_ipv4.c_str());

    bzero(&server_addr_, sizeof(server_addr_));
	server_addr_.sin_family = host->h_addrtype;
	server_addr_.sin_port = htons(this->server_port);
	memcpy(&server_addr_.sin_addr, (struct in_addr *) host->h_addr, sizeof(server_addr_.sin_addr));
    this->server_addr = &server_addr_;

    std::cout<<"Socket successfully initialized\n";
    return 1;
}

int TFTPClient::sendPacket(TFTP_Packet* packet){

    return sendto(socket_descriptor, (char*)packet->getData(), packet->getSize(), 0,(const sockaddr *)server_addr, sizeof(*server_addr));
}

bool TFTPClient::sendFile(char* filename, char* destination_filename, const char* transfer_mode){
    std::cout<<"sending file, filename: "+std::string(filename)+", destination_filename:  "+std::string(destination_filename)+", mode: "+std::string(transfer_mode)<<std::endl;


    string cmd = "cp "+(string)filename+" tmp";
    system(cmd.c_str());
    if( transfer_mode == "netascii")
        unix2dos("tmp");
    cmd = "rm tmp";
    TFTP_Packet packet_wrq, packet_data;
    std::ifstream file("tmp", std::ifstream::binary);
    char memBlock[TFTP_PACKET_DATA_SIZE];

    if(!file){
        logger->ERROR("ERROR to open file when send file");
        system(cmd.c_str());
        return false;
    }

    packet_wrq.createWRQ(destination_filename, transfer_mode);
    packet_wrq.dumpData();
    timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    sendPacket(&packet_wrq);
    unsigned short last_packet_number = 0;//????????????
    int wait_status, timeout_count=0;

    struct timespec tn;
    while(1){
        bool is_lastpacket = file.eof();
        wait_status = waitForPacketACK(last_packet_number, TFTP_CLIENT_SERVER_TIMEOUT, is_lastpacket);

        //timeout
        if( wait_status == TFTP_CLIENT_ERROR_RECEIVE){
            logger->ERROR("Failed to send file");
            std::cout << "failed\n";
            return false;
        }
        if( wait_status == TFTP_CLIENT_ERROR_TIMEOUT || wait_status == TFTP_CLIENT_ERROR_PACKET_UNEXPECTED){
            loss ++;
            timeout_count ++;
            logger->WARNING("resend "+std::to_string(timeout_count)+" times when sending file");
            if( timeout_count >= this->resend_max_count){
                system(cmd.c_str());
                if (is_lastpacket)
                    break;
                logger->ERROR("Failed to resend file");
                std::cout << "failed\n";
                return false;
            }
            if( last_packet_number == 0)
                sendPacket(&packet_wrq);
            else
                sendPacket(&packet_data);
            continue;
        }
        timeout_count = 0;
        ack++;
        //?????????ACK???????????????
        if (is_lastpacket) {
            cout<<std::endl;
			break;
		}
        //??????????????????
        file.read(memBlock, TFTP_PACKET_DATA_SIZE);
        last_packet_number++;
        packet_data.createData(last_packet_number, memBlock, file.gcount());
        packet_data.dumpData();

	    clock_gettime(CLOCK_REALTIME, &tn);
        unsigned int start = tn.tv_nsec;
        sendPacket(&packet_data);
        clock_gettime(CLOCK_REALTIME, &tn);
        unsigned int end = tn.tv_nsec;

        logger->DEBUG("RCV:"+std::to_string(packet_data.getSize())+"bits  use:"+ std::to_string(end-start)+"nanosecond");
        printf("\rspeed :%dKB/S",  packet_data.getSize() *1000000*8/(end-start) );
    }
    
    system(cmd.c_str());
    cout<<"success\n";
    return true;

}
bool TFTPClient::getFile(char* filename, char* destination_filename, const char* transfer_mode){
    std::cout << "getting file, filename: "+std::string(filename)+", destination_filename: "+std::string(destination_filename)+", mode: "+std::string(transfer_mode)<<std::endl;
    TFTP_Packet packet_rrq, packet_ack;
    //????????????????????????
    std::ofstream file;
    
    if( ! strcmp((char*)transfer_mode, TFTP_DEFAULT_TRANSFER_MODE)){
        file.open(filename, std::ofstream::binary|std::ofstream::trunc);
    } else 
    if( ! strcmp((char*)transfer_mode, TFTP_ASCII_TRANSFER_MODE)){
        file.open(filename, std::ofstream::out|std::ofstream::trunc);
    } else{
        logger->ERROR("unknown transfer mode: "+ std::string(transfer_mode));
        return false;
    }
    
    if( !file ){
        logger->ERROR("error when open output file");
        return false;
    }


    char buffer[TFTP_PACKET_DATA_SIZE];
    //??????RRQ??????
    packet_rrq.createRRQ(destination_filename, transfer_mode);

    logger->DEBUG("send rrq request");
    packet_rrq.dumpData();
    sendPacket(&packet_rrq);
    
    
    unsigned short last_packet_number= 1 ;//????????????
    int wait_status, timeout_count=0;
    struct timespec tn;
    while( 1 ){
        
	    clock_gettime(CLOCK_REALTIME, &tn);
        unsigned int start = tn.tv_nsec;

        wait_status = waitForPacketData(last_packet_number, TFTP_CLIENT_SERVER_TIMEOUT);

        clock_gettime(CLOCK_REALTIME, &tn);
        unsigned int end = tn.tv_nsec;
        //?????????ERROR???
        if( wait_status == TFTP_CLIENT_ERROR_PACKET_UNEXPECTED){
            received_packet.dumpData();
            file.close();
            return false;
        }else//????????????
        if(wait_status == TFTP_CLIENT_ERROR_TIMEOUT){
            loss ++;
            timeout_count ++;
            if(timeout_count >= this->resend_max_count ){
                logger->ERROR("There may be connection timeout event");
                file.close();
                return false;
            }
            logger->ERROR("Timeout "+std::to_string(timeout_count)+" times");
            //first packet timeout??? should resend the rrq packet
            if( last_packet_number == 1)
                sendPacket(&packet_rrq);
            else{
                packet_ack.createACK(last_packet_number - 1);
                sendPacket(&packet_ack);
            }  
            continue;
        }
        ack++;
        //?????????????????????,?????????????????????
        if(last_packet_number != received_packet.getNumber()){
            logger->ERROR("Unexpected packet number "+std::to_string(received_packet.getNumber())\
                 +" , expected "+std::to_string(last_packet_number));
            packet_ack.createACK(last_packet_number - 1);
            sendPacket(&packet_ack);
            continue;
        }

        logger->DEBUG("RCV:"+std::to_string(received_packet.getSize())+"bits  use:"+ std::to_string(end-start)+"nanosecond");
        printf("\rspeed :%dKB/S",  received_packet.getSize() *1000000*8/(end-start) );
        //??????????????????
        received_packet.dumpData(); 
        last_packet_number ++;
        timeout_count = 0;
        //2 bytes opcode + 2 bytes packet number
        if( received_packet.copyData( 2 + 2, buffer, TFTP_PACKET_DATA_SIZE)) {
            file.write(buffer, received_packet.getSize() - 2 - 2); 
            //if (received_packet.getNumber() % 10 == 0)
                logger->DEBUG("received packet "+std::to_string(received_packet.getNumber()));

            packet_ack.createACK(last_packet_number - 1);
            sendPacket(&packet_ack);
            if(received_packet.isLastPacket()){//???????????????
                //netascii???????????????????????????unix??????????????????????????????'\n'???'\r\n'
                std::cout<<std::endl;
                if( transfer_mode=="netascii")
                    dos2unix((string)filename);
                break;
                
            }
        }
    }
    file.close();
    cout<<"success\n";
    return true;
}
int TFTPClient::waitForPacketACK(WORD packet_number, int timeout_ms, bool is_lastpacket){
    int wait_status = waitForPacket(&this->received_packet, timeout_ms);
    
    if(wait_status != TFTP_CLIENT_ERROR_NO_ERROR){
        if(wait_status == TFTP_CLIENT_ERROR_CONNECTION_CLOSED && is_lastpacket) {
            return TFTP_CLIENT_ERROR_NO_ERROR;
        }
        return TFTP_CLIENT_ERROR_TIMEOUT;
    }

    if (received_packet.isError()) {
    	int error_code = received_packet.getWord(2);

        logger->ERROR("Client received error packet"+ std::to_string(error_code)+" : "\
            +std::string(error_message[error_code]));
		return TFTP_CLIENT_ERROR_RECEIVE;
	}else
    if( this->received_packet.isACK()){
        logger->DEBUG("ACK for packet"+std::to_string(received_packet.getNumber())+" , expected: " +std::to_string(packet_number));
        if( received_packet.getNumber()!=packet_number)
            return TFTP_CLIENT_ERROR_PACKET_UNEXPECTED;
        return TFTP_CLIENT_ERROR_NO_ERROR;
    }
    if( this->received_packet.isData()){
        logger->DEBUG("DATA ACK for packet"+std::to_string(received_packet.getNumber())+" , expected: " +std::to_string(packet_number));
        return TFTP_CLIENT_ERROR_NO_ERROR;
    }
    return TFTP_CLIENT_ERROR_NO_ERROR;
    
}
int TFTPClient::waitForPacketData(WORD packet_number, int timeout_ms){
    int wait_status = waitForPacket(&this->received_packet, timeout_ms);
    
    if(wait_status != TFTP_CLIENT_ERROR_NO_ERROR){
        return wait_status;
    }

    if (received_packet.isError()) {
    	int error_code = received_packet.getWord(2);

        logger->ERROR("Client received error packet"+ std::to_string(error_code)+" : "\
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

	// struct timespec tn;
	// clock_gettime(CLOCK_REALTIME, &tn);
    // unsigned int start = tn.tv_nsec;

    int select_ready = select(FD_SETSIZE,&fd_reader,NULL,NULL, &connection_timer);
    //int select_ready = select(socket_descriptor+1,&fd_reader,NULL,NULL, &connection_timer);
    if( select_ready < 0){
        loss ++;
        logger->ERROR("select error when waiting for packet");
        return TFTP_CLIENT_ERROR_SELECT;
    } else 
    if( select_ready == 0 ){
        loss ++;
        logger->ERROR("select timout while waiting for packet");
        return TFTP_CLIENT_ERROR_TIMEOUT;
    } 
    int res = recvfrom(this->socket_descriptor,(char*)packet->getData(0), TFTP_PACKET_MAX_SIZE, 0, (sockaddr *)&from, &fromlen);  
    if( res == 0) {
        loss ++ ;
        logger->ERROR("connection was closed by server");
        return TFTP_CLIENT_ERROR_CONNECTION_CLOSED;
    } else
    if(res == -1){
        logger->ERROR("socket error when waiting for packet");
        return TFTP_CLIENT_ERROR_RECEIVE;
    }
    ack++;
	// clock_gettime(CLOCK_REALTIME, &tn);
    // unsigned int end = tn.tv_nsec;
    // logger->DEBUG("RCV:"+std::to_string(res)+"bits  use:"+ std::to_string(end-start)+"nanosecond");
    // printf("speed :%dKB/S",  res *1000000*8/(end-start) );
    //logger->DEBUG(" server port: "+std::to_string(from.sin_port)+"; my port: "+std::to_string(server_addr->sin_port));
    if( server_addr->sin_port == htons(this->server_port)){
        server_addr->sin_port = from.sin_port;
    }
    packet->setSize(res);
    packet->dumpData();

    return TFTP_CLIENT_ERROR_NO_ERROR;

}

void dos2unix(string file){
    std::ifstream is (file.c_str());
    std::ofstream os ("temp");
    char c;
    while (is.get(c))
        if (c != '\r')
            os.put(c); 

    string command = "mv temp " + file;
    system(command.c_str());
}

void unix2dos(string file){
    std::ifstream is (file.c_str());
    std::ofstream os ("temp");
    char c;
    while (is.get(c)){
        if (c == '\n')
            os.put('\r');
        os.put(c); 
    }
    string command = "mv temp " + file;
    system(command.c_str());
}