#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
using namespace std;

#include "io.h"
#pragma comment(lib, "ws2_32.lib")

#include <string>
#include <winnt.h>

#include <string>
#include <algorithm>
#define EXIT_CODE NULL         //退出码

void workerThreadFun(void* _wokerPara);

template<class T>
void deleter(T* p) {
	delete p;
	p = nullptr;
}


int main() {
//创建iocp
	HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

//创建客户端集合
	vector<SOCKET_HANDLE*> clients;
//创建server并绑定iocp
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	//创建server
	SOCKET_HANDLE server;
	
	server.socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	
	server.addr.sin_family = AF_INET;
	server.addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	server.addr.sin_port = htons(65432);
	
	bind(server.socket, (sockaddr *)&server.addr, sizeof(server.addr));
	listen(server.socket, 10);

	//绑定iocp
		//需要将两个参数绑定到iocp。1套接字，2一个自定义结构(在这里是句柄)。异步io完成时GetQueuedCompletionStatus返回2和异步id(ol)。
	CreateIoCompletionPort((HANDLE)server.socket, iocp, (DWORD)&server, 0);

//获取函数指针
	LPFN_ACCEPTEX wsaAccept;
	GUID guidAccept = WSAID_ACCEPTEX;
	DWORD bytes;
	//任意一个有效socket即可
	WSAIoctl(server.socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAccept, sizeof(guidAccept),
		&wsaAccept, sizeof(wsaAccept), &bytes, NULL, NULL);

	//获取解析accept的函数指针 在工作线程中使用
	LPFN_GETACCEPTEXSOCKADDRS getClientAddr;
	GUID guidGetClientAddr = WSAID_GETACCEPTEXSOCKADDRS;
	WSAIoctl(server.socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidGetClientAddr, sizeof(guidGetClientAddr), 
		&getClientAddr, sizeof(getClientAddr), &bytes, NULL, NULL);

//创建工作线程
	//线程数等于处理器*2
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	int threadNums = si.dwNumberOfProcessors * 2;

	//创建线程
	HANDLE* threadHandles = new HANDLE[threadNums];
	WOKERTHREADPARA** wokerParas = new WOKERTHREADPARA*[threadNums];

	for (int i = 0; i < threadNums; i++) {
		wokerParas[i] = new WOKERTHREADPARA(iocp, server, clients, wsaAccept, getClientAddr, i);
		threadHandles[i] = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)workerThreadFun, wokerParas[i], 0, NULL);
	}
		
	cout << "建立工作者线程" << threadNums << "个" << endl;

	

	//获取两个函数指针并创建10个异步accept
		

	//创建十个异步accept
	for (int i = 0; i < 10; i++) {
		//创建io
		LISTEN_IO* lIO = server.addIO<LISTEN_IO>(WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED));
		//投递io
		bool ret = wsaAccept(server.socket, lIO->clientSocket, lIO->wsaBuf.buf, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &lIO->bytes, &lIO->overLapped);

		if (!ret && WSAGetLastError() != WSA_IO_PENDING) {
			cout << "投递失败！错误码: " << WSAGetLastError() << endl;
			return false;
		}
		else
			cout << "投递成功" << endl;
	}

	//阻塞
	cout << "listenning......" << endl;

	string str;
	do {
		cin >> str;
	} while (str != "quit");

	//退出所有线程
	for (int i = 0; i < threadNums; i++) 
		PostQueuedCompletionStatus(iocp, 0, (DWORD)EXIT_CODE, NULL);

	//等待所有线程退出
	WaitForMultipleObjects(threadNums, threadHandles, true, INFINITE);

	//释放所有客户端
	for_each(clients.begin(), clients.end(), deleter<SOCKET_HANDLE>);

	//释放线程参数
	for (int i = 0; i < threadNums; i++)
		delete wokerParas[i];
	delete[]wokerParas;

	//释放线程句柄
	for (int i = 0; i < threadNums; i++)
		CloseHandle(threadHandles[i]);
	delete[] threadHandles;
	//关闭服务器
	closesocket(server.socket);
	//关闭iocp
	CloseHandle(iocp);

}


void workerThreadFun(void* _wokerPara) {

	WOKERTHREADPARA* wokerPara = (WOKERTHREADPARA*)_wokerPara;

	OVERLAPPED *ol = nullptr;
	SOCKET_HANDLE* socketHandle = nullptr;
	DWORD ioBytes = 0;

	while (true) {

		GetQueuedCompletionStatus(wokerPara->iocp, &ioBytes, (PULONG_PTR)&socketHandle, &ol, INFINITE);

		if (socketHandle == EXIT_CODE) {
			printf("工作者线程 %d 号退出！\n", wokerPara->threadId);
			//cout << wokerPara->threadId << "线程退出" << endl;	使用cout格式会乱
			return;
		}
			

		//跟据成员的位置找到对象的位置。
		IO* io = CONTAINING_RECORD(ol, IO, overLapped);

		//处理连接
		if (strcmp(typeid(*io).name(), "class LISTEN_IO") == 0) {
			LISTEN_IO* lIO = (LISTEN_IO*)io;

			//1.将客户添加到客户vector中并绑定iocp
			sockaddr_in *localAddr = nullptr;
			sockaddr_in *remoteAddr = nullptr;
			int remoteLen = sizeof(sockaddr_in);
			int localLen = sizeof(sockaddr_in);

			//获取客户段地址
			wokerPara->getClientAddr(lIO->wsaBuf.buf, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, (LPSOCKADDR *)&localAddr, &localLen, (LPSOCKADDR *)&remoteAddr, &remoteLen);

			//创建客户端并绑定iocp
			SOCKET_HANDLE* client = wokerPara->addClient(new SOCKET_HANDLE(lIO->clientSocket, remoteAddr));
			CreateIoCompletionPort((HANDLE)client->socket, wokerPara->iocp, (DWORD)client, 0);

			cout << "客户端 " << inet_ntoa(client->addr.sin_addr) << ":" << ntohs(client->addr.sin_port) << " 连入" << endl;

			//2.客户端创建异步recv
			CLIENT_IO* cIO = client->addIO<CLIENT_IO>(RECV);
			WSARecv(client->socket, &cIO->wsaBuf, 1, &cIO->bytes, &cIO->flag, &cIO->overLapped, NULL);	//WSARecv收的字节数不能超过cIO->wsaBu.len;WSARecv完成时收的字节数返回到cIO->bytes中

			//3.使用原listenIO重新创建异步accept io可以重复使用
			lIO->clientSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			wokerPara->wsaAccept(wokerPara->server.socket, lIO->clientSocket, lIO->wsaBuf.buf, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &lIO->bytes, &lIO->overLapped);
		}
		//处理客户端io
		else {
			CLIENT_IO* cIO = (CLIENT_IO*)io;
			//处理收到的数据
				//关闭
			if (ioBytes == 0) {
				cout << "客户端 " << inet_ntoa(socketHandle->addr.sin_addr) << ':' << ntohs(socketHandle->addr.sin_port) << " 断开连接" << endl;
				wokerPara->removeClient(socketHandle);
			}
			else if (cIO->type == RECV) {
				//1.数据处理&删除io
				cout << "收到 " << inet_ntoa(socketHandle->addr.sin_addr) << ':' << ntohs(socketHandle->addr.sin_port) << " 信息:" << cIO->wsaBuf.buf << endl;

				//2.创建异步send
				cIO->type = SEND;
				cIO->wsaBuf.len = strlen("hello, i am server") + 1;	//WSASend发送cIO->wsaBuf.len个字节
				strcpy_s(cIO->wsaBuf.buf, cIO->wsaBuf.len, "hello, i am server");
				WSASend(socketHandle->socket, &cIO->wsaBuf, 1, &cIO->bytes, cIO->flag, &cIO->overLapped, NULL);	//WSASend发的字节数为cIO->wsaBu.len;WSASend完成时发的字节数返回到cIO->bytes中
			}
			else {
				//1.数据处理&删除io
				cout << "已向 " << inet_ntoa(socketHandle->addr.sin_addr) << ':' << ntohs(socketHandle->addr.sin_port) << " 发送成功" << endl;

				//2.创建异步recv
				WSARecv(socketHandle->socket, &cIO->wsaBuf, 1, &cIO->bytes, &cIO->flag, &cIO->overLapped, NULL);	//等待关闭
			}

		}

	}

}
