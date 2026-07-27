# Isolation 인증 서버
로그인 서버와 DB를 같은 인스턴스에 두고 있는 모놀리식(Monolithic) 서버

### 소스코드 빌드
build.sh을 실행한다.
```
sh build.sh
```
⚠️ Dockerfile.server에서 사용하는 스크립트이므로 수정할 때 주의하라

### Docker 빌드
bdocker.sh을 실행한다.
```
sh bdocker.sh
```
⚠️ Docker Compose가 이미지가 동일하다고 판단하는 이슈가 있음

### Docker Compose 실행
```
docker compose up --build
```
⚠️ Docker 빌드 이슈를 해결하기 위해 --build 옵션을 사용하라

### Docker Compose Container 접속
```
docker compose exec redis sh
docker compose exec server-app bash
```

### Github Actions test
```
# workflow 테스트 명령어 예시
sudo act -W .github/workflows/test-git.yml
```
.actrc 내용
```
-P self-hosted=catthehacker/ubuntu:act-latest
```

### 의존 패키지  
* Redis-Server(7.0.15)  
* libboost-all-dev

### 의존 라이브러리  

* [Redis++](https://github.com/sewenew/redis-plus-plus?tab=readme-ov-file)  
    redis-server 통신
* [Hiredis](https://github.com/redis/hiredis)(1.3.0)  
    redis++ 라이브러리가 의존함
* [Protocol Buffers](https://github.com/protocolbuffers/protobuf)(31.0.0)  
    데이터 직렬화 지원
* [Boost.asio](https://www.boost.org/library/latest/asio/)  
    비동기 네트워크 처리 구현
* [sha256](https://www.zedwood.com/article/cpp-sha256-function)  
    비밀번호 암호화용 hash 함수

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