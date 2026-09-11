// ==========================================================================
// NetworkManager.cpp -- Multiplayer networking implementation
// ==========================================================================

#include "Hunt.h"
#include "Network/NetworkManager.h"

// Constants needed by network code
const int bufSizeMax = 189;

// Global instance
NetworkManager g_Network;

// --- Serialization helpers (free functions, no class state needed) ---

void putInt(byte data[], int *pos, long in) {
    data[*pos] = static_cast<int>(((in & 0XFF)));
    *pos += 1;
}

void putInt2(byte data[], int *pos, long in) {
    data[*pos]     = static_cast<int>(((in >> 8) & 0XFF));
    data[*pos + 1] = static_cast<int>(((in & 0XFF)));
    *pos += 2;
}

void putFloat(byte data[], int *pos, long in) {
    data[*pos]     = static_cast<int>(((in >> 24) & 0xFF));
    data[*pos + 1] = static_cast<int>(((in >> 16) & 0xFF));
    data[*pos + 2] = static_cast<int>(((in >> 8) & 0XFF));
    data[*pos + 3] = static_cast<int>(((in & 0XFF)));
    *pos += 4;
}

int readInt(const byte data[], int *pos) {
    int result = data[*pos];
    *pos += 1;
    return result;
}

int readInt2(const byte data[], int *pos) {
    int result = (data[*pos] << 8) + data[*pos + 1];
    *pos += 2;
    return result;
}

float readFloat(const byte data[], int *pos) {
    int v = (data[*pos] << 24) + (data[*pos + 1] << 16) + (data[*pos + 2] << 8) + data[*pos + 3];
    *pos += 4;
    return *reinterpret_cast<float*>(&v);
}

// --- NetworkManager implementation ---

bool NetworkManager::Initialize() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        PrintLog("NetworkManager: WSAStartup failed\n");
        return false;
    }
    return true;
}

void NetworkManager::Shutdown() {
    StopServer();
    StopClient();
    WSACleanup();
}

bool NetworkManager::StartServer() {
    if (m_isHost) return true; // already started

    struct addrinfo *result = nullptr;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    int iResult = getaddrinfo(nullptr, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        DoHalt2("Multiplayer Host: getaddrinfo failed");
        return false;
    }

    m_listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (m_listenSocket == INVALID_SOCKET) {
        freeaddrinfo(result);
        DoHalt2("Multiplayer Host: socket failed");
        return false;
    }

    iResult = bind(m_listenSocket, result->ai_addr, static_cast<int>(result->ai_addrlen));
    if (iResult == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        DoHalt2("Multiplayer Host: bind failed");
        return false;
    }

    freeaddrinfo(result);

    iResult = listen(m_listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        DoHalt2("Multiplayer Host: listen failed");
        return false;
    }

    m_isHost = true;
    return true;
}

bool NetworkManager::ConnectToServer(const char* address) {
    struct addrinfo *result = nullptr;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int iResult = getaddrinfo(address, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        DoHalt2("Multiplayer Client: getaddrinfo failed");
        return false;
    }

    m_connectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (m_connectSocket == INVALID_SOCKET) {
        freeaddrinfo(result);
        DoHalt2("Multiplayer Client: socket failed");
        return false;
    }

    iResult = connect(m_connectSocket, result->ai_addr, static_cast<int>(result->ai_addrlen));
    if (iResult == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(m_connectSocket);
        m_connectSocket = INVALID_SOCKET;
        DoHalt2("Multiplayer Client: connect failed");
        return false;
    }

    freeaddrinfo(result);
    m_isConnected = true;
    return true;
}

void NetworkManager::Disconnect() {
    StopClient();
}

bool NetworkManager::RecvPacket(SOCKET socket, int bufSize, bool init) {
    int iResult = recv(socket, m_recvBuf, bufSize, 0);
    if (iResult > 0) {
        m_recvbuflen = iResult;
        if (init) {
            const byte* tdata2 = reinterpret_cast<const byte*>(m_recvBuf);
            int pos = 0;
            int test = readInt(tdata2, &pos);
            if (test == 43981) {
                if (!m_isHost) {
                    pos = 0;
                    // Client receives initial player count
                    if (m_recvbuflen > 175) {
                        PlayersCount = 2;
                    }
                }
                return true;
            }
            return false;
        }
        return true;
    } else if (iResult == 0) {
        return false;
    } else {
        return false;
    }
}

void NetworkManager::SendPacket(SOCKET socket, const int bufSize, bool init) {
    int iResult;
    if (m_isHost) {
        // Host sends to all connected clients
        iResult = send(socket, m_recvBuf, bufSize, 0);
        if (iResult == SOCKET_ERROR) {
            DoHalt("Multiplayer: Send failed");
        }
    } else {
        iResult = send(socket, m_recvBuf, bufSize, 0);
        if (iResult == SOCKET_ERROR) {
            DoHalt("Multiplayer: Send failed");
        }
    }
}

void NetworkManager::StopServer() {
    m_haltThread = false;
    if (m_commsThread) {
        WaitForSingleObject(m_commsThread, INFINITE);
        CloseHandle(m_commsThread);
        m_commsThread = nullptr;
    }

    if (m_clientSocket != INVALID_SOCKET) {
        int iResult = shutdown(m_clientSocket, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            closesocket(m_clientSocket);
            m_clientSocket = INVALID_SOCKET;
            DoHalt2("Multiplayer Host: shutdown failed\n");
            return;
        }
        closesocket(m_clientSocket);
        m_clientSocket = INVALID_SOCKET;
    }

    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    m_isHost = false;
}

void NetworkManager::StopClient() {
    m_haltThread = false;
    if (m_commsThread) {
        WaitForSingleObject(m_commsThread, INFINITE);
        CloseHandle(m_commsThread);
        m_commsThread = nullptr;
    }

    if (m_connectSocket != INVALID_SOCKET) {
        int iResult = shutdown(m_connectSocket, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            closesocket(m_connectSocket);
            m_connectSocket = INVALID_SOCKET;
            DoHalt2("Multiplayer Client: Shutdown failed");
            return;
        }
        closesocket(m_connectSocket);
        m_connectSocket = INVALID_SOCKET;
    }

    m_isConnected = false;
}

void NetworkManager::StartServerCommsThread() {
    m_haltThread = true;
    m_commsThread = CreateThread(nullptr, 0, ServerThreadProc, this, 0, m_commsThreadID);
    if (m_commsThread) {
        SetThreadPriority(m_commsThread, THREAD_PRIORITY_HIGHEST);
    }
}

void NetworkManager::StartClientCommsThread() {
    m_haltThread = true;
    m_commsThread = CreateThread(nullptr, 0, ClientThreadProc, this, 0, m_commsThreadID);
    if (m_commsThread) {
        SetThreadPriority(m_commsThread, THREAD_PRIORITY_HIGHEST);
    }
}

DWORD WINAPI NetworkManager::ServerThreadProc(LPVOID param) {
    NetworkManager* mgr = static_cast<NetworkManager*>(param);
    return mgr->ServerThread();
}

DWORD WINAPI NetworkManager::ClientThreadProc(LPVOID param) {
    NetworkManager* mgr = static_cast<NetworkManager*>(param);
    return mgr->ClientThread();
}

DWORD NetworkManager::ServerThread() {
    // The original server comms thread processing
    // Accept connection, then handle game state sync
    // (This mirrors the logic from the original ServerCommsThread)
    PrintLog("NetworkManager: Server comms thread started\n");

    // Accept a client connection
    m_clientSocket = accept(m_listenSocket, nullptr, nullptr);
    if (m_clientSocket == INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        DoHalt2("Multiplayer Host: accept failed");
        return 1;
    }

    // Main communication loop
    while (m_haltThread) {
        // Receive updates from client
        if (!RecvPacket(m_clientSocket, bufSizeMax, false)) {
            break;
        }

        // Process received data and send updates
        SendPacket(m_clientSocket, 10, false);

        Sleep(10);
    }

    return 0;
}

DWORD NetworkManager::ClientThread() {
    PrintLog("NetworkManager: Client comms thread started\n");

    // Main communication loop
    while (m_haltThread) {
        // Send updates to server
        SendPacket(m_connectSocket, 10, false);

        // Receive updates from server
        if (!RecvPacket(m_connectSocket, bufSizeMax, false)) {
            break;
        }

        Sleep(10);
    }

    return 0;
}
