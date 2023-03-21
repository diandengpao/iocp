#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
using namespace std;

#include "Winsock2.h"
#pragma comment(lib, "ws2_32.lib")

#define PORT 65432	//服务器端口常量

int main() {


	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData)) {
		cout << "加载winsock.dll失败！\n";
		return 0;
	}

	//创建套接字
					//AF_INET使用ip地址,0:套接字的协议，依赖于第二个参数，0表示默认
	int sock;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		cout << "创建套接字失败！错误代码:" << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}



	//服务器地址
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	//连接服务器
	if (connect(sock, (sockaddr *)&serverAddr, sizeof(sockaddr))) {
		cout << "连接失败！错误代码:" << WSAGetLastError() << endl;
		closesocket(sock);
		WSACleanup();
		return 0;
	}
	else
		cout << "连接成功" << endl;


	char msgBuffer[1000];
	int size;

	strcpy_s(msgBuffer, "hi, i am client");


	//第三个参数表示要发送的数据长度	//strlen需要+1
	if ((size = send(sock, msgBuffer, strlen(msgBuffer) + 1, 0)) < 0)
		cout << "发送信息失败！错误代码:" << WSAGetLastError() << endl;
	else if (size == 0)
		cout << "对方已关闭连接！\n";
	else
		cout << "信息发送成功！\n";


	if ((size = recv(sock, msgBuffer, sizeof(msgBuffer), 0)) < 0) {
		cout << "接收信息失败！错误代码:" << WSAGetLastError() << endl;
		closesocket(sock);//关闭已连接套接字
		WSACleanup(); //注销WinSock动态链接库
		return 0;
	}
	else if (size == 0) {
		cout << "对方已关闭连接！\n";
		closesocket(sock);//关闭已连接套接字
		WSACleanup(); //注销WinSock动态链接库
		return 0;
	}
	else
		cout << "The message from Server: " << msgBuffer << endl;

	closesocket(sock); //关闭socket
	WSACleanup(); //注销WinSock动态链接库
	cout << "已关闭套接字" << endl;
	return 0;

}
