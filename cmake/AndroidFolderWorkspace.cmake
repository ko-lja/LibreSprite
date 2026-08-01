# The Android build uses an external, explicit source manifest instead of
# src/app/CMakeLists.txt. Keep folder-workspace sources linked there too.
target_sources(main-lib PRIVATE
  ${SRC}/app/commands/cmd_open_folder.cpp
  ${SRC}/app/ui/folder_tree_model.cpp
  ${SRC}/app/ui/folder_view.cpp)
