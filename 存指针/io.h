#pragma once

#include <winsock2.h>
#include <vector>
#include <assert.h>
#include <MSWSock.h>

#define bufferMaxSize 100

enum OPERATION_TYPE {
	RECV,                    //有数据从客户端过来
	SEND,                    //发送数据到客户端
};

class IO {
public:
	OVERLAPPED overLapped;          //重叠CLIENT_IO ol不用放在首位置也可以
	WSABUF wsaBuf;                  //收发数据区
	char buffer[bufferMaxSize];

	DWORD bytes;
public:
	IO() {
		memset(&overLapped, 0, sizeof(overLapped));	//第一次使用必须清0，之后重复使用不需清0
		wsaBuf.buf = buffer;
		wsaBuf.len = bufferMaxSize;
		bytes = 0;
	}
	virtual ~IO() {}

};

class LISTEN_IO :public IO {
public:
	SOCKET clientSocket;
public:
	LISTEN_IO(const SOCKET& clientSocket) : clientSocket(clientSocket) {}
	virtual ~LISTEN_IO() {}
};


class CLIENT_IO : public IO {
public:
	OPERATION_TYPE type;            //操作类型
	DWORD flag;
public:
	CLIENT_IO(const OPERATION_TYPE& type) :type(type), flag(0) {}
	virtual ~CLIENT_IO() {}
};


class SOCKET_HANDLE {
public:
	SOCKET socket;
	SOCKADDR_IN addr;
	vector<IO*> ios;

public:
	SOCKET_HANDLE() {}
	SOCKET_HANDLE(const SOCKET& socket, const SOCKADDR_IN* addr) :
		socket(socket), addr(*addr) {}

	~SOCKET_HANDLE() {
		if (socket != INVALID_SOCKET)
			closesocket(socket);

		for (int i = 0; i < ios.size(); i++)
			delete ios[i];
		ios.clear();
	}

	template<class T>
	T* addIO(const OPERATION_TYPE& type) {
		T* p = new T(type);
		ios.push_back(p);
		return p;
	}

	template<class T>
	T* addIO(const SOCKET& socket) {
		T* p = new T(socket);
		ios.push_back(p);
		return p;
	}

	bool removeIO(const CLIENT_IO* p) {
		assert(p != nullptr);
		for (auto i = ios.begin(); i != ios.end(); i++) {
			if (*i == p) {
				delete p;
				p == nullptr;
				ios.erase(i);
				return true;
			}
		}
		return false;
	}

	bool operator==(const SOCKET_HANDLE& a) {
		if (socket == a.socket)
			return true;
		else
			return false;
	}
};

struct WOKERTHREADPARA {
	HANDLE iocp;
	SOCKET_HANDLE server;
	vector<SOCKET_HANDLE*> clients;
	LPFN_ACCEPTEX wsaAccept;
	LPFN_GETACCEPTEXSOCKADDRS getClientAddr;
public:

	SOCKET_HANDLE* addClient(SOCKET_HANDLE* client) {
		clients.push_back(client);
		return client;
	}

	void removeClient(SOCKET_HANDLE* client) {
		for (auto i = clients.begin(); i != clients.end(); i++) {
			if (*i == client) {
				delete client;
				client = nullptr;
				clients.erase(i);
				return;
			}
		}
	}
};
