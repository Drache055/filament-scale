#pragma once

// Automatischer Rollback nach einem Update (CLAUDE.md 12.6, Logik in core/boot_check_core).
// Der Arduino-Core bestätigt eine neue Firmware sonst schon vor setup(); hier erst nach der Prüfung.
void bootCheckBegin();      // am Anfang von setup()
void bootCheckSetupDone();  // am Ende von setup()
void bootCheckLoop();       // regelmäßig aus dem net-Task
void bootCheckConfirm();    // vor einem gewollten Neustart über die Webseite: ungeprüfte Firmware gilt als gut
bool bootCheckPending();    // neue Firmware noch nicht bestätigt
const char *bootCheckRolledBack();  // Version, die zuletzt verworfen wurde, sonst ""
