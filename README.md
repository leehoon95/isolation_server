# Isolation Monolithic 서버

### 의존 패키지  
* Redis-Server(7.0.15)  

### 의존 라이브러리
* [Redis++](https://github.com/sewenew/redis-plus-plus?tab=readme-ov-file)  
    서버가 redis-server와 통신하기 위함
* [Hiredis](https://github.com/redis/hiredis)(1.3.0)  
    redis++에서 의존
* [Protocol Buffers](https://github.com/protocolbuffers/protobuf)(31.0.0)  
    플랫폼과 언어 중립적 정의 언어 사용, 데이터 직렬화 지원
* [Boost.asio](https://www.boost.org/library/latest/asio/)  
    apt libboost-all-dev 설치
   
### 서버 구성 요소와 역할
---
* 로그인 서버  
    클라이언트 로그인 기능 제공
* Redis-Server  
    클라이언트 계정 및 데이터 관리

### Redis server key, value 설명
---

* Account Register
    |key|field|desc|
    |-|-|-|
    |id:{id}|password|sha256 hash|
    ||nickname|닉네임|
    ||personalColor|{h/s/v}|

* Connected Client
    |key|field|desc|
    |-|-|-|
    |client:{token}|connectedTime|연결된 시간|
    ||loginId|로그인 id|
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