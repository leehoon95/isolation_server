# 필요시 argument 값을 수정하라
docker build -t leehoon95/isolation-server -f Dockerfile.server . --progress=plain \
    --build-arg CACHEBUST=$(date +%s)