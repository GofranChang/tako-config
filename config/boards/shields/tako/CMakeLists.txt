include_guard(GLOBAL)

# USB-aware display blanking for every Tako screen (ZMK v0.3).
if(CONFIG_ZMK_DISPLAY)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/usb_display.c)
endif()

# Optional probes, compiled only for the diagnostic dongle build.
if(CONFIG_SHIELD_TAKO_DONGLE_DIAGNOSTICS)
  if(NOT CONFIG_SHIELD_TAKO_DONGLE OR NOT CONFIG_SHIELD_TAKO_CIRQUE_RECEIVER)
    message(FATAL_ERROR "Tako diagnostics requires dongle and Cirque receiver shields")
  endif()
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/dongle_diagnostics.c)
  foreach(symbol IN ITEMS
      zmk_split_transport_central_peripheral_event_handler
      zmk_input_split_report_peripheral_event
      zmk_usb_hid_send_keyboard_report
      zmk_usb_hid_send_consumer_report
      zmk_usb_hid_send_mouse_report)
    zephyr_ld_options(-Wl,--wrap=${symbol})
  endforeach()
endif()

# The upstream Pinnacle producer uses blocking input_report() calls. Move its
# work off the system queue, which Bluetooth needs for TX buffer completion.
# Scope the redirection to this ONE source; never interpose the kernel globally.
if(CONFIG_SHIELD_TAKO_CIRQUE_RIGHT)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/cirque_work_queue.c)
  set_property(GLOBAL PROPERTY TAKO_CIRQUE_QUEUE_HEADER
               "${CMAKE_CURRENT_LIST_DIR}/cirque_work_queue.h")
  function(tako_configure_cirque_queue)
    if(NOT TARGET drivers__input)
      message(FATAL_ERROR "Cannot find input driver library for Cirque queue fix")
    endif()
    get_target_property(driver_sources drivers__input SOURCES)
    set(pinnacle_sources ${driver_sources})
    list(FILTER pinnacle_sources INCLUDE REGEX "/input_pinnacle\\.c$")
    list(LENGTH pinnacle_sources source_count)
    if(NOT source_count EQUAL 1)
      message(FATAL_ERROR "Expected exactly one pinned Pinnacle source")
    endif()
    get_property(queue_header GLOBAL PROPERTY TAKO_CIRQUE_QUEUE_HEADER)
    set_property(SOURCE ${pinnacle_sources} TARGET_DIRECTORY drivers__input
                 APPEND PROPERTY COMPILE_OPTIONS -include "${queue_header}")
  endfunction()
  # Shields are configured before drivers/modules; apply after they exist.
  cmake_language(DEFER DIRECTORY "${ZEPHYR_BASE}" CALL tako_configure_cirque_queue)
endif()
