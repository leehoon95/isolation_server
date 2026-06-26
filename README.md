# Isolation 인증 서버
로그인 서버와 DB를 같은 인스턴스에 두고 있는 모놀리식(Monolithic) 서버
### 의존 패키지  
* Redis-Server(7.0.15)  

### 의존 라이브러리  

* [Redis++](https://github.com/sewenew/redis-plus-plus?tab=readme-ov-file)  
    서버가 redis-server와 통신하기 위함
* [Hiredis](https://github.com/redis/hiredis)(1.3.0)  
    redis++에서 의존
* [Protocol Buffers](https://github.com/protocolbuffers/protobuf)(31.0.0)  
    데이터 직렬화 지원.
* [Boost.asio](https://www.boost.org/library/latest/asio/)  
    비동기 네트워크 처리 구현. apt install libboost-all-dev 명령으로 설치
* [sha256](https://www.zedwood.com/article/cpp-sha256-function)  
    비밀번호 암호화용 hash 함수

### 사전 준비
1. [isolation_pb](https://github.com/leehoon95/isolation_pb) repository를 최상위 경로에서 clone
2. isolation_pb 안에서 Windows에서는 cs.bat, 우분투에서는 cpp.sh을 실행해서 소스파일을 생성한다
   
### 빌드 방법
1. mkdir build
2. cmake ..
3. make

### Redis server 데이터 구조

* Connected Client
    |key|field|desc|
    |-|-|-|
    |client:{token}|connectedTime|연결된 시간|
    ||loginId|로그인 id|
    1. client 연결시 생성
    2. loginId 필드는 로그인 성공시 설정됨

* Registered Account
    |key|field|desc|
    |-|-|-|
    |id:{id}|password|sha256 hash|
    ||nickname|닉네임|
    ||personalColor|hsv|
    1. 계정생성 완료시 생성
    
* Logined Account
    |key|field|desc|
    |-|-|-|
    |logined:{id}|token|실제 client 식별자|
    ||loginTime|로그인 성공 시간|
    1. 로그인 성공시 생성
    2. 중복 로그인 방지책