#pragma once
#include <dsp/processor.h>
#include <dsp/loop/phase_control_loop.h>
#include <dsp/taps/windowed_sinc.h>
#include <dsp/multirate/polyphase_bank.h>
#include <dsp/math/step.h>

#define LINE_SIZE       945

#define SYNC_LEN        70
#define SYNC_SIDE_LEN   17
#define SYNC_L_START    (LINE_SIZE - SYNC_SIDE_LEN)
#define SYNC_R_START    (SYNC_LEN/2)
#define SYNC_R_END      (SYNC_R_START + (SYNC_LEN/2) + SYNC_SIDE_LEN)
#define SYNC_HALF_LEN   ((SYNC_LEN/2) + SYNC_SIDE_LEN)

#define EQUAL_LEN       35

#define HBLANK_START    SYNC_LEN
#define HBLANK_END      155
#define HBLANK_LEN      (HBLANK_END - HBLANK_START + 1)

#define SYNC_LEVEL      (-0.428)

#define COLORBURST_START    84
#define COLORBURST_LEN      33

#define MAX_LOCK    1000

dsp::complex_t PHASE_REF[2] = {
    { -0.707106781186547f,  0.707106781186547f },
    { -0.707106781186547f, -0.707106781186547f }
};

class LineSync : public dsp::Processor<float, float> {
    using base_type = dsp::Processor<float, float>;
public:
    LineSync() {}

    LineSync(dsp::stream<float>* in, double omega, double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8) { init(in, omega, omegaGain, muGain, omegaRelLimit, interpPhaseCount, interpTapCount); }

    ~LineSync() {
        if (!base_type::_block_init) { return; }
        base_type::stop();
        dsp::multirate::freePolyphaseBank(interpBank);
        dsp::buffer::free(buffer);
    }

    void init(dsp::stream<float>* in, double omega, double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 128, int interpTapCount = 8) {
        _omega = omega;
        _omegaGain = omegaGain;
        _muGain = muGain;
        _omegaRelLimit = omegaRelLimit;
        _interpPhaseCount = interpPhaseCount;
        _interpTapCount = interpTapCount;

        generateInterpTaps();
        buffer = dsp::buffer::alloc<float>(STREAM_BUFFER_SIZE + _interpTapCount);
        bufStart = &buffer[_interpTapCount - 1];

        // TODO: Needs tuning, so do the gains
        maxPeriod = (int32_t)(1.0001 * (float)(1 << 30));
        minPeriod = (int32_t)(0.9999 * (float)(1 << 30));
    
        base_type::init(in);
    }

    void setInterpParams(int interpPhaseCount, int interpTapCount) {
        assert(base_type::_block_init);
        std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
        base_type::tempStop();
        _interpPhaseCount = interpPhaseCount;
        _interpTapCount = interpTapCount;
        dsp::multirate::freePolyphaseBank(interpBank);
        dsp::buffer::free(buffer);
        generateInterpTaps();
        buffer = dsp::buffer::alloc<float>(STREAM_BUFFER_SIZE + _interpTapCount);
        bufStart = &buffer[_interpTapCount - 1];
        base_type::tempStart();
    }

    void reset() {
        assert(base_type::_block_init);
        std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
        base_type::tempStop();
        offset = 0;
        phase = 0;
        base_type::tempStart();
    }

    int run() {
        int count = base_type::_in->read();
        if (count < 0) { return -1; }

        // Copy data to work buffer
        memcpy(bufStart, base_type::_in->readBuf, count * sizeof(float));

        // Process samples while they are available
        while (offset < count) {
            // While the offset is negative, out put zeros
            while (offset < 0 && pixel < LINE_SIZE) {
                // Output a zero
                base_type::out.writeBuf[pixel++] = 0.0f;

                // Increment the phase
                phase += period;
                offset += (phase >> 30);
                phase &= 0x3FFFFFFF;
            }

            // Process as much of a line as possible
            while (offset < count && pixel < LINE_SIZE) {
                // Compute the output sample
                volk_32f_x2_dot_prod_32f(&base_type::out.writeBuf[pixel++], &buffer[offset], interpBank.phases[(phase >> 23) & 0x7F], _interpTapCount);
                
                // Increment the phase
                phase += period;
                offset += (phase >> 30);
                phase &= 0x3FFFFFFF;
            }

            // If the line is done, process it
            if (pixel == LINE_SIZE) {
                // Compute averages. (TODO: Try faster method)
                float left = 0.0f, right = 0.0f;
                int lc = 0, rc = 0;
                for (int i = SYNC_L_START; i < LINE_SIZE; i++) {
                    left += base_type::out.writeBuf[i];
                    lc++;
                }
                for (int i = 0; i < SYNC_R_START; i++) {
                    left += base_type::out.writeBuf[i];
                    lc++;
                }
                for (int i = SYNC_R_START; i < SYNC_R_END; i++) {
                    right += base_type::out.writeBuf[i];
                    rc++;
                }

                // Compute the error
                float error = (left - right) * (1.0f/((float)SYNC_HALF_LEN));

                // Compute the change in phase and frequency due to the error
                float periodDelta = error * _omegaGain;
                float phaseDelta = error * _muGain;

                // Normalize the phase delta (TODO: Make faster)
                while (phaseDelta <= -1.0f) {
                    phaseDelta += 1.0f;
                    offset--;
                }
                while (phaseDelta >= 1.0f) {
                    phaseDelta -= 1.0f;
                    offset++;
                }

                // Update the period (TODO: Clamp error*omegaGain to prevent weird shit with corrupt samples)
                period += (int32_t)(periodDelta * (float)(1 << 30));
                period = std::clamp<uint32_t>(period, minPeriod, maxPeriod);

                // Update the phase
                phase += (int32_t)(phaseDelta * (float)(1 << 30));

                // Normalize the phase
                uint32_t overflow = phase >> 30;
                if (overflow) {
                    if (error < 0) {
                        offset -= 4 - overflow;
                    }
                    else {
                        offset += overflow;
                    }
                }
                phase &= 0x3FFFFFFF;

                // Find the lowest value
                float lowest = INFINITY;
                int lowestId = -1;
                for (int i = 0; i < LINE_SIZE; i++) {
                    float val =  base_type::out.writeBuf[i];
                    if (val < lowest) {
                        lowest = val;
                        lowestId = i;
                    }
                }

                // Check the the line is in lock
                bool lineLocked = (lowestId < SYNC_R_END || lowestId >= SYNC_L_START);

                // Update the lock status based on the line lock
                if (!lineLocked && locked) {
                    locked--;
                }
                else if (lineLocked && locked < MAX_LOCK) {
                    locked++;
                }

                // If not locked, attempt to lock by forcing the sync to happen at the right spot
                // TODO: This triggers waaaay too easily at low SNR
                if (!locked && fastLock) {
                    offset += lowestId - SYNC_R_START;
                    locked = MAX_LOCK / 2;
                }

                // Output line
                if (!base_type::out.swap(LINE_SIZE)) { break; }
                pixel = 0;
            }
        }

        // Get the offset ready for the next buffer
        offset -= count;

        // Update delay buffer
        memmove(buffer, &buffer[count], (_interpTapCount - 1) * sizeof(float));

        // Swap if some data was generated
        base_type::_in->flush();
        return 0;
    }

    float syncBias = 0;

    uint32_t period = (0x800072F3 >> 1);//(1 << 31) + 1;

    int locked = 0;
    bool fastLock = true;

protected:
    void generateInterpTaps() {
        double bw = 0.5 / (double)_interpPhaseCount;
        dsp::tap<float> lp = dsp::taps::windowedSinc<float>(_interpPhaseCount * _interpTapCount, dsp::math::hzToRads(bw, 1.0), dsp::window::nuttall, _interpPhaseCount);
        interpBank = dsp::multirate::buildPolyphaseBank<float>(_interpPhaseCount, lp);
        dsp::taps::free(lp);
    }

    dsp::multirate::PolyphaseBank<float> interpBank;

    double _omega;
    double _omegaGain;
    double _muGain;
    double _omegaRelLimit;
    int _interpPhaseCount;
    int _interpTapCount;
    float* buffer;
    float* bufStart;

    uint32_t phase = 0;
    uint32_t maxPeriod;
    uint32_t minPeriod;
    float syncLevel = -0.03f;

    int offset = 0;
    int pixel = 0;
};