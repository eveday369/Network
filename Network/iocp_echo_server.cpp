#include <WinSock2.h>
#include <iostream>

#pragma warning(disable:4996)

using namespace std;

#define BUFSIZE 512

// ���� ���� ������ ���� ����ü
struct SOCK_INFO
{
	OVERLAPPED overlapped;
	SOCKET sock;
	char buf[BUFSIZE + 1];
	int recvbytes;
	int sendbytes;
	WSABUF wsabuf;
};

DWORD WINAPI ThreadProc(LPVOID arg)
{
	HANDLE hIocp = (HANDLE)arg;
	int ret;

	while (1)
	{
		// �񵿱� ����� �Ϸ� ��ٸ���
		DWORD cbTransferred;
		SOCKET clientSock;
		SOCK_INFO *sInfo;
		ret = GetQueuedCompletionStatus(hIocp, &cbTransferred, (LPDWORD)&clientSock, (LPOVERLAPPED *)&sInfo, INFINITE);

		// Ŭ���̾�Ʈ ���� ���
		SOCKADDR_IN addrClient;
		int addrlen = sizeof(addrClient);
		getpeername(sInfo->sock, (SOCKADDR *)&addrClient, &addrlen);

		// �񵿱� ����� ��� Ȯ��
		if (ret == 0 || cbTransferred == 0)
		{
			if (ret == 0)
			{
				DWORD temp1, temp2;
				WSAGetOverlappedResult(sInfo->sock, &(sInfo->overlapped), &temp1, FALSE, &temp2);
				cout << "WSAGetOverlappedResult()" << endl;
			}
			closesocket(sInfo->sock);
			cout << "[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ� = " << inet_ntoa(addrClient.sin_addr) << " ��Ʈ ��ȣ = " << ntohs(addrClient.sin_port);
			cout << endl;
			delete sInfo;
			continue;
		}

		// ������ ���۷� ����
		if (sInfo->recvbytes == 0)
		{
			sInfo->recvbytes = cbTransferred;
			sInfo->sendbytes = 0;
			// ���� ������ ���
			sInfo->buf[sInfo->recvbytes] = '\0';
			cout << "[TCP/" << inet_ntoa(addrClient.sin_addr) << ':' << ntohs(addrClient.sin_port) << ']' << sInfo->buf << endl;
		}
		else
		{
			sInfo->sendbytes += cbTransferred;
		}

		if (sInfo->recvbytes > sInfo->sendbytes)
		{
			// ������ ������
			ZeroMemory(&(sInfo->overlapped), sizeof(sInfo->overlapped));
			sInfo->wsabuf.buf = sInfo->buf + sInfo->sendbytes;
			sInfo->wsabuf.len = sInfo->recvbytes - sInfo->sendbytes;

			DWORD sendbytes;
			ret = WSASend(sInfo->sock, &(sInfo->wsabuf), 1, &sendbytes, 0, &(sInfo->overlapped), NULL);
			if (ret == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					cout << "WSASend()";
				}
				continue;
			}
		}
		else
		{
			sInfo->recvbytes = 0;

			// ������ �ޱ�
			ZeroMemory(&(sInfo->overlapped), sizeof(sInfo->overlapped));
			sInfo->wsabuf.buf = sInfo->buf;
			sInfo->wsabuf.len = BUFSIZE;

			DWORD recvbytes;
			DWORD flags = 0;
			ret = WSARecv(sInfo->sock, &(sInfo->wsabuf), 1, &recvbytes, &flags, &(sInfo->overlapped), NULL);
			if (ret == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					cout << "WSARecv()";
				}
				continue;
			}
		}
	}
	return 0;
}

int main(int argc, char* argv[])
{
	int ret;
	HANDLE hThread;
	DWORD dwThreadId;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		return -1;
	}

	// IOCP ����
	HANDLE hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIocp == NULL)
		return -1;

	// CPU ���� Ȯ��
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	// (CPU���� * 2)���� �۾��� ������ ����
	for (auto i = 0; i < (int)si.dwNumberOfProcessors * 2; i++)
	{
		hThread = CreateThread(NULL, 0, ThreadProc, hIocp, 0, &dwThreadId);
		if (hThread == NULL)
		{
			return -1;
		}
		CloseHandle(hThread);
	}

	// socket()
	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET)
	{
		cout << "socket()";
		exit(1);
	}

	// bind()
	SOCKADDR_IN addrServer;
	ZeroMemory(&addrServer, sizeof(addrServer));
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(9000);
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	ret = bind(listenSock, (SOCKADDR *)&addrServer, sizeof(addrServer));
	if (ret == SOCKET_ERROR)
	{
		cout << "bind()";
		return -1;
	}

	// listen()
	ret = listen(listenSock, SOMAXCONN);
	if (ret == SOCKET_ERROR)
	{
		cout << "listen()";
		return -1;
	}

	while (1)
	{
		// accept()
		SOCKADDR_IN addrClient;
		int addrlen = sizeof(addrClient);
		SOCKET clientSock = accept(listenSock, (SOCKADDR *)&addrClient, &addrlen);
		if (clientSock == INVALID_SOCKET)
		{
			cout << "Socket error";
			continue;
		}
		cout << "[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ� = " << inet_ntoa(addrClient.sin_addr) << " ��Ʈ ��ȣ = " << ntohs(addrClient.sin_port) << endl;

		// ���ϰ� ����� �Ϸ� ��Ʈ ����
		HANDLE hResult = CreateIoCompletionPort((HANDLE)clientSock, hIocp, (DWORD)clientSock, 0);
		if (hResult == NULL)
			exit(-1);

		// ���� ���� ����ü �Ҵ�
		SOCK_INFO *sInfo = new SOCK_INFO;
		if (sInfo == NULL)
		{
			cout << "[����] �޸𸮰� �����մϴ�" << endl;
			break;
		}
		ZeroMemory(&(sInfo->overlapped), sizeof(sInfo->overlapped));
		sInfo->sock = clientSock;
		sInfo->recvbytes = 0;
		sInfo->sendbytes = 0;
		sInfo->wsabuf.buf = sInfo->buf;
		sInfo->wsabuf.len = BUFSIZE;

		// �񵿱� ����� ����
		DWORD recvbytes;
		DWORD flags = 0;
		ret = WSARecv(clientSock, &(sInfo->wsabuf), 1, &recvbytes, &flags, &(sInfo->overlapped), NULL);
		if (ret == SOCKET_ERROR)
		{
			if (WSAGetLastError() != ERROR_IO_PENDING)
			{
				cout << "WSARecv()";
			}
			continue;
		}
	}

	WSACleanup();

	return 0;
}