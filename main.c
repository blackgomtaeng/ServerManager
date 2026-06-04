#include "sm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    GlobalServerConfig myServerGlobal;
    int targetPort = 9999;
    int adminMenuSelection = 0;
    int isEngineInitialized = 0;

    while (1) {
        printf("\n===========================================\n");
        printf("   [통합 관리자 제어 프로그램 인터페이스]   \n");
        printf("===========================================\n");
        printf(" 1. 사전 모니터링 서버 시스템 전체 구동\n");
        printf(" 2. 시스템 관리 제어반 종료\n");
        printf("-------------------------------------------\n");
        printf("명령 번호 선택: ");
        
        if (scanf("%d", &adminMenuSelection) != 1) {
            while (getchar() != '\n');
            continue;
        }

        if (adminMenuSelection == 2) {
            if (isEngineInitialized) {
                shutdownServerEngine(&myServerGlobal);
            }
            printf("관리 서버 제어 프로그램을 안전하게 종료합니다.\n");
            break;
        }

        if (adminMenuSelection == 1) {
            if (!isEngineInitialized) {
                printf("\n[설정] 생성할 특정 문자 코드의 길이를 입력하세요: ");
                if (scanf("%d", &myServerGlobal.codeLength) != 1 || myServerGlobal.codeLength > 63) {
                    myServerGlobal.codeLength = 7;
                }

                char rawStart[128] = {0,};
                char rawEnd[128] = {0,};
                while (getchar() != '\n');

                printf("[설정] 시작 기간 입력 (자유 포맷 예: 2026.06.01 0000): ");
                fgets(rawStart, sizeof(rawStart), stdin);
                rawStart[strcspn(rawStart, "\r\n")] = 0;
                parseFlexibleDateTime(rawStart, &myServerGlobal.validStart);

                printf("[설정] 만료 기간 입력 (자유 포맷 예: 2026-06-15 18:00): ");
                fgets(rawEnd, sizeof(rawEnd), stdin);
                rawEnd[strcspn(rawEnd, "\r\n")] = 0;
                parseFlexibleDateTime(rawEnd, &myServerGlobal.validEnd);

                char rawRole[128] = {0,};
                printf("[설정] 지정 권한 등급 입력 (일반유저는 Enter / 운영자 op / 관리자 admin / 매니저 m): ");
                fgets(rawRole, sizeof(rawRole), stdin);
                rawRole[strcspn(rawRole, "\r\n")] = 0;

                for (int i = 0; rawRole[i]; i++) {
                    rawRole[i] = tolower((unsigned char)rawRole[i]);
                }

                if (strlen(rawRole) == 0) {
                    strcpy(myServerGlobal.targetRole, "USER");
                } else if (strcmp(rawRole, "operator") == 0 || strcmp(rawRole, "op") == 0) {
                    strcpy(myServerGlobal.targetRole, "OPERATOR");
                } else if (strcmp(rawRole, "administrator") == 0 || strcmp(rawRole, "admin") == 0 || strcmp(rawRole, "adm") == 0) {
                    strcpy(myServerGlobal.targetRole, "ADMIN");
                } else if (strcmp(rawRole, "manager") == 0 || strcmp(rawRole, "m") == 0) {
                    strcpy(myServerGlobal.targetRole, "MANAGER");
                } else {
                    strcpy(myServerGlobal.targetRole, "USER");
                }

                manageSecurityCode(&myServerGlobal, 1, NULL, NULL);

                int initRes = initServerEngine(&myServerGlobal, targetPort);
                if (initRes != 0) {
                    printf("오류: %s (코드: %d)\n", myServerGlobal.serverStatusMsg, initRes);
                    continue;
                }
                isEngineInitialized = 1;
            }

            int listenRes = startServerListen(&myServerGlobal);
            if (listenRes != 0) {
                printf("오류: %s (코드: %d)\n", myServerGlobal.serverStatusMsg, listenRes);
                shutdownServerEngine(&myServerGlobal);
                isEngineInitialized = 0;
                continue;
            }

            while (myServerGlobal.isRunning) {
                LocalContext currentEventCtx;
                processClientEvent(&myServerGlobal, &currentEventCtx);
            }
        }
    }
    return 0;
}