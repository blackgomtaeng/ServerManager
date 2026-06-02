#ifndef SM_H
#define SM_H

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define SM_API __declspec(dllexport)
    typedef int socklen_t;
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #define SM_API __attribute__((visibility("default")))
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

#include <time.h>

typedef struct {
    char permit;
    char authCode[64];
    char ipv4[32];
    int port;
    char expTime[32];
    char remainingDays[8];
} ConnectedClientRow;

typedef struct {
    int port;
    int is_running;
#if defined(_WIN32) || defined(_WIN64)
    SOCKET serverFD;
#else
    int serverFD;
#endif
    char serverStatusMessage[256];
    char generatedAuthCode[64];
    int codeLength;
    struct tm validStart;
    struct tm validEnd;
    ConnectedClientRow dbRows[100];
    int dbRowCount;
} GlobalServerConfig;

typedef struct {
#if defined(_WIN32) || defined(_WIN64)
    SOCKET clientSock;
#else
    int clientSock;
#endif
    struct sockaddr_in clientAddr;
    socklen_t addrLen;
    int readBytes;
    char buffer[67584];
} LocalContext;

SM_API int initServerEngine(GlobalServerConfig *globalConfig, int port);
SM_API void generateSecureCode(GlobalServerConfig *globalConfig);
SM_API int startServerListen(GlobalServerConfig *globalConfig);
SM_API int processClientEvent(GlobalServerConfig *globalConfig, LocalContext *local_ctx);
SM_API void shutdownServerEngine(GlobalServerConfig *globalConfig);
SM_API int getConnectedTableJson(GlobalServerConfig *globalConfig, char *out_json, int max_len);

#endif
