cmake_minimum_required(VERSION 3.20)

add_executable(imfx "main.cpp")
target_include_directories(imfx PRIVATE "${CONAN_INCLUDE_DIRS}")
target_link_libraries(imfx PRIVATE opencv_imgcodecs opencv_imgproc opencv_core jasper png16 webp z jpeg)
install(
    FILES "${PROJECT_BINARY_DIR}/bin/imfx"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    DESTINATION bin)
target_compile_options(imfx PRIVATE -fwhole-program)
