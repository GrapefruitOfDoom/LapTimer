#include "SDL.h"
#include "SDL_opengl.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>

static const int DEFAULT_RACE_DURATION_SECONDS = 4 * 60 * 60;

std::string format_time(int total_seconds) {
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << hours << ":"
        << std::setw(2) << std::setfill('0') << minutes << ":"
        << std::setw(2) << std::setfill('0') << seconds;
    return oss.str();
}

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "Error: SDL_Init failed\n";
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_Window* window = SDL_CreateWindow("Lap Timer ImGui",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800, 600,
                                          window_flags);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    bool timer_running = false;
    int race_duration_seconds = DEFAULT_RACE_DURATION_SECONDS;
    int elapsed_seconds = 0;
    double current_laps = 0.0;

    int pit_count = -1;
    int last_pit_time = -1;
    static char command_input[32] = "";

    Uint32 last_tick = SDL_GetTicks();

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        if (timer_running) {
            Uint32 now = SDL_GetTicks();
            if (now - last_tick >= 1000) {
                elapsed_seconds++;
                last_tick += 1000;
                if (elapsed_seconds >= race_duration_seconds) {
                    timer_running = false;
                }
            }
        } else {
            last_tick = SDL_GetTicks();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Lap Timer");

        static char duration_input[16];
        snprintf(duration_input, sizeof(duration_input), "%d", race_duration_seconds);
        if (ImGui::InputText("Race Duration (seconds)", duration_input, sizeof(duration_input),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            try {
                int val = std::stoi(duration_input);
                if (val > 0) race_duration_seconds = val;
            } catch (...) {}
        }

        ImGui::Separator();

        if (!timer_running) {
            if (ImGui::Button("Start Timer")) {
                timer_running = true;
            }
        } else {
            if (ImGui::Button("Stop Timer")) {
                timer_running = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Timer")) {
            timer_running = false;
            elapsed_seconds = 0;
            current_laps = 0.0;
            pit_count = -1;
            last_pit_time = -1;
        }

        ImGui::Separator();

        ImGui::Text("Elapsed Time: %s", format_time(elapsed_seconds).c_str());
        ImGui::Text("Time Remaining: %s", format_time(race_duration_seconds - elapsed_seconds).c_str());
        ImGui::Text("Race Duration: %s", format_time(race_duration_seconds).c_str());

        ImGui::Separator();

        static char laps_input[32];
        snprintf(laps_input, sizeof(laps_input), "%.2f", current_laps);
        if (ImGui::InputText("Laps Completed", laps_input, sizeof(laps_input),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            try {
                double val = std::stod(laps_input);
                if (val >= 0) current_laps = val;
            } catch (...) {}
        }

        if (elapsed_seconds > 0 && current_laps > 0) {
            double avg_lap_time = static_cast<double>(elapsed_seconds) / current_laps;
            int remaining_seconds = race_duration_seconds - elapsed_seconds;
            double projected_laps = current_laps + remaining_seconds / avg_lap_time;

            ImGui::Text("Average Lap Time: %.2f seconds", avg_lap_time);
            ImGui::Text("Projected Laps at End: %.2f", projected_laps);
        } else {
            ImGui::Text("Average Lap Time: N/A");
            ImGui::Text("Projected Laps at End: N/A");
        }

        ImGui::Separator();

        if (ImGui::InputText("Command Input", command_input, sizeof(command_input),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string cmd(command_input);
            if (cmd == "1p") {
                pit_count++;
                last_pit_time = elapsed_seconds;
            }
            command_input[0] = '\0';
        }

        ImGui::Text("Pit Stops: %d", pit_count);
        if (last_pit_time >= 0) {
            ImGui::Text("Time Since Last Pit: %s", format_time(elapsed_seconds - last_pit_time).c_str());
        } else {
            ImGui::Text("Type '1p' into the text box every time you make a pit stop. \nDo it once now, before you press start");
        }

        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
