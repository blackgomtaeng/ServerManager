#include "sm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>
#endif

// 공통 설정 입력 루틴
void inputServerSettings(GlobalServerConfig *gServer) {
    printf("\n[설정] 생성할 특정 문자 코드의 길이를 입력하세요: ");
    if (scanf("%d", &gServer->codeLength) != 1 || gServer->codeLength > 63) gServer->codeLength = 7;
    while (getchar() != '\n');

    char rawStart[128] = {0}, rawEnd[128] = {0}, rawRole[128] = {0};
    printf("[설정] 시작 기간 입력 (예: 202606040000): ");
    fgets(rawStart, sizeof(rawStart), stdin);
    rawStart[strcspn(rawStart, "\r\n")] = 0;
    parseFlexibleDateTime(rawStart, &gServer->validStart);

    printf("[설정] 만료 기간 입력 (예: 202606041700): ");
    fgets(rawEnd, sizeof(rawEnd), stdin);
    rawEnd[strcspn(rawEnd, "\r\n")] = 0;
    parseFlexibleDateTime(rawEnd, &gServer->validEnd);

    printf("[설정] 지정 권한 등급 입력 (Enter=USER / op / admin / m / tu): ");
    fgets(rawRole, sizeof(rawRole), stdin);
    rawRole[strcspn(rawRole, "\r\n")] = 0;
    for (int i = 0; rawRole[i]; i++) rawRole[i] = tolower((unsigned char)rawRole[i]);

    if (strlen(rawRole) == 0) strcpy(gServer->targetRole, "USER");
    else if (strcmp(rawRole, "op") == 0) strcpy(gServer->targetRole, "OPERATOR");
    else if (strcmp(rawRole, "admin") == 0) strcpy(gServer->targetRole, "ADMIN");
    else if (strcmp(rawRole, "m") == 0) strcpy(gServer->targetRole, "MANAGER");
    else if (strcmp(rawRole, "tu") == 0) strcpy(gServer->targetRole, "TU");
    else strcpy(gServer->targetRole, "USER");

    manageSecurityCode(gServer, 1, NULL, NULL);
}

int main() {
    GlobalServerConfig gServer;
    int targetPort = 9999, isEngineInitialized = 0;
    memset(&gServer, 0, sizeof(gServer));

    gServer.serverStatusMsg = malloc(256);
    gServer.targetRole = malloc(32);
    gServer.generatedAuthCode = NULL;
    gServer.encryptedAuthCode = NULL;
    gServer.dbRowCount = 0;
    gServer.dbRows = NULL;
    gServer.dbCapacity = 0;

    while (1) {
        printf("\n===========================================\n");
        printf("   [통합 관리자 제어 프로그램 인터페이스]   \n");
        printf("===========================================\n");
        printf(" 1. 사전 모니터링 서버 시스템 전체 구동\n");
        printf(" 2. 시스템 관리 제어반 종료\n");
        printf(" ※ sh? : 서버 명령어 확인\n");
        printf("-------------------------------------------\n");
        printf("명령 번호 및 명령어 입력: ");

        char command[32];
        if (scanf("%31s", command) != 1) { while (getchar() != '\n'); continue; }

        if (strcmp(command, "2") == 0) {
            if (isEngineInitialized) shutdownServerEngine(&gServer);
            printf("관리 서버 제어 프로그램을 안전하게 종료합니다.\n");
            break;
        }

        if (strcmp(command, "1") == 0) {
            if (!isEngineInitialized) {
                inputServerSettings(&gServer);
                int initRes = initServerEngine(&gServer, targetPort);
                if (initRes != 0) { printf("오류: %s (코드: %d)\n", gServer.serverStatusMsg, initRes); continue; }
                isEngineInitialized = 1;
            }
            int listenRes = startServerListen(&gServer);
            if (listenRes != 0) { printf("오류: %s (코드: %d)\n", gServer.serverStatusMsg, listenRes); shutdownServerEngine(&gServer); isEngineInitialized = 0; continue; }
            while (gServer.isRunning) {
                LocalContext *currentEventCtx = malloc(sizeof(LocalContext) + 65536 + 2048);
                if (!currentEventCtx) { printf("메모리 할당 실패\n"); break; }
                processClientEvent(&gServer, currentEventCtx);
                free(currentEventCtx);
            }
        }

        if (strcmp(command, "sac") == 0) {
            inputServerSettings(&gServer);
            printf("\n========================================================================================\n");
            printf("   [통합 멀티 관제 서버 엔진 가동]\n");
            printf("   지정 등급 권한        : %s\n", gServer.targetRole);
            printf("   원본 인증 코드 (Server): %s\n", gServer.generatedAuthCode);
            printf("   암호화된 코드 (Client) : %s\n", gServer.encryptedAuthCode);
            printf("========================================================================================\n");
#if defined(_WIN32) || defined(_WIN64)
            Sleep(10000);
#else
            sleep(10);
#endif
        }

        if (strcmp(command, "sl") == 0) { printServerList(&gServer); exportServerListToCsv(&gServer); }
        if (strcmp(command, "ar") == 0) { gServer.dbRowCount = 0; memset(gServer.dbRows, 0, sizeof(ConnectedClientRow) * 100); printf("\n[전체 초기화 완료]\n"); }
        if (strcmp(command, "rs") == 0) { if (gServer.generatedAuthCode) free(gServer.generatedAuthCode); if (gServer.encryptedAuthCode) free(gServer.encryptedAuthCode); gServer.generatedAuthCode = NULL; gServer.encryptedAuthCode = NULL; printf("\n[특정 부분 초기화 완료 - 인증코드]\n"); }

        if (strcmp(command, "sh?") == 0) {
            printf("\n===========================================\n");
            printf("   [명령어 목록]\n");
            printf(" sac : 서버 인증코드 생성\n");
            printf(" sl  : 서버 리스트 출력\n");
            printf(" ar  : 전체 초기화\n");
            printf(" rs  : 특정 부분 초기화\n");
            printf(" p/pro/promotion : 승격\n");
            printf(" r/rel/relegation: 강등\n");
            printf("-------------------------------------------\n");
#if defined(_WIN32) || defined(_WIN64)
            Sleep(5000);
#else
            sleep(5);
#endif
        }

        if (strncmp(command, "p", 1) == 0) {
            int rowIndex, extendDays, port; char role[32], ip[32];
            printf("승격 대상 입력 (번호 권한 IP 포트): ");
            scanf("%d %31s %31s %d", &rowIndex, role, ip, &port);
            printf("연장 일 수 입력 (+일수): ");
            scanf("%d", &extendDays);
            promoteTemporaryUser(&gServer, rowIndex-1, extendDays);
        }

        if (strncmp(command, "r", 1) == 0) {
            int rowIndex, reduceDays, port; char role[32], ip[32];
            printf("강등 대상 입력 (번호 권한 IP 포트): ");
            scanf("%d %31s %31s %d", &rowIndex, role, ip, &port);
            printf("감축 일 수 입력 (-일수): ");
            scanf("%d", &reduceDays);
            relegateUser(&gServer, rowIndex-1, reduceDays);
        }
    }

    free(gServer.serverStatusMsg);
    free(gServer.targetRole);
    if (gServer.generatedAuthCode) free(gServer.generatedAuthCode);
    if (gServer.encryptedAuthCode) free(gServer.encryptedAuthCode);
    /* dbRows freed by shutdownServerEngine if allocated */
    return 0;
}
