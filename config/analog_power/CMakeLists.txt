# SPDX-License-Identifier: MIT
# Apply only to the exact analog-matrix-module revision pinned in west.yml.
# Generated copies keep the west checkout untouched. Defer until all upstream
# libraries have registered their sources, independent of module ordering.
set_property(GLOBAL PROPERTY TAKO_ANALOG_POWER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

function(tako_analog_power_patch)
  get_property(libraries GLOBAL PROPERTY ZEPHYR_LIBS)
  get_property(output_dir GLOBAL PROPERTY TAKO_ANALOG_POWER_BINARY_DIR)
  file(MAKE_DIRECTORY "${output_dir}/patched")
  set(patched_count 0)
  foreach(library IN LISTS libraries)
    get_target_property(sources "${library}" SOURCES)
    get_target_property(source_dir "${library}" SOURCE_DIR)
    if(NOT sources)
      continue()
    endif()
    set(updated_sources)
    foreach(source IN LISTS sources)
      get_filename_component(name "${source}" NAME)
      if(name STREQUAL "zmk_kscan_ec_matrix.c" OR name STREQUAL "zmk_analog_matrix.c")
        if(IS_ABSOLUTE "${source}")
          set(original "${source}")
        else()
          set(original "${source_dir}/${source}")
        endif()
        file(SHA256 "${original}" actual_hash)
        if(name STREQUAL "zmk_kscan_ec_matrix.c")
          set(expected_hash "3628fe64b5d55d4df80f835ee8ed2a52b17d096374afdde897735165edc2d897")
          set(before "k_busy_wait(cfg->matrix_warm_up_us);")
          set(after [=[// Millisecond-scale analog settling does not need a running CPU.
        // Keep sub-millisecond timing waits unchanged.
        if (cfg->matrix_warm_up_us >= 1000) {
            k_sleep(K_USEC(cfg->matrix_warm_up_us));
        } else {
            k_busy_wait(cfg->matrix_warm_up_us);
        }]=])
        else()
          set(expected_hash "5a89a5afeebfa18c25b58fc71c005dcc53ed2e3745df73db691cfd91ac0da7fb")
          set(before [=[    k_thread_suspend(&common_data->thread);

    return 0;]=])
          set(after [=[    const struct zmk_analog_matrix_common_cfg *cfg = dev->config;

    // Finish any scan before suspension; never freeze it with matrix power on.
    // If calibration holds the lock, refuse suspend so ZMK aborts poweroff.
    int ret = k_mutex_lock(&common_data->mutex, K_SECONDS(1));
    if (ret < 0) {
        return ret;
    }

    k_thread_suspend(&common_data->thread);
    if (cfg->power) {
        cfg->power(dev, false);
    }
    k_mutex_unlock(&common_data->mutex);

    return 0;]=])
        endif()
        if(NOT actual_hash STREQUAL expected_hash)
          message(FATAL_ERROR "Tako power fix: ${name} changed upstream; review the patch before updating west.yml")
        endif()
        file(READ "${original}" content)
        string(FIND "${content}" "${before}" found)
        if(found EQUAL -1)
          message(FATAL_ERROR "Tako power fix: expected code missing in ${name}")
        endif()
        string(REPLACE "${before}" "${after}" content "${content}")
        set(generated "${output_dir}/patched/${name}")
        file(WRITE "${generated}" "${content}")
        get_filename_component(header_dir "${original}" DIRECTORY)
        target_include_directories("${library}" PRIVATE "${header_dir}")
        list(APPEND updated_sources "${generated}")
        math(EXPR patched_count "${patched_count} + 1")
        message(STATUS "Tako power fix: using patched ${name}")
      else()
        list(APPEND updated_sources "${source}")
      endif()
    endforeach()
    set_property(TARGET "${library}" PROPERTY SOURCES "${updated_sources}")
  endforeach()
  if(NOT patched_count EQUAL 2)
    message(FATAL_ERROR "Tako power fix: expected both EC/analog source files, found ${patched_count}")
  endif()
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL tako_analog_power_patch)
