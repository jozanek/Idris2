#pragma once

int idris2_system(const char *command);
void *idris2_copyCommand(const char *command);
int idris2_systemCopy(void *command);
