// ==========================================================================
// Network.cpp
// ==========================================================================

#include "Hunt.h"

const int bufSizeMax = 189;
const int bufSizeClient = 31;
const int bufSizeHostInit = 800;
const int bufSizeHost = 800;

// Forward declarations
DWORD WINAPI ServerCommsThread(LPVOID lpParameter);
DWORD WINAPI ClientCommsThread(LPVOID lpParameter);

void putInt(byte data[], int *pos, long in) {
	data[*pos] = static_cast<int>(((in & 0XFF)));
	*pos += 1;
}

void putInt2(byte data[], int *pos, long in) {
	data[*pos] = static_cast<int>(((in >> 8) & 0XFF));
	data[*pos + 1] = static_cast<int>(((in & 0XFF)));
	*pos += 2;
}

void putFloat(byte data[], int *pos, long in) {
	data[*pos]   = static_cast<int>(((in >> 24) & 0xFF));
	data[*pos+1] = static_cast<int>(((in >> 16) & 0xFF));
	data[*pos+2] = static_cast<int>(((in >> 8) & 0XFF));
	data[*pos+3] = static_cast<int>(((in & 0XFF)));
	*pos += 4;
}

int readInt(const byte data[], int *pos) {
	int pos2 = *pos;
	*pos += 1;
	return static_cast<int>(((data[pos2])));
}

int readInt2(const byte data[], int *pos) {
	int pos2 = *pos;
	*pos += 2;
	return static_cast<int>(((data[pos2] << 8)
		+ (data[pos2 + 1])));
}

float readFloat(const byte data[], int *pos) {
	int pos2 = *pos;
	*pos += 4;
	return static_cast<float>(((data[pos2] << 24)
		+ (data[pos2 + 1] << 16)
		+ (data[pos2 + 2] << 8)
		+ (data[pos2 + 3])));
}

bool RecvPacket(SOCKET *socket, int bufSize, bool init){
	iResult = recv(*socket, recvbuf, bufSize, 0);
	if (iResult > 0) {

		const byte *tdata2 = reinterpret_cast<const byte*>(recvbuf);
		int pos = 0;

		Vector3d posTemp;
		posTemp.x = readFloat(tdata2, &pos) / 10000.f;
		posTemp.y = readFloat(tdata2, &pos) / 10000.f;
		posTemp.z = readFloat(tdata2, &pos) / 10000.f;
		MPlayers[0].pos = posTemp;

		MPlayers[0].alpha = readFloat(tdata2, &pos) / 10000.f;

		MPlayers[0].alpha += 1.5 * pi;
		if (MPlayers[0].alpha > pi * 2) MPlayers[0].alpha -= 2 * pi;

		int t = readInt(tdata2, &pos) - 1;
		if (t >= 0) mGunShot[0] = t;

		t = readInt(tdata2, &pos) - 1;
		if (t >= 0) mHunterCall[0] = t;

		t = readInt(tdata2, &pos) - 1;
		if (t >= 0) mHunterCallType[0] = t;

		if (!Host) {

			Wind.alpha = readFloat(tdata2, &pos) / 10000.f;
			Wind.speed = readFloat(tdata2, &pos) / 10000.f;

			for (int c = 0; c < 6; c++) {
				Characters[c].pos.x = readFloat(tdata2, &pos) / 10000.f;
				Characters[c].pos.z = readFloat(tdata2, &pos) / 10000.f;
				if (Characters[c].Clone == AI_DIMOR || Characters[c].Clone == AI_PTERA) Characters[c].pos.y = readFloat(tdata2, &pos) / 10000.f; //potentially all 6 ambs
				Characters[c].alpha = readFloat(tdata2, &pos) / 10000.f;
				Characters[c].bend = readFloat(tdata2, &pos) / 10000.f;
				Characters[c].Phase = readInt(tdata2, &pos);
				if (init) {
					Characters[c].scale = readFloat(tdata2, &pos) / 10000.f;
					Characters[c].deathType = readInt(tdata2, &pos);
					Characters[c].waterDieAnim = readInt(tdata2, &pos);
				}
			}
		} else {
			for (int c = 0; c < 6; c++) {
				mDamage[0][c] += readInt2(tdata2, &pos);
			}
		}

		return true;
	}
	else return false;
}

void SendPacket(SOCKET *socket, const int bufSize, bool init) {

	int pos = 0;
	byte tdata[bufSizeMax];

	long px = PlayerX * 10000.f;
	long py = PlayerY * 10000.f;
	long pz = PlayerZ * 10000.f;
	long pa = PlayerAlpha * 10000.f;
	putFloat(tdata, &pos, px);
	putFloat(tdata, &pos, py);
	putFloat(tdata, &pos, pz);
	putFloat(tdata, &pos, pa);

	long ps = sendGunShot + 1;
	sendGunShot = -1;
	putInt(tdata, &pos, ps);

	long pc = sendHunterCall + 1;
	sendHunterCall = -1;
	putInt(tdata, &pos, pc);

	long pct = sendHunterCallType + 1;
	sendHunterCallType = -1;
	putInt(tdata, &pos, pct);

	if (Host) {
		long wa = Wind.alpha * 10000.;
		long ws = Wind.speed * 10000.;
		putFloat(tdata, &pos, wa);
		putFloat(tdata, &pos, ws);

		for (int c = 0; c < 6; c++) {
			long charX = Characters[c].pos.x * 10000.f;
			putFloat(tdata, &pos, charX);
			long charZ = Characters[c].pos.z * 10000.f;
			putFloat(tdata, &pos, charZ);
			if (Characters[c].Clone == AI_DIMOR || Characters[c].Clone == AI_PTERA) { //potentially all 6 ambs
				long charY = Characters[c].pos.y * 10000.f;
				putFloat(tdata, &pos, charY);
			}
			long charA = Characters[c].alpha * 10000.f;
			putFloat(tdata, &pos, charA);
			long charB = Characters[c].bend * 10000.f;
			putFloat(tdata, &pos, charB);
			long charPh = Characters[c].Phase;
			putInt(tdata, &pos, charPh);
			if (init) {
				long charScale = Characters[c].scale * 10000.f;
				putFloat(tdata, &pos, charScale);
				//long charKill = Characters[c].killType;	// not for classic ambients
				//putInt(tdata, &pos, charKill);
				long charDeath = Characters[c].deathType;
				putInt(tdata, &pos, charDeath);
				long charWDeath = Characters[c].waterDieAnim;
				putInt(tdata, &pos, charWDeath);
				//long charRoar = Characters[c].roarAnim;	// not for classic ambients
				//putInt(tdata, &pos, charRoar);
			}
		}

	} else {
		for (int c = 0; c < 6; c++) {
			long Damage = sendDamage[c];
			sendDamage[c] = 0;
			putInt2(tdata, &pos, Damage);
		}
	}

	const char *sendbuf = reinterpret_cast<const char*>(tdata);

	iSendResult = send(*socket, sendbuf, bufSize, 0);
	if (iSendResult == SOCKET_ERROR) {
		PrintLog("send failed");
		closesocket(*socket);
		WSACleanup();
		DoHalt("Multiplayer: Send failed");
	}
}

void ShutDownServer() {

	//shutdown thread
	PrintLog("Server Comms Thread Shutting Down...\n");
	HaltThread = false;
	WaitForSingleObject(CommsThreadHandle, INFINITE);
	CloseHandle(CommsThreadHandle);

	
	//tell clients to shut down

	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		PrintLog("shutdown failed\n");
		closesocket(ClientSocket);
		WSACleanup();
		DoHalt2("Multiplayer Host: shutdown failed\n");
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();

	//test
	PrintLog("ShutDown multiplayer\n");
	PrintLog("COMPLETE!\n");
}

void ShutDownClient() {

	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		PrintLog("shutdown failed");
		closesocket(ConnectSocket);
		WSACleanup();
		DoHalt2("Multiplayer Client: Shutdown failed");
	}

	//shutdown thread
	PrintLog("Client Comms Thread Shutting Down...\n");
	HaltThread = false;
	WaitForSingleObject(CommsThreadHandle, INFINITE);
	CloseHandle(CommsThreadHandle);

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	//test
	PrintLog("Client connection closed!\n");
	PrintLog("COMPLETE!\n");
}

void _StartupServer() {
	//server

	PrintLog("Starting Server...\n");



	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		PrintLog("WSAStartup failed\n");
		DoHalt2("Multiplayer Host: WSAStartup failed");
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(nullptr, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		PrintLog("getaddrinfo failed\n");
		WSACleanup();
		DoHalt2("Multiplayer Host: getaddrinfo failed");
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		PrintLog("socket failed\n");
		freeaddrinfo(result);
		WSACleanup();
		DoHalt2("Multiplayer Host: socket failed");
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, static_cast<int>(result->ai_addrlen));
	if (iResult == SOCKET_ERROR) {
		PrintLog("bind failed\n");
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		DoHalt2("Multiplayer Host: bind failed");
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		PrintLog("listen failed\n");
		closesocket(ListenSocket);
		WSACleanup();
		DoHalt2("Multiplayer Host: listen failed");
	}

	PrintLog("Waiting for client...\n");


	//CLIENT CONNECTION - TEMPORARY, check for new clients for x amount of time each iteration? Shutdown on exit, kick clients first


	// Accept a client socket
	ClientSocket = accept(ListenSocket, nullptr, nullptr);
	if (ClientSocket == INVALID_SOCKET) {
		PrintLog("accept failed\n");
		closesocket(ListenSocket);
		WSACleanup();
		DoHalt2("Multiplayer Host: accept failed");
	}
	else PrintLog("Client accepted!\n");

	// No longer need server socket
	closesocket(ListenSocket);

	/*
	// Receive until the peer shuts down the connection
	do {

		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			PrintLog("Bytes received: ");
			char bytesRec[25];
			_itoa(iResult, bytesRec, 10);
			PrintLog(bytesRec);
			PrintLog("\n");


			// Echo the buffer back to the sender
			iSendResult = send(ClientSocket, recvbuf, iResult, 0);
			if (iSendResult == SOCKET_ERROR) {
				PrintLog("send failed\n");
				closesocket(ClientSocket);
				WSACleanup();
				DoHalt2("Multiplayer Host: send failed");
			}
			PrintLog("Bytes sent: ");
			char bytesSent[25];
			_itoa(iSendResult, bytesSent, 10);
			PrintLog(bytesSent);
			PrintLog("\n");
		}
		else if (iResult == 0) {}
		else {
			PrintLog("recv failed\n");
			closesocket(ClientSocket);
			WSACleanup();
			DoHalt2("Multiplayer Host: recv failed");
		}

	} while (iResult > 0);
	*/
}

void _StartupClient() {
	//CLIENT



	struct addrinfo
		//		  *result = nullptr,
		*ptr = nullptr
		//		  ,
		//		  hints
		;

	const char *sendbuf = "this is a test";

	/*
	// Validate the parameters
	if (argc != 2) {
		printf("usage: %s server-name\n", argv[0]);
		return 1;
	}
	*/

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		PrintLog("WSAStartup failed");
		DoHalt2("Multiplayer Client: WSAStartup failed");
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(static_cast<LPCTSTR>(ServerAddress), DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		PrintLog("getaddrinfo failed");
		WSACleanup();
		DoHalt2("Multiplayer Client: getaddrinfo failed");
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			PrintLog("socket failed\n");
			WSACleanup();
			DoHalt2("Multiplayer Client: Socket failed");
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen));
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		PrintLog("Unable to connect to server!\n");
		WSACleanup();
		DoHalt2("Multiplayer Client: Unable to connect to server!");
	}

	/*
	// Send an initial buffer
	iResult = send(ConnectSocket, sendbuf, static_cast<int>(strlen(sendbuf)), 0);
	if (iResult == SOCKET_ERROR) {
		PrintLog("send failed");
		closesocket(ConnectSocket);
		WSACleanup();
		DoHalt2("Multiplayer Client: Send failed");
	}

	//PrintLog("Bytes Sent: %ld\n", iResult);
	PrintLog("Bytes sent: ");
	char bytesSent[25];
	_itoa(iResult, bytesSent, 10);
	PrintLog(bytesSent);
	PrintLog("\n");

	// Receive until the peer closes the connection
	bool responded = false;
	do {
		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0)
		{
			//PrintLog("Bytes received: %d\n", iResult);
			PrintLog("Bytes received: ");
			char bytesSent[25];
			_itoa(iResult, bytesSent, 10);
			PrintLog(bytesSent);
			PrintLog("\n");
			responded = true;
		}
		else if (iResult == 0) {
			//PrintLog("Connection closed\n");
		}
		else {
			PrintLog("recv failed\n");
		}

	} while (!responded);
	*/

	//multiplayer test end
}

void StartupServerCommsThread() {
	PrintLog("Starting Server Comms...\n");
	_StartupServer();
	CommsThreadHandle = CreateThread(0, 0, ServerCommsThread, nullptr, 0, CommsThreadID);
	PrintLog("Server Comms Thread Started\n");
}

void StartupClientCommsThread() {
	PrintLog("Starting Client Comms...\n");
	_StartupClient();
	CommsThreadHandle = CreateThread(0, 0, ClientCommsThread, nullptr, 0, CommsThreadID);
	PrintLog("Client Comms Thread Started\n");
}

DWORD WINAPI ServerCommsThread(LPVOID lpParameter)
{
	bool init = true;

	while (HaltThread) {

		bool result = RecvPacket(&ClientSocket, bufSizeClient, false);
		if (result) {

			if (init) {
				SendPacket(&ClientSocket, bufSizeHostInit, true);
				init = false;
				PrintLogVerbose("INIT_PACKET_SENT\n");//TEST
			} else SendPacket(&ClientSocket, bufSizeHost, false);

		} else if (iResult != 0) {
			PrintLog("recv failed\n");
			closesocket(ClientSocket);
			WSACleanup();
			DoHalt("Multiplayer Host: recv failed");
		}
	}
	PrintLog("Server Comms Thread Shutdown Successful!\n");
	return 0;
}

DWORD WINAPI ClientCommsThread(LPVOID lpParameter)
{
	bool init = true;

	while (HaltThread) {

		SendPacket(&ConnectSocket,bufSizeClient, false);

		// Receive until the peer closes the connection
		bool responded = false;
		do {

			if (init) {
				responded = RecvPacket(&ConnectSocket, bufSizeHostInit, true);
			}
			else responded = RecvPacket(&ConnectSocket, bufSizeHost, false);

			if (!responded && iResult != 0) {
				PrintLog("recv failed\n");
			}

		} while (!responded);

		if (init) PrintLogVerbose("INIT_PACKET_RECV\n");//TEST
		init = false;

		//Sleep(10);//test
		// if laggy, add sleep statement
	}
	PrintLog("Client Comms Thread Shutdown Successful!\n");
	return 0;
}