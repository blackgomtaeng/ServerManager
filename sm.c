#define BUILDING_SM_DLL
#include "sm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

int initServerEngine(GlobalServerConfig *globalConfig, int port) {
    if (globalConfig == NULL) return -1;
    globalConfig->port = port;
    globalConfig->isRunning = 0;
    globalConfig->dbRowCount = 0;
    memset(globalConfig->serverStatusMsg, 0, sizeof(globalConfig->serverStatusMsg));
    memset(globalConfig->dbRows, 0, sizeof(globalConfig->dbRows));

#if defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        strcpy(globalConfig->serverStatusMsg, "WSAStartup Failure");
        return -2;
    }
#endif

    globalConfig->serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (globalConfig->serverFd == INVALID_SOCKET) {
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        strcpy(globalConfig->serverStatusMsg, "Socket Creation Error");
        return -3;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(globalConfig->port);

    if (bind(globalConfig->serverFd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        shutdownServerEngine(globalConfig);
        strcpy(globalConfig->serverStatusMsg, "Bind Failure (Port in Use)");
        return -4;
    }

    strcpy(globalConfig->serverStatusMsg, "Engine Initialized Successfully");
    return 0;
}

void generateSecureCode(char *outCode, int codeLength) {
    if (outCode == NULL || codeLength <= 0) return;
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int charsetSize = sizeof(charset) - 1;
    srand((unsigned int)time(NULL));
    for (int i = 0; i < codeLength; i++) {
        outCode[i] = charset[rand() % charsetSize];
    }
    outCode[codeLength] = '\0';
}

int startServerListen(GlobalServerConfig *globalConfig) {
    if (globalConfig == NULL) return -1;
    if (listen(globalConfig->serverFd, 3) == SOCKET_ERROR) {
        strcpy(globalConfig->serverStatusMsg, "Listen Failure");
        return -2;
    }
    globalConfig->isRunning = 1;
    strcpy(globalConfig->serverStatusMsg, "Server is Live and Listening");
    printf("\n===========================================\n");
    printf("   [통합 관리자 서버 엔진 활성화]   \n");
    printf("   생성된 인증 코드: %s\n", globalConfig->generatedAuthCode);
    printf("   포트 번호 [%d]에서 무한 루프 관제 시작\n", globalConfig->port);
    printf("===========================================\n\n");
    printf("허가여부\t특정코드\t아이피\t\t포트\t유효만료시각\t\t남은일수\n");
    return 0;
}

int verifyClientCredentials(const char *submittedCode, const char *generatedCode, const struct tm *start, const struct tm *end, int *outRemainingDays, char *outExpTimeStr) {
    time_t now = time(NULL);
    struct tm startMod = *start;
    struct tm endMod = *end;
    time_t startTime = mktime(&startMod);
    time_t endTime = mktime(&endMod);
    
    double diffSec = difftime(endTime, now);
    if (outRemainingDays) *outRemainingDays = (int)(diffSec / 86400.0);
    if (outExpTimeStr) strftime(outExpTimeStr, 32, "%Y-%m-%d %H:%M:%S", &endMod);

    if (strcmp(submittedCode, generatedCode) != 0) return 0;
    if (now < startTime || now > endTime) return 0;
    return 1;
}

int registerClientToTable(GlobalServerConfig *globalConfig, char permit, const char *authCode, const char *ip, int port, const char *expTime, const char *remainingDays) {
    if (globalConfig == NULL || globalConfig->dbRowCount >= 100) return -1;
    ConnectedClientRow *row = &globalConfig->dbRows[globalConfig->dbRowCount];
    row->permit = permit;
    strncpy(row->authCode, authCode, sizeof(row->authCode) - 1);
    strncpy(row->ip, ip, sizeof(row->ip) - 1);
    row->port = port;
    strncpy(row->expTime, expTime, sizeof(row->expTime) - 1);
    strncpy(row->remainingDays, remainingDays, sizeof(row->remainingDays) - 1);
    globalConfig->dbRowCount++;
    return 0;
}

int processClientEvent(GlobalServerConfig *globalConfig, LocalContext *localCtx) {
    if (globalConfig == NULL || localCtx == NULL) return -1;
    localCtx->addrLen = sizeof(localCtx->clientAddr);
    localCtx->clientSock = accept(globalConfig->serverFd, (struct sockaddr*)&localCtx->clientAddr, &localCtx->addrLen);
    if (localCtx->clientSock == INVALID_SOCKET) return -1;

    memset(localCtx->buffer, 0, sizeof(localCtx->buffer));
    localCtx->readBytes = recv(localCtx->clientSock, localCtx->buffer, sizeof(localCtx->buffer) - 1, 0);
    
    int returnSignal = 0;
    char clientIp[64] = {0,};
    int clientPort = ntohs(localCtx->clientAddr.sin_port);
    strcpy(clientIp, inet_ntoa(localCtx->clientAddr.sin_addr));

    if (localCtx->readBytes > 0) {
        if (strstr(localCtx->buffer, "CMD:SHUTDOWN") != NULL) {
            printf("\n🚨 [원격 제어] 외부 플랫폼으로부터 서버 소켓 즉각 중지 명령 수신.\n");
            send(localCtx->clientSock, "SHUTDOWN_ACK", 12, 0);
#if defined(_WIN32) || defined(_WIN64)
            closesocket(localCtx->clientSock);
#else
            close(localCtx->clientSock);
#endif
            remoteShutdownServer(globalConfig);
            return 1;
        }

        char submittedCode[64] = {0,};
        char *authPtr = strstr(localCtx->buffer, "AUTH:");
        if (authPtr != NULL) {
            sscanf(authPtr, "AUTH:%63s", submittedCode);
            for(int i = 0; submittedCode[i]; i++) {
                submittedCode[i] = toupper((unsigned char)submittedCode[i]);
            }
        }

        int outRemainingDays = 0;
        char outExpTimeStr[32] = {0,};
        int isValid = verifyClientCredentials(submittedCode, globalConfig->generatedAuthCode, &globalConfig->validStart, &globalConfig->validEnd, &outRemainingDays, outExpTimeStr);

        char currentPermit = 'N';
        if (isValid) {
            send(localCtx->clientSock, "YES", 3, 0);
            memset(localCtx->buffer, 0, sizeof(localCtx->buffer));
            int ackBytes = recv(localCtx->clientSock, localCtx->buffer, sizeof(localCtx->buffer) - 1, 0);
            if (ackBytes > 0 && (strstr(localCtx->buffer, "Y") != NULL || strstr(localCtx->buffer, "y") != NULL)) {
                currentPermit = 'Y';
            } else {
                returnSignal = 1;
            }
        } else {
            send(localCtx->clientSock, "NO", 2, 0);
            returnSignal = 1;
        }

        const char *finalCode = isValid ? globalConfig->generatedAuthCode : (strlen(submittedCode) > 0 ? submittedCode : "NONE");
        char remStr[16];
        snprintf(remStr, sizeof(remStr), "+%02d", isValid ? outRemainingDays : 0);

        printf("%c\t\t%s\t%s\t%d\t%s\t%s\n", currentPermit, finalCode, clientIp, clientPort, outExpTimeStr, remStr);
        registerClientToTable(globalConfig, currentPermit, finalCode, clientIp, clientPort, outExpTimeStr, remStr);
    }

#if defined(_WIN32) || defined(_WIN64)
    closesocket(localCtx->clientSock);
#else
    close(localCtx->clientSock);
#endif
    return returnSignal;
}

void shutdownServerEngine(GlobalServerConfig *globalConfig) {
    if (globalConfig == NULL) return;
    globalConfig->isRunning = 0;
#if defined(_WIN32) || defined(_WIN64)
    if (globalConfig->serverFd != INVALID_SOCKET) closesocket(globalConfig->serverFd);
    WSACleanup();
#else
    if (globalConfig->serverFd != INVALID_SOCKET) close(globalConfig->serverFd);
#endif
    strcpy(globalConfig->serverStatusMsg, "Engine offline. Resources freed.");
}

int getConnectedTableJson(GlobalServerConfig *globalConfig, char *outJson, int maxLen) {
    if (globalConfig == NULL || outJson == NULL) return -1;
    int offset = snprintf(outJson, maxLen, "{\"total_connections\": %d, \"rows\": [", globalConfig->dbRowCount);
    for (int i = 0; i < globalConfig->dbRowCount; i++) {
        ConnectedClientRow *row = &globalConfig->dbRows[i];
        int written = snprintf(outJson + offset, maxLen - offset,
            "{\"permit\":\"%c\",\"auth_code\":\"%s\",\"ip\":\"%s\",\"port\":%d,\"exp_time\":\"%s\",\"remaining\":\"%s\"}%s",
            row->permit, row->authCode, row->ip, row->port, row->expTime, row->remainingDays,
            (i == globalConfig->dbRowCount - 1) ? "" : ",");
        if (written < 0 || offset + written >= maxLen) return -2;
        offset += written;
    }
    snprintf(outJson + offset, maxLen - offset, "]}");
    return 0;
}

int getSingleRowData(GlobalServerConfig *globalConfig, int index, char *outPermit, char *outAuth, char *outIp, int *outPort, char *outExp, char *outRem) {
    if (globalConfig == NULL || index < 0 || index >= globalConfig->dbRowCount) return -1;
    ConnectedClientRow *row = &globalConfig->dbRows[index];
    if (outPermit) *outPermit = row->permit;
    if (outAuth) strcpy(outAuth, row->authCode);
    if (outIp) strcpy(outIp, row->ip);
    if (outPort) *outPort = row->port;
    if (outExp) strcpy(outExp, row->expTime);
    if (outRem) strcpy(outRem, row->remainingDays);
    return 0;
}

void remoteShutdownServer(GlobalServerConfig *globalConfig) {
    if (globalConfig == NULL) return; // 이전 호환 처리 유지
    globalConfig->isRunning = 0;
    shutdownServerEngine(globalConfig);
}
