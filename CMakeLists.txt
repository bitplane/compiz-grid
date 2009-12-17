find_package (Compiz REQUIRED)

include (CompizPlugin)

compiz_plugin(grid PLUGINDEPS composite opengl)
