#pragma once
#include <QString>
int run_gui();
void converter_set_initial_open_path(const QString& path);
inline void igi1conv_set_initial_open_path(const QString& path) { converter_set_initial_open_path(path); }
inline void igi2conv_set_initial_open_path(const QString& path) { converter_set_initial_open_path(path); }
