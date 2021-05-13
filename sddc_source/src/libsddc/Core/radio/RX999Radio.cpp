#include "RadioHandler.h"

#define ADC_FREQ (128u*1000*1000)
#define IF_FREQ (ADC_FREQ / 4)

#define HIGH_MODE 0x80
#define LOW_MODE 0x00

#define MODE HIGH_MODE

RX999Radio::RX999Radio(fx3class *fx3)
    : RadioHardware(fx3)
{
    // high mode gain = 0.409, start=-30
    // low mode gain = 0.059, start = -30
#if (MODE == HIGH_MODE)
    float ratio = 0.409f;
#else
    float ratio = 0.059f;
#endif
    for (uint8_t i = 0; i < if_step_size; i++)
    {
        this->if_steps[i] = -30.0f + ratio * (i + 1);
    }
}

void RX999Radio::Initialize(uint32_t adc_rate)
{
    SampleRate = adc_rate;
    Fx3->Control(STARTADC, adc_rate);
}


rf_mode RX999Radio::PrepareLo(uint64_t freq)
{
    if (freq < 10 * 1000) return NOMODE;
    if (freq > 6000ll * 1000 * 1000) return NOMODE;

    if ( freq >= this->SampleRate / 2)
        return VHFMODE;
    else
        return HFMODE;
}

bool RX999Radio::UpdatemodeRF(rf_mode mode)
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


bool RX999Radio::UpdateattRF(int att)
{
    return false;
}

uint64_t RX999Radio::TuneLo(uint64_t freq)
{
    if (!(gpios & VHF_EN))
    {
        // this is in HF mode
        return 0;
    }
    else
    {
        int sel;
        // set preselector
        if (freq <= 120*1000*1000) sel = 0b111;
        else if (freq <= 250*1000*1000) sel = 0b101;
        else if (freq <= 300*1000*1000) sel = 0b110;
        else if (freq <= 380*1000*1000) sel = 0b100;
        else if (freq <= 500*1000*1000) sel = 0b000;
        else if (freq <= 1000ll*1000*1000) sel = 0b010;
        else if (freq <= 2000ll*1000*1000) sel = 0b001;
        else sel = 0b011;

        Fx3->Control(TUNERTUNE, freq + IF_FREQ);

        Fx3->SetArgument(PRESELECTOR, sel);
        // Set VCXO
        return freq - IF_FREQ;
    }
}

int RX999Radio::getRFSteps(const float **steps)
{
    return 0;
}

int RX999Radio::getIFSteps(const float **steps)
{
    *steps = this->if_steps;
    return if_step_size;
}

bool RX999Radio::UpdateGainIF(int gain_index)
{
    uint8_t gain = MODE | (gain_index + 1);

    DbgPrintf("UpdateGainIF %d \n", gain);

    return Fx3->SetArgument(AD8340_VGA, gain);
}
