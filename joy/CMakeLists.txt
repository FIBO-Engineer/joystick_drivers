# http://ros.org/doc/groovy/api/catkin/html/user_guide/supposed.html
cmake_minimum_required(VERSION 2.8.3)
project(joy)

# Load catkin and all dependencies required for this package
set(CATKIN_DEPS roscpp diagnostic_updater sensor_msgs)
find_package(catkin REQUIRED ${CATKIN_DEPS})
catkin_package(DEPENDS ${CATKIN_DEPS})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # Look for <linux/joystick.h>
  include(CheckIncludeFiles)
  check_include_files(linux/joystick.h HAVE_LINUX_JOYSTICK_H)

  if(HAVE_LINUX_JOYSTICK_H)
    include_directories(msg/cpp ${catkin_INCLUDE_DIRS})
    add_executable(joy_node src/joy_node.cpp)
    target_link_libraries(joy_node ${catkin_LIBRARIES})
  else(HAVE_LINUX_JOYSTICK_H)
    message("Warning: no <linux/joystick.h>; won't build joy node")
  endif(HAVE_LINUX_JOYSTICK_H)

  #catkin_add_nosetests(test/test_joy_msg_migration.py)

  # Install targets
  if(HAVE_LINUX_JOYSTICK_H)
  install(TARGETS joy_node
    ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
    LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
    RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})
  endif(HAVE_LINUX_JOYSTICK_H)
  install(DIRECTORY migration_rules
    DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
    )
else()
  find_package(SDL2 REQUIRED)

  include(CheckIncludeFiles)
    include_directories(msg/cpp ${catkin_INCLUDE_DIRS})
  add_executable(joy_node src/joy_node_win.cpp)

  set_target_properties(SDL2::SDL2 PROPERTIES
    MAP_IMPORTED_CONFIG_MINSIZEREL Release
    MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
    )

  set_target_properties(SDL2::SDL2main PROPERTIES
    MAP_IMPORTED_CONFIG_MINSIZEREL Release
    MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
    )

  target_link_libraries(joy_node 
    ${catkin_LIBRARIES}
    SDL2::SDL2 SDL2::SDL2main
    )


  file(DOWNLOAD
      https://raw.githubusercontent.com/gabomdq/SDL_GameControllerDB/e84ccd2617a2c5c507e08a0b6221a7bc23d97695/gamecontrollerdb.txt
      ${PROJECT_SOURCE_DIR}/launch/gamecontrollerdb.txt
      SHOW_PROGRESS
  )    

  install(DIRECTORY launch/
    DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
    FILES_MATCHING PATTERN "*.launch"
  )    

  install(TARGETS joy_node
    ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
    LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
    RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})

  install(DIRECTORY migration_rules
    DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
    )

endif()

