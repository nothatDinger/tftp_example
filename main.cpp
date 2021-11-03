#include "./tftp_client.h"

int main(){

    TFTPClient client((char*)"127.0.0.1",69);
    if (client.UDPconnectServer() != 1) {

	    cout << "Error while connecting to server " << 69 << endl;

	return 0;
	  
	}
}