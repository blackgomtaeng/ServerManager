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

int parseFlexibleDateTime(const char *inputStr, struct tm *outputTm) {
    if (inputStr == NULL || outputTm == NULL) return -1;
    char digits[32] = {0,};
    int digitIdx = 0;
    
    for (int i = 0; inputStr[i] != '\0' && digitIdx < 31; i++) {
        if (isdigit((unsigned char)inputStr[i])) {
            digits[digitIdx++] = inputStr[i];
        }
    }
    digits[digitIdx] = '\0';

    int year = 0, mon = 0, day = 0, hour = 0, min = 0;
    if (digitIdx == 12) {
        sscanf(digits, "%4d%2d%2d%2d%2d", &year, &mon, &day, &hour, &min);
    } else if (digitIdx == 8) {
        sscanf(digits, "%4d%2d%2d", &year, &mon, &day);
        hour = 0; min = 0;
    } else {
        return -2;
    }

    memset(outputTm, 0, sizeof(struct tm));
    outputTm->tm_year = year - 1900;
    outputTm->tm_mon = mon - 1;
    outputTm->tm_mday = day;
    outputTm->tm_hour = hour;
    outputTm->tm_min = min;
    return 0;
}

void manageSecurityCode(GlobalServerConfig *globalConfig, int mode, const char *input, char *output) {
    const char xorKey = 0x5A;
    if (mode == 1) {
        const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        int charsetSize = sizeof(charset) - 1;
        srand((unsigned int)time(NULL));
        for (int i = 0; i < globalConfig->codeLength; i++) {
            globalConfig->generatedAuthCode[i] = charset[rand() % charsetSize];
        }
        globalConfig->generatedAuthCode[globalConfig->codeLength] = '\0';
        
        int i = 0;
        while (globalConfig->generatedAuthCode[i] != '\0') {
            sprintf(&globalConfig->encryptedAuthCode[i * 2], "%02X", (unsigned char)(globalConfig->generatedAuthCode[i] ^ xorKey));
            i++;
        }
        globalConfig->encryptedAuthCode[i * 2] = '\0';
    } else if (mode == 2 && input != NULL && output != NULL) {
        int len = strlen(input);
        int i = 0;
        for (i = 0; i < len / 2; i++) {
            unsigned int val;
            sscanf(&input[i * 2], "%2X", &val);
            output[i] = (char)(val ^ xorKey);
        }
        output[i] = '\0';
    }
}

void writeLogToCsv(char permit, const char *role, const char *code, const char *ip, int port, const char *exp) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[32];
    snprintf(filename, sizeof(filename), "s%04d%02d%02d.csv", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

    FILE *fp = fopen(filename, "a");
    if (fp != NULL) {
        fprintf(fp, "%02d:%02d:%02d,%c,%s,%s,%s,%d,%s\n", 
                t->tm_hour, t->tm_min, t->tm_sec, permit, role, code, ip, port, exp);
        fclose(fp);
    }
}

int startServerListen(GlobalServerConfig *globalConfig) {
    if (globalConfig == NULL) return -1;
    if (listen(globalConfig->serverFd, 3) == SOCKET_ERROR) {
        strcpy(globalConfig->serverStatusMsg, "Listen Failure");
        return -2;
    }
    globalConfig->isRunning = 1;
    strcpy(globalConfig->serverStatusMsg, "Server is Live and Listening");
    
    printf("\n========================================================================================\n");
    printf("   [통합 멀티 관제 서버 엔진 가동] - 다중 연결 지원 및 CSV 자동화 기록 작동 중\n");
    printf("   지정 등급 권한        : %s\n", globalConfig->targetRole);
    printf("   원본인증코드 (Server) : %s\n", globalConfig->generatedAuthCode);
    printf("   암호화된 코드(Client) : %s\n", globalConfig->encryptedAuthCode);
    printf("========================================================================================\n\n");
    printf("번호\t허가여부\t접근등급\t입력암호코드\t\t\t아이피\t\t포트\t유효만료시각\t\t남은일수\n");
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
    char clientIp[32] = {0,};
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
            globalConfig->isRunning = 0;
            shutdownServerEngine(globalConfig);
            return 1;
        }

        char submittedCipher[128] = {0,};
        char decryptedPlain[128] = {0,};
        char clientRole[32] = {0,};
        
        char *authPtr = strstr(localCtx->buffer, "AUTH:");
        if (authPtr != NULL) {
            sscanf(authPtr, "AUTH:%127s", submittedCipher);
            for(int i = 0; submittedCipher[i]; i++) {
                submittedCipher[i] = toupper((unsigned char)submittedCipher[i]);
            }
            manageSecurityCode(globalConfig, 2, submittedCipher, decryptedPlain);
        }
        
        char *rolePtr = strstr(localCtx->buffer, "ROLE:");
        if (rolePtr != NULL) {
            sscanf(rolePtr, "ROLE:%31s", clientRole);
            for(int i = 0; clientRole[i]; i++) {
                clientRole[i] = toupper((unsigned char)clientRole[i]);
            }
        } else {
            strcpy(clientRole, "USER");
        }

        time_t now = time(NULL);
        struct tm startTmp = globalConfig->validStart;
        struct tm endTmp = globalConfig->validEnd;
        time_t startTime = mktime(&startTmp);
        time_t endTime = mktime(&endTmp);
        double diffSec = difftime(endTime, now);
        int remainingDays = (int)(diffSec / 86400.0);

        char expTimeStr[32] = {0,};
        strftime(expTimeStr, sizeof(expTimeStr), "%Y-%m-%d %H:%M:%S", &endTmp);

        int isValid = 1;
        if (strcmp(decryptedPlain, globalConfig->generatedAuthCode) != 0) isValid = 0;
        if (now < startTime || now > endTime) isValid = 0;
        if (strcmp(clientRole, globalConfig->targetRole) != 0) isValid = 0;

        char currentPermit = 'N';
        if (isValid) {
            send(localCtx->clientSock, "YES", 3, 0);
            memset(localCtx->buffer, 0, sizeof(localCtx->buffer));
            int ackBytes = recv(localCtx->clientSock, localCtx->buffer, sizeof(localCtx->buffer) - 1, 0);
            if (ackBytes > 0 && (strstr(localCtx->buffer, "Y") != NULL || strstr(localCtx->buffer, "y") != NULL)) {
                currentPermit = 'Y';
            }
        } else {
            send(localCtx->clientSock, "NO", 2, 0);
        }

        if (globalConfig->dbRowCount < 100) {
            ConnectedClientRow *row = &globalConfig->dbRows[globalConfig->dbRowCount];
            row->permit = currentPermit;
            strcpy(row->authCode, submittedCipher);
            strcpy(row->ip, clientIp);
            row->port = clientPort;
            strcpy(row->expTime, expTimeStr);
            strcpy(row->userRole, clientRole);
            snprintf(row->remainingDays, sizeof(row->remainingDays), "+%02d", isValid ? remainingDays : 0);
            globalConfig->dbRowCount++;
            
            printf("%d\t%c\t\t%s\t\t%s\t%s\t%d\t%s\t%s\n", 
                   globalConfig->dbRowCount, currentPermit, clientRole, 
                   strlen(submittedCipher) > 0 ? submittedCipher : "NONE", 
                   clientIp, clientPort, expTimeStr, row->remainingDays);
        }
        
        writeLogToCsv(currentPermit, clientRole, submittedCipher, clientIp, clientPort, expTimeStr);
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
    f (globalConfig->serverFd != INVALID_SOCKET) close(globalConfig->serverFd);
#endif
    strcpy(globalConfig->serverStatusMsg, "Engine offline. Resources freed.");
}