
target_compile_options( vmviewer PRIVATE
  -Wall -Wextra -Wno-unused-parameter -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-null-pointer-subtraction -fexperimental-library
  )

set_target_properties( vmviewer PROPERTIES CXX_STANDARD 23 )

target_sources( vmviewer PRIVATE
  main.cpp
  peerconnection.cpp
  peerconnection.h
  websocket.cpp
  websocket.h
  medooze_mgr.cpp
  medooze_mgr.h
  monitor_manager.cpp
  monitor_manager.h
  main_wnd.h
  tunnel_loggin.h
  tunnel_loggin.cpp
  )

target_link_libraries( vmviewer PRIVATE
  ${WEBRTC_LIBRARIES}
  nlohmann_json
  )

if( FAKE_RENDERER )
  message( STATUS "Compiling with fake renderer" )
  target_compile_definitions( vmviewer PRIVATE
    FAKE_RENDERER
    )
else()
  target_sources( vmviewer PRIVATE
    main_wnd.cpp
    )
  
  target_include_directories( vmviewer PRIVATE
    ${CAIRO_INCLUDE_DIR}
    ${GTK_INCLUDE_DIRS}
    )

  message(STATUS "${GTK_CFLAGS_OTHER}")

  target_link_libraries( vmviewer PRIVATE
    ${WEBRTC_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${GTK_LIBRARIES}
    nlohmann_json
    # /usr/local/lib/libfmt.a
    )

  target_compile_options( vmviewer PRIVATE ${GTK_CFLAGS_OTHER} )
endif()



