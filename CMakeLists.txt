cmake_minimum_required(VERSION 3.16)
project(studyctl VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------- vendored SQLite ----------------
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_OMIT_LOAD_EXTENSION=1     # no dlopen, smaller and safer
    SQLITE_DQS=0                     # reject double-quoted string literals
    SQLITE_DEFAULT_MEMSTATUS=0       # small speedup
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_COLUMN_METADATA=1
)
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

# ---------------- core library ----------------
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
     src/db/*.cpp src/model/*.cpp src/repo/*.cpp
     src/control/*.cpp src/service/*.cpp src/cli/*.cpp src/util/*.cpp)

if(CORE_SOURCES)
  add_library(studyctl_core STATIC ${CORE_SOURCES})
  target_include_directories(studyctl_core PUBLIC src)
  target_link_libraries(studyctl_core PUBLIC sqlite3)

  if(MSVC)
    target_compile_options(studyctl_core PRIVATE /W4 /permissive-)
  else()
    target_compile_options(studyctl_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
  endif()
else()
  # No sources yet (fresh skeleton) - keep the graph linkable until real
  # files land under src/{db,model,repo,control,service,cli,util}.
  add_library(studyctl_core INTERFACE)
  target_include_directories(studyctl_core INTERFACE src)
  target_link_libraries(studyctl_core INTERFACE sqlite3)
endif()

# ---------------- executable ----------------
add_executable(studyctl src/main.cpp)
target_link_libraries(studyctl PRIVATE studyctl_core)

# Embed the SQL migration files into the binary so the tool stays single-file.
# (Alternative: install them alongside. Embedding is more portable.)
add_custom_command(TARGET studyctl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/sql $<TARGET_FILE_DIR:studyctl>/sql)

# ---------------- tests ----------------
enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
if(TEST_SOURCES)
  add_executable(studyctl_tests ${TEST_SOURCES})
  target_include_directories(studyctl_tests PRIVATE third_party/doctest)
  target_link_libraries(studyctl_tests PRIVATE studyctl_core)
  add_test(NAME unit COMMAND studyctl_tests)
endif()
