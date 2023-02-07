#pragma once
#include "../net.h"
#include <vector>
#include <string>
#include <functional>
#include <thread>

namespace net::rigctl {
    enum Mode {
        MODE_INVALID = -1,
        MODE_USB,
        MODE_LSB,
        MODE_CW,
        MODE_CWR,
        MODE_RTTY,
        MODE_RTTYR,
        MODE_AM,
        MODE_FM,
        MODE_WFM,
        MODE_AMS,
        MODE_PKTLSB,
        MODE_PKTUSB,
        MODE_PKTFM,
        MODE_ECSSUSB,
        MODE_ECSSLSB,
        MODE_FA,
        MODE_SAM,
        MODE_SAL,
        MODE_SAH,
        MODE_DSB
    };

    enum VFO {
        VFO_INVALID = -1,
        VFO_VFOA,
        VFO_VFOB,
        VFO_VFOC,
        VFO_CURR_VFO,
        VFO_VFO,
        VFO_MEM,
        VFO_MAIN,
        VFO_SUB,
        VFO_TX,
        VFO_RX
    };

    enum RepeaterShift {
        REPEATER_SHIFT_INVALID = -1,
        REPEATER_SHIFT_NONE,
        REPEATER_SHIFT_PLUS,
        REPEATER_SHIFT_MINUS
    };

    enum Function {
        FUNCTION_INVALID = -1,
        FUNCTION_FAGC,
        FUNCTION_NB,
        FUNCTION_COMP,
        FUNCTION_VOX,
        FUNCTION_TONE,
        FUNCTION_TSQL,
        FUNCTION_SBKIN,
        FUNCTION_FBKIN,
        FUNCTION_ANF,
        FUNCTION_NR,
        FUNCTION_AIP,
        FUNCTION_APF,
        FUNCTION_MON,
        FUNCTION_MN,
        FUNCTION_RF,
        FUNCTION_ARO,
        FUNCTION_LOCK,
        FUNCTION_MUTE,
        FUNCTION_VSC,
        FUNCTION_REV,
        FUNCTION_SQL,
        FUNCTION_ABM,
        FUNCTION_BC,
        FUNCTION_MBC,
        FUNCTION_RIT,
        FUNCTION_AFC,
        FUNCTION_SATMODE,
        FUNCTION_SCOPE,
        FUNCTION_RESUME,
        FUNCTION_TBURST,
        FUNCTION_TUNER,
        FUNCTION_XIT
    };

    enum Level {
        LEVEL_INVALID = -1,
        LEVEL_PREAMP,
        LEVEL_ATT,
        LEVEL_VOX,
        LEVEL_AF,
        LEVEL_RF,
        LEVEL_SQL,
        LEVEL_IF,
        LEVEL_APF,
        LEVEL_NR,
        LEVEL_PBT_IN,
        LEVEL_PBT_OUT,
        LEVEL_CWPITCH,
        LEVEL_RFPOWER,
        LEVEL_RFPOWER_METER,
        LEVEL_RFPOWER_METER_WATTS,
        LEVEL_MICGAIN,
        LEVEL_KEYSPD,
        LEVEL_NOTCHF,
        LEVEL_COMP,
        LEVEL_AGC,
        LEVEL_BKINDL,
        LEVEL_BAL,
        LEVEL_METER,
        LEVEL_VOXGAIN,
        LEVEL_ANTIVOX,
        LEVEL_SLOPE_LOW,
        LEVEL_SLOPE_HIGH,
        LEVEL_RAWSTR,
        LEVEL_SWR,
        LEVEL_ALC,
        LEVEL_STRENGTH
    };

    enum Param {
        PARAM_INVALID = -1,
        PARAM_ANN,
        PARAM_APO,
        PARAM_BACKLIGHT,
        PARAM_BEEP,
        PARAM_TIME,
        PARAM_BAT,
        PARAM_KEYLIGHT
    };

    enum VFOMemOp {
        VFO_MEM_OP_INVALID = -1,
        VFO_MEM_OP_CPY,
        VFO_MEM_OP_XCHG,
        VFO_MEM_OP_FROM_VFO,
        VFO_MEM_OP_TO_VFO,
        VFO_MEM_OP_MCL,
        VFO_MEM_OP_UP,
        VFO_MEM_OP_DOWN,
        VFO_MEM_OP_BAND_UP,
        VFO_MEM_OP_BAND_DOWN,
        VFO_MEM_OP_LEFT,
        VFO_MEM_OP_RIGHT,
        VFO_MEM_OP_TUNE,
        VFO_MEM_OP_TOGGLE
    };

    enum ScanFunc {
        SCAN_FUNC_INVALID = -1,
        SCAN_FUNC_STOP,
        SCAN_FUNC_MEM,
        SCAN_FUNC_SLCT,
        SCAN_FUNC_PRIO,
        SCAN_FUNC_PROG,
        SCAN_FUNC_DELTA,
        SCAN_FUNC_VFO,
        SCAN_FUNC_PLT
    };

    enum TranceiveMode {
        TRANCEIVE_MODE_INVALID = -1,
        TRANCEIVE_MODE_OFF,
        TRANCEIVE_MODE_RIG,
        TRANCEIVE_MODE_POLL
    };

    enum ResetType {
        RESET_TYPE_INVALID          = -1,
        RESET_TYPE_SOFT_RESET       = (1 << 0),
        RESET_TYPE_VFO_RESET        = (1 << 1),
        RESET_TYPE_MEM_CLEAR_RESET  = (1 << 2),
        RESET_TYPE_MASTER_RESET     = (1 << 3)
    };

    class Client {
    public:
        Client(std::shared_ptr<Socket> sock);
        ~Client();

        bool isOpen();
        void close();

        double getFreq();
        int setFreq(double freq);
        
        Mode getMode();
        int setMode(Mode mode);

        VFO getVFO();
        int setVFO(VFO vfo);

        double getRIT();
        int setRIT(double rit);

        double getXIT();
        int setXIT(double rit);

        int getPTT();
        int setPTT(bool ptt);

        // TODO: get/setSplitVFO

        double getSplitFreq();
        int setSplitFreq(double splitFreq);

        // TODO: get/setSplitMode

        int getAntenna();
        int setAntenna(int ant);

        // TODO: sendMorse

        int getDCD();

        RepeaterShift getRepeaterShift();
        int setRepeaterShift(RepeaterShift shift);

        double getRepeaterOffset();
        int setRepeaterOffset(double offset);

        double getCTCSSTone();
        int setCTCSSTone(double tone);
        
        // TODO: get/setDCSCode

        double getCTCSSSquelch();
        int setCTCSSSquelch(double squelch);

        // TODO: get/setDCSSquelch

        double getTuningStep();
        int setTuningStep(double step);

        std::vector<Function> getSupportedFunctions();
        int getFunction(Function func);
        int setFunction(Function func, bool enabled);

        std::vector<Level> getSupportedLevels();
        double getLevel(Level level);
        int setLevel(Level level, double value);

        std::vector<Param> getSupportedParams();
        double getParam(Param param);
        int setParam(Param param, double value);

        int setBank(int bank);

        int getMem();
        int setMem(int mem);

        int vfoOp(VFOMemOp op);

        int scan(ScanFunc func);

        // TODO: get/setChannel

        TranceiveMode getTranceiveMode();
        int setTranceiveMode(TranceiveMode mode);

        int reset(ResetType type);
        
    private:
        int recvStatus();

        int getInt(std::string cmd);
        int setInt(std::string cmd, int value);
        double getFloat(std::string cmd);
        int setFloat(std::string cmd, double value);
        std::string getString(std::string cmd);
        int setString(std::string cmd, std::string value);

        std::shared_ptr<Socket> sock;

    };

    class Server {
    public:
        Server(std::shared_ptr<net::Listener> listener);
        ~Server();

        bool listening();
        void stop();

        template<typename Func>
        void onGetFreq(Func func) {
            getFreqHandler = func;
        }
        template<typename Func, typename T>
        void onGetFreq(Func func, T* ctx) {
            getFreqHandler = std::bind(func, ctx, std::placeholders::_1);
        }

        template<typename Func>
        void onSetFreq(Func func) {
            setFreqHandler = func;
        }
        template<typename Func, typename T>
        void onSetFreq(Func func, T* ctx) {
            setFreqHandler = std::bind(func, ctx, std::placeholders::_1);
        }

    private:
        void listenWorker();
        void acceptWorker(std::shared_ptr<Socket> sock);
        void sendStatus(std::shared_ptr<Socket> sock, int status);
        void sendInt(std::shared_ptr<Socket> sock, int value);
        void sendFloat(std::shared_ptr<Socket> sock, double value);

        std::function<int(double&)> getFreqHandler = NULL;
        std::function<int(double)> setFreqHandler = NULL;

        std::thread listenThread;
        std::shared_ptr<net::Listener> listener;

        std::mutex socketsMtx;
        std::vector<std::shared_ptr<net::Socket>> sockets;
    };

    std::shared_ptr<Client> connect(std::string host, int port = 4532);
    std::shared_ptr<Server> listen(std::string host, int port = 4532);
}