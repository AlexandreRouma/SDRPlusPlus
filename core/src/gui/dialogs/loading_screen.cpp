#include <GL/glew.h>
#include <gui/dialogs/loading_screen.h>
#include <gui/main_window.h>
#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <gui/icons.h>
#include <gui/style.h>

namespace LoadingScreen {
    GLFWwindow* _win;

    void setWindow(GLFWwindow* win) {
        _win = win;
    }

    void show(std::string msg) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
        ImGui::Begin("Main", NULL, WINDOW_FLAGS);


        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
        ImGui::OpenPopup("Credits");
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDarkening, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginPopupModal("Credits", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

        ImGui::PushFont(style::hugeFont);
        ImGui::Text("SDR++    ");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::Image(icons::LOGO, ImVec2(128, 128));
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::Text("This software is brought to you by\n\n");

        ImGui::Columns(3, "CreditColumns", true);

        // Contributors
        ImGui::Text("Contributors");
        ImGui::BulletText("Ryzerth (Creator)");
        ImGui::BulletText("aosync");
        ImGui::BulletText("Benjamin Kyd");
        ImGui::BulletText("Tobias Mädel");
        ImGui::BulletText("Raov");
        ImGui::BulletText("Howard0su");

        // Libraries
        ImGui::NextColumn();
        ImGui::Text("Libraries");
        ImGui::BulletText("SoapySDR (PothosWare)");
        ImGui::BulletText("Dear ImGui (ocornut)");
        ImGui::BulletText("spdlog (gabime)");
        ImGui::BulletText("json (nlohmann)");
        ImGui::BulletText("portaudio (PA Comm.)");

        // Patrons
        ImGui::NextColumn();
        ImGui::Text("Patrons");
        ImGui::BulletText("SignalsEverywhere");
        ImGui::BulletText("Lee Donaghy");

        ImGui::Columns(1, "CreditColumnsEnd", true);

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text(msg.c_str());

        ImGui::EndPopup();
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(1);

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(_win, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0666f, 0.0666f, 0.0666f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(_win);
    }
}