
#include "tftp_packt.h"

using namespace std;

TFTP_Packet::TFTP_Packet()	{

	clear();

}

void TFTP_Packet::clear()	{

	current_packet_size = 0;
	
	memset(data, 0, TFTP_PACKET_MAX_SIZE);

}

unsigned char* TFTP_Packet::getData(int offset) {

	return &(data[offset]);

}

bool TFTP_Packet::copyData(int offset, char* dest, int length) {

	if (offset > this->getSize()) return false;
	if (length < (this->getSize() - offset)) return false; //- out of bounds

	memcpy(dest, &(data[offset]), (this->getSize()-offset));

	return true;

}

void TFTP_Packet::dumpData() {

#ifdef DEBUG

	cout << "\n--------------DATA DUMP---------------------\n";
	cout << "Size: " << current_packet_size << endl;

	for (int i = 0; i < current_packet_size; i++) {
		cout << data[i];
	}

	cout << "\n--------------------------------------------\n\n";

#endif

}

bool TFTP_Packet::addByte(BYTE b) {

	if (current_packet_size >= TFTP_PACKET_MAX_SIZE) {
		return false;
	}

	data[current_packet_size] = (unsigned char)b;
	current_packet_size++;

	return true;

}

bool TFTP_Packet::addWord(WORD w) {

	if (!addByte(*(((BYTE*)&w)+1))) return false;

	return addByte(*((BYTE*)&w));

}

bool TFTP_Packet::addString(const char* str) {

	int n = strlen(str);

	for (int i=0; i < n; i++) {

		if (!addByte(*(str + i))) { 

			return false;

		}

	}

	return true;

}

bool TFTP_Packet::addMemory(char* buffer, int len) {

	if (current_packet_size + len >= TFTP_PACKET_MAX_SIZE)	{
		return false;
	}

	memcpy(&(data[current_packet_size]), buffer, len);
	current_packet_size += len;

	return true;

}

BYTE TFTP_Packet::getByte(int offset) {

	return (BYTE)data[offset];

}

WORD TFTP_Packet::getWord(int offset) {

	WORD hi = getByte(offset);
	WORD lo = getByte(offset + 1);

	return ((hi<<8)|lo);

}

WORD TFTP_Packet::getNumber() {

	if (this->isData() || this->isACK()) {

		return this->getWord(2);

	} else {

		return 0;

	}

}

bool TFTP_Packet::getString(int offset, char* buffer, int len) {

	if (offset > current_packet_size) return false;
	if (len < current_packet_size - offset) return false;

	memcpy(buffer, &(data[offset]), current_packet_size - offset);

	return true;

}

bool TFTP_Packet::createRRQ(char* filename) {

/*	 2 bytes     string    1 byte     string   1 byte
     Opcode		 Filename  0		  Mode	   0	*/

	clear();
	addWord(TFTP_OPCODE_READ);
	addString(filename);
	addByte(0);
	addString(TFTP_DEFAULT_TRANSFER_MODE);
	addByte(0);

	return true;

}

bool TFTP_Packet::createWRQ(char* filename) {

/*	structure is the same as RRQ  */

	clear();
	addWord(TFTP_OPCODE_WRITE);
	addString(filename);
	addByte(0);
	addString(TFTP_DEFAULT_TRANSFER_MODE);
	addByte(0);

	return true;

}

bool TFTP_Packet::createACK(WORD packet_num) {

	clear();
	addWord(TFTP_OPCODE_ACK);
	addWord(packet_num);

	return true;

}

bool TFTP_Packet::createData(WORD block, char* data, int data_size) {

/*	   2 bytes    2 bytes       n bytes
----------------------------------------
DATA  | 03    |   Block #  |    Data    |
---------------------------------------- */

	clear();
	addWord(TFTP_OPCODE_DATA);
	addWord(block);

	addMemory(data, data_size);

	return true;

}

bool TFTP_Packet::createError(WORD error_code, char* message) {

/*        2 bytes  2 bytes        string    1 byte
          ----------------------------------------
	ERROR | 05    |  ErrorCode |   ErrMsg   |   0  |
          ----------------------------------------  */

	clear();
	addWord(TFTP_OPCODE_ERROR);
	addWord(error_code);
	addString(message);
	addByte(0);

	return true;

}

int TFTP_Packet::getSize() {
	return current_packet_size;
}

bool TFTP_Packet::setSize(int size) {
	if (size <= TFTP_PACKET_MAX_SIZE) {
		current_packet_size = size;
		return true;
	} else {
		return false;
	}
}

bool TFTP_Packet::isRRQ() {

	return (this->getWord(0) == TFTP_OPCODE_READ);

}

bool TFTP_Packet::isWRQ() {

	return (this->getWord(0) == TFTP_OPCODE_WRITE);
}

bool TFTP_Packet::isACK() {

	return (this->getWord(0) == TFTP_OPCODE_ACK);

}


bool TFTP_Packet::isData() {

	return (this->getWord(0) == TFTP_OPCODE_DATA);

}

bool TFTP_Packet::isError() {

	return (this->getWord(0) == TFTP_OPCODE_ERROR);

}

bool TFTP_Packet::isLastPacket(){
	return this->getSize() - 4 < TFTP_PACKET_DATA_SIZE ;
}
TFTP_Packet::~TFTP_Packet() {
	
	

}