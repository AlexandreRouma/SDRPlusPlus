#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <gui/main_window.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <gui/icons.h>
#include <version.h>
#include <spdlog/spdlog.h>
#include <gui/widgets/bandplan.h>
#include <stb_image.h>
#include <config.h>
#include <core.h>
#include <options.h>
#include <duktape/duktape.h>
#include <duktape/duk_console.h>
#include <filesystem>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#ifndef INSTALL_PREFIX
    #define INSTALL_PREFIX "/usr"
#endif

namespace core {
    ConfigManager configManager;
    ScriptManager scriptManager;
    ModuleManager moduleManager;

    void setInputSampleRate(double samplerate) {
        // NOTE: Zoom controls won't work
        spdlog::info("New DSP samplerate: {0}", samplerate);
        gui::waterfall.setBandwidth(samplerate);
        gui::waterfall.setViewOffset(0);
        gui::waterfall.setViewBandwidth(samplerate);
        sigpath::signalPath.setSampleRate(samplerate);
        setViewBandwidthSlider(samplerate);
    }
};

bool maximized = false;
bool fullScreen = false;

static void glfw_error_callback(int error, const char* description) {
    spdlog::error("Glfw Error {0}: {1}", error, description);
}

static void maximized_callback(GLFWwindow* window, int n) {
    if (n == GLFW_TRUE) {
        maximized = true;
    }
    else {
        maximized = false;
    }
}

duk_ret_t test_func(duk_context *ctx) {
    printf("Hello from C++\n");
    return 1;
}

// main
int sdrpp_main(int argc, char *argv[]) {
    spdlog::info("SDR++ v" VERSION_STR);

    // Load default options and parse command line
    options::loadDefaults();
    if (!options::parse(argc, argv)) { return -1; }

#ifdef _WIN32
    if (!options::opts.showConsole) { FreeConsole(); }
#endif

    // Check root directory
    if (!std::filesystem::exists(options::opts.root)) {
        spdlog::warn("Root directory {0} does not exist, creating it", options::opts.root);
        if (!std::filesystem::create_directory(options::opts.root)) {
            spdlog::error("Could not create root directory {0}", options::opts.root);
            return -1;
        }
    }

    if (!std::filesystem::is_directory(options::opts.root)) {
        spdlog::error("{0} is not a directory", options::opts.root);
        return -1;
    }

    // ======== DEFAULT CONFIG ========
    json defConfig;
    defConfig["bandColors"]["amateur"] = "#FF0000FF";
    defConfig["bandColors"]["aviation"] = "#00FF00FF";
    defConfig["bandColors"]["broadcast"] = "#0000FFFF";
    defConfig["bandColors"]["marine"] = "#00FFFFFF";
    defConfig["bandColors"]["military"] = "#FFFF00FF";
    defConfig["bandPlan"] = "General";
    defConfig["bandPlanEnabled"] = true;
    defConfig["centerTuning"] = false;
    defConfig["colorMap"] = "Classic";
    defConfig["fftHeight"] = 300;
    defConfig["frequency"] = 100000000.0;
    defConfig["max"] = 0.0;
    defConfig["maximized"] = false;
    defConfig["menuOrder"] = {
        "Source",
        "Radio",
        "Recorder",
        "Sinks",
        "Audio",
        "Scripting",
        "Band Plan",
        "Display"
    };
    defConfig["menuWidth"] = 300;
    defConfig["min"] = -120.0;

    defConfig["moduleInstances"]["Radio"] = "radio";
    defConfig["moduleInstances"]["Recorder"] = "recorder";
    defConfig["moduleInstances"]["SoapySDR Source"] = "soapy_source";
    defConfig["moduleInstances"]["PlutoSDR Source"] = "plutosdr_source";
    defConfig["moduleInstances"]["RTL-TCP Source"] = "rtl_tcp_source";
    defConfig["moduleInstances"]["RTL-SDR Source"] = "rtl_sdr_source";
    defConfig["moduleInstances"]["AirspyHF+ Source"] = "airspyhf_source";
    defConfig["moduleInstances"]["Airspy Source"] = "airspy_source";
    defConfig["moduleInstances"]["File Source"] = "file_source";
    defConfig["moduleInstances"]["SDRplay Source"] = "sdrplay_source";
    defConfig["moduleInstances"]["Audio Sink"] = "audio_sink";

    defConfig["modules"] = json::array();
    defConfig["offset"] = 0.0;
    defConfig["showWaterfall"] = true;
    defConfig["source"] = "";
    defConfig["streams"] = json::object();
    defConfig["windowSize"]["h"] = 720;
    defConfig["windowSize"]["w"] = 1280;

#ifdef _WIN32
    defConfig["modulesDirectory"] = "./modules";
    defConfig["resourcesDirectory"] = "./res";
#else
    defConfig["modulesDirectory"] = INSTALL_PREFIX "/lib/sdrpp/plugins";
    defConfig["resourcesDirectory"] = INSTALL_PREFIX "/share/sdrpp";
#endif

    // Load config
    spdlog::info("Loading config");
    core::configManager.setPath(options::opts.root + "/config.json");
    core::configManager.load(defConfig);
    core::configManager.enableAutoSave();

    // Fix config
    core::configManager.aquire();
    for (auto const& item : defConfig.items()) {
        if (!core::configManager.conf.contains(item.key())) {
            spdlog::warn("Missing key in config {0}, repairing", item.key());
            core::configManager.conf[item.key()] = defConfig[item.key()];
        }
    }
    core::configManager.release(true);

    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }

#ifdef __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 120
    const char* glsl_version = "#version 120";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    
    core::configManager.aquire();
    int winWidth = core::configManager.conf["windowSize"]["w"];
    int winHeight = core::configManager.conf["windowSize"]["h"];
    maximized = core::configManager.conf["maximized"];
    std::string resDir = core::configManager.conf["resourcesDirectory"];
    json bandColors = core::configManager.conf["bandColors"];
    core::configManager.release();

    // Create window with graphics context
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    GLFWwindow* window = glfwCreateWindow(winWidth, winHeight, "SDR++ v" VERSION_STR " (Built at " __TIME__ ", " __DATE__ ")", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);

#if (GLFW_VERSION_MAJOR == 3) && (GLFW_VERSION_MINOR >= 3)
    if (maximized) {
        glfwMaximizeWindow(window);
    }

    glfwSetWindowMaximizeCallback(window, maximized_callback);
#endif

    // Load app icon
    GLFWimage icons[10];
    icons[0].pixels = stbi_load(((std::string)(resDir + "/icons/sdrpp.png")).c_str(), &icons[0].width, &icons[0].height, 0, 4);
    icons[1].pixels = (unsigned char*)malloc(16 * 16 * 4); icons[1].width = icons[1].height = 16;
    icons[2].pixels = (unsigned char*)malloc(24 * 24 * 4); icons[2].width = icons[2].height = 24;
    icons[3].pixels = (unsigned char*)malloc(32 * 32 * 4); icons[3].width = icons[3].height = 32;
    icons[4].pixels = (unsigned char*)malloc(48 * 48 * 4); icons[4].width = icons[4].height = 48;
    icons[5].pixels = (unsigned char*)malloc(64 * 64 * 4); icons[5].width = icons[5].height = 64;
    icons[6].pixels = (unsigned char*)malloc(96 * 96 * 4); icons[6].width = icons[6].height = 96;
    icons[7].pixels = (unsigned char*)malloc(128 * 128 * 4); icons[7].width = icons[7].height = 128;
    icons[8].pixels = (unsigned char*)malloc(196 * 196 * 4); icons[8].width = icons[8].height = 196;
    icons[9].pixels = (unsigned char*)malloc(256 * 256 * 4); icons[9].width = icons[9].height = 256;
    stbir_resize_uint8(icons[0].pixels, icons[0].width, icons[0].height, icons[0].width * 4, icons[1].pixels, 16, 16, 16 * 4, 4);
    stbir_resize_uint8(icons[0].pixels, icons[0].width, icons[0].height, icons[0].width * 4, icons[2].pixels, 24, 24, 24 * 4, 4);
    stbir_resize_uint8(icons[0].pixels, icons[0].width, icons[0].height, icons[0].width * 4, icons[3].pixels, 32, 32, 32 * 4, 4);
    stbir_resize_uint8(icons[0].pixels, icons[0].width, icons[0].height, icons[0].width * 4, icons[4].pixels, 48, 48, 48 * 4, 4);
    stbir_resize_uint8(icons[0].pixels, icons[0].width, icons[0].height, icons[0].width * 4, icons[5].pixels, 64, 64, 64 * 4, 4);
    stbir_resize_uint8(icons[0].pixels, icons[0].width, icons[0].height, icons[0].width * 4, icons[6].pixels, 96, 96, 96 * 4, 4);
    stbir_resize_uint8(icons[0].pixels, icons[0].width, icons[0].height, icons[0].width * 4, icons[7].pixels, 128, 128, 128 * 4, 4);
    stbir_resize_uint8(icons[0].pixels, icons[0].width, icons[0].height, icons[0].width * 4, icons[8].pixels, 196, 196, 196 * 4, 4);
    stbir_resize_uint8(icons[0].pixels, icons[0].width, icons[0].height, icons[0].width * 4, icons[9].pixels, 256, 256, 256 * 4, 4);
    glfwSetWindowIcon(window, 10, icons);
    stbi_image_free(icons[0].pixels);
    for (int i = 1; i < 10; i++) {
        free(icons[i].pixels);
    }

    if (glewInit() != GLEW_OK) {
        spdlog::error("Failed to initialize OpenGL loader!");
        return 1;
    }


    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = NULL;

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    if (!style::setDarkStyle(resDir)) { return -1; }

    LoadingScreen::setWindow(window);

    LoadingScreen::show("Loading icons");
    spdlog::info("Loading icons");
    if (!icons::load(resDir)) { return -1; }

    LoadingScreen::show("Loading band plans");
    spdlog::info("Loading band plans");
    bandplan::loadFromDir(resDir + "/bandplans");

    LoadingScreen::show("Loading band plan colors");
    spdlog::info("Loading band plans color table");
    bandplan::loadColorTable(bandColors);

    windowInit();

    spdlog::info("Ready.");

    bool _maximized = maximized;
    int fsWidth, fsHeight, fsPosX, fsPosY;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        //ImGui::ShowDemoWindow();

        if (_maximized != maximized) {
            _maximized = maximized;
            core::configManager.aquire();
            core::configManager.conf["maximized"]= _maximized;
            if (!maximized) {
                glfwSetWindowSize(window, core::configManager.conf["windowSize"]["w"], core::configManager.conf["windowSize"]["h"]);
            }
            core::configManager.release(true);
        }

        int _winWidth, _winHeight;
        glfwGetWindowSize(window, &_winWidth, &_winHeight);

        if (ImGui::IsKeyPressed(GLFW_KEY_F11)) {
            fullScreen = !fullScreen;
            if (fullScreen) {
                spdlog::info("Fullscreen: ON");
                fsWidth = _winWidth;
                fsHeight = _winHeight;
                glfwGetWindowPos(window, &fsPosX, &fsPosY);
                const GLFWvidmode * mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, 0);
            }
            else {
                spdlog::info("Fullscreen: OFF");
                glfwSetWindowMonitor(window, nullptr,  fsPosX, fsPosY, fsWidth, fsHeight, 0);
            }
        }

        if ((_winWidth != winWidth || _winHeight != winHeight) && !maximized && _winWidth > 0 && _winHeight > 0) {
            winWidth = _winWidth;
            winHeight = _winHeight;
            core::configManager.aquire();
            core::configManager.conf["windowSize"]["w"] = winWidth;
            core::configManager.conf["windowSize"]["h"] = winHeight;
            core::configManager.release(true);
        }

        if (winWidth > 0 && winHeight > 0) {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(_winWidth, _winHeight));
            drawWindow();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0666f, 0.0666f, 0.0666f, 1.0f);
        //glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapInterval(1); // Enable vsync
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
