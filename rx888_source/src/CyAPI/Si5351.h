// MIT License Copyright (c) 2017  ik1xpv@gmail.com, http://www.steila.com/blog

#ifndef SI5351_H
#define SI5351_H

#include <basetsd.h>

void Si5351init();
void si5351aSetFrequency(UINT32 freq, UINT32 freq2);
void si5351aOutputOff(UINT8 clk);


#endif
