#include <WinSock2.h>
#include <iostream>

#pragma warning(disable:4996)

using namespace std;

#define BUFSIZE 512

// 소켓 정보 저장을 위한 구조체
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
		// 비동기 입출력 완료 기다리기
		DWORD cbTransferred;
		SOCKET clientSock;
		SOCK_INFO *sInfo;
		ret = GetQueuedCompletionStatus(hIocp, &cbTransferred, (LPDWORD)&clientSock, (LPOVERLAPPED *)&sInfo, INFINITE);

		// 클라이언트 정보 얻기
		SOCKADDR_IN addrClient;
		int addrlen = sizeof(addrClient);
		getpeername(sInfo->sock, (SOCKADDR *)&addrClient, &addrlen);

		// 비동기 입출력 결과 확인
		if (ret == 0 || cbTransferred == 0)
		{
			if (ret == 0)
			{
				DWORD temp1, temp2;
				WSAGetOverlappedResult(sInfo->sock, &(sInfo->overlapped), &temp1, FALSE, &temp2);
				cout << "WSAGetOverlappedResult()" << endl;
			}
			closesocket(sInfo->sock);
			cout << "[TCP 서버] 클라이언트 종료: IP 주소 = " << inet_ntoa(addrClient.sin_addr) << " 포트 번호 = " << ntohs(addrClient.sin_port);
			cout << endl;
			delete sInfo;
			continue;
		}

		// 데이터 전송량 갱신
		if (sInfo->recvbytes == 0)
		{
			sInfo->recvbytes = cbTransferred;
			sInfo->sendbytes = 0;
			// 받은 데이터 출력
			sInfo->buf[sInfo->recvbytes] = '\0';
			cout << "[TCP/" << inet_ntoa(addrClient.sin_addr) << ':' << ntohs(addrClient.sin_port) << ']' << sInfo->buf << endl;
		}
		else
		{
			sInfo->sendbytes += cbTransferred;
		}

		if (sInfo->recvbytes > sInfo->sendbytes)
		{
			// 데이터 보내기
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

			// 데이터 받기
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

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		return -1;
	}

	// IOCP 생성
	HANDLE hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIocp == NULL)
		return -1;

	// CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	// (CPU개수 * 2)개의 작업자 스레드 생성
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
		cout << "[TCP 서버] 클라이언트 접속: IP 주소 = " << inet_ntoa(addrClient.sin_addr) << " 포트 번호 = " << ntohs(addrClient.sin_port) << endl;

		// 소켓과 입출력 완료 포트 연결
		HANDLE hResult = CreateIoCompletionPort((HANDLE)clientSock, hIocp, (DWORD)clientSock, 0);
		if (hResult == NULL)
			exit(-1);

		// 소켓 정보 구조체 할당
		SOCK_INFO *sInfo = new SOCK_INFO;
		if (sInfo == NULL)
		{
			cout << "[오류] 메모리가 부족합니다" << endl;
			break;
		}
		ZeroMemory(&(sInfo->overlapped), sizeof(sInfo->overlapped));
		sInfo->sock = clientSock;
		sInfo->recvbytes = 0;
		sInfo->sendbytes = 0;
		sInfo->wsabuf.buf = sInfo->buf;
		sInfo->wsabuf.len = BUFSIZE;

		// 비동기 입출력 시작
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