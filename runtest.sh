cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DSERVER_NAME_VAR=${1:-isolation-server} \
    -DREDIS_SERVER_HOST_VAR=${2:-redis} \
    -DREDIS_SERVER_PORT_VAR=${3:-6379} \
    -DSET_TEST=${4:-ON} \
    && cmake --build build -j$(nproc)
    && ctest --test-dir build