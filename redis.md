Isolation-Redis 서버
-

ID: 유저가 설정하고 온라인 공간에서 식별할 수 있는 값
client token: 서버와 클라이언트, Redis가 내부적으로 사용하는 64bit 중복사용 불가값

Redis 사용 목록
-
중괄호 안 내용은 string 타입(서식 또는 용도를 의미)
* Account Register
    |key|field|value|
    |-|-|-|
    |id:{id}|password|{sha256 hash}|
    ||nickname|{다른 player에게 노출되는 식별자}||
    ||personalColor|{hhh/sss/vvv}|

* Connected Client
    |key|field|value|
    |-|-|-|
    |client:{token}|connectedTime|{time}|
    ||loginId|{id}|
    1. client가 연결되는 즉시 생성
    2. loginId 필드는 로그인 성공시 생성, 로그아웃하면 제거
    3. 로그인 성공 후 player data를 전달하기 위한 인증용
    
* Logined Account
    |key|field|value|
    |-|-|-|
    |logined:{id}|token|{token}|
    ||loginTime|{time}|
    1. 로그인 성공하면 생성
    2. 중복 로그인 방지책