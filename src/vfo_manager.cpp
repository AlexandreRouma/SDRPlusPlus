#include <vfo_manager.h>

namespace vfoman {
    std::map<std::string, VFO_t> vfos;
    ImGui::WaterFall* _wtf;
    SignalPath* _sigPath;

    void init(ImGui::WaterFall* wtf, SignalPath* sigPath) {
        _wtf = wtf;
        _sigPath = sigPath;
    }

    dsp::stream<dsp::complex_t>* create(std::string name, int reference, float offset, float bandwidth, float sampleRate, int blockSize) {
        if (vfos.find(name) != vfos.end()) {
            spdlog::warn("Tried to add VFO with an already existing name: {0}", name);
            return NULL;
        }
        spdlog::info("Creating new VFO '{0}'", name);
        VFO_t vfo;
        vfo.dspVFO = _sigPath->addVFO(name, sampleRate, bandwidth, offset);
        vfo.wtfVFO = new ImGui::WaterfallVFO;
        vfo.wtfVFO->setReference(reference);
        vfo.wtfVFO->setBandwidth(bandwidth);
        vfo.wtfVFO->setOffset(offset);
        _wtf->vfos[name] = vfo.wtfVFO;
        vfos[name] = vfo;
        return vfo.dspVFO->output;
    }

    void setOffset(std::string name, float offset) {
        if (vfos.find(name) == vfos.end()) {
            return;
        }
        VFO_t vfo = vfos[name];
        vfo.wtfVFO->setOffset(offset);
        vfo.dspVFO->setOffset(vfo.wtfVFO->centerOffset);
    }

    void setCenterOffset(std::string name, float offset) {
        if (vfos.find(name) == vfos.end()) {
            return;
        }
        VFO_t vfo = vfos[name];
        vfo.wtfVFO->setCenterOffset(offset);
        vfo.dspVFO->setOffset(offset);
    }

    void setBandwidth(std::string name, float bandwidth) {
        if (vfos.find(name) == vfos.end()) {
            return;
        }
        VFO_t vfo = vfos[name];
        vfo.wtfVFO->setBandwidth(bandwidth);
        vfo.dspVFO->setBandwidth(bandwidth);
    }

    void setSampleRate(std::string name, float sampleRate, float bandwidth) {
        if (vfos.find(name) == vfos.end()) {
            return;
        }
        vfos[name].dspVFO->setOutputSampleRate(sampleRate, bandwidth);
    }

    void setReference(std::string name, int ref){
        if (vfos.find(name) == vfos.end()) {
            return;
        }
        vfos[name].wtfVFO->setReference(ref);
    }

    int getOutputBlockSize(std::string name) {
        if (vfos.find(name) == vfos.end()) {
            return -1;
        }
        return vfos[name].dspVFO->getOutputBlockSize();
    }

    void remove(std::string name) {
        if (vfos.find(name) == vfos.end()) {
            return;
        }
        VFO_t vfo = vfos[name];
        _wtf->vfos.erase(name);
        _sigPath->removeVFO(name);
        delete vfo.wtfVFO;
        vfos.erase(name);
    }

    void updateFromWaterfall() {
        for (auto const& [name, vfo] : vfos) {
            if (vfo.wtfVFO->centerOffsetChanged) {
                vfo.wtfVFO->centerOffsetChanged = false;
                vfo.dspVFO->setOffset(vfo.wtfVFO->centerOffset);
            }
        }
    }
};