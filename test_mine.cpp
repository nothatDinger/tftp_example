
#include "cmdline.h"
#include "tftp_client.h"
#include <iostream>
using namespace std;
int main(){
	

	Logger l = Logger(Logger::file_and_terminal, Logger::debug, "./log.txt");

	TFTPClient client(&l,"162.14.65.50", 69);
	client.UDPconnectServer();


	client.getFile("test.txt", "success", "octet");
	return 0;
}