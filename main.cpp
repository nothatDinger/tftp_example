
#include "cmdline.h"
#include "tftp_client.h"
#include <iostream>
using namespace std;
int main(int argc, char* argv[]){

	cmdline::parser par;
	

	par.add<string>("host", 'h', "host name( ipv4 only )", true);
	par.add<int>("port", 'p', "server's port number", false, 69, cmdline::range(1, 65535));
	// par.add("get", 'g', "option to get file from server(default option between Get and Put)");
	// par.add("put", 'p', "option to put file to server");
	par.add<string>("type", 't', "method(put or get, default get) to proceed file or string (default get)", false, "get",cmdline::oneof<string>("get", "put"));
	par.add<string>("mode", 'm',"the transfer mode you want to use( octet or netascii, default octet)", false, "octet", cmdline::oneof<string>("octet", "netascii"));
	par.add<string>("file", 'f', "the file you want to put or get", true);
	bool ok = par.parse(argc, argv);
	
    // if (argc==1 || par.exist("help")){
   	// 	 std::cerr<<par.usage();
   	// 	 return 0;
 	// }
  
  	if (!ok){
   		std::cerr<<par.error()<<endl<<par.usage();
    	return 0;
  	}
	Logger l = Logger(Logger::file_and_terminal, Logger::debug, "./log.txt");

	TFTPClient client(&l,par.get<string>("host"), par.get<int>("port"));
	client.UDPconnectServer();

	const char* filename = par.get<string>("file").c_str();
	const char* host = par.get<string>("host").c_str();
	const char* mode = par.get<string>("mode").c_str();
	string type = par.get<string>("type");

	if(type == "get")
		client.getFile((char* )filename, (char* )host, mode);
	if(type == "put"){
		client.sendFile((char* )filename, (char* )host, mode);
	}
		
	return 0;
}