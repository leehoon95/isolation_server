cd tests
cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    && cmake --build build -j${nproc} \
    && build/run_unit_tests --output-on-failure --gtest_output=xml:build/gtest_results.xml
