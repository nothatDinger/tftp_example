#include <iostream>

#include <cstring>
#define BUFFER_SIZE 1024
#define READ_REQUEST 1
#define WRITE_REQUEST 2
#define DATA 3
#define ACKNOWLEDGEMENT 4
#define WRONG_RECV 5
using namespace std;
FILE* fp = fopen("log.txt", "a+");
#define RECV_LOOP_COUNT 6	//超时最大重发次数
#define TIME_OUT_SEC 5	//超时时间
using namespace std;
int sendRequest(int op, const char filename[], int type) {//type=1为文本格式(txt)，2为二进制文件
	char buf[516];
	if (op != READ_REQUEST && op != WRITE_REQUEST) {
		cout << "makeRequest Error!\n";
		 fprintf(fp, "makeRequest Error!\n");
		return false;
	}
	 
	if (op == READ_REQUEST)
		fprintf(fp, "请求下载数据:%s\n", filename);
	else
		fprintf(fp, "请求上传数据:%s\n", filename);
	memset(buf, 0, sizeof(buf));
	buf[1] = op;
	strcpy(buf + 2, filename);
	int len = strlen(filename) + 3;

  cout <<"fasdfasdfa";
	if (type == 2) {
		strcpy(buf + len, "octet");
		//return sendto(sServSock, buf, len + 6, 0, (LPSOCKADDR)&addr, addrLen);
    	cout << "\n--------------DATA DUMP---------------------\n";
	cout << "Size: " << strlen(buf) << endl;

	for (int i = 0; i < 17; i++) {
		//cout << data[i];
		printf("%02x ", buf[i]);
	}

	cout << "\n--------------------------------------------\n\n";
	}
	else {
		strcpy(buf + len, "netascii");
		//return sendto(sServSock, buf, len + 9, 0, (LPSOCKADDR)&addr, addrLen);
          	cout << "\n--------------DATA DUMP---------------------\n";
	cout << "Size: " << len << endl;

	for (int i = 0; i < len; i++) {
		//cout << data[i];
		printf("%02x ", buf[i]);
	}

	cout << "\n--------------------------------------------\n\n";
	
	}
  return 0;
}
int main()
{
  sendRequest(READ_REQUEST, "test.txt",2);
}