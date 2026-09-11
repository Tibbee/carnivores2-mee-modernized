// ==========================================================================
// NetworkManager.h -- Class-based multiplayer networking
// ==========================================================================

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include "Core/GameTypes.h"

// Forward declarations
struct TPlayer;
struct TCharacter;

class NetworkManager {
public:
    bool Initialize();
    void Shutdown();

    bool StartServer();
    bool ConnectToServer(const char* address);
    void Disconnect();

    bool IsHost() const { return m_isHost; }
    bool IsConnected() const { return m_isConnected; }
    bool IsMultiplayer() const { return m_isMultiplayer; }

    void SetMultiplayer(bool val) { m_isMultiplayer = val; }
    void SetHost(bool val) { m_isHost = val; }

    // Thread control
    void StartServerCommsThread();
    void StartClientCommsThread();
    void StopServer();
    void StopClient();

    // Packet I/O
    bool RecvPacket(SOCKET socket, int bufSize, bool init);
    void SendPacket(SOCKET socket, const int bufSize, bool init);

    // Accessors (for legacy code that reads globals directly)
    SOCKET GetListenSocket() const { return m_listenSocket; }
    SOCKET GetClientSocket() const { return m_clientSocket; }
    SOCKET GetConnectSocket() const { return m_connectSocket; }

    // Server address
    char m_serverAddress[128];

private:
    SOCKET m_listenSocket = INVALID_SOCKET;
    SOCKET m_clientSocket = INVALID_SOCKET;
    SOCKET m_connectSocket = INVALID_SOCKET;

    HANDLE m_commsThread = nullptr;
    LPDWORD m_commsThreadID = nullptr;
    bool m_haltThread = false;
    bool m_isHost = false;
    bool m_isConnected = false;
    bool m_isMultiplayer = false;

    char m_recvBuf[DEFAULT_BUFLEN];
    int m_recvbuflen = 0;

    void StartupServer();
    void StartupClient();

    static DWORD WINAPI ServerThreadProc(LPVOID param);
    static DWORD WINAPI ClientThreadProc(LPVOID param);

    DWORD ServerThread();
    DWORD ClientThread();
};

// Global network manager instance
extern NetworkManager g_Network;
