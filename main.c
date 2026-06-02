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

                memset(&myServerGlobal.validStart, 0, sizeof(struct tm));
                memset(&myServerGlobal.validEnd, 0, sizeof(struct tm));

                printf("[설정] 시작 기간 입력 (YYYY MM DD HH MM): ");
                scanf("%d %d %d %d %d", 
                      &myServerGlobal.validStart.tm_year, &myServerGlobal.validStart.tm_mon, &myServerGlobal.validStart.tm_mday,
                      &myServerGlobal.validStart.tm_hour, &myServerGlobal.validStart.tm_min);
                myServerGlobal.validStart.tm_year -= 1900;
                myServerGlobal.validStart.tm_mon -= 1;

                printf("[설정] 만료 기간 입력 (YYYY MM DD HH MM): ");
                scanf("%d %d %d %d %d", 
                      &myServerGlobal.validEnd.tm_year, &myServerGlobal.validEnd.tm_mon, &myServerGlobal.validEnd.tm_mday,
                      &myServerGlobal.validEnd.tm_hour, &myServerGlobal.validEnd.tm_min);
                myServerGlobal.validEnd.tm_year -= 1900;
                myServerGlobal.validEnd.tm_mon -= 1;

                generateSecureCode(myServerGlobal.generatedAuthCode, myServerGlobal.codeLength);

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
                int eventResult = processClientEvent(&myServerGlobal, &currentEventCtx);
                if (eventResult == 1) {
                    printf("\n[안내] 검증 거부 또는 원격 중지 이벤트가 감지되어 메인 제어반 인터페이스로 복귀합니다.\n");
                    myServerGlobal.isRunning = 0;
                }
            }
        }
    }
    return 0;
}
