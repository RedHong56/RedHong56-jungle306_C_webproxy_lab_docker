#include "csapp.h"
#include <netdb.h>

int main(int argc, char **argv)
{
    struct addrinfo *p, *listp, hints;   // addrinfo: 주소 정보 구조체 (p는 순회용, listp는 리스트 시작)
    char buf[MAXLINE];                   // IP 문자열 저장 버퍼
    int rc, flags;

    // 인자 검사: 도메인 이름을 1개 받아야 함 (예: ./hostinfo google.com)
    if (argc != 2) {
        fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
        exit(0);
    }

    // --- getaddrinfo 호출 준비 ---
    memset(&hints, 0, sizeof(struct addrinfo)); // hints 구조체 초기화 (모든 필드를 0으로)
    hints.ai_family = AF_INET;                  // IPv4 주소만 반환
    hints.ai_socktype = SOCK_STREAM;            // TCP 소켓만 반환 (연결 지향)

    // host = argv[1], service = NULL → 도메인 이름만 IP로 변환
    // &listp = 반환되는 addrinfo 리스트의 시작 주소
    rc = getaddrinfo(argv[1], NULL, &hints, &listp);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc)); // 에러 메시지 출력
        exit(1);
    }

    // --- addrinfo 리스트 순회하며 IP 출력 ---
    flags = NI_NUMERICHOST; // DNS 이름 대신 숫자 IP 출력 옵션

    // 리스트를 따라가며(p = p->ai_next) 각 주소를 출력
    for (p = listp; p; p = p->ai_next) {
        // 소켓 주소 → 문자열 형태로 변환 (예: "199.16.156.102")
        getnameinfo(p->ai_addr, p->ai_addrlen,
                    buf, MAXLINE,      // host 출력 버퍼
                    NULL, 0,           // service 문자열은 필요 없음
                    flags);

        printf("%s\n", buf); // IP 주소 출력
    }

    // --- 메모리 해제 ---
    freeaddrinfo(listp); // getaddrinfo가 malloc한 메모리 반환

    exit(0);
}
