#include "sm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    GlobalServerConfig gSrverGlobal;
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
            if (isEngineInitialized) 
                shutdownServerEngine(&gSrverGlobal);
            printf("관리 서버 제어 프로그램을 안전하게 종료합니다.\n");
            break;
        }

        if (adminMenuSelection == 1) {
            if (!isEngineInitialized) {
                printf("\n[설정] 생성할 특정 문자 코드의 길이를 입력하세요: ");
                if (scanf("%d", &gSrverGlobal.codeLength) != 1 || gSrverGlobal.codeLength > 63) 
                    gSrverGlobal.codeLength = 7;

                memset(&gSrverGlobal.validStart, 0, sizeof(struct tm));
                memset(&gSrverGlobal.validEnd, 0, sizeof(struct tm));

                printf("[설정] 시작 기간 입력 (YYYY MM DD HH MM): ");
                scanf("%d %d %d %d %d", 
                      &gSrverGlobal.validStart.tm_year, &gSrverGlobal.validStart.tm_mon, &gSrverGlobal.validStart.tm_mday,
                      &gSrverGlobal.validStart.tm_hour, &gSrverGlobal.validStart.tm_min);
                gSrverGlobal.validStart.tm_year -= 1900;
                gSrverGlobal.validStart.tm_mon -= 1;

                printf("[설정] 만료 기간 입력 (YYYY MM DD HH MM): ");
                scanf("%d %d %d %d %d", 
                      &gSrverGlobal.validEnd.tm_year, &gSrverGlobal.validEnd.tm_mon, &gSrverGlobal.validEnd.tm_mday,
                      &gSrverGlobal.validEnd.tm_hour, &gSrverGlobal.validEnd.tm_min);
                gSrverGlobal.validEnd.tm_year -= 1900;
                gSrverGlobal.validEnd.tm_mon -= 1;

                generateSecureCode(&gSrverGlobal);

                int init_res = initServerEngine(&gSrverGlobal, targetPort);
                if (init_res != 0) {
                    printf("오류: %s (코드: %d)\n", gSrverGlobal.serverStatusMessage, init_res);
                    continue;
                }
                isEngineInitialized = 1;
            }

            int listen_res = startServerListen(&gSrverGlobal);
            if (listen_res != 0) {
                printf("오류: %s (코드: %d)\n", gSrverGlobal.serverStatusMessage, listen_res);
                shutdownServerEngine(&gSrverGlobal);
                isEngineInitialized = 0;
                continue;
            }

            while (gSrverGlobal.is_running) {
                LocalContext current_event_ctx;
                int event_result = processClientEvent(&gSrverGlobal, &current_event_ctx);
                if (event_result == 1) {
                    printf("\n[안내] 검증 거부 이벤트가 감지되어 메인 제어반 인터페이스로 복귀합니다.\n");
                    gSrverGlobal.is_running = 0;
                }
            }
        }
    }
    return 0;
}
