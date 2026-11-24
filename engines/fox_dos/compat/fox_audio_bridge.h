#pragma once
void fox_opl_write(int,int);
void OPL_SetSampleRate(int);
int OPL_Init(unsigned int);
void OPL_SetCallback(unsigned long,void(*)(void*),void*);
void OPL_Shutdown(void);

void fox_opl_bus(int);
