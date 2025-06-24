#include "SDL.h"
#include "SDL_opengl.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>

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
    std::vector<std::pair<int, double>> actual_laps_log;

    // New vector to store pit stop points permanently:
    std::vector<std::pair<int, double>> pit_stops;

    Uint32 last_tick = SDL_GetTicks();
    bool done = false;

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
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

        ImGui::Begin("Lap Timer Inputs");

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
            actual_laps_log.clear();
            pit_stops.clear();  // Clear pit stops on reset
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
                if (val >= 0) {
                    current_laps = val;
                    actual_laps_log.push_back({ elapsed_seconds, current_laps });
                }
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

                // Add new pit stop dot at current lap and time
                pit_stops.push_back({ elapsed_seconds, current_laps });
            }
            command_input[0] = '\0';
        }

        ImGui::Text("Pit Stops: %d", pit_count);
        if (last_pit_time >= 0) {
            ImGui::Text("Time Since Last Pit: %s", format_time(elapsed_seconds - last_pit_time).c_str());
        } else {
            ImGui::Text("Type '1p' before first lap to log pit.");
        }

        ImGui::End();

        ImGui::SetNextWindowSize(ImVec2(1500 * 0.5f, 1200 * 0.5f));
        ImGui::Begin("Lap Graph");

        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetContentRegionAvail();

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = win_size;

        ImGui::InvisibleButton("canvas", canvas_size);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(30, 30, 30, 255));
        draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(255, 255, 255, 255));

        float scale = 0.5f;

        auto to_screen = [&](float x_units, float y_laps) -> ImVec2 {
            float sx = canvas_pos.x + (x_units / 1440.0f) * canvas_size.x;
            float sy = canvas_pos.y + canvas_size.y - (y_laps / 1000.0f) * canvas_size.y;
            return ImVec2(sx, sy);
        };

        for (int gx = 0; gx <= 1440; gx += 240) {
            float x = canvas_pos.x + (gx / 1440.0f) * canvas_size.x;
            draw_list->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_pos.y + canvas_size.y), IM_COL32(100, 100, 100, 100));
            int sec = gx * 10;
            int hr = sec / 3600;
            int min = (sec % 3600) / 60;
            int s = sec % 60;
            char buf[16];
            snprintf(buf, sizeof(buf), "%d:%02d:%02d", hr, min, s);
            draw_list->AddText(ImVec2(x + 2, canvas_pos.y + 2), IM_COL32(180, 180, 180, 255), buf);
        }
        for (int gy = 0; gy <= 1000; gy += 100) {
            float y = canvas_pos.y + canvas_size.y - (gy / 1000.0f) * canvas_size.y;
            draw_list->AddLine(ImVec2(canvas_pos.x, y), ImVec2(canvas_pos.x + canvas_size.x, y), IM_COL32(100, 100, 100, 100));
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", gy);
            draw_list->AddText(ImVec2(canvas_pos.x + 2, y - 12), IM_COL32(180, 180, 180, 255), buf);
        }

        std::vector<ImVec2> ref_points;
        for (int x = 0; x <= 1440; ++x) {
            int time_sec = x * 10;
            int block = time_sec / 2100;
            int pos_in_block = time_sec % 2100;

            double laps = 0.0;
            if (pos_in_block < 2040) {
                laps = block * (2040.0 / 15.0);
                laps += pos_in_block / 15.0;
            } else {
                laps = (block + 1) * (2040.0 / 15.0);
            }
            if (laps > 1000.0) laps = 1000.0;

            ref_points.push_back(to_screen((float)x, (float)laps));
        }
        for (size_t i = 1; i < ref_points.size(); ++i) {
            draw_list->AddLine(ref_points[i - 1], ref_points[i], IM_COL32(255, 0, 0, 128), 2.0f);
        }

        if (!actual_laps_log.empty()) {
            std::vector<ImVec2> actual_points;
            for (const auto& p : actual_laps_log) {
                float x_units = p.first / 10.0f;
                if (x_units > 1440.0f) break;
                double laps_val = std::min(p.second, 1000.0);
                actual_points.push_back(to_screen(x_units, (float)laps_val));
            }
            for (size_t i = 1; i < actual_points.size(); ++i) {
                draw_list->AddLine(actual_points[i - 1], actual_points[i], IM_COL32(0, 255, 0, 255), 2.0f);
            }
        }

        // Draw all pit stop dots permanently:
        for (const auto& pit : pit_stops) {
            float x_units = pit.first / 10.0f;
            double laps_val = pit.second;
            ImVec2 pos = to_screen(x_units, (float)laps_val);
            float radius = 4.0f;
            draw_list->AddCircleFilled(pos, radius, IM_COL32(0, 255, 0, 255));
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
