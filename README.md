# ServerManager

## 01 CMakelists.txt 빌드 명령어
```build_cmd
mkdir build
cd build
cmake ..
cmake --build .
```

<img width="33" height="31" alt="image" src="https://github.com/user-attachments/assets/7d9bf985-8bbe-413d-9ee3-3f093533cf90" />
이미지 파일은 복사하는 기능을 일부분 캡쳐한 것이며 위의 코드블록 내에서 명령어들을 전부 눌러서 실행하면 다음과 같이 다른 터미널에 붙여넣을 수 있으니 참고하여 접근하세요.




## 02 헤더파일과 소스파일에 대한 주요 기능들

### 01 sm.h
 * 역할: 헤더 파일로 구조체와 함수 선언 제공.
 * 주요 구조체
         - ConnectedClientRow: 클라이언트 접속 정보(허가여부, 코드, IP, 포트, 만료기간, 권한 등).
         - GlobalServerConfig: 서버 전체 설정(포트, 상태, 인증코드, 기간, DB 배열 등).
         - LocalContext: 클라이언트 연결 컨텍스트(소켓, 주소, 버퍼).
 * 주요 함수 선언
         - 서버 엔진 초기화/리스닝/종료
         - 인증코드 관리
         - 날짜 파싱
         - 로그 기록
         - 리스트 출력/CSV 저장
         - 승격/강등 기능


### 02 sm.c
 * 역할: 서버 엔진의 실제 동작 로직 구현
 * 주요 기능
         - initServerEngine: 서버 소켓 생성 및 바인딩, 클라이언트 배열 초기화.
         - parseFlexibleDateTime: 날짜/시간 문자열을 struct tm으로 변환.
         - manageSecurityCode: 인증코드 생성 및 암호화/복호화 처리.
         - writeLogToCsv: 접속 로그를 CSV 파일에 기록.
         - startServerListen: 서버 소켓 리스닝 시작, 상태 메시지 출력.
         - processClientEvent: 클라이언트 연결 처리, 인증 검증, 권한 확인, DB에 기록.
         - shutdownServerEngine: 서버 종료 및 리소스 해제.
         - printServerList, exportServerListToCsv: 서버 리스트 출력 및 CSV 저장.
         - promoteTemporaryUser, relegateUser: 사용자 승격/강등 처리.
 * 특징
         - 클라이언트 인증은 AUTH:와 ROLE: 메시지를 기반으로 수행.
         - 인증코드 검증, 기간 검증, 권한 검증을 통해 접속 허가 여부 결정.
         - DB 배열은 동적으로 확장(realloc)하여 관리.


### 03 main.c
 * 역할: 프로그램의 진입점. 서버 엔진 초기화, 설정 입력, 명령어 처리 루프를 담당
 * inputServerSettings: 서버 코드 길이, 시작/만료 기간, 권한 등급을 입력받아 설정.
 * main:
         - GlobalServerConfig 구조체 초기화 및 동적 메모리 할당.
         - 명령어 루프(1, 2, sac, sl, ar, rs, sh?, p, r) 처리.
         - 서버 시작, 종료, 인증코드 생성, 리스트 출력, 초기화, 승격/강등 기능 수행.



[핵심 정리]
 * sm.h: 구조체와 함수 선언 → 모듈 간 연결
 * sm.c: 실제 서버 동작 로직 → 소켓, 인증, 로그, DB 관리.
 * main.c: 사용자 입력과 명령어 루프 → 서버 제어 인터페이스.
 * 구조
   main.c(제어/UI) ↔ sm.c(엔진 로직) ↔ sm.h(인터페이스)로 깔끔하게 분리
   현재 코드의 주요 개선 포인트는 문자열 처리 시 strcpy 사용, 동적 메모리 관리 일관성 유지, 권한 입력 대소문자 무시 처리
