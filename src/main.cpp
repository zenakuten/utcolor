#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <imgui_internal.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct ColoredChar {
    char ch;
    uint8_t r, g, b;
};

// UT2004 treats 0 as null terminator in color codes, so clamp 0 -> 1
static uint8_t ut_color(float f) {
    uint8_t v = (uint8_t)(f * 255.0f);
    return v == 0 ? 1 : v;
}

// Copy encoded UT color string to clipboard.
// On Windows, use CF_TEXT directly to avoid UTF-8 mangling of raw bytes.
// On other platforms, fall back to SDL clipboard.
static void CopyToClipboard(const std::string& data) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, data.size() + 1);
    if (hMem) {
        char* dst = (char*)GlobalLock(hMem);
        memcpy(dst, data.data(), data.size());
        dst[data.size()] = '\0';
        GlobalUnlock(hMem);
        SetClipboardData(CF_TEXT, hMem);
    }
    CloseClipboard();
#else
    SDL_SetClipboardText(data.c_str());
#endif
}

enum SelectionSource { SEL_NONE, SEL_INPUT, SEL_GRID };

struct SelectionState {
    int sel_start = 0;
    int sel_end = 0;
    SelectionSource source = SEL_NONE;
};

static int InputTextCallback(ImGuiInputTextCallbackData* data) {
    auto* state = static_cast<SelectionState*>(data->UserData);
    int s = std::min(data->SelectionStart, data->SelectionEnd);
    int e = std::max(data->SelectionStart, data->SelectionEnd);
    if (s != e) {
        // User is highlighting text in the input - take over selection
        state->sel_start = s;
        state->sel_end = e;
        state->source = SEL_INPUT;
    } else if (state->source == SEL_INPUT) {
        // User cleared their input selection - clear ours too
        state->sel_start = 0;
        state->sel_end = 0;
        state->source = SEL_NONE;
    }
    // If source is SEL_GRID, don't touch it - let the grid selection persist
    return 0;
}

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("UTColor", 900, 600, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Load a larger font for the text input
    ImFontConfig font_cfg;
    font_cfg.SizePixels = 24.0f;
    ImFont* large_font = io.Fonts->AddFontDefault(&font_cfg);

    // Application state
    char text_buf[256] = "";
    float current_color[3] = {1.0f, 1.0f, 1.0f};
    float gradient_start[3] = {1.0f, 0.0f, 0.0f};
    float gradient_end[3] = {0.0f, 0.0f, 1.0f};
    std::vector<ColoredChar> colored_text;
    SelectionState sel_state;
    int prev_sel_start = 0, prev_sel_end = 0;
    float left_col_width = 260.0f;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("UTColor", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // === Two-column layout with draggable splitter ===
        float avail_width = ImGui::GetContentRegionAvail().x;
        float avail_height = ImGui::GetContentRegionAvail().y;
        float splitter_thickness = 6.0f;
        float min_left = 200.0f;
        float min_right = 200.0f;
        left_col_width = std::clamp(left_col_width, min_left, avail_width - min_right - splitter_thickness);
        float right_width = avail_width - left_col_width - splitter_thickness;

        // --- Left column: Color Picker ---
        ImGui::BeginChild("##picker_col", ImVec2(left_col_width, -1), ImGuiChildFlags_None);
        float inner_width = ImGui::GetContentRegionAvail().x;

        ImGui::SeparatorText("Color Picker");
        ImGui::SetNextItemWidth(inner_width);
        bool color_changed = ImGui::ColorPicker3("##color", current_color,
                            ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview);

        if (!colored_text.empty()) {
            if (ImGui::Button("Apply Color to All")) {
                uint8_t r = ut_color(current_color[0]);
                uint8_t g = ut_color(current_color[1]);
                uint8_t b = ut_color(current_color[2]);
                for (auto& cc : colored_text) {
                    cc.r = r;
                    cc.g = g;
                    cc.b = b;
                }
            }
        }

        // --- Gradient ---
        ImGui::SeparatorText("Gradient");
        ImGui::ColorEdit3("Start", gradient_start, ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();
        ImGui::ColorEdit3("End", gradient_end, ImGuiColorEditFlags_NoInputs);

        // Draw a gradient preview bar
        ImVec2 bar_pos = ImGui::GetCursorScreenPos();
        float bar_h = 20.0f;
        ImDrawList* gdl = ImGui::GetWindowDrawList();
        ImU32 col_left = IM_COL32(
            (uint8_t)(gradient_start[0] * 255), (uint8_t)(gradient_start[1] * 255),
            (uint8_t)(gradient_start[2] * 255), 255);
        ImU32 col_right = IM_COL32(
            (uint8_t)(gradient_end[0] * 255), (uint8_t)(gradient_end[1] * 255),
            (uint8_t)(gradient_end[2] * 255), 255);
        gdl->AddRectFilledMultiColor(
            bar_pos, ImVec2(bar_pos.x + inner_width, bar_pos.y + bar_h),
            col_left, col_right, col_right, col_left);
        ImGui::Dummy(ImVec2(inner_width, bar_h));

        if (ImGui::Button("Apply to Selection", ImVec2(inner_width, 0))) {
            bool has_sel = sel_state.sel_start != sel_state.sel_end
                && sel_state.sel_end <= (int)colored_text.size();
            if (has_sel) {
                int count = sel_state.sel_end - sel_state.sel_start;
                for (int i = 0; i < count; i++) {
                    float t = (count > 1) ? (float)i / (float)(count - 1) : 0.0f;
                    auto& cc = colored_text[sel_state.sel_start + i];
                    cc.r = ut_color(gradient_start[0] + (gradient_end[0] - gradient_start[0]) * t);
                    cc.g = ut_color(gradient_start[1] + (gradient_end[1] - gradient_start[1]) * t);
                    cc.b = ut_color(gradient_start[2] + (gradient_end[2] - gradient_start[2]) * t);
                }
            }
        }
        if (ImGui::Button("Apply to All", ImVec2(inner_width, 0))) {
            int count = (int)colored_text.size();
            if (count > 0) {
                for (int i = 0; i < count; i++) {
                    float t = (count > 1) ? (float)i / (float)(count - 1) : 0.0f;
                    auto& cc = colored_text[i];
                    cc.r = ut_color(gradient_start[0] + (gradient_end[0] - gradient_start[0]) * t);
                    cc.g = ut_color(gradient_start[1] + (gradient_end[1] - gradient_start[1]) * t);
                    cc.b = ut_color(gradient_start[2] + (gradient_end[2] - gradient_start[2]) * t);
                }
            }
        }
        ImGui::EndChild();

        // --- Vertical splitter ---
        ImGui::SameLine();
        ImVec2 splitter_pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##splitter", ImVec2(splitter_thickness, avail_height));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive()) {
            left_col_width += io.MouseDelta.x;
            left_col_width = std::clamp(left_col_width, min_left, avail_width - min_right - splitter_thickness);
        }
        // Draw the splitter line
        ImDrawList* sdl = ImGui::GetWindowDrawList();
        ImU32 splitter_col = ImGui::IsItemHovered() || ImGui::IsItemActive()
            ? IM_COL32(180, 180, 180, 255) : IM_COL32(80, 80, 80, 255);
        float cx = splitter_pos.x + splitter_thickness * 0.5f;
        sdl->AddLine(ImVec2(cx, splitter_pos.y), ImVec2(cx, splitter_pos.y + avail_height), splitter_col, 2.0f);

        ImGui::SameLine();

        // --- Right column: Text input, grid, preview, output ---
        ImGui::BeginChild("##right_col", ImVec2(right_width, -1), ImGuiChildFlags_None);

        // --- Text Input ---
        ImGui::SeparatorText("Text Input");
        ImGui::Text("Highlight characters in the input to select them for coloring.");

        // Render InputText with invisible text using larger font
        ImGui::PushFont(large_font);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
        ImGui::SetNextItemWidth(-1);
        bool text_changed = ImGui::InputText("##text", text_buf, sizeof(text_buf),
            ImGuiInputTextFlags_CallbackAlways, InputTextCallback, &sel_state);
        ImGui::PopStyleColor();

        // Overlay colored characters on top of the InputText
        if (!colored_text.empty()) {
            ImVec2 rect_min = ImGui::GetItemRectMin();
            ImVec2 rect_max = ImGui::GetItemRectMax();
            ImVec2 frame_pad = ImGui::GetStyle().FramePadding;

            float scroll_x = 0.0f;
            ImGuiContext& g = *ImGui::GetCurrentContext();
            if (g.InputTextState.ID == ImGui::GetItemID()) {
                scroll_x = g.InputTextState.Scroll.x;
            }

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->PushClipRect(
                ImVec2(rect_min.x + frame_pad.x, rect_min.y),
                ImVec2(rect_max.x - frame_pad.x, rect_max.y), true);

            float font_size = large_font->FontSize;
            float x = rect_min.x + frame_pad.x - scroll_x;
            float y = rect_min.y + frame_pad.y;

            for (const auto& cc : colored_text) {
                char ch_str[2] = {cc.ch, '\0'};
                ImU32 col = IM_COL32(cc.r, cc.g, cc.b, 255);
                draw_list->AddText(large_font, font_size, ImVec2(x, y), col, ch_str);
                x += large_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, ch_str).x;
            }

            draw_list->PopClipRect();
        }
        ImGui::PopFont();

        if (text_changed) {
            std::string new_text(text_buf);
            std::vector<ColoredChar> new_colored;
            new_colored.reserve(new_text.size());
            for (size_t i = 0; i < new_text.size(); i++) {
                if (i < colored_text.size() && colored_text[i].ch == new_text[i]) {
                    new_colored.push_back(colored_text[i]);
                } else {
                    new_colored.push_back({new_text[i], 255, 255, 255});
                }
            }
            colored_text = std::move(new_colored);
        }

        bool has_selection = sel_state.sel_start != sel_state.sel_end
            && sel_state.sel_end <= (int)colored_text.size();

        // Auto-apply picker color only when the color or selection changes
        bool selection_changed = sel_state.sel_start != prev_sel_start
            || sel_state.sel_end != prev_sel_end;
        if (has_selection && (color_changed || selection_changed)) {
            uint8_t r = ut_color(current_color[0]);
            uint8_t g = ut_color(current_color[1]);
            uint8_t b = ut_color(current_color[2]);
            for (int i = sel_state.sel_start; i < sel_state.sel_end; i++) {
                colored_text[i].r = r;
                colored_text[i].g = g;
                colored_text[i].b = b;
            }
        }
        prev_sel_start = sel_state.sel_start;
        prev_sel_end = sel_state.sel_end;

        // --- Character Grid ---
        ImGui::SeparatorText("Character Grid");
        if (!colored_text.empty()) {
            ImGui::Text("Click to select, Shift+click to extend range.");
            for (int i = 0; i < (int)colored_text.size(); i++) {
                if (i > 0) ImGui::SameLine();

                const auto& cc = colored_text[i];
                ImVec4 btn_color(cc.r / 255.0f, cc.g / 255.0f, cc.b / 255.0f, 1.0f);

                ImGui::PushID(i);
                ImGui::PushStyleColor(ImGuiCol_Button, btn_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    ImVec4(btn_color.x * 0.8f, btn_color.y * 0.8f, btn_color.z * 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                    ImVec4(btn_color.x * 0.6f, btn_color.y * 0.6f, btn_color.z * 0.6f, 1.0f));

                float luminance = 0.299f * btn_color.x + 0.587f * btn_color.y + 0.114f * btn_color.z;
                ImVec4 text_col = luminance > 0.5f ? ImVec4(0,0,0,1) : ImVec4(1,1,1,1);
                ImGui::PushStyleColor(ImGuiCol_Text, text_col);

                char label[4];
                label[0] = cc.ch;
                label[1] = '\0';

                bool in_selection = has_selection && i >= sel_state.sel_start && i < sel_state.sel_end;
                if (in_selection) {
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 0, 1));
                }

                if (ImGui::Button(label, ImVec2(30, 30))) {
                    if (io.KeyShift && sel_state.source == SEL_GRID) {
                        // Extend range from existing grid anchor
                        int anchor = sel_state.sel_start;
                        sel_state.sel_start = std::min(anchor, i);
                        sel_state.sel_end = std::max(anchor, i) + 1;
                    } else {
                        sel_state.sel_start = i;
                        sel_state.sel_end = i + 1;
                    }
                    sel_state.source = SEL_GRID;
                }

                if (in_selection) {
                    ImGui::PopStyleColor(); // Border
                    ImGui::PopStyleVar();   // FrameBorderSize
                }

                ImGui::PopStyleColor(4); // Button colors + Text
                ImGui::PopID();
            }
        } else {
            ImGui::TextDisabled("Type some text above to get started.");
        }

        // --- Live Preview ---
        ImGui::SeparatorText("Live Preview");
        ImGui::PushFont(large_font);
        for (int i = 0; i < (int)colored_text.size(); i++) {
            if (i > 0) ImGui::SameLine(0, 0);
            const auto& cc = colored_text[i];
            char ch_str[2] = {cc.ch, '\0'};
            ImGui::TextColored(ImVec4(cc.r / 255.0f, cc.g / 255.0f, cc.b / 255.0f, 1.0f), "%s", ch_str);
        }
        if (colored_text.empty()) {
            ImGui::TextDisabled("(empty)");
        }
        ImGui::PopFont();

        // --- Encoded Output ---
        ImGui::SeparatorText("Encoded Output");
        std::string encoded;
        for (size_t i = 0; i < colored_text.size(); i++) {
            const auto& cc = colored_text[i];
            // Emit color code at start or when color changes from previous char
            bool need_code = (i == 0);
            if (i > 0) {
                const auto& prev = colored_text[i - 1];
                if (cc.r != prev.r || cc.g != prev.g || cc.b != prev.b)
                    need_code = true;
            }
            if (need_code) {
                encoded += (char)0x1B;
                encoded += (char)cc.r;
                encoded += (char)cc.g;
                encoded += (char)cc.b;
            }
            encoded += cc.ch;
        }

        std::string hex_display;
        for (size_t i = 0; i < encoded.size(); i++) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02X ", (uint8_t)encoded[i]);
            hex_display += hex;
        }
        ImGui::TextWrapped("%s", hex_display.c_str());

        if (ImGui::Button("Copy to Clipboard")) {
            CopyToClipboard(encoded);
        }

        ImGui::EndChild();

        ImGui::End();

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
