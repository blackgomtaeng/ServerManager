#ifndef SM_H
#define SM_H

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #ifdef BUILDING_SM_DLL
        #define SM_API __declspec(dllexport)
    #else
        #define SM_API __declspec(dllimport)
    #endif
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
    char authCode[128];
    char ip[32];
    int port;
    char expTime[32];
    char remainingDays[16];
    char userRole[32];
} ConnectedClientRow;

typedef struct {
    int port;
    int isRunning;
#if defined(_WIN32) || defined(_WIN64)
    SOCKET serverFd;
#else
    int serverFd;
#endif
    char serverStatusMsg[256];
    char generatedAuthCode[64];
    char encryptedAuthCode[128];
    int codeLength;
    struct tm validStart;
    struct tm validEnd;
    ConnectedClientRow dbRows[100];
    int dbRowCount;
    char targetRole[32];
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
    char buffer[65536 + 2048];
} LocalContext;

SM_API int initServerEngine(GlobalServerConfig *globalConfig, int port);
SM_API int startServerListen(GlobalServerConfig *globalConfig);
SM_API int processClientEvent(GlobalServerConfig *globalConfig, LocalContext *localCtx);
SM_API void shutdownServerEngine(GlobalServerConfig *globalConfig);
SM_API void manageSecurityCode(GlobalServerConfig *globalConfig, int mode, const char *input, char *output);
SM_API int parseFlexibleDateTime(const char *inputStr, struct tm *outputTm);
SM_API void writeLogToCsv(char permit, const char *role, const char *code, const char *ip, int port, const char *exp);

#endif
