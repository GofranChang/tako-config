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
