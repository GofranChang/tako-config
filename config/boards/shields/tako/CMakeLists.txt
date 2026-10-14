# USB-aware display blanking for every Tako screen (ZMK v0.3).
if(CONFIG_ZMK_DISPLAY)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/usb_display.c)
endif()
