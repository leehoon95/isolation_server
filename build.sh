cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DSERVER_NAME_VAR=${1:-isolation-server} \
    -DREDIS_SERVER_HOST_VAR=${2:-redis} \
    -DREDIS_SERVER_PORT_VAR=${3:-6379} \
    && cmake --build build -j$(nproc) 
