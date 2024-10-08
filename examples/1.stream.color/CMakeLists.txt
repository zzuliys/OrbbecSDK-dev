
cmake_minimum_required(VERSION 3.5)
project(ob_color)

add_executable(ob_color color.cpp)

set_property(TARGET ob_color PROPERTY CXX_STANDARD 11)
target_link_libraries(ob_color PUBLIC ob::OrbbecSDK ob::examples::utils)
set_target_properties (ob_color PROPERTIES FOLDER "examples")

install(TARGETS ob_color RUNTIME DESTINATION bin)

# target_include_directories(ob_color PUBLIC ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/windows/include)
# file(GLOB LIB_FFMPEG_FILES "${CMAKE_SOURCE_DIR}//3rdparty/ffmpeg/windows/lib/x64/*.lib")
# target_link_libraries(ob_color PUBLIC ${LIB_FFMPEG_FILES})

set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg-4.4")
file(GLOB LIB_FILES "${FFMPEG_DIR}/lib/*.lib")
message("****${LIB_FILES}")
target_link_libraries(ob_color PUBLIC ${LIB_FILES})
include_directories("${FFMPEG_DIR}/include")

message("***${LIB_FFMPEG_FILES}")

target_include_directories(ob_color PUBLIC "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.2/include")
file(GLOB LIB_CUDA_FILES "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.2/lib/x64/*.lib")
target_link_libraries(ob_color PUBLIC ${LIB_CUDA_FILES})
# message("***${LIB_CUDA_FILES}")
# message("***** ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include")
