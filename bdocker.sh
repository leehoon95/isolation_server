# 필요시 argument 값을 수정하라
docker build -t isolation-server -f Dockerfile.server . --progress=plain \
    --build-arg CACHEBUST=$(date +%s) \
    --build-arg SERVER_NAME=isolation-server \
    --build-arg REDIS_SERVER_HOST=redis \
    --build-arg REDIS_SERVER_PORT=6379