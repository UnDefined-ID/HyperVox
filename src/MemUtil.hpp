#pragma once
int processRssMB();          // resident memory of this process in MB (0 if unknown)
void releaseFreeMemory();     // hands free heap pages back to the OS
