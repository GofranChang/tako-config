include_guard(GLOBAL)

# The pinned nxtkb driver applies invert-x, but ignores invert-y in its
# absolute delta and edge-coordinate paths. Keep both in the same coordinate
# system. Also retain per-axis fractional movement instead of dropping it.
function(tako_nxtkb_absolute_y)
  if(NOT CONFIG_SHIELD_TAKO_CIRQUE_RIGHT)
    return()
  endif()
  get_target_property(sources drivers__input SOURCES)
  set(patched "")
  set(patch_count 0)
  foreach(source IN LISTS sources)
    if(source MATCHES "/cirque-input-module/drivers/input/input_pinnacle\\.c$")
      file(READ "${source}" content)
      get_filename_component(driver_directory "${source}" DIRECTORY)
      target_include_directories(drivers__input PRIVATE "${driver_directory}")
      set(delta_anchor "\tif (config->invert_x) {\n\t\t*delta_x = -*delta_x;\n\t}")
      set(position_anchor "\tif (config->invert_x) {\n\t\tx = width - x;\n\t}")
      foreach(anchor IN ITEMS delta_anchor position_anchor)
        string(FIND "${content}" "${${anchor}}" found)
        if(found EQUAL -1)
          message(FATAL_ERROR "Pinned Cirque absolute-Y patch anchor missing: ${anchor}")
        endif()
      endforeach()
      string(REPLACE "${delta_anchor}"
        "${delta_anchor}\n\tif (config->invert_y) {\n\t\t*delta_y = -*delta_y;\n\t}"
        content "${content}")
      string(REPLACE "${position_anchor}"
        "${position_anchor}\n\tif (config->invert_y) {\n\t\ty = height - y;\n\t}"
        content "${content}")
      # Tap zones must follow the same mounting transform as pointer motion.
      set(tap_anchor [=[	int32_t right = pinnacle_abs_right(config);
	int32_t left = pinnacle_abs_left(config);
	int32_t top = pinnacle_abs_top(config);
	int32_t bottom = pinnacle_abs_bottom(config);
	int32_t left_boundary = MAX(left, right - config->absolute_secondary_tap_area_width);
	int32_t top_boundary = MAX(top, bottom - config->absolute_secondary_tap_area_height);

	return x >= left_boundary && y >= top_boundary;]=])
      set(tap_fixed [=[	int32_t logical_x, logical_y, width, height;
	pinnacle_absolute_logical_position(config, x, y, &logical_x, &logical_y,
	                                   &width, &height);
	return logical_x >= MAX(0, width - config->absolute_secondary_tap_area_width) &&
	       logical_y >= MAX(0, height - config->absolute_secondary_tap_area_height);]=])
      set(tap_function "static bool pinnacle_absolute_point_in_lower_right(")
      foreach(anchor IN ITEMS tap_anchor tap_function)
        string(FIND "${content}" "${${anchor}}" found)
        if(found EQUAL -1)
          message(FATAL_ERROR "Pinned Cirque tap-zone patch anchor missing: ${anchor}")
        endif()
      endforeach()
      string(REPLACE "${tap_anchor}" "${tap_fixed}" content "${content}")
      set(position_declaration [=[static void pinnacle_absolute_logical_position(const struct pinnacle_config *config,
                                             uint16_t raw_x, uint16_t raw_y,
                                             int32_t *logical_x, int32_t *logical_y,
                                             int32_t *logical_width, int32_t *logical_height);

]=])
      string(REPLACE "${tap_function}" "${position_declaration}${tap_function}"
        content "${content}")
      # Preserve subpixel movement before the receiver's speed processor.
      set(scale_anchor [=[static int16_t pinnacle_scale_absolute_delta(const struct device *dev, int32_t delta)
{
	const struct pinnacle_config *config = dev->config;
	int64_t scaled = (int64_t)delta * config->absolute_relative_multiplier;

	scaled /= config->absolute_relative_divisor;

	return CLAMP(scaled, INT16_MIN, INT16_MAX);
}]=])
      set(scale_fixed [=[static int16_t pinnacle_scale_absolute_delta(const struct device *dev, int32_t delta,
                                               int32_t *remainder)
{
	const struct pinnacle_config *config = dev->config;
	int64_t scaled = tako_cirque_scale_delta(delta, config->absolute_relative_multiplier,
                                              config->absolute_relative_divisor, remainder);
	return CLAMP(scaled, INT16_MIN, INT16_MAX);
}]=])
      set(x_call "pinnacle_scale_absolute_delta(dev, delta_x)")
      set(y_call "pinnacle_scale_absolute_delta(dev, delta_y)")
      set(reset_anchor "drv_data->scroll_remainder = 0;")
      foreach(anchor IN ITEMS scale_anchor x_call y_call reset_anchor)
        string(FIND "${content}" "${${anchor}}" found)
        if(found EQUAL -1)
          message(FATAL_ERROR "Pinned Cirque motion patch anchor missing: ${anchor}")
        endif()
      endforeach()
      string(REPLACE "${scale_anchor}" "${scale_fixed}" content "${content}")
      string(REPLACE "${x_call}"
        "pinnacle_scale_absolute_delta(dev, delta_x, &drv_data->motion_remainder_x)"
        content "${content}")
      string(REPLACE "${y_call}"
        "pinnacle_scale_absolute_delta(dev, delta_y, &drv_data->motion_remainder_y)"
        content "${content}")
      # All existing scroll-reset sites are contact or mode boundaries.
      string(REGEX REPLACE "(\n[\t ]*)drv_data->scroll_remainder = 0;"
        "\\1drv_data->scroll_remainder = 0;\\1drv_data->motion_remainder_x = 0;\\1drv_data->motion_remainder_y = 0;"
        content "${content}")
      string(REPLACE "#include \"input_pinnacle.h\""
        "#include \"input_pinnacle.h\"\n#include \"cirque_motion.h\"" content "${content}")
      target_include_directories(drivers__input PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
      file(READ "${driver_directory}/input_pinnacle.h" header)
      set(header_anchor "\tint32_t scroll_remainder;")
      string(FIND "${header}" "${header_anchor}" found)
      if(found EQUAL -1)
        message(FATAL_ERROR "Pinned Cirque motion state anchor missing")
      endif()
      string(REPLACE "${header_anchor}"
        "${header_anchor}\n\tint32_t motion_remainder_x;\n\tint32_t motion_remainder_y;"
        header "${header}")
      if(CONFIG_SHIELD_TAKO_CIRQUE_PROBE)
        set(irq_anchor "\tk_work_submit(&drv_data->work);")
        set(work_anchor "\tpinnacle_handle_interrupt(drv_data->dev);")
        set(read_anchor [=[	if (rc) {
		LOG_ERR("Failed to read data packets");
		return rc;
	}]=])
        foreach(anchor IN ITEMS irq_anchor work_anchor read_anchor)
          string(FIND "${content}" "${${anchor}}" found)
          if(found EQUAL -1)
            message(FATAL_ERROR "Pinned Cirque probe anchor missing: ${anchor}")
          endif()
        endforeach()
        string(FIND "${content}" "config->absolute_tap_max_movement" found)
        if(found EQUAL -1)
          message(FATAL_ERROR "Pinned Cirque tap threshold probe anchor missing")
        endif()
        string(REPLACE "config->absolute_tap_max_movement"
          "tako_cirque_probe_tap_limit(config->absolute_tap_max_movement)" content "${content}")
        string(PREPEND content "#include \"cirque_probe.h\"\n")
        string(REPLACE "${irq_anchor}" "\ttako_cirque_probe_irq();\n${irq_anchor}" content "${content}")
        string(REPLACE "${work_anchor}" "\ttako_cirque_probe_work();\n${work_anchor}" content "${content}")
        string(REPLACE "${read_anchor}"
          "\ttako_cirque_probe_sample(rc, rc ? 0 : (pinnacle_is_relative_mode(dev) ? 63 : sample->abs_z));\n${read_anchor}"
          content "${content}")
        string(REPLACE "(void)input_report_rel(dev, code, value, sync, K_NO_WAIT);"
          "tako_cirque_probe_report(input_report_rel(dev, code, value, sync, K_NO_WAIT), code, value, sync);"
          content "${content}")
      endif()
      set(output "${PROJECT_BINARY_DIR}/tako-nxtkb-compat/input_pinnacle.c")
      file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tako-nxtkb-compat")
      file(WRITE "${PROJECT_BINARY_DIR}/tako-nxtkb-compat/input_pinnacle.h" "${header}")
      file(WRITE "${output}" "/* Generated absolute-Y correction for the pinned nxtkb driver. */\n${content}")
      list(APPEND patched "${output}")
      math(EXPR patch_count "${patch_count} + 1")
    else()
      list(APPEND patched "${source}")
    endif()
  endforeach()
  if(NOT patch_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one pinned Cirque input driver")
  endif()
  set_property(TARGET drivers__input PROPERTY SOURCES "${patched}")
endfunction()
cmake_language(DEFER DIRECTORY "${ZEPHYR_BASE}" CALL tako_nxtkb_absolute_y)
