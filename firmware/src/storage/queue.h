#pragma once
#include "core/queue_core.h"

// Warteschlange auf LittleFS (/queue.jsonl), threadsicher. Logik siehe core/queue_core.
void queueBegin();
bool queueAdd(const QueueEntry &e);
int queueCount();
bool queuePeek(QueueEntry &out);  // ältester Eintrag
void queuePopFront();
void queueClear();
uint32_t queueDropped();          // wegen dauerhafter Fehler (HTTP 4xx) verworfene Einträge seit dem Start
void queueCountDropped();
