include_guard(GLOBAL)

# Run after the existing API and absolute-motion adapters. The producer emits
# one atomic XY event; the central expands it inside its input thread, before
# the normal speed/scroll processors, without queueing a second frame.
function(tako_cirque_motion_packet)
  if(CONFIG_SHIELD_TAKO_CIRQUE_RIGHT)
    get_target_property(sources drivers__input SOURCES)
    set(driver_count 0)
    foreach(source IN LISTS sources)
      if(source MATCHES "/tako-nxtkb-compat/input_pinnacle\\.c$")
        file(READ "${source}" content)
        set(helper [=[static void pinnacle_report_motion(const struct device *dev, int16_t x,
                                   int16_t y, bool sync)
{
    int32_t value = tako_cirque_pack_xy(x, y);
    int result = input_report(dev, INPUT_EV_MSC, TAKO_CIRQUE_MOTION_CODE,
                              value, sync, K_NO_WAIT);
#if IS_ENABLED(CONFIG_SHIELD_TAKO_CIRQUE_PROBE)
    tako_cirque_probe_report(result, TAKO_CIRQUE_MOTION_CODE, value, sync);
#else
    (void)result;
#endif
}

]=])
        set(helper_anchor "static inline bool pinnacle_bus_is_ready(const struct device *dev)")
        set(edge_anchor [=[pinnacle_report_rel(dev, INPUT_REL_X, CLAMP(delta_x, INT16_MIN, INT16_MAX), false);
	pinnacle_report_rel(dev, INPUT_REL_Y, CLAMP(delta_y, INT16_MIN, INT16_MAX), true);]=])
        set(relative_anchor [=[pinnacle_report_rel(dev, INPUT_REL_X, sample->rel_x, false);
			pinnacle_report_rel(dev, INPUT_REL_Y, sample->rel_y, !buttons_changed);]=])
        set(absolute_anchor [=[pinnacle_report_rel(dev, INPUT_REL_X, rel_x, false);
		pinnacle_report_rel(dev, INPUT_REL_Y, rel_y, true);]=])
        foreach(anchor IN ITEMS helper_anchor edge_anchor relative_anchor absolute_anchor)
          string(FIND "${content}" "${${anchor}}" found)
          if(found EQUAL -1)
            message(FATAL_ERROR "Pinned Cirque packet anchor missing: ${anchor}")
          endif()
        endforeach()
        string(PREPEND content "#include \"cirque_motion_packet.h\"\n")
        string(REPLACE "${helper_anchor}" "${helper}${helper_anchor}" content "${content}")
        string(REPLACE "${edge_anchor}"
          "pinnacle_report_motion(dev, CLAMP(delta_x, INT16_MIN, INT16_MAX), CLAMP(delta_y, INT16_MIN, INT16_MAX), true);"
          content "${content}")
        string(REPLACE "${relative_anchor}"
          "pinnacle_report_motion(dev, sample->rel_x, sample->rel_y, !buttons_changed);"
          content "${content}")
        string(REPLACE "${absolute_anchor}" "pinnacle_report_motion(dev, rel_x, rel_y, true);"
          content "${content}")
        file(WRITE "${source}" "${content}")
        math(EXPR driver_count "${driver_count} + 1")
      endif()
    endforeach()
    if(NOT driver_count EQUAL 1)
      message(FATAL_ERROR "Expected exactly one adapted Cirque packet producer")
    endif()
  endif()

  if(CONFIG_SHIELD_TAKO_CIRQUE_RECEIVER)
    get_target_property(sources app SOURCES)
    set(adapted "")
    set(listener_count 0)
    foreach(source IN LISTS sources)
      if(source MATCHES "(^|/)input_listener\\.c$")
        if(IS_ABSOLUTE "${source}")
          set(listener_source "${source}")
        else()
          set(listener_source "${APPLICATION_SOURCE_DIR}/src/pointing/input_listener.c")
        endif()
        file(READ "${listener_source}" content)
        set(anchor "    // First, process to update the event data as needed.")
        string(FIND "${content}" "${anchor}" found)
        if(found EQUAL -1)
          message(FATAL_ERROR "Pinned ZMK packet listener anchor missing")
        endif()
        set(expand [=[    if (evt->dev == DEVICE_DT_GET(DT_NODELABEL(tako_cirque_split)) &&
        evt->type == INPUT_EV_MSC && evt->code == TAKO_CIRQUE_MOTION_CODE) {
        struct input_event axis = *evt;
        axis.type = INPUT_EV_REL;
        axis.code = INPUT_REL_X;
        axis.value = tako_cirque_unpack_x(evt->value);
        axis.sync = false;
        input_handler(config, data, &axis);
        axis.code = INPUT_REL_Y;
        axis.value = tako_cirque_unpack_y(evt->value);
        axis.sync = evt->sync;
        input_handler(config, data, &axis);
        return;
    }

]=])
        string(PREPEND content "#include \"cirque_motion_packet.h\"\n")
        string(REPLACE "${anchor}" "${expand}${anchor}" content "${content}")
        set(output "${PROJECT_BINARY_DIR}/tako-nxtkb-compat/input_listener.c")
        file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tako-nxtkb-compat")
        file(WRITE "${output}" "${content}")
        list(APPEND adapted "${output}")
        math(EXPR listener_count "${listener_count} + 1")
      else()
        list(APPEND adapted "${source}")
      endif()
    endforeach()
    if(NOT listener_count EQUAL 1)
      set(candidate_sources "${sources}")
      list(FILTER candidate_sources INCLUDE REGEX "input_listener")
      message(FATAL_ERROR "Expected exactly one pinned ZMK input listener: ${candidate_sources}")
    endif()
    target_include_directories(app PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    set_property(TARGET app PROPERTY SOURCES "${adapted}")
  endif()
endfunction()
# ZMK app sources are registered after find_package(Zephyr) returns.
# Defer to the application directory, not the earlier Zephyr module stage.
cmake_language(DEFER DIRECTORY "${APPLICATION_SOURCE_DIR}" CALL tako_cirque_motion_packet)
