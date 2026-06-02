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
    globalConfig->is_running = 0;
    globalConfig->dbRowCount = 0;
    memset(globalConfig->serverStatusMessage, 0, sizeof(globalConfig->serverStatusMessage));
    memset(globalConfig->dbRows, 0, sizeof(globalConfig->dbRows));

#if defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        strcpy(globalConfig->serverStatusMessage, "WSAStartup Failure");
        return -2;
    }
#endif

    globalConfig->serverFD = socket(AF_INET, SOCK_STREAM, 0);
    if (globalConfig->serverFD == INVALID_SOCKET) {
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
        strcpy(globalConfig->serverStatusMessage, "Socket Creation Error");
        return -3;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(globalConfig->port);

    if (bind(globalConfig->serverFD, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        shutdownServerEngine(globalConfig);
        strcpy(globalConfig->serverStatusMessage, "Bind Failure (Port in Use)");
        return -4;
    }

    strcpy(globalConfig->serverStatusMessage, "Engine Initialized Successfully");
    return 0;
}

void generateSecureCode(GlobalServerConfig *globalConfig) {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int charset_size = sizeof(charset) - 1;
    srand((unsigned int)time(NULL));
    for (int i = 0; i < globalConfig->codeLength; i++) 
        globalConfig->generatedAuthCode[i] = charset[rand() % charset_size];
    globalConfig->generatedAuthCode[globalConfig->codeLength] = '\0';
}

int startServerListen(GlobalServerConfig *globalConfig) {
    if (globalConfig == NULL) return -1;
    if (listen(globalConfig->serverFD, 3) == SOCKET_ERROR) {
        strcpy(globalConfig->serverStatusMessage, "Listen Failure");
        return -2;
    }
    globalConfig->is_running = 1;
    strcpy(globalConfig->serverStatusMessage, "Server is Live and Listening");
    printf("\n===========================================\n");
    printf("   [통합 관리자 서버 엔진 활성화]   \n");
    printf("   생성된 인증 코드: %s\n", globalConfig->generatedAuthCode);
    printf("   포트 번호 [%d]에서 무한 루프 관제 시작\n", globalConfig->port);
    printf("===========================================\n\n");
    printf("허가여부\t특정코드\t아이피\t\t포트\t유효만료시각\t\t남은일수\n");
    return 0;
}

int processClientEvent(GlobalServerConfig *globalConfig, LocalContext *local_ctx) {
    if (globalConfig == NULL || local_ctx == NULL) return -1;
    local_ctx->addrLen = sizeof(local_ctx->clientAddr);
    local_ctx->clientSock = accept(globalConfig->serverFD, (struct sockaddr*)&local_ctx->clientAddr, &local_ctx->addrLen);
    if (local_ctx->clientSock == INVALID_SOCKET) return -1;

    memset(local_ctx->buffer, 0, sizeof(local_ctx->buffer));
    local_ctx->readBytes = recv(local_ctx->clientSock, local_ctx->buffer, sizeof(local_ctx->buffer) - 1, 0);
    
    int return_signal = 0;
    char client_ip[32] = {0,};
    int client_port = ntohs(local_ctx->clientAddr.sin_port);
    strcpy(client_ip, inet_ntoa(local_ctx->clientAddr.sin_addr));

    if (local_ctx->readBytes > 0) {
        char submitted_code[64] = {0,};
        char *auth_ptr = strstr(local_ctx->buffer, "AUTH:");
        if (auth_ptr != NULL) {
            sscanf(auth_ptr, "AUTH:%63s", submitted_code);
            for(int i = 0; submitted_code[i]; i++) 
                submitted_code[i] = toupper((unsigned char)submitted_code[i]);
        }

        time_t now = time(NULL);
        time_t start_time = mktime(&globalConfig->validStart);
        time_t end_time = mktime(&globalConfig->validEnd);
        double diff_sec = difftime(end_time, now);
        int remainingDays = (int)(diff_sec / 86400.0);

        char expTime_str[32] = {0,};
        strftime(expTime_str, sizeof(expTime_str), "%Y-%m-%d %H:%M:%S", &globalConfig->validEnd);

        int is_valid = 1;
        if (strcmp(submitted_code, globalConfig->generatedAuthCode) != 0) is_valid = 0;
        if (now < start_time || now > end_time) is_valid = 0;

        char current_permit = 'N';
        if (is_valid) {
            send(local_ctx->clientSock, "YES", 3, 0);
            memset(local_ctx->buffer, 0, sizeof(local_ctx->buffer));
            int ack_bytes = recv(local_ctx->clientSock, local_ctx->buffer, sizeof(local_ctx->buffer) - 1, 0);
            if (ack_bytes > 0 && (strstr(local_ctx->buffer, "Y") != NULL || strstr(local_ctx->buffer, "y") != NULL)) 
                current_permit = 'Y';
            else
                return_signal = 1;
        } 
        else
        {
            send(local_ctx->clientSock, "NO", 2, 0);
            return_signal = 1;
        }

        printf("%c\t\t%s\t%s\t%d\t%s\t+%02d\n", 
               current_permit, is_valid ? globalConfig->generatedAuthCode : (strlen(submitted_code) > 0 ? submitted_code : "NONE"), 
               client_ip, client_port, expTime_str, is_valid ? remainingDays : 0);

        if (globalConfig->dbRowCount < 100) {
            ConnectedClientRow *row = &globalConfig->dbRows[globalConfig->dbRowCount];
            row->permit = current_permit;
            strcpy(row->authCode, is_valid ? globalConfig->generatedAuthCode : submitted_code);
            strcpy(row->ipv4, client_ip);
            row->port = client_port;
            strcpy(row->expTime, expTime_str);
            snprintf(row->remainingDays, sizeof(row->remainingDays), "+%02d", is_valid ? remainingDays : 0);
            globalConfig->dbRowCount++;
        }
    }

#if defined(_WIN32) || defined(_WIN64)
    closesocket(local_ctx->clientSock);
#else
    close(local_ctx->clientSock);
#endif
    return return_signal;
}

void shutdownServerEngine(GlobalServerConfig *globalConfig) {
    if (globalConfig == NULL) return;
    globalConfig->is_running = 0;
#if defined(_WIN32) || defined(_WIN64)
    if (globalConfig->serverFD != INVALID_SOCKET) closesocket(globalConfig->serverFD);
    WSACleanup();
#else
    if (globalConfig->serverFD != INVALID_SOCKET) close(globalConfig->serverFD);
#endif
    strcpy(globalConfig->serverStatusMessage, "Engine offline. Resources freed.");
}

int getConnectedTableJson(GlobalServerConfig *globalConfig, char *out_json, int max_len) {
    if (globalConfig == NULL || out_json == NULL) return -1;
    int offset = snprintf(out_json, max_len, "{\"total_connections\": %d, \"rows\": [", globalConfig->dbRowCount);
    
    for (int i = 0; i < globalConfig->dbRowCount; i++) {
        ConnectedClientRow *row = &globalConfig->dbRows[i];
        int written = snprintf(out_json + offset, max_len - offset,
            "{\"permit\":\"%c\",\"auth_code\":\"%s\",\"ip\":\"%s\",\"port\":%d,\"expTime\":\"%s\",\"remaining\":\"%s\"}%s",
            row->permit, row->authCode, row->ipv4, row->port, row->expTime, row->remainingDays,
            (i == globalConfig->dbRowCount - 1) ? "" : ",");
        if (written < 0 || offset + written >= max_len) return -2;
        offset += written;
    }
    
    snprintf(out_json + offset, max_len - offset, "]}");
    return 0;
}
