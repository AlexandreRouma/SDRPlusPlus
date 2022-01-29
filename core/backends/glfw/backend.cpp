#include <backend.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <utils/opengl_include_code.h>
#include <version.h>
#include <core.h>
#include <filesystem>
#include <stb_image.h>
#include <stb_image_resize.h>
#include <gui/gui.h>

namespace backend {
    const char* OPENGL_VERSIONS_GLSL[] = {
        "#version 120",
        "#version 300 es",
        "#version 120"
    };

    const int OPENGL_VERSIONS_MAJOR[] = {
        3,
        3,
        2
    };

    const int OPENGL_VERSIONS_MINOR[] = {
        0,
        1,
        1
    };

    const bool OPENGL_VERSIONS_IS_ES[] = {
        false,
        true,
        false
    };

    #define OPENGL_VERSION_COUNT (sizeof(OPENGL_VERSIONS_GLSL) / sizeof(char*))

    bool maximized = false;
    bool fullScreen = false;
    int winHeight;
    int winWidth;
    bool _maximized = maximized;
    int fsWidth, fsHeight, fsPosX, fsPosY;
    int _winWidth, _winHeight;
    GLFWwindow* window;
    GLFWmonitor* monitor;

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

    int init(std::string resDir) {
        // Load config
        core::configManager.acquire();
        winWidth = core::configManager.conf["windowSize"]["w"];
        winHeight = core::configManager.conf["windowSize"]["h"];
        maximized = core::configManager.conf["maximized"];
        fullScreen = core::configManager.conf["fullscreen"];
        core::configManager.release();

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
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac

        // Create window with graphics context
        monitor = glfwGetPrimaryMonitor();
        window = glfwCreateWindow(winWidth, winHeight, "SDR++ v" VERSION_STR " (Built at " __TIME__ ", " __DATE__ ")", NULL, NULL);
        if (window == NULL)
            return 1;
        glfwMakeContextCurrent(window);
    #else
        const char* glsl_version = "#version 120";
        monitor = NULL;
        for (int i = 0; i < OPENGL_VERSION_COUNT; i++) {
            glsl_version = OPENGL_VERSIONS_GLSL[i];
            glfwWindowHint(GLFW_CLIENT_API, OPENGL_VERSIONS_IS_ES[i] ? GLFW_OPENGL_ES_API : GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_VERSIONS_MAJOR[i]);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_VERSIONS_MINOR[i]);

            // Create window with graphics context
            monitor = glfwGetPrimaryMonitor();
            window = glfwCreateWindow(winWidth, winHeight, "SDR++ v" VERSION_STR " (Built at " __TIME__ ", " __DATE__ ")", NULL, NULL);
            if (window == NULL) {
                spdlog::info("OpenGL {0}.{1} {2}was not supported", OPENGL_VERSIONS_MAJOR[i], OPENGL_VERSIONS_MINOR[i], OPENGL_VERSIONS_IS_ES[i] ? "ES " : "");
                continue;
            }
            spdlog::info("Using OpenGL {0}.{1}{2}", OPENGL_VERSIONS_MAJOR[i], OPENGL_VERSIONS_MINOR[i], OPENGL_VERSIONS_IS_ES[i] ? " ES" : "");
            glfwMakeContextCurrent(window);
            break;
        }

    #endif

        // Add callback for max/min if GLFW supports it
    #if (GLFW_VERSION_MAJOR == 3) && (GLFW_VERSION_MINOR >= 3)
        if (maximized) {
            glfwMaximizeWindow(window);
        }

        glfwSetWindowMaximizeCallback(window, maximized_callback);
    #endif

        // Load app icon
        if (!std::filesystem::is_regular_file(resDir + "/icons/sdrpp.png")) {
            spdlog::error("Icon file '{0}' doesn't exist!", resDir + "/icons/sdrpp.png");
            return 1;
        }

        GLFWimage icons[10];
        icons[0].pixels = stbi_load(((std::string)(resDir + "/icons/sdrpp.png")).c_str(), &icons[0].width, &icons[0].height, 0, 4);
        icons[1].pixels = (unsigned char*)malloc(16 * 16 * 4);
        icons[1].width = icons[1].height = 16;
        icons[2].pixels = (unsigned char*)malloc(24 * 24 * 4);
        icons[2].width = icons[2].height = 24;
        icons[3].pixels = (unsigned char*)malloc(32 * 32 * 4);
        icons[3].width = icons[3].height = 32;
        icons[4].pixels = (unsigned char*)malloc(48 * 48 * 4);
        icons[4].width = icons[4].height = 48;
        icons[5].pixels = (unsigned char*)malloc(64 * 64 * 4);
        icons[5].width = icons[5].height = 64;
        icons[6].pixels = (unsigned char*)malloc(96 * 96 * 4);
        icons[6].width = icons[6].height = 96;
        icons[7].pixels = (unsigned char*)malloc(128 * 128 * 4);
        icons[7].width = icons[7].height = 128;
        icons[8].pixels = (unsigned char*)malloc(196 * 196 * 4);
        icons[8].width = icons[8].height = 196;
        icons[9].pixels = (unsigned char*)malloc(256 * 256 * 4);
        icons[9].width = icons[9].height = 256;
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

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.IniFilename = NULL;

        // Setup Platform/Renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(window, true);

        if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
            // If init fail, try to fall back on GLSL 1.2
            spdlog::warn("Could not init using OpenGL with normal GLSL version, falling back to GLSL 1.2");
            if (!ImGui_ImplOpenGL3_Init("#version 120")) {
                spdlog::error("Failed to initialize OpenGL with GLSL 1.2");
                return -1;
            }
        }

        // Set window size and fullscreen state
        glfwGetWindowSize(window, &_winWidth, &_winHeight);
        if (fullScreen) {
            spdlog::info("Fullscreen: ON");
            fsWidth = _winWidth;
            fsHeight = _winHeight;
            glfwGetWindowPos(window, &fsPosX, &fsPosY);
            const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, 0);
        }

        // Everything went according to plan
        return 0;
    }

    void beginFrame() {
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void render(bool vsync) {
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(gui::themeManager.clearColor.x, gui::themeManager.clearColor.y, gui::themeManager.clearColor.z, gui::themeManager.clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapInterval(vsync);
        glfwSwapBuffers(window);
    }

    void getMouseScreenPos(double& x, double& y) {
        glfwGetCursorPos(window, &x, &y);
    }

    void setMouseScreenPos(double x, double y) {
        glfwSetCursorPos(window, x, y);
    }

    int renderLoop() {
        // Main loop
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            beginFrame();
            
            if (_maximized != maximized) {
                _maximized = maximized;
                core::configManager.acquire();
                core::configManager.conf["maximized"] = _maximized;
                if (!maximized) {
                    glfwSetWindowSize(window, core::configManager.conf["windowSize"]["w"], core::configManager.conf["windowSize"]["h"]);
                }
                core::configManager.release(true);
            }

            glfwGetWindowSize(window, &_winWidth, &_winHeight);

            if (ImGui::IsKeyPressed(GLFW_KEY_F11)) {
                fullScreen = !fullScreen;
                if (fullScreen) {
                    spdlog::info("Fullscreen: ON");
                    fsWidth = _winWidth;
                    fsHeight = _winHeight;
                    glfwGetWindowPos(window, &fsPosX, &fsPosY);
                    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
                    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, 0);
                    core::configManager.acquire();
                    core::configManager.conf["fullscreen"] = true;
                    core::configManager.release();
                }
                else {
                    spdlog::info("Fullscreen: OFF");
                    glfwSetWindowMonitor(window, nullptr, fsPosX, fsPosY, fsWidth, fsHeight, 0);
                    core::configManager.acquire();
                    core::configManager.conf["fullscreen"] = false;
                    core::configManager.release();
                }
            }

            if ((_winWidth != winWidth || _winHeight != winHeight) && !maximized && _winWidth > 0 && _winHeight > 0) {
                winWidth = _winWidth;
                winHeight = _winHeight;
                core::configManager.acquire();
                core::configManager.conf["windowSize"]["w"] = winWidth;
                core::configManager.conf["windowSize"]["h"] = winHeight;
                core::configManager.release(true);
            }

            if (winWidth > 0 && winHeight > 0) {
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(ImVec2(_winWidth, _winHeight));
                gui::mainWindow.draw();
            }

            render();
        }
    }

    int end() {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();

        return 0; // TODO: Int really needed?
    }
}