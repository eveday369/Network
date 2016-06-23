#include <WinSock2.h>
#include <iostream>

#pragma warning(disable:4996)

using namespace std;

#define BUFSIZE 1000
#define PORT 9000

int main()
{
	int recval;
	SOCKET clientSocket;
	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		cout << "WSAStartup Error" << endl;
	}

	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET)
	{
		cout << "socket() error" << endl;
	}

	SOCKADDR_IN clientAddr;
	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(PORT);
	clientAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	recval = connect(clientSocket, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
	if (recval == SOCKET_ERROR)
	{
		cout << "connect()" << endl;
	}

	char readBuffer[BUFSIZE + 1];
	int bLength;

	while (1)
	{
		ZeroMemory(readBuffer, sizeof(readBuffer));
		cout << endl << "[전송 데이터]: ";

		if (fgets(readBuffer, BUFSIZE + 1, stdin) == NULL)
		{
			break;
		}

		bLength = strlen(readBuffer);
		if (readBuffer[bLength - 1] == '\n')
			readBuffer[bLength - 1] = '\n';
		if (strlen(readBuffer) == 0)
			break;

		recval = send(clientSocket, readBuffer, strlen(readBuffer), 0);
		if (recval == SOCKET_ERROR)
		{
			cout << "send()";
			break;
		}

		cout << "[TCP 클라이언트] " << recval << "바이트를 보냈습니다." << endl;
		recval = recv(clientSocket, readBuffer, recval, 0);
		if (recval == 0)
		{
			break;
		}
		readBuffer[BUFSIZE] = '\n';
		cout << "[TCP 클라이언트] " << recval << "바이트를 받았습니다" << endl;
		cout << "[받은 데이터] " << readBuffer << endl;
	}
	closesocket(clientSocket);

	WSACleanup();
	return 0;
}