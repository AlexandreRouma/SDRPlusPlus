#include "RadioHandler.h"

#define R820T_FREQ (32000000)	// R820T reference frequency
#define R820T2_IF_CARRIER (4570000)

const float BBRF103Radio::steps[BBRF103Radio::step_size] =  {
    0.0f, 0.9f, 1.4f, 2.7f, 3.7f, 7.7f, 8.7f, 12.5f, 14.4f, 15.7f,
    16.6f, 19.7f, 20.7f, 22.9f, 25.4f, 28.0f, 29.7f, 32.8f,
    33.8f, 36.4f, 37.2f, 38.6f, 40.2f, 42.1f, 43.4f, 43.9f,
    44.5f, 48.0f, 49.6f
};

const float BBRF103Radio::if_steps[BBRF103Radio::if_step_size] =  {
    -4.7f, -2.1f, 0.5f, 3.5f, 7.7f, 11.2f, 13.6f, 14.9f, 16.3f, 19.5f, 23.1f, 26.5f, 30.0f, 33.7f, 37.2f, 40.8f
};

const float BBRF103Radio::hfsteps[3] = {
    -20.0f, -10.0f, 0.0f
};

BBRF103Radio::BBRF103Radio(fx3class* fx3)
    : RadioHardware(fx3)
{
    
}

void BBRF103Radio::Initialize(uint32_t adc_rate)
{
    this->SampleRate = adc_rate;
    Fx3->Control(STARTADC, adc_rate);
}

rf_mode BBRF103Radio::PrepareLo(uint64_t freq)
{
    if (freq < 10 * 1000) return NOMODE;
    if (freq > 1750 * 1000 * 1000) return NOMODE;

    if ( freq >= this->SampleRate / 2)
        return VHFMODE;
    else
        return HFMODE;
}

bool BBRF103Radio::UpdatemodeRF(rf_mode mode)
{
    if (mode == VHFMODE)
    {
        // switch to VHF Attenna
        FX3UnsetGPIO(ATT_SEL0 | ATT_SEL1);

        // Initialize Tuner
        return Fx3->Control(TUNERINIT, (uint32_t)R820T_FREQ);
    }

    else if (mode == HFMODE )   // (mode == HFMODE || mode == VLFMODE) no more VLFMODE
    {
        // Stop Tuner
        Fx3->Control(TUNERSTDBY);

        // switch to HF Attenna
        return FX3SetGPIO(ATT_SEL0 | ATT_SEL1);
    }

    return false;
}

bool BBRF103Radio::UpdateattRF(int att)
{
    if (gpios & (ATT_SEL0 | ATT_SEL1))  {
        // this is in HF mode
        if (att > 2) att = 2;
        if (att < 0) att = 0;
        switch (att)
        {
        case 1: //11
            gpios |= ATT_SEL0 | ATT_SEL1;
            break;
        case 0: //01
            gpios |=  ATT_SEL0;
            gpios &=  ~ATT_SEL1;
            break;
        case 2:   //10
        default:
            gpios |=  ATT_SEL1;
            gpios &=  ~ATT_SEL0;
            break;
        }
        return Fx3->Control(GPIOFX3, gpios);
    }
    else {
        uint16_t index = att;
        // this is in VHF mode
        return Fx3->SetArgument(R82XX_ATTENUATOR, index);
    }
}

uint64_t BBRF103Radio::TuneLo(uint64_t freq)
{
   if (gpios & (ATT_SEL0 | ATT_SEL1))  {
        // this is in HF mode
        return 0;
    }
    else {
        // this is in VHF mode
        Fx3->Control(TUNERTUNE, freq);
        return freq - R820T2_IF_CARRIER;
    }
}

int BBRF103Radio::getRFSteps(const float** steps )
{
    if (gpios & (ATT_SEL0 | ATT_SEL1))  {
        *steps = this->hfsteps;
        return 3;
    }
    else
    {
        *steps = this->steps;
        return step_size;
    }
}

int BBRF103Radio::getIFSteps(const float** steps )
{
    if (gpios & (ATT_SEL0 | ATT_SEL1))  {
        return 0;
    }
    else
    {
        *steps = this->if_steps;
        return if_step_size;
    }
}

bool BBRF103Radio::UpdateGainIF(int attIndex)
{
    if (gpios & (ATT_SEL0 | ATT_SEL1))  {
        // this is in HF mode
        return false;
    }
    else {
        // this is in VHF mode
        return Fx3->SetArgument(R82XX_VGA, (uint16_t)attIndex);
    }
}