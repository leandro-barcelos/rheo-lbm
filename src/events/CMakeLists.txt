add_library(rheo_events INTERFACE)
add_library(rheo::events ALIAS rheo_events)
target_include_directories(rheo_events INTERFACE include)
