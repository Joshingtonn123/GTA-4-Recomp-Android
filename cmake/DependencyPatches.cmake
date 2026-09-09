# Source fixes required by LibertyRecomp but not present in the pinned upstream
# submodules. Keep the public gitlinks unchanged and reproduce the reviewed edits
# from this repository, including newly added dependency source files.
#
# Normal root configuration calls this automatically. Before configuring a
# standalone dependency tool, the same preparation can be run explicitly:
#   cmake -DLIBERTY_DEPENDENCY_ROOT=/path/to/LibertyRecomp \
#         -P /path/to/LibertyRecomp/cmake/DependencyPatches.cmake
#
# All touched files are checked before any patch is applied. Unknown local edits
# cause a failure rather than a reset, restore, partial patch, or overwrite.
include_guard(GLOBAL)

function(liberty_dependency_file_hash path output)
    if(IS_SYMLINK "${path}" OR IS_DIRECTORY "${path}")
        message(FATAL_ERROR "Dependency patch expects a regular file: ${path}")
    endif()
    if(NOT EXISTS "${path}")
        set(${output} "absent" PARENT_SCOPE)
        return()
    endif()
    file(READ "${path}" content)
    # Hash canonical source newlines so a Windows checkout is accepted too.
    string(REPLACE "\r\n" "\n" content "${content}")
    string(SHA256 digest "${content}")
    set(${output} "${digest}" PARENT_SCOPE)
endfunction()

function(liberty_apply_dependency_patches repository_root)
    find_package(Git REQUIRED QUIET)
    get_filename_component(repository_root "${repository_root}" ABSOLUTE)
    set(patch_directory "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/dependency-patches")
    file(READ "${patch_directory}/manifest.json" manifest)
    string(JSON schema GET "${manifest}" schema)
    if(NOT schema EQUAL 1)
        message(FATAL_ERROR "Unsupported LibertyRecomp dependency patch manifest")
    endif()

    # Serialize preparation across build directories sharing the same checkout.
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${repository_root}" rev-parse --absolute-git-dir
        RESULT_VARIABLE git_directory_result OUTPUT_VARIABLE git_directory
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT git_directory_result EQUAL 0)
        set(git_directory "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    file(LOCK "${git_directory}/liberty-dependency-patches.lock" GUARD FUNCTION TIMEOUT 30)

    string(JSON dependency_count LENGTH "${manifest}" dependencies)
    math(EXPR last_dependency "${dependency_count} - 1")
    set(planned_dependencies)
    foreach(index RANGE ${last_dependency})
        string(JSON entry GET "${manifest}" dependencies ${index})
        string(JSON name GET "${entry}" name)
        string(JSON relative GET "${entry}" path)
        string(JSON patch_name GET "${entry}" patch)
        string(JSON expected_patch_hash GET "${entry}" sha256)
        if(DEFINED LIBERTY_DEPENDENCY_ONLY AND NOT "${LIBERTY_DEPENDENCY_ONLY}" STREQUAL "${name}")
            continue()
        endif()
        set(source "${repository_root}/${relative}")
        if(NOT EXISTS "${source}/.git")
            message(STATUS "LibertyRecomp dependency patch: ${name} not initialized; skipped")
            continue()
        endif()

        set(strict_dependency_patches ON)
        if(DEFINED ENV{LIBERTY_RECOMP_STRICT_DEP_PATCHES})
            set(strict_dependency_patches "$ENV{LIBERTY_RECOMP_STRICT_DEP_PATCHES}")
        endif()
        string(TOLOWER "${strict_dependency_patches}" strict_dependency_patches)

        set(base_revision "")
        if("${entry}" MATCHES "base_revision")
            string(JSON base_revision GET "${entry}" base_revision)
        endif()
        if(NOT base_revision STREQUAL "")
            execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" rev-parse HEAD
                OUTPUT_VARIABLE dependency_head_revision OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
            if(NOT dependency_head_revision STREQUAL base_revision)
                if(strict_dependency_patches STREQUAL "on" OR strict_dependency_patches STREQUAL "1" OR strict_dependency_patches STREQUAL "true" OR strict_dependency_patches STREQUAL "yes")
                    message(FATAL_ERROR
                        "Dependency patch pin drift: ${name} expects ${base_revision} but checkout is ${dependency_head_revision}; set LIBERTY_RECOMP_STRICT_DEP_PATCHES=OFF to allow upstream drift.")
                endif()
                message(WARNING
                    "Dependency patch pin drift: ${name} expects ${base_revision} but checkout is ${dependency_head_revision}; skipping patch verification for this upstream-moved dependency.")
                continue()
            endif()
        endif()

        set(patch "${patch_directory}/${patch_name}")
        file(SHA256 "${patch}" actual_patch_hash)
        if(NOT actual_patch_hash STREQUAL expected_patch_hash)
            message(FATAL_ERROR "Dependency patch checksum mismatch: ${patch}")
        endif()
        string(JSON file_count LENGTH "${entry}" files)
        math(EXPR last_file "${file_count} - 1")
        set(includes_${index})
        foreach(file_index RANGE ${last_file})
            string(JSON path GET "${entry}" files ${file_index} path)
            string(JSON before GET "${entry}" files ${file_index} before_sha256)
            string(JSON after GET "${entry}" files ${file_index} after_sha256)
            if(IS_ABSOLUTE "${path}" OR path MATCHES "(^|/)\\.\\.(/|$)")
                message(FATAL_ERROR "Invalid dependency patch path: ${path}")
            endif()
            liberty_dependency_file_hash("${source}/${path}" current)
            if(current STREQUAL after)
                continue()
            endif()
            if(NOT current STREQUAL before)
                message(FATAL_ERROR
                    "Local dependency changes differ from the reviewed patch: ${relative}/${path}\n"
                    "No source files have been changed. Preserve and review the local edits first.")
            endif()
            list(APPEND includes_${index} "--include=${path}")
        endforeach()
        if(includes_${index})
            execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" apply
                --check --ignore-space-change ${includes_${index}} "${patch}"
                RESULT_VARIABLE check_result ERROR_VARIABLE check_error)
            if(NOT check_result EQUAL 0)
                message(FATAL_ERROR "Cannot prepare ${name}; no patches applied:\n${check_error}")
            endif()
            list(APPEND planned_dependencies ${index})
        endif()
    endforeach()

    foreach(index IN LISTS planned_dependencies)
        string(JSON entry GET "${manifest}" dependencies ${index})
        string(JSON name GET "${entry}" name)
        string(JSON relative GET "${entry}" path)
        string(JSON patch_name GET "${entry}" patch)
        set(source "${repository_root}/${relative}")
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" apply
            --ignore-space-change ${includes_${index}} "${patch_directory}/${patch_name}"
            RESULT_VARIABLE apply_result ERROR_VARIABLE apply_error)
        if(NOT apply_result EQUAL 0)
            message(FATAL_ERROR "Dependency patch failed for ${name}: ${apply_error}")
        endif()
        string(JSON file_count LENGTH "${entry}" files)
        math(EXPR last_file "${file_count} - 1")
        foreach(file_index RANGE ${last_file})
            string(JSON path GET "${entry}" files ${file_index} path)
            string(JSON after GET "${entry}" files ${file_index} after_sha256)
            liberty_dependency_file_hash("${source}/${path}" current)
            if(NOT current STREQUAL after)
                message(FATAL_ERROR "Dependency post-patch checksum mismatch: ${relative}/${path}")
            endif()
        endforeach()
        message(STATUS "LibertyRecomp dependency patch applied: ${name}")
    endforeach()
endfunction()

if("${CMAKE_SCRIPT_MODE_FILE}" STREQUAL "${CMAKE_CURRENT_LIST_FILE}")
    if(NOT DEFINED LIBERTY_DEPENDENCY_ROOT)
        get_filename_component(LIBERTY_DEPENDENCY_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    endif()
    liberty_apply_dependency_patches("${LIBERTY_DEPENDENCY_ROOT}")
endif()
