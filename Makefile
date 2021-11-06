.PHONY:clean
tftp: main.o logger.o tftp_client.o tftp_packet.o
	g++ -Wall -g main.o tftp_client.o tftp_packet.o logger.o -o tftp
main.o: main.cpp
	g++ -Wall -g -c main.cpp -o main.o
client: tftp_client.cpp tftp_client.h
	g++ -Wall -g -c tftp_client.cpp -o tftp_client.o
packet: tftp_packet.cpp tftp_packet.h
	g++ -Wall -g -c tftp_packet.cpp -o tft_packet.o
logger: logger.cpp logger.h
	g++ -Wall -g -c logger.cpp -o logger.o
clean:
	rm -f *.o main