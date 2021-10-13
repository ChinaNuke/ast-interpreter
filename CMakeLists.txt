cmake_minimum_required(VERSION 3.1.0)
project(assign1)

find_package(Clang REQUIRED CONFIG HINTS ${LLVM_DIR} ${LLVM_DIR}/lib/cmake/clang NO_DEFAULT_PATH)

include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS} SYSTEM)
link_directories(${LLVM_LIBRARY_DIRS})

file(GLOB SOURCE "./*.cpp")

add_executable(ast-interpreter ${SOURCE})

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )


target_link_libraries(ast-interpreter
  clangAST
  clangBasic
  clangFrontend
  clangTooling
  )

install(TARGETS ast-interpreter
  RUNTIME DESTINATION bin)
