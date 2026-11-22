#pragma once

// Namen, die sich im Web nie anmelden lassen: "admin" und "generic" sind fest an die Rollen
// Admin und Generic gebunden (Groß-/Kleinschreibung egal, Leerzeichen am Rand werden ignoriert).
bool userNameReserved(const char *name);
