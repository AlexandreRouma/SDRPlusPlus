#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <core.h>
#include <radio_interface.h>
#include <options.h>
#include <spdlog/spdlog.h>
#include <signal_path/signal_path.h>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
	#define strcpy strcpy_s
#endif


SDRPP_MOD_INFO{
	/* Name:            */ "frequency_manager",
	/* Description:     */ "Basic frequency manager module for SDR++",
	/* Author:          */ "zimm",
	/* Version:         */ 0, 0, 1,
	/* Max instances    */ 1
};

static ConfigManager config;

class FreqManModule : public ModuleManager::Instance {
public:
	FreqManModule(std::string name) {
		this->name = name;

		CheckConfigFileIntegrity();
		config.aquire();
		if (config.conf.size() > 0)
		{
			for (const auto& item : config.conf.items())
			{
				bookmarks.push_back((std::string)item.key());
				if (config.conf[item.key()].contains("category"))
					if (!VectorArrayContainsString(categories, (std::string)config.conf[item.key()]["category"]))
						categories.push_back((std::string)config.conf[item.key()]["category"]);
			}

			std::sort(categories.begin(), categories.end());
			selected_category = categories[0];

			for (const auto& i : bookmarks)
			{
				if (config.conf[i].contains("category") && ((std::string)config.conf[i]["category"] == selected_category))
					displayed_bookmarks.push_back(i);
			}
		}
		config.release();
		std::sort(displayed_bookmarks.begin(), displayed_bookmarks.end());
		
		// add collapsable header
		gui::menu.registerEntry(name, menuHandler, this);
	}

	~FreqManModule() {

	}

	void enable() {
		enabled = true;
	}

	void disable() {
		enabled = false;
	}

	bool isEnabled() {
		return enabled;
	}

private:

	static void CheckConfigFileIntegrity()
	{
		config.aquire();
		bool modified = false;
		if (config.conf.size() > 0)
		{
			for (const auto& item : config.conf.items())
			{
				if ((!config.conf[item.key()].contains("category") || !config.conf[item.key()]["category"].is_string()) ||
					(!config.conf[item.key()].contains("frequency") || !config.conf[item.key()]["frequency"].is_number()) ||
					(!config.conf[item.key()].contains("mode") || !config.conf[item.key()]["mode"].is_string()) ||
					(!config.conf[item.key()].contains("bandwidth") || !config.conf[item.key()]["bandwidth"].is_number())
					)
				{
					spdlog::warn("Frequency Manager: Invalid entry found in config file, removing '{0}'.", (std::string)item.key());
					config.conf.erase(item.key());
					modified = true;
				}
			}
		}
		config.release(modified);
	}

	static std::string GetActiveVFOName()
	{
		return gui::waterfall.selectedVFO;
	}

	static void GetActiveVFOFrequency(double *frequency)
	{
		*frequency = (double)gui::freqSelect.frequency;
	}

	static std::string GetActiveVFOFrequencyShort(double freq)
	{
		std::string ret_val = "";
		std::stringstream stream;

		if (freq < 1000000)
		{
			stream << std::fixed << std::setprecision(3) << freq / 1000.0;
			ret_val = stream.str();
			ret_val += " kHz";
		}
		else if (freq >= 1000000000)
		{
			stream << std::fixed << std::setprecision(3) << freq / 1000000000.0;
			ret_val = stream.str();
			ret_val += " GHz";
		}
		else
		{
			stream << std::fixed << std::setprecision(3) << freq / 1000000.0;
			ret_val = stream.str();
			ret_val += " MHz";
		}
		
		return ret_val;
	}

	static void GetActiveVFOBandwidth(int *bw)
	{
		if (GetActiveVFOName() != "")
			*bw = (int)sigpath::vfoManager.getBandwidth(GetActiveVFOName());
		else
			*bw = 0;		
	}

	static std::string GetActiveVFODemod()
	{
		std::string demod;
		std::string vfo = GetActiveVFOName();
		if (core::modComManager.interfaceExists(vfo))
		{
			int modeNum;
			core::modComManager.callInterface(vfo, RADIO_IFACE_CMD_GET_MODE, NULL, &modeNum);
			switch (modeNum)
			{
			case RADIO_IFACE_MODE_NFM:
				demod = "NFM";
				break;
			case RADIO_IFACE_MODE_WFM:
				demod = "WFM";
				break;
			case RADIO_IFACE_MODE_AM:
				demod = "AM";
				break;
			case RADIO_IFACE_MODE_DSB:
				demod = "DSB";
				break;
			case RADIO_IFACE_MODE_LSB:
				demod = "LSB";
				break;
			case RADIO_IFACE_MODE_USB:
				demod = "USB";
				break;
			case RADIO_IFACE_MODE_CW:
				demod = "CW";
				break;
			case RADIO_IFACE_MODE_RAW:
				demod = "RAW";
				break;
			default:
				demod = "N/A";
				break;
			}
		}
		return demod;
	}

	static void SaveBookmark(std::string description, std::string category, double frequency, int bandwidth, std::string demod)
	{
		config.aquire();
		config.conf[description] = json({});
		config.conf[description]["category"] = category;
		config.conf[description]["frequency"] = frequency;
		config.conf[description]["bandwidth"] = bandwidth;
		config.conf[description]["mode"] = demod;
		config.release(true);
	}

	static void EditBookmark(std::string description, std::string category, double frequency, int bandwidth)
	{
		config.aquire();
		config.conf[description]["category"] = category;
		config.conf[description]["frequency"] = frequency;
		config.conf[description]["bandwidth"] = bandwidth;
		config.release(true);
	}

	static void DeleteBookmark(std::string description)
	{
		config.aquire();
		if (config.conf.contains(description))
			config.conf.erase(description);
		config.release(true);
	}

	static bool VectorArrayContainsString(std::vector<std::string>& v, std::string item)
	{
		auto it = std::find(v.begin(), v.end(), item);
		if (it != v.end())
			return true;
		else
			return false;
	}

	static void VectorArrayDeleteElement(std::vector<std::string>& v, std::string item)
	{
		auto itr = std::find(v.begin(), v.end(), item);
		if (itr != v.end()) v.erase(itr);
	}

	static void menuHandler(void* ctx)
	{
		FreqManModule* _this = (FreqManModule*)ctx;

		float menuWidth = ImGui::GetContentRegionAvailWidth();
		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::SetNextItemWidth(menuWidth);

		static int vfoBW;
		static double vfoFreq;
		static char description[128] = "";
		static char category[128] = "";
		std::string vfoName = GetActiveVFOName();
		std::string vfoDemod = GetActiveVFODemod();
		static std::string original_description = "";
		static std::string original_category = "";

		// categories dropdown
		static int current_category_index;
		std::string categories_dropdown_list = "";
		for (int i = 0; i < _this->categories.size(); i++)
		{
			categories_dropdown_list += _this->categories[i];
			categories_dropdown_list += '\0';
		}

		ImGui::AlignTextToFramePadding();
		ImGui::Text("Category: ");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
		bool disable_ctl = false;
		if (_this->selected_category.size() == 0)
		{
			style::beginDisabled();
			disable_ctl = true;
		}
		if (ImGui::Combo("##_sdrpp_freq_mgr_category", &current_category_index, categories_dropdown_list.c_str()))
		{
			_this->selected_category = _this->categories[current_category_index];
			_this->displayed_bookmarks.clear();
			_this->selected_bookmarks.clear();
			for (const auto& i : _this->bookmarks)
			{
				if (config.conf[i].contains("category") && ((std::string)config.conf[i]["category"] == _this->selected_category))
					_this->displayed_bookmarks.push_back(i);
			}
		}
		if (disable_ctl)
		{
			style::endDisabled();
			disable_ctl = false;
		}

		// VFO dropdown
		const char* vfos[] = { "Selected" };
		static int current_vfo = 0;

		ImGui::AlignTextToFramePadding();
		style::beginDisabled();
		ImGui::Text("VFO: ");
		ImGui::SameLine(ImGui::CalcTextSize("Category: ").x + style.ItemSpacing.x);
		ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
		ImGui::Combo("##_sdrpp_freq_mgr_vfo", &current_vfo, vfos, IM_ARRAYSIZE(vfos));
		style::endDisabled();

		// controls buttons
		bool unused_open = true;
		ImVec2 button_sz((menuWidth / 3 - style.ItemSpacing.x / 3 - 2), 24);

		ImGui::AlignTextToFramePadding();

		// add new bookmark button
		if (vfoName.size() == 0)
		{
			style::beginDisabled();
			disable_ctl = true;
		}

		if (ImGui::Button("Add", button_sz))
		{
			ImGui::OpenPopup("Add new bookmark");
			GetActiveVFOBandwidth(&vfoBW);
			GetActiveVFOFrequency(&vfoFreq);
			strcpy(description, (GetActiveVFOFrequencyShort(vfoFreq) + " " + vfoDemod).c_str());
		}

		if (disable_ctl)
		{
			style::endDisabled();
			disable_ctl = false;
		}

		ImGui::SetNextWindowPos(ImVec2(2, 300));
		if (ImGui::BeginPopupModal("Add new bookmark", &unused_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::SetWindowSize("Add new bookmark", ImVec2(330, 210));

			ImGui::Text("Category: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0); ImGui::SetNextItemWidth(200.0);
			if (ImGui::InputTextWithHint("##_sdrpp_freq_mgr_add_history", "General", category, IM_ARRAYSIZE(category), ImGuiInputTextFlags_CallbackHistory, HistoryHandling::CategoryHistoryCallback))
			{
			}
			ImGui::SameLine();
			ImGui::TextDisabled(" ?"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("If a category is not provided, \"General\" will be used.\nStart typing and use UP/DOWN keys to cycle through history.");
			if (strlen(description) == 0 || VectorArrayContainsString(_this->bookmarks, (std::string)description))
			{
				ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::HSV(254.0f, 255.0f, 210.0f));
				disable_ctl = true;
			}
			ImGui::Text("Description: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0); ImGui::SetNextItemWidth(200.0);
			if (disable_ctl)
			{
				ImGui::PopStyleColor();
				disable_ctl = false;
			}
			if (ImGui::InputText("##_sdrpp_freq_mgr_add_descr", description, IM_ARRAYSIZE(description), ImGuiInputTextFlags_AutoSelectAll))
			{
			}

			if (strlen(description) == 0)
			{
				ImGui::SameLine();
				ImGui::TextDisabled(" ?"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("Description cannot be empty.");
			}

			if (VectorArrayContainsString(_this->bookmarks, (std::string)description))
			{
				ImGui::SameLine();
				ImGui::TextDisabled(" ?"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("A bookmark by that name already exists.\nDescription must be unique.");
			}

			ImGui::Text("Frequency: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0); ImGui::SetNextItemWidth(200.0);
			ImGui::InputDouble("##_sdrpp_freq_mgr_add_freq", &vfoFreq, 1.0, 100.0, "%.0f");
			ImGui::Text("Bandwidth: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0); ImGui::SetNextItemWidth(200.0);
			ImGui::InputInt("##_sdrpp_freq_mgr_add_bw", &vfoBW);
			ImGui::Text("Demodulator: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0);
			ImGui::Text(vfoDemod.c_str());

			//ImGui::NewLine();
			ImGui::TextDisabled("Active VFO: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0); ImGui::TextDisabled(vfoName.c_str());
			ImGui::Separator();

			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() / 2 - 94);
			if (ImGui::Button("Add", ImVec2(90, 24)))
			{
				if (strlen(description) == 0)
				{
					spdlog::error("Frequency Manager: Description cannot be empty.");
				}
				else if (VectorArrayContainsString(_this->bookmarks, (std::string)description))
				{
					spdlog::error("Frequency Manager: A bookmark by that name already exists. Description must be unique.");
				}
				else
				{
					if (strlen(category) == 0)
						strcpy(category, "General");
					
					SaveBookmark(description, category, vfoFreq, vfoBW, vfoDemod);

					if (!VectorArrayContainsString(_this->categories, std::string(category)))
						_this->categories.push_back(category);
					
					std::sort(_this->categories.begin(), _this->categories.end());
					
					_this->bookmarks.push_back(description);

					if (_this->selected_category.size() == 0)
						_this->selected_category = category;

					if (strncmp(_this->selected_category.c_str(), category, 1) > 0)
						current_category_index += 1;
					
					config.aquire();
					if (config.conf[description].contains("category") && ((std::string)config.conf[description]["category"] == _this->selected_category))
						_this->displayed_bookmarks.push_back(description);
					config.release();

					spdlog::info("Frequency Manager: Saving bookmark '{0}' set at {1} Hz.", description, std::to_string((unsigned int)vfoFreq));
					
					strcpy(category, "");
					strcpy(description, "");
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(90, 24)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
		ImGui::SameLine();

		// edit bookmark button
		if (_this->selected_bookmarks.size() != 1)
		{
			style::beginDisabled();
			disable_ctl = true;
		}

		static char mode[4] = "";
		if (ImGui::Button("Edit", button_sz))
		{
			ImGui::OpenPopup("Edit bookmark");
			config.aquire();
			vfoFreq = config.conf[_this->selected_bookmarks[0]]["frequency"];
			vfoBW = config.conf[_this->selected_bookmarks[0]]["bandwidth"];
			strcpy(mode, std::string(config.conf[_this->selected_bookmarks[0]]["mode"]).c_str());
			strcpy(description, _this->selected_bookmarks[0].c_str());
			strcpy(category, std::string(config.conf[_this->selected_bookmarks[0]]["category"]).c_str());
			config.release();
			original_description = description;
			original_category = category;
		}
		if (disable_ctl)
		{
			style::endDisabled();
			disable_ctl = false;
		}
		ImGui::SetNextWindowPos(ImVec2(2, 300));

		if (ImGui::BeginPopupModal("Edit bookmark", &unused_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::SetWindowSize("Edit bookmark", ImVec2(330, 210));

			ImGui::Text("Category: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0); ImGui::SetNextItemWidth(200.0);
			ImGui::InputTextWithHint("##_sdrpp_freq_mgr_edit_category", "General", category, IM_ARRAYSIZE(category), ImGuiInputTextFlags_CallbackHistory, HistoryHandling::CategoryHistoryCallback);
			ImGui::SameLine();
			ImGui::TextDisabled(" ?"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("If a category is not provided, \"General\" will be used.\nStart typing and use UP/DOWN keys to cycle through history.");
			if (strlen(description) == 0)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::HSV(254.0f, 255.0f, 210.0f));
				disable_ctl = true;
			}
			ImGui::Text("Description: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0); ImGui::SetNextItemWidth(200.0);
			if (disable_ctl)
			{
				ImGui::PopStyleColor();
				disable_ctl = false;
			}
			ImGui::InputText("##_sdrpp_freq_mgr_edit_descr", description, IM_ARRAYSIZE(description));
			if (strlen(description) == 0)
			{
				ImGui::SameLine();
				ImGui::TextDisabled(" ?"); if (ImGui::IsItemHovered()) ImGui::SetTooltip("Description cannot be empty.");
			}
			ImGui::Text("Frequency: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0); ImGui::SetNextItemWidth(200.0);
			ImGui::InputDouble("##_sdrpp_freq_mgr_edit_freq", &vfoFreq, 1.0, 100.0, "%.0f");
			ImGui::Text("Bandwidth: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0); ImGui::SetNextItemWidth(200.0);
			ImGui::InputInt("##_sdrpp_freq_mgr_edit_bw", &vfoBW);
			ImGui::Text("Demodulator: "); ImGui::SameLine(); ImGui::SetCursorPosX(100.0);
			ImGui::Text(mode);

			ImGui::NewLine();
			ImGui::Separator();

			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() / 2 - 94);
			if (ImGui::Button("Edit", ImVec2(90, 24)))
			{
				if (strlen(description) == 0)
				{
					spdlog::error("Frequency Manager: Description cannot be empty.");
				}
				else
				{
					if (strlen(category) == 0)
						strcpy(category, "General");

					if (strcmp(original_description.c_str(), description) == 0)
					{
						EditBookmark(original_description, category, vfoFreq, vfoBW);
					}
					else
					{
						DeleteBookmark(original_description);
						SaveBookmark(description, category, vfoFreq, vfoBW, mode);
						VectorArrayDeleteElement(_this->displayed_bookmarks, original_description);
						_this->displayed_bookmarks.push_back(description);
						VectorArrayDeleteElement(_this->bookmarks, original_description);
						if (!VectorArrayContainsString(_this->bookmarks, std::string(description)))
							_this->bookmarks.push_back(description);
					}

					if (strcmp(original_category.c_str(), category) != 0)
					{
						if (!VectorArrayContainsString(_this->categories, std::string(category)))
						{
							_this->categories.push_back(category);
							std::sort(_this->categories.begin(), _this->categories.end());
							if (strncmp(_this->selected_category.c_str(), category, 1) > 0)
								current_category_index += 1;
						}
						VectorArrayDeleteElement(_this->displayed_bookmarks, std::string(description));
					}

					std::sort(_this->displayed_bookmarks.begin(), _this->displayed_bookmarks.end());
					strcpy(category, "");
					strcpy(description, "");
					_this->selected_bookmarks.clear();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(90, 24)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
		ImGui::SameLine();

		// delete bookmark button
		if (_this->selected_bookmarks.size() == 0)
		{
			style::beginDisabled();
			disable_ctl = true;
		}

		if (ImGui::Button("Delete", button_sz))
		{
			if (_this->selected_bookmarks.size() > 0)
				for (const auto& i : _this->selected_bookmarks)
				{
					spdlog::info("Frequency Manager: Deleting saved bookmark '{0}'.", i);
					
					auto itr = std::find(_this->bookmarks.begin(), _this->bookmarks.end(), i);
					if (itr != _this->bookmarks.end()) _this->bookmarks.erase(itr);
					itr = std::find(_this->displayed_bookmarks.begin(), _this->displayed_bookmarks.end(), i);
					if (itr != _this->displayed_bookmarks.end()) _this->displayed_bookmarks.erase(itr);

					DeleteBookmark(i);
				}
			_this->selected_bookmarks.clear();
		}

		if (disable_ctl)
		{
			style::endDisabled();
			disable_ctl = false;
		}

		// saved frequencies table
		static ImGuiTableFlags table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg;
		ImGui::BeginTable("bookmarks_table", 2, table_flags, ImVec2(0.0f, 300));
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 67);
		ImGui::TableSetupColumn("Frequency", ImGuiTableColumnFlags_WidthStretch, 33);
		ImGui::TableHeadersRow();
		static bool tmpzzz;
		if (_this->displayed_bookmarks.size() > 0)
		{
			for (int i = 0; i < _this->displayed_bookmarks.size(); i++)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				const bool selected = VectorArrayContainsString(_this->selected_bookmarks, _this->displayed_bookmarks[i]);
				if (ImGui::Selectable(_this->displayed_bookmarks[i].c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap))
				{
					if (ImGui::GetIO().KeyCtrl)
					{
						if (selected)
							_this->selected_bookmarks.erase(std::remove(_this->selected_bookmarks.begin(), _this->selected_bookmarks.end(), _this->displayed_bookmarks[i]), _this->selected_bookmarks.end());
						else
							_this->selected_bookmarks.push_back(_this->displayed_bookmarks[i]);
					}
					else
					{
						_this->selected_bookmarks.clear();
						_this->selected_bookmarks.push_back(_this->displayed_bookmarks[i]);
					}

					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						// TODO: set frequency, bw, demod, fft marker
						spdlog::info("Frequency Manager: applying bookmark {0} to VFO {1}", _this->displayed_bookmarks[i], vfoName);
						spdlog::warn("Functionality not yet implemented.");
					}
				}
				
				ImGui::TableSetColumnIndex(1);
				std::string fr_sh = "err";
				config.aquire();
				if (config.conf[_this->displayed_bookmarks[i]].contains("frequency") && config.conf[_this->displayed_bookmarks[i]]["frequency"].is_number())
					fr_sh = GetActiveVFOFrequencyShort((double)config.conf[_this->displayed_bookmarks[i]]["frequency"]);
				config.release();
				ImGui::Text(fr_sh.c_str());
			}
		}

		ImGui::EndTable();

		// import / export
		ImGui::Separator();
		ImGui::AlignTextToFramePadding();
		button_sz.x = menuWidth / 2 - style.ItemSpacing.x / 2;
		style::beginDisabled();
		ImGui::Button("Import Bookmarks", button_sz); ImGui::SameLine();
		ImGui::Button("Export Selected", button_sz);
		style::endDisabled();
	}

	std::string name;
	bool enabled = true;

	std::vector<std::string> categories;
	std::string selected_category;
	std::vector<std::string> bookmarks;
	std::vector<std::string> displayed_bookmarks;
	std::vector<std::string> selected_bookmarks;

	struct HistoryHandling
	{
		static int CategoryHistoryCallback(ImGuiInputTextCallbackData* data)
		{
			return 0;
		}
	};
};

MOD_EXPORT void _INIT_() {
	json def = json({});
	config.setPath(options::opts.root + "/frequency_manager_config.json");
	config.load(def);
	config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
	return new FreqManModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
	config.disableAutoSave();
	config.save();
	delete (FreqManModule*)instance;
}

MOD_EXPORT void _END_() {
}
