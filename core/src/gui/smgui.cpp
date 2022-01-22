#include "smgui.h"
#include "style.h"
#include <options.h>
#include <gui/widgets/stepped_slider.h>
#include <gui/gui.h>

namespace SmGui {
    std::map<FormatString, const char*> fmtStr = {
        { FMT_STR_NONE, "" },
        { FMT_STR_INT_DEFAULT, "%d" },
        { FMT_STR_INT_DB, "%d dB" },
        { FMT_STR_FLOAT_DEFAULT, "%f" },
        { FMT_STR_FLOAT_NO_DECIMAL, "%.0f" },
        { FMT_STR_FLOAT_ONE_DECIMAL, "%.1f" },
        { FMT_STR_FLOAT_TWO_DECIMAL, "%.2f" },
        { FMT_STR_FLOAT_THREE_DECIMAL, "%.3f" },
        { FMT_STR_FLOAT_DB_NO_DECIMAL, "%.0f dB" },
        { FMT_STR_FLOAT_DB_ONE_DECIMAL, "%.1f dB" },
        { FMT_STR_FLOAT_DB_TWO_DECIMAL, "%.2f dB" },
        { FMT_STR_FLOAT_DB_THREE_DECIMAL, "%.3f dB" }
    };

    DrawList* rdl = NULL;
    bool forceSyncForNext = false;
    std::string diffId = "";
    DrawListElem diffValue;
    bool nextItemFillWidth = false;

    std::string ImStrToString(const char* imstr) {
        int len = 0;
        const char* end = imstr;
        while (*end) { end += strlen(end) + 1; }
        return std::string(imstr, end);
    }

    // Rec/Play functions
    void setDiff(std::string id, SmGui::DrawListElem value) {
        diffId = id;
        diffValue = value;
    }

    // TODO: Add getDiff function for client

    void startRecord(DrawList* dl) {
        rdl = dl;
    }

    void stopRecord() {
        rdl = NULL;
    }

    #define SET_DIFF_VOID(id)       diffId = id; syncRequired = elem.forceSync;
    #define SET_DIFF_BOOL(id, bo)   diffId = id; diffValue.type = DRAW_LIST_ELEM_TYPE_BOOL; diffValue.b = bo; syncRequired = elem.forceSync;
    #define SET_DIFF_INT(id, in)    diffId = id; diffValue.type = DRAW_LIST_ELEM_TYPE_INT; diffValue.i = in; syncRequired = elem.forceSync;
    #define SET_DIFF_FLOAT(id, fl)  diffId = id; diffValue.type = DRAW_LIST_ELEM_TYPE_FLOAT; diffValue.f = fl; syncRequired = elem.forceSync;
    #define SET_DIFF_STRING(id, st) diffId = id; diffValue.type = DRAW_LIST_ELEM_TYPE_STRING; diffValue.str = st; syncRequired = elem.forceSync;

    void DrawList::draw(std::string& diffId, DrawListElem& diffValue, bool& syncRequired) {
        int elemCount = elements.size();
        nextItemFillWidth = false;

        for (int i = 0; i < elemCount;) {
            // Get element
            DrawListElem& elem = elements[i++];
            if (elem.type != DRAW_LIST_ELEM_TYPE_DRAW_STEP) { continue; }

            // Format calls
            if (elem.step == DRAW_STEP_FILL_WIDTH) { FillWidth(); }
            else if (elem.step == DRAW_STEP_SAME_LINE) { SameLine(); }
            else if (elem.step == DRAW_STEP_BEGIN_DISABLED) { BeginDisabled(); }
            else if (elem.step == DRAW_STEP_END_DISABLED) { EndDisabled(); }

            // Widget Calls
            else if (elem.step == DRAW_STEP_COMBO) {
                if (Combo(elements[i].str.c_str(), &elements[i+1].i, elements[i+2].str.c_str(), elements[i+3].i)) {
                    SET_DIFF_INT(elements[i].str, elements[i+1].i);
                }
                i += 4;
            }
            else if (elem.step == DRAW_STEP_BUTTON) {
                if (Button(elements[i].str.c_str(), ImVec2(elements[i+1].f, elements[i+2].f))) {
                    SET_DIFF_VOID(elements[i].str);
                }
                i += 3;
            }
            else if (elem.step == DRAW_STEP_COLUMNS) {
                Columns(elements[i].i, elements[i+1].str.c_str(), elements[i+2].b);
                i += 3;
            }
            else if (elem.step == DRAW_STEP_NEXT_COLUMN) { NextColumn(); }
            else if (elem.step == DRAW_STEP_RADIO_BUTTON) {
                if (RadioButton(elements[i].str.c_str(), elements[i+1].b)) {
                    SET_DIFF_VOID(elements[i].str);
                }
                i += 2;
            }
            else if (elem.step == DRAW_STEP_BEGIN_GROUP) { BeginGroup(); }
            else if (elem.step == DRAW_STEP_END_GROUP) { EndGroup(); }
            else if (elem.step == DRAW_STEP_LEFT_LABEL) {
                LeftLabel(elements[i].str.c_str());
                i++;
            }
            else if (elem.step == DRAW_STEP_SLIDER_INT) {
                if (SliderInt(elements[i].str.c_str(), &elements[i+1].i, elements[i+2].i, elements[i+3].i, (FormatString)elements[i+4].i, elements[i+5].i)) {
                    SET_DIFF_INT(elements[i].str, elements[i+1].i);
                }
                i += 6;
            }
            else if (elem.step == DRAW_STEP_SLIDER_FLOAT_WITH_STEPS) {
                if (SliderFloatWithSteps(elements[i].str.c_str(), &elements[i+1].f, elements[i+2].f, elements[i+3].f, elements[i+4].f, (FormatString)elements[i+5].i)) {
                    SET_DIFF_FLOAT(elements[i].str, elements[i+1].f);
                }
                i += 6;
            }
            else if (elem.step == DRAW_STEP_INPUT_INT) {
                if (InputInt(elements[i].str.c_str(), &elements[i+1].i, elements[i+2].i, elements[i+3].i, elements[i+4].i)) {
                    SET_DIFF_INT(elements[i].str, elements[i+1].i);
                }
                i += 5;
            }
            else if (elem.step == DRAW_STEP_CHECKBOX) {
                if (Checkbox(elements[i].str.c_str(), &elements[i+1].b)) {
                    SET_DIFF_BOOL(elements[i].str, elements[i+1].b);
                }
                i += 2;
            }
            else if (elem.step == DRAW_STEP_SLIDER_FLOAT) {
                if (SliderFloat(elements[i].str.c_str(), &elements[i+1].f, elements[i+2].f, elements[i+3].f, (FormatString)elements[i+4].i, elements[i+5].i)) {
                    SET_DIFF_FLOAT(elements[i].str, elements[i+1].f);
                }
                i += 6;
            }
            else if (elem.step == DRAW_STEP_INPUT_TEXT) {
                char tmpBuf[4096];
                strcpy(tmpBuf, elements[i+1].str.c_str());
                if (InputText(elements[i].str.c_str(), tmpBuf, elements[i+2].i, elements[i+3].i)) {
                    elements[i+1].str = tmpBuf;
                    SET_DIFF_STRING(elements[i].str, tmpBuf);
                }
                i += 4;
            }
            else if (elem.step == DRAW_STEP_TEXT) {
                Text(elements[i].str.c_str());
                i++;
            }
            else if (elem.step == DRAW_STEP_TEXT_COLORED) {
                TextColored(ImVec4(elements[i].f, elements[i+1].f, elements[i+2].f, elements[i+3].f), elements[i+4].str.c_str());
                i += 5;
            }
            else if (elem.step == DRAW_STEP_OPEN_POPUP) {
                OpenPopup(elements[i].str.c_str(), elements[i+1].i);
                i += 2;
            }
            else if (elem.step == DRAW_STEP_BEGIN_POPUP) {
                gui::mainWindow.lockWaterfallControls = true;
                if (!BeginPopup(elements[i].str.c_str(), elements[i+1].i)) {
                    i += 2;
                    while (i < elemCount && !(elements[i].type == DRAW_LIST_ELEM_TYPE_DRAW_STEP && elements[i].step == DRAW_STEP_END_POPUP)) { i++; }
                    i++;
                }
                else {
                    i += 2;
                }
            }
            else if (elem.step == DRAW_STEP_END_POPUP) {
                EndPopup();
            }
            else if (elem.step == DRAW_STEP_BEGIN_TABLE) {
                if (!BeginTable(elements[i].str.c_str(), elements[i+1].i, elements[i+2].i, ImVec2(elements[i+3].f, elements[i+4].f), elements[i+5].f)) {
                    i += 6;
                    while (i < elemCount && !(elements[i].type == DRAW_LIST_ELEM_TYPE_DRAW_STEP && elements[i].step == DRAW_STEP_END_TABLE)) { i++; }
                    i++;
                }
                else {
                    i += 6;
                }
            }
            else if (elem.step == DRAW_STEP_END_TABLE) {
                EndTable();
            }
            else if (elem.step == DRAW_STEP_TABLE_NEXT_ROW) {
                TableNextRow(elements[i].i, elements[i+1].f);
                i += 2;
            }
            else if (elem.step == DRAW_STEP_TABLE_SET_COLUMN_INDEX) {
                TableSetColumnIndex(elements[i].i);
                i++;
            }
            else if (elem.step == DRAW_STEP_SET_NEXT_ITEM_WIDTH) {
                SetNextItemWidth(elements[i].f);
                i++;
            }
            else {
                spdlog::error("Invalid widget in Drawlist");
            }

            if (elem.step != DRAW_STEP_FILL_WIDTH) { nextItemFillWidth = false; }
        }
    }

    // Drawlist stuff
    void DrawList::pushStep(DrawStep step, bool forceSync) {
        DrawListElem elem;
        elem.type = DRAW_LIST_ELEM_TYPE_DRAW_STEP;
        elem.step = step;
        elem.forceSync = forceSync;
        elements.push_back(elem);
    }

    void DrawList::pushBool(bool b) {
        DrawListElem elem;
        elem.type = DRAW_LIST_ELEM_TYPE_BOOL;
        elem.b = b;
        elements.push_back(elem);
    }
    
    void DrawList::pushInt(int i) {
        DrawListElem elem;
        elem.type = DRAW_LIST_ELEM_TYPE_INT;
        elem.i = i;
        elements.push_back(elem);
    }
    
    void DrawList::pushFloat(float f) {
        DrawListElem elem;
        elem.type = DRAW_LIST_ELEM_TYPE_FLOAT;
        elem.f = f;
        elements.push_back(elem);
    }
    
    void DrawList::pushString(std::string str) {
        DrawListElem elem;
        elem.type = DRAW_LIST_ELEM_TYPE_STRING;
        elem.str = str;
        elements.push_back(elem);
    }

    int DrawList::loadItem(DrawListElem& elem, uint8_t* data, int len) {
        // Get type
        int i = 0;
        elem.type = (DrawListElemType)data[i++];

        // Read data depending on type
        if (elem.type == DRAW_LIST_ELEM_TYPE_DRAW_STEP && len >= 2) {
            elem.step = (DrawStep)data[i++];
            elem.forceSync = data[i++];
        }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_BOOL && len-- >= 1) {
            elem.b = data[i++];
        }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_INT && len >= 4) {
            elem.i = *(int*)&data[i];
            i += 4;
        }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_FLOAT && len >= 4) {
            elem.f = *(float*)&data[i];
            i += 4;
        }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_STRING && len >= 2) {
            uint16_t slen = *(uint16_t*)&data[i];
            if (len < slen + 2) { return -1; }
            elem.str = std::string(&data[i + 2], &data[i + 2 + slen]);
            i += slen + 2;
        }
        else {
            return -1;
        }

        return i;
    }
    
    int DrawList::load(void* data, int len) {
        uint8_t* buf = (uint8_t*)data;
        elements.clear();
        int i = 0;

        // Load all entries
        while (len > 0) {
            // Load entry type
            DrawListElem elem;
            int consumed = loadItem(elem, &buf[i], len);
            if (consumed < 0) { return -1; }
            i += consumed;
            len -= consumed;

            // Add element to list
            elements.push_back(elem);
        }

        // Validate and clear if invalid
        if (!validate()) {
            spdlog::error("Drawlist validation failed");
            //elements.clear();
            return -1;
        }

        return i;
    }

    int DrawList::storeItem(DrawListElem& elem, void* data, int len) {
        // Check size requirement
        uint8_t* buf = (uint8_t*)data;
        if (len < 1) { return -1; }
        int i = 0;
        len--;

        // Save entry type
        buf[i++] = elem.type;

        // Check type and save data accordingly            
        if (elem.type == DRAW_LIST_ELEM_TYPE_DRAW_STEP && len >= 2) {
            buf[i++] = elem.step;
            buf[i++] = elem.forceSync;
        }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_BOOL && len >= 1) {
            buf[i++] = elem.b;
        }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_INT && len >= 4) {
            *(int*)&buf[i] = elem.i;
            i += 4;
        }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_FLOAT && len >= 4) {
            *(float*)&buf[i] = elem.f;
            i += 4;
        }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_STRING) {
            int slen = elem.str.size();
            if (len < slen + 2) { return -1; }
            *(uint16_t*)&buf[i] = slen;
            memcpy(&buf[i + 2], elem.str.c_str(), slen);
            i += slen + 2;
        }
        else {
            return -1;
        }

        return i;
    }

    int DrawList::store(void* data, int len) {
        uint8_t* buf = (uint8_t*)data;
        int i = 0;

        // Iterate through all element and write the data in the buffer
        for (auto& elem : elements) {
            int count = storeItem(elem, &buf[i], len);
            if (count < 0) { return -1; }
            i += count;
            len -= count;
        }

        return i;
    }

    int DrawList::getItemSize(DrawListElem& elem) {
        if (elem.type == DRAW_LIST_ELEM_TYPE_DRAW_STEP) { return 3; }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_BOOL) { return 2; }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_INT) { return 5; }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_FLOAT) { return 5; }
        else if (elem.type == DRAW_LIST_ELEM_TYPE_STRING) { return 3 + elem.str.size(); }
        return -1;
    }

    int DrawList::getSize() {
        int size = 0;

        // Iterate through all element to add up the total size
        for (auto& elem : elements) {
            size += getItemSize(elem);
        }

        return size;
    }

    bool DrawList::checkTypes(int firstId, int n, ...) {
        va_list args;
        va_start(args, n);

        // Check if enough elements are left
        if (firstId + n > elements.size()) {
            va_end(args);
            return false;
        }

        // Check the type of each element
        for (int i = 0; i < n; i++) {
            if (va_arg(args, int) != (int)elements[firstId + i].type) {
                va_end(args);
                return false;
            }
        }

        va_end(args);
        return true;
    }

    #define VALIDATE_WIDGET(n, ws, ...)     if (step == ws) { if (!checkTypes(i, n, __VA_ARGS__)) { return false; }; i += n; }
    #define E_VALIDATE_WIDGET(n, ws, ...)   else VALIDATE_WIDGET(n, ws, __VA_ARGS__)

    bool DrawList::validate() {
        int count = elements.size();
        for (int i = 0; i < count;) {
            if (elements[i].type != DRAW_LIST_ELEM_TYPE_DRAW_STEP) { return false; }
            DrawStep step = elements[i++].step;

            VALIDATE_WIDGET(4, DRAW_STEP_COMBO, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_INT)
            
            E_VALIDATE_WIDGET(3, DRAW_STEP_BUTTON, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_FLOAT)
            
            E_VALIDATE_WIDGET(3, DRAW_STEP_COLUMNS, DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_BOOL)
            
            E_VALIDATE_WIDGET(2, DRAW_STEP_RADIO_BUTTON, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_BOOL)
           
            E_VALIDATE_WIDGET(1, DRAW_STEP_LEFT_LABEL, DRAW_LIST_ELEM_TYPE_STRING)
            
            E_VALIDATE_WIDGET(6, DRAW_STEP_SLIDER_INT, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_INT,
                                                        DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_INT)
            
            E_VALIDATE_WIDGET(6, DRAW_STEP_SLIDER_FLOAT_WITH_STEPS, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_FLOAT,
                                                        DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_INT)
            
            E_VALIDATE_WIDGET(5, DRAW_STEP_INPUT_INT, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_INT,
                                                        DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_INT)
            
            E_VALIDATE_WIDGET(2, DRAW_STEP_CHECKBOX, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_BOOL)
            
            E_VALIDATE_WIDGET(6, DRAW_STEP_SLIDER_FLOAT, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_FLOAT,
                                                        DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_INT)
            
            E_VALIDATE_WIDGET(4, DRAW_STEP_INPUT_TEXT, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_INT)
            
            E_VALIDATE_WIDGET(1, DRAW_STEP_TEXT, DRAW_LIST_ELEM_TYPE_STRING)
            
            E_VALIDATE_WIDGET(5, DRAW_STEP_TEXT_COLORED, DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_FLOAT,
                                                DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_STRING)
            
            E_VALIDATE_WIDGET(2, DRAW_STEP_OPEN_POPUP, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_INT)
           
            E_VALIDATE_WIDGET(2, DRAW_STEP_BEGIN_POPUP, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_INT)
            
            E_VALIDATE_WIDGET(6, DRAW_STEP_BEGIN_TABLE, DRAW_LIST_ELEM_TYPE_STRING, DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_INT,
                                                        DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_FLOAT, DRAW_LIST_ELEM_TYPE_FLOAT)
            E_VALIDATE_WIDGET(2, DRAW_STEP_TABLE_NEXT_ROW, DRAW_LIST_ELEM_TYPE_INT, DRAW_LIST_ELEM_TYPE_FLOAT)

            E_VALIDATE_WIDGET(1, DRAW_STEP_TABLE_SET_COLUMN_INDEX, DRAW_LIST_ELEM_TYPE_INT)

            E_VALIDATE_WIDGET(1, DRAW_STEP_SET_NEXT_ITEM_WIDTH, DRAW_LIST_ELEM_TYPE_FLOAT)
        }

        return true;
    }

    // Signaling functions
    void ForceSync() {
        forceSyncForNext = true;
    }

    // Format functions
    void FillWidth() {
        if (!options::opts.serverMode) {
            nextItemFillWidth = true;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
            return;
        }
        if (rdl) { rdl->pushStep(DRAW_STEP_FILL_WIDTH, false); }
    }

    void SameLine() {
        if (!options::opts.serverMode) { ImGui::SameLine(); return; }
        if (rdl) { rdl->pushStep(DRAW_STEP_SAME_LINE, false); }
    }

    void BeginDisabled() {
        if (!options::opts.serverMode) { style::beginDisabled(); return; }
        if (rdl) { rdl->pushStep(DRAW_STEP_BEGIN_DISABLED, false); }
    }

    void EndDisabled() {
        if (!options::opts.serverMode) { style::endDisabled(); return; }
        if (rdl) { rdl->pushStep(DRAW_STEP_END_DISABLED, false); }
    }


    // Widget functions
    bool Combo(const char *label, int *current_item, const char *items_separated_by_zeros, int popup_max_height_in_items) {
        if (!options::opts.serverMode) { return ImGui::Combo(label, current_item, items_separated_by_zeros, popup_max_height_in_items); }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_COMBO, forceSyncForNext);
            rdl->pushString(label);
            rdl->pushInt(*current_item);
            rdl->pushString(ImStrToString(items_separated_by_zeros));
            rdl->pushInt(popup_max_height_in_items);
            forceSyncForNext = false;
        }
        if (diffId == label && diffValue.type == DRAW_LIST_ELEM_TYPE_INT) {
            *current_item = diffValue.i;
            return true;
        }
        return false;
    }

    bool Button(const char *label, ImVec2 size) {
        if (!options::opts.serverMode) {
            if (nextItemFillWidth) { size.x = ImGui::GetContentRegionAvailWidth(); }
            return ImGui::Button(label, size);
        }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_BUTTON, forceSyncForNext);
            rdl->pushString(label);
            rdl->pushFloat(size.x);
            rdl->pushFloat(size.y);
            forceSyncForNext = false;
        }
        return (diffId == label);
    }

    void Columns(int count, const char *id, bool border) {
        if (!options::opts.serverMode) { ImGui::Columns(count, id, border); return; }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_COLUMNS, forceSyncForNext);
            rdl->pushInt(count);
            rdl->pushString(id);
            rdl->pushBool(border);
            forceSyncForNext = false;
        }
    }
    
    void NextColumn() {
        if (!options::opts.serverMode) { ImGui::NextColumn(); return; }
        if (rdl) { rdl->pushStep(DRAW_STEP_NEXT_COLUMN, false); }
    }
    
    bool RadioButton(const char *label, bool active) {
        if (!options::opts.serverMode) { return ImGui::RadioButton(label, active); }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_RADIO_BUTTON, forceSyncForNext);
            rdl->pushString(label);
            rdl->pushBool(active);
            forceSyncForNext = false;
        }
        return (diffId == label);
    }
    
    void BeginGroup() {
        if (!options::opts.serverMode) { ImGui::BeginGroup(); return; }
        if (rdl) { rdl->pushStep(DRAW_STEP_BEGIN_GROUP, false); }
    }
    
    void EndGroup() {
        if (!options::opts.serverMode) { ImGui::EndGroup(); return; }
        if (rdl) { rdl->pushStep(DRAW_STEP_END_GROUP, false); }
    }
    
    void LeftLabel(const char *text) {
        if (!options::opts.serverMode) { ImGui::LeftLabel(text); return; }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_LEFT_LABEL, forceSyncForNext);
            rdl->pushString(text);
            forceSyncForNext = false;
        }
    }
    
    bool SliderInt(const char *label, int *v, int v_min, int v_max, FormatString format, ImGuiSliderFlags flags) {
        if (!options::opts.serverMode) { return ImGui::SliderInt(label, v, v_min, v_max, fmtStr[format], flags); }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_SLIDER_INT, forceSyncForNext);
            rdl->pushString(label);
            rdl->pushInt(*v);
            rdl->pushInt(v_min);
            rdl->pushInt(v_max);
            rdl->pushInt(format);
            rdl->pushInt(flags);
            forceSyncForNext = false;
        }
        if (diffId == label && diffValue.type == DRAW_LIST_ELEM_TYPE_INT) {
            *v = diffValue.i;
            return true;
        }
        return false;
    }

    bool SliderFloatWithSteps(const char *label, float *v, float v_min, float v_max, float v_step, FormatString display_format) {
        if (!options::opts.serverMode) { return ImGui::SliderFloatWithSteps(label, v, v_min, v_max, v_step, fmtStr[display_format]); }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_SLIDER_FLOAT_WITH_STEPS, forceSyncForNext);
            rdl->pushString(label);
            rdl->pushFloat(*v);
            rdl->pushFloat(v_min);
            rdl->pushFloat(v_max);
            rdl->pushFloat(v_step);
            rdl->pushInt(display_format);
            forceSyncForNext = false;
        }
        if (diffId == label && diffValue.type == DRAW_LIST_ELEM_TYPE_FLOAT) {
            *v = diffValue.f;
            return true;
        }
        return false;
    }

    bool InputInt(const char *label, int *v, int step, int step_fast, ImGuiInputTextFlags flags) {
        if (!options::opts.serverMode) { return ImGui::InputInt(label, v, step, step_fast, flags); }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_INPUT_INT, forceSyncForNext);
            rdl->pushString(label);
            rdl->pushInt(*v);
            rdl->pushInt(step);
            rdl->pushInt(step_fast);
            rdl->pushInt(flags);
            forceSyncForNext = false;
        }
        if (diffId == label && diffValue.type == DRAW_LIST_ELEM_TYPE_INT) {
            *v = diffValue.i;
            return true;
        }
        return false;
    }
    
    bool Checkbox(const char *label, bool *v) {
        if (!options::opts.serverMode) { return ImGui::Checkbox(label, v); }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_CHECKBOX, forceSyncForNext);
            rdl->pushString(label);
            rdl->pushBool(*v);
            forceSyncForNext = false;
        }
        if (diffId == label && diffValue.type == DRAW_LIST_ELEM_TYPE_BOOL) {
            *v = diffValue.b;
            return true;
        }
        return false;
    }

    bool SliderFloat(const char *label, float *v, float v_min, float v_max, FormatString format, ImGuiSliderFlags flags) {
        if (!options::opts.serverMode) { return ImGui::SliderFloat(label, v, v_min, v_max, fmtStr[format], flags); }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_SLIDER_FLOAT, forceSyncForNext);
            rdl->pushString(label);
            rdl->pushFloat(*v);
            rdl->pushFloat(v_min);
            rdl->pushFloat(v_max);
            rdl->pushInt(format);
            rdl->pushInt(flags);
            forceSyncForNext = false;
        }
        if (diffId == label && diffValue.type == DRAW_LIST_ELEM_TYPE_FLOAT) {
            *v = diffValue.f;
            return true;
        }
        return false;
    }

    bool InputText(const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags) {
        if (!options::opts.serverMode) { return ImGui::InputText(label, buf, buf_size, flags); }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_INPUT_TEXT, forceSyncForNext);
            rdl->pushString(label);
            rdl->pushString(buf);
            rdl->pushInt(buf_size);
            rdl->pushInt(flags);
            forceSyncForNext = false;
        }
        if (diffId == label && diffValue.type == DRAW_LIST_ELEM_TYPE_STRING && diffValue.str.size() <= buf_size) {
            strcpy(buf, diffValue.str.c_str());
            return true;
        }
        return false;
    }

    void Text(const char* str) {
        if (!options::opts.serverMode) { ImGui::Text("%s", str); return; }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_TEXT, false);
            rdl->pushString(str);
        }
    }

    void TextColored(const ImVec4 &col, const char *str) {
        if (!options::opts.serverMode) { ImGui::TextColored(col, "%s", str); return; }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_TEXT_COLORED, false);
            rdl->pushFloat(col.x);
            rdl->pushFloat(col.y);
            rdl->pushFloat(col.z);
            rdl->pushFloat(col.w);
            rdl->pushString(str);
        }
    }

    void OpenPopup(const char *str_id, ImGuiPopupFlags popup_flags) {
        if (!options::opts.serverMode) { ImGui::OpenPopup(str_id, popup_flags); return; }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_OPEN_POPUP, false);
            rdl->pushString(str_id);
            rdl->pushInt(popup_flags);
        }
    }

    bool BeginPopup(const char *str_id, ImGuiWindowFlags flags) {
        if (!options::opts.serverMode) { return ImGui::BeginPopup(str_id, flags); }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_BEGIN_POPUP, false);
            rdl->pushString(str_id);
            rdl->pushInt(flags);
        }
        return true;
    }

    void EndPopup() {
        if (!options::opts.serverMode) { ImGui::EndPopup(); return; }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_END_POPUP, false);
        }
    }

    bool BeginTable(const char *str_id, int column, ImGuiTableFlags flags, const ImVec2 &outer_size, float inner_width) {
        if (!options::opts.serverMode) { return ImGui::BeginTable(str_id, column, flags, outer_size, inner_width); }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_BEGIN_TABLE, false);
            rdl->pushString(str_id);
            rdl->pushInt(column);
            rdl->pushInt(flags);
            rdl->pushFloat(outer_size.x);
            rdl->pushFloat(outer_size.y);
            rdl->pushFloat(inner_width);
        }
        return true;
    }

    void EndTable() {
        if (!options::opts.serverMode) { ImGui::EndTable(); return; }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_END_TABLE, false);
        }
    }

    void TableNextRow(ImGuiTableRowFlags row_flags, float min_row_height) {
        if (!options::opts.serverMode) { ImGui::TableNextRow(row_flags, min_row_height); return; }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_TABLE_NEXT_ROW, false);
            rdl->pushInt(row_flags);
            rdl->pushFloat(min_row_height);
        }
    }

    void TableSetColumnIndex(int column_n) {
        if (!options::opts.serverMode) { ImGui::TableSetColumnIndex(column_n); return; }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_TABLE_SET_COLUMN_INDEX, false);
            rdl->pushInt(column_n);
        }
    }
    
    void SetNextItemWidth(float item_width) {
        if (!options::opts.serverMode) { ImGui::SetNextItemWidth(item_width); return; }
        if (rdl) {
            rdl->pushStep(DRAW_STEP_SET_NEXT_ITEM_WIDTH, false);
            rdl->pushFloat(item_width);
        }
    }

    // Config configs
    void ForceSyncForNext() {
        forceSyncForNext = true;
    }
}
