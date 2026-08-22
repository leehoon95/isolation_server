# Docker 이미지 빌드시 사용하는 스크립트
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc) 
