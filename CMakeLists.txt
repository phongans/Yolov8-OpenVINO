cmake_minimum_required(VERSION 3.12)
set(PROJECT_NAME libinferenceopv)
set(SRC main.cpp)

project(${PROJECT_NAME})
find_package(OpenCV REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    /opt/intel/openvino/runtime/include
    /opt/intel/openvino/runtime/include/ie
    /opt/intel/openvino/runtime/include/ngraph
)

message(STATUS "OpenCV library status:")
message(STATUS "version:${OpenCV_VERSION}")
message(STATUS "libraries:${OpenCV_LIBS}")
message(STATUS "include path:${OpenCV_INCLUDE_DIRS}")
### generate detector.so
add_library(${PROJECT_NAME} SHARED ${CMAKE_CURRENT_SOURCE_DIR}/inferenceopv.cpp)
target_include_directories(${PROJECT_NAME}
      PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
      PUBLIC  ${OpenCV_INCLUDE_DIRS}
      /opt/intel/openvino/runtime/include
      /opt/intel/openvino/runtime/include/ie
      /opt/intel/openvino/runtime/include/ngraph
)
target_link_libraries(${PROJECT_NAME}
      ${OpenCV_LIBS}
      /opt/intel/openvino/runtime/lib/intel64/libopenvino.so
    )

add_executable(${PROJECT_NAME}_exe ${SRC})
target_link_libraries(${PROJECT_NAME}_exe
            ${OpenCV_LIBS}
            ${PROJECT_NAME}
    )