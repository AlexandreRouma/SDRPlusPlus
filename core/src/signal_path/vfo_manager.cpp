#include <signal_path/vfo_manager.h>
#include <signal_path/signal_path.h>

VFOManager::VFO::VFO(std::string name, int reference, float offset, float bandwidth, float sampleRate, int blockSize) {
    this->name = name;
    dspVFO = sigpath::signalPath.addVFO(name, sampleRate, bandwidth, offset);
    wtfVFO = new ImGui::WaterfallVFO;
    wtfVFO->setReference(reference);
    wtfVFO->setBandwidth(bandwidth);
    wtfVFO->setOffset(offset);
    output = dspVFO->output;
    printf("Created VFO: %p", wtfVFO);
    gui::waterfall.vfos[name] = wtfVFO;
}

VFOManager::VFO::~VFO() {
    gui::waterfall.vfos.erase(name);
    sigpath::signalPath.removeVFO(name);
    delete wtfVFO;
}

void VFOManager::VFO::setOffset(float offset) {
    wtfVFO->setOffset(offset);
    dspVFO->setOffset(wtfVFO->centerOffset);
}

void VFOManager::VFO::setCenterOffset(float offset) {
    wtfVFO->setCenterOffset(offset);
    dspVFO->setOffset(offset);
}

void VFOManager::VFO::setBandwidth(float bandwidth) {
    wtfVFO->setBandwidth(bandwidth);
    dspVFO->setBandwidth(bandwidth);
}

void VFOManager::VFO::setSampleRate(float sampleRate, float bandwidth) {
    dspVFO->setOutputSampleRate(sampleRate, bandwidth);
    wtfVFO->setBandwidth(bandwidth);
}

void VFOManager::VFO::setReference(int ref) {
    wtfVFO->setReference(ref);
}

int VFOManager::VFO::getOutputBlockSize() {
    return dspVFO->getOutputBlockSize();
}


VFOManager::VFOManager() {

}

VFOManager::VFO* VFOManager::createVFO(std::string name, int reference, float offset, float bandwidth, float sampleRate, int blockSize) {
    if (vfos.find(name) != vfos.end() || name == "") {
        return NULL;
    }
    VFOManager::VFO* vfo = new VFO(name, reference, offset, bandwidth, sampleRate, blockSize);
    vfos[name] = vfo;
    return vfo;
}

void VFOManager::deleteVFO(VFOManager::VFO* vfo) {
    std::string name = "";
    for (auto const& [_name, _vfo] : vfos) {
        if (_vfo == vfo) {
            name == _name;
            break;
        }
    }
    if (name == "") {
        return;
    }
    vfos.erase(name);
}

void VFOManager::setOffset(std::string name, float offset) {
    if (vfos.find(name) == vfos.end()) {
        return;
    }
    vfos[name]->setOffset(offset);
}

void VFOManager::setCenterOffset(std::string name, float offset) {
    if (vfos.find(name) == vfos.end()) {
        return;
    }
    vfos[name]->setCenterOffset(offset);
}

void VFOManager::setBandwidth(std::string name, float bandwidth) {
    if (vfos.find(name) == vfos.end()) {
        return;
    }
    vfos[name]->setBandwidth(bandwidth);
}

void VFOManager::setSampleRate(std::string name, float sampleRate, float bandwidth) {
    if (vfos.find(name) == vfos.end()) {
        return;
    }
    vfos[name]->setSampleRate(sampleRate, bandwidth);
}

void VFOManager::setReference(std::string name, int ref) {
    if (vfos.find(name) == vfos.end()) {
        return;
    }
    vfos[name]->setReference(ref);
}

int VFOManager::getOutputBlockSize(std::string name) {
    if (vfos.find(name) == vfos.end()) {
        return -1;
    }
    return vfos[name]->getOutputBlockSize();
}

void VFOManager::updateFromWaterfall(ImGui::WaterFall* wtf) {
    for (auto const& [name, vfo] : vfos) {
        if (vfo->wtfVFO->centerOffsetChanged) {
            spdlog::info("UH OH: Change!");
            vfo->wtfVFO->centerOffsetChanged = false;
            vfo->dspVFO->setOffset(vfo->wtfVFO->centerOffset);
        }
    }
}
