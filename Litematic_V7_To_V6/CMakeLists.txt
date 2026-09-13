cmake_minimum_required(VERSION 3.13)
project(Litematic_V7_To_V6 LANGUAGES CXX)

add_executable(Litematic_V7_To_V6
    main.cpp
    LitematicFileConversion.cpp
)

# 中文错误信息以 UTF-8 源码存储，MSVC 默认按 ANSI 解析会乱码
if(MSVC)
    target_compile_options(Litematic_V7_To_V6 PRIVATE /utf-8)
endif()

set_release_static_link(Litematic_V7_To_V6)

target_link_libraries(Litematic_V7_To_V6
    PUBLIC
        ${COMMON_LIBS}
)
