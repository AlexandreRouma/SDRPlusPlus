#include "RadioHandler.h"

#define ADC_FREQ (64u*1000*1000)
#define IF_FREQ (ADC_FREQ / 4)

#define HIGH_MODE 0x80
#define LOW_MODE 0x00

#define MODE HIGH_MODE

RXLucyRadio::RXLucyRadio(fx3class *fx3)
    : RadioHardware(fx3)
{
    // initialize steps
    for (uint8_t i = 0; i < if_step_size; i++) {
        this->if_steps[if_step_size - i - 1] = -(
            ((i & 0x01) != 0) * 0.5f +
            ((i & 0x02) != 0) * 1.0f +
            ((i & 0x04) != 0) * 2.0f +
            ((i & 0x08) != 0) * 4.0f +
            ((i & 0x010) != 0) * 8.0f +
            ((i & 0x020) != 0) * 16.0f
            );
    }

    for (uint8_t i = 0; i < step_size; i++)
    {
            this->steps[step_size - i - 1] = -1.0f * i;
    }
}

void RXLucyRadio::Initialize(uint32_t adc_rate)
{
    SampleRate = adc_rate;
    Fx3->Control(STARTADC, adc_rate);
}


rf_mode RXLucyRadio::PrepareLo(uint64_t freq)
{
    if (freq < 35000ll * 1000) return NOMODE;
    if (freq > 6000ll * 1000 * 1000) return NOMODE;

    if ( freq >= this->SampleRate / 2)
        return VHFMODE;
    else
        return HFMODE;
}


bool RXLucyRadio::UpdateattRF(int att)
{
    if (att > step_size - 1) att = step_size - 1;
    if (att < 0) att = 0;
    uint8_t d = step_size - att - 1;

    DbgPrintf("UpdateattRF %f \n", this->steps[att]);
    return Fx3->SetArgument(VHF_ATTENUATOR, d);
}
bool RXLucyRadio::UpdateGainIF(int att)  //HF103 now
{
    if (att > if_step_size - 1) att = if_step_size - 1;
    if (att < 0) att = 0;
    uint8_t d = if_step_size - att - 1;

    DbgPrintf("UpdateattRF %f \n", this->if_steps[att]);

    return Fx3->SetArgument(DAT31_ATT, d);
}

uint64_t RXLucyRadio::TuneLo(uint64_t freq)
{
    if (!(gpios & VHF_EN))
    {
        // this is in HF mode
        return 0;
    }
    else
    {
        Fx3->Control(TUNERTUNE, freq + IF_FREQ);

        // Set VCXO
        return freq - IF_FREQ;
    }

}
bool RXLucyRadio::UpdatemodeRF(rf_mode mode)
{
    if (mode == VHFMODE)
    {
        // switch to VHF Attenna
        FX3SetGPIO(VHF_EN);

        // Initialize VCO

        // Initialize Mixer
        return Fx3->Control(TUNERINIT, (uint32_t)0);
    }
    else if (mode == HFMODE)
    {
        Fx3->Control(TUNERSTDBY);
        return FX3UnsetGPIO(VHF_EN);                // switch to HF Attenna
    }
    return false;
}

int RXLucyRadio::getRFSteps(const float **steps)
{
    *steps = this->steps;
    return step_size;
}

int RXLucyRadio::getIFSteps(const float** steps)
{
        *steps = this->if_steps;
        return if_step_size;
}



