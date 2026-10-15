# Fontes oficiais das dependências compartilhadas do SnesRecompProject.
#
# Inclua este arquivo no CMakeLists.txt de cada jogo. As dependências ficam
# fixadas como submodules em revisões conhecidas. Se uma fonte não estiver
# inicializada no clone, o CMake executa `git submodule update --init` para
# obtê-la. O projeto principal continua sem cópias vendorizadas das fontes.
#
# Estrutura esperada:
#   dependences/SDL3/                 SDL 3.4.10
#   dependences/lua-5.4.9/src/        Lua 5.4.9 (submodule oficial)
#   dependences/libjuice-1.7.2/       libjuice 1.7.2

include_guard(GLOBAL)

get_filename_component(SNESRECOMP_DEPENDENCES_ROOT
    "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

# The original workspace keeps shared dependencies one level above each game.
# Keep that checkout usable while allowing this repository to stand alone.
if(NOT EXISTS "${SNESRECOMP_DEPENDENCES_ROOT}/SDL3/CMakeLists.txt"
   AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../../dependences/SDL3/CMakeLists.txt")
    get_filename_component(SNESRECOMP_DEPENDENCES_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/../../../dependences" ABSOLUTE)
endif()

set(SNESRECOMP_SDL3_SOURCE_DIR
    "${SNESRECOMP_DEPENDENCES_ROOT}/SDL3" CACHE PATH
    "SDL3 source tree shared by projects in this repository" FORCE)
set(FETCHCONTENT_SOURCE_DIR_SDL3
    "${SNESRECOMP_SDL3_SOURCE_DIR}" CACHE PATH
    "Use the repository's SDL3 submodule" FORCE)

set(SNESRECOMP_LUA_SOURCE_DIR
    "${SNESRECOMP_DEPENDENCES_ROOT}/lua-5.4.9" CACHE PATH
    "Lua 5.4.9 source tree shared by projects in this repository" FORCE)
set(FETCHCONTENT_SOURCE_DIR_SNES_LUA
    "${SNESRECOMP_LUA_SOURCE_DIR}" CACHE PATH
    "Use the repository's Lua submodule" FORCE)

set(RNET_LIBJUICE_ROOT
    "${SNESRECOMP_DEPENDENCES_ROOT}/libjuice-1.7.2" CACHE PATH
    "libjuice 1.7.2 source tree shared by projects in this repository" FORCE)

set(_SNESRECOMP_DEPENDENCY_CHECKS
    "SDL3|${SNESRECOMP_SDL3_SOURCE_DIR}/CMakeLists.txt|dependences/SDL3"
    "Lua 5.4.9|${SNESRECOMP_LUA_SOURCE_DIR}/src/lua.h|dependences/lua-5.4.9/src"
    "libjuice 1.7.2|${RNET_LIBJUICE_ROOT}/CMakeLists.txt|dependences/libjuice-1.7.2")
set(_SNESRECOMP_SUBMODULES_TO_INIT)

foreach(_dependency IN LISTS _SNESRECOMP_DEPENDENCY_CHECKS)
    string(REPLACE "|" ";" _dependency_parts "${_dependency}")
    list(GET _dependency_parts 0 _dependency_name)
    list(GET _dependency_parts 1 _dependency_marker)
    list(GET _dependency_parts 2 _submodule_path)
    if(NOT EXISTS "${_dependency_marker}")
        list(APPEND _SNESRECOMP_SUBMODULES_TO_INIT "${_submodule_path}")
    endif()
endforeach()

if(_SNESRECOMP_SUBMODULES_TO_INIT)
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        message(FATAL_ERROR
            "Fontes compartilhadas ausentes e Git não encontrado. Instale o Git "
            "e clone este repositório com `git clone --recurse-submodules`, "
            "ou execute `git submodule update --init --recursive` na raiz.")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SNESRECOMP_DEPENDENCES_ROOT}"
            rev-parse --show-toplevel
        RESULT_VARIABLE _git_root_result
        OUTPUT_VARIABLE _git_root
        ERROR_VARIABLE _git_root_error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _git_root_result EQUAL 0)
        message(FATAL_ERROR
            "Fontes compartilhadas ausentes e a pasta dependences não está "
            "dentro de um clone Git. Clone o repositório com "
            "`git clone --recurse-submodules`. Detalhes: ${_git_root_error}")
    endif()

    message(STATUS
        "Inicializando submodules compartilhados ausentes: "
        "${_SNESRECOMP_SUBMODULES_TO_INIT}")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${_git_root}"
            submodule update --init --recursive --
            ${_SNESRECOMP_SUBMODULES_TO_INIT}
        RESULT_VARIABLE _submodule_result
        OUTPUT_VARIABLE _submodule_output
        ERROR_VARIABLE _submodule_error)
    if(NOT _submodule_result EQUAL 0)
        message(FATAL_ERROR
            "Não foi possível inicializar os submodules compartilhados. "
            "Verifique a conexão de rede e os URLs em .gitmodules, ou execute "
            "`git submodule update --init --recursive` manualmente.\n"
            "Saída: ${_submodule_output}\nErro: ${_submodule_error}")
    endif()
endif()

foreach(_dependency IN LISTS _SNESRECOMP_DEPENDENCY_CHECKS)
    string(REPLACE "|" ";" _dependency_parts "${_dependency}")
    list(GET _dependency_parts 0 _dependency_name)
    list(GET _dependency_parts 1 _dependency_marker)
    if(NOT EXISTS "${_dependency_marker}")
        message(FATAL_ERROR
            "A fonte de ${_dependency_name} continua ausente: "
            "${_dependency_marker}. Confira se o submodule correspondente "
            "está registrado em .gitmodules e se sua revisão está disponível.")
    endif()
endforeach()

message(STATUS
    "Dependências compartilhadas prontas: SDL3 3.4.10, Lua 5.4.9 e "
    "libjuice 1.7.2 em ${SNESRECOMP_DEPENDENCES_ROOT}")
