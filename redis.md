Isolation-Redis 서버
-

nickname: 유저가 설정하고 온라인 공간에서 식별할 수 있는 값
client token: 서버와 클라이언트, Redis가 내부적으로 사용하는 64bit 중복사용 불가값

Redis key-value 목록
-
중괄호 안 내용은 고정되지 않음
* user 로그인시 (String)  
    |key|value|desc|
    |-|-|-|
    |user:{client token(uint64)}|{nickname(string)}|client token으로 구분

* session 생성시 (Hash)  
    |key|field|value|desc|
    |-|-|-|-|
    |session:{session token(uin64)}|hostToken|{uint64}|hostToken은 이 session을 생성요청한 host의 client token
    ||name|{string}|게임에서 session list에 노출되는 이름
    ||maxClientCount|{uint}|접속가능한 client 수
    ||password|{string}|비밀번호
    ||joinCode|{string}|Unity Relay에 사용되는 코드
    
* session 클라이언트 참가 client
    |key|field|value|desc|
    |-|-|-|-|
    |session:{session token}:clients|{client token(uint64)}|{nickname(string)}|session에 참가한 client