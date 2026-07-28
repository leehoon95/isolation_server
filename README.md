# Isolation 인증 서버
비동기 Isolation 게임 로그인 서버

### 주요 특징
1. **Non-Blocking Async I/O**  
    * Boost.Asio 기반 비동기 TCP 통신 처리
2. **인메모리 데이터 관리**
    * redis-server 연동
3. **자동화된 개발/배포 환경**  
    * Github Actions CI/CD 구현
    * Docker Compose 멀티 컨테이너 환경 구축

### CI/CD 관련 파일
1. [Dockerfile.server](./Dockerfile.server)
2. [docker-compose.yml](./docker-compose.yml)
3. [deploy.yml](./.github/workflows/deploy.yml)


### 소스코드 빌드
build.sh을 실행한다.
```
sh build.sh
```
⚠️ Dockerfile.server에서 참조하는 스크립트이므로 수정할 때 주의

### 서버 이미지 빌드
```
sh makeimg.sh
```

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

### Github Actions Workflow test
```
# workflow 테스트 명령어 예시
sudo act -W .github/workflows/deploy.yml --secret-file .secrets
```

```
# .secrets 파일 작성 예시
DOCKERHUB_USERNAME="leehoon95"
DOCKERHUB_TOKEN="***"
INSTANCE_HOST="***"
INSTANCE_USER="***"
INSTANCE_SSH_PRIVATE_KEY="-----BEGIN RSA PRIVATE KEY-----
***
-----END RSA PRIVATE KEY-----"
```

```
#.actrc 내용
-P self-hosted=catthehacker/ubuntu:act-latest
-P ubuntu-latest=catthehacker/ubuntu:act-latest
```
원격 서버 운영체제에 따라 수정
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