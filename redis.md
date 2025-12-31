Isolation-Redis 서버
-

ID: 유저가 설정하고 온라인 공간에서 식별할 수 있는 값
client token: 서버와 클라이언트, Redis가 내부적으로 사용하는 64bit 중복사용 불가값

Redis key-value 목록
-
중괄호 안 내용은 string 타입(서식 또는 용도를 의미)
* Account Register(Hash)
    |key|field|value|
    |-|-|-|
    |id:{id}|password|{sha256 hash}|
    ||nickname|{다른 player에게 노출되는 식별자}||
    ||personalColor|{hhh/sss/vvv}|

* Authorized token(Hash)
    |key|field|value|
    |-|-|-|
    |authorized:{token}|id|{id}|
    ||loginTime||
    
    1. token은 client가 접속할 때 마다 부여되고 게임 데이터에 접속하기 위한 수단으로 사용됨
    2. token 타입은 uint64