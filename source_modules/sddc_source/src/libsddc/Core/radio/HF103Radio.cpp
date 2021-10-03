#include "RadioHandler.h"

HF103Radio::HF103Radio(fx3class* fx3)
    : RadioHardware(fx3)
{
    // initialize steps
    for (uint8_t i = 0 ; i < step_size; i++) {
        this->steps[step_size - i - 1] = -(
            ((i & 0x01) != 0) * 0.5f +
            ((i & 0x02) != 0) * 1.0f +
            ((i & 0x04) != 0) * 2.0f +
            ((i & 0x08) != 0) * 4.0f +
            ((i & 0x010) != 0) * 8.0f +
            ((i & 0x020) != 0) * 16.0f
        );
    }
}

rf_mode HF103Radio::PrepareLo(uint64_t freq)
{
    if (freq > 32 * 1000 * 1000) return NOMODE;

    return HFMODE;
}

bool HF103Radio::UpdatemodeRF(rf_mode mode)
{
    if (mode == VHFMODE)
        return false;

    return true;
}

bool HF103Radio::UpdateattRF(int att)
{
    if (att > step_size - 1) att = step_size - 1;
    if (att < 0) att = 0;
    uint8_t d = step_size - att - 1;

    DbgPrintf("UpdateattRF %f \n", this->steps[att]);

    return Fx3->SetArgument(DAT31_ATT, d);
}

int HF103Radio::getRFSteps(const float** steps )
{
    *steps = this->steps;

    return step_size;
}
