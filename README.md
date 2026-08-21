# Isolation 인증 서버
Isolation 게임 로그인 서버

## 주요 특징
1. **Non-Blocking Async I/O**  
    * Boost.Asio 기반 비동기 TCP 통신 처리
2. **인메모리 데이터 관리**
    * redis-server 연동
3. **자동화된 개발/배포(CI/CD) 환경**  
    * Github Actions
    * Docker Compose 멀티 컨테이너
    * Pulumi(C#) AWS 인프라 배포
4. **코드 & workflow 테스트 환경**

### 의존 패키지  
* libhiredis1.1.0 (빌드&런타임 의존)
* libboost-all-dev
* libgtest-dev
* libgmock-dev

### 의존 라이브러리  
* [Redis++](https://github.com/sewenew/redis-plus-plus?tab=readme-ov-file)  
    redis-server 통신
* [Protocol Buffers](https://github.com/protocolbuffers/protobuf)(31.0.0)  
    데이터 직렬화
* [Boost.asio](https://www.boost.org/library/latest/asio/)  
    비동기 네트워크 처리 구현
* [sha256](https://www.zedwood.com/article/cpp-sha256-function)  
    비밀번호 암호화용 hash 함수

## 설치 방법
우분투 환경을 지원 한다
```
git clone https://github.com/leehoon95/isolation_server.git
```

## Redis++ 라이브러리 설치
1. redis-plus-plus git 프로젝트를 clone
2. 프로젝트 최상위 경로에서 아래 명령어 실행
```
cmake -B build \
        -DREDIS_PLUS_PLUS_BUILD_TEST=OFF \
        -DREDIS_PLUS_PLUS_BUILD_SHARED=OFF \
        -DREDIS_PLUS_PLUS_BUILD_STATIC=ON \
    && cmake --build build -j$(nproc) \
    && sudo cmake --install build
```
⚠️Redis++는 정적 링크, hiredis는 libhiredis1.1.0 패키지를 동적 링크한다

## Protocol Buffers 라이브러리 설치
1. 31.0 버전 프로젝트를 다운로드
2. 프로젝트 최상위 경로에서 아래 명령어 실행
```
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -Dprotobuf_BUILD_TESTS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    && cmake --build build -j$(nproc) \
    && sudo cmake --install build
```

## 배포
GitHub Actions 기반으로 main 브랜치에 커밋이 push되면 AWS 리소스와 Docker 컨테이너를 배포한다  
로컬에서 테스트 가능하며 .secrets 파일로 GitHub Secrets 기능을 대신한다

⚠️workflow 실행을 위해 nektos/act와 Docker 설치 필요
```
sh rundeploy.sh
```
.secrets 파일은 프로젝트 최상위 디렉토리에 놓아야 한다  
필수적인 key-value는 아래와 같다

```
# .secrets 파일 내용

DOCKERHUB_USERNAME=leehoon95

DOCKERHUB_TOKEN=***

PULUMI_ACCESS_TOKEN=***

# client 접속을 Listening하는 포트
SERVER_LISTENING_PORT=***

AWS_EC2_USERNAME="ubuntu"

# ssh, scp 커맨드 실행을 위해 필요
AWS_EC2_PRIVATE_KEY=***
```
로컬 테스트에 필요한 추가적 key-value는 아래와 같다
```
# actions/checkout 액션에서 사용한다 (GitHub PAT)
GITHUB_TOKEN=*** 

# OIDC를 사용할 수 없기 때문에 IAM 사용자 로그인
AWS_ACCESS_KEY_ID=***
AWS_SECRET_ACCESS_KEY=***
```

## Pulumi Config

```
environment: null
config:
  aws-isolation-server:aws-ec2-type: t3.small       # EC2 type
  aws-isolation-server:region: ap-northeast-2       # Region (서울)
  aws-isolation-server:aws-eip-id: ***              # Elastic IP 주소
  aws-isolation-server:aws-ssh-public:              # ssh 접속용
    secure: ***
  aws-isolation-server:aws-ssh-private:             # EC2 인스턴스 초기화 확인용 (Pulumi 코드에서 인스턴스으로 접속)
    secure: ***
```


## Redis server 데이터 구조

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