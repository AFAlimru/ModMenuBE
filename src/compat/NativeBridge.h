#pragma once
#include "Admission.h"
#include <string>
namespace modmenu::compat {
bool installNativeBridge();
bool nativeReady();
Observation nativeObservation();
std::string gameLanguage();
} // namespace modmenu::compat
