
cmake_minimum_required(VERSION 3.4.1)

file(GLOB_RECURSE GIF_DIRECTORIES ${CMAKE_SOURCE_DIR}/giflib/*.*)

add_library(
        native-lib
        SHARED
#        testgif.cpp
        native-lib.cpp
        ${GIF_DIRECTORIES}
#        giflib/dgif_lib.c
#        giflib/egif_lib.c
#        giflib/gif_err.c
#        giflib/gif_hash.c
#        giflib/gifalloc.c
#        giflib/openbsd-reallocarray.c
#        giflib/quantize.c
)


target_link_libraries(
        native-lib
        jnigraphics
        log
        android)