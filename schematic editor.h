// visual_editor.hpp
#pragma once

#include <bits/stdc++.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

using namespace std;
class point {
public:
    int x, y;
    point() : x(0), y(0) {}
    point(int x_, int y_) : x(x_), y(y_) {}

    bool operator==(const point& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const point& other) const {
        return !(*this == other);
    }
};

class button {
public:
    point center;
    int delta;
    button(point c_, int d_) : center(c_), delta(d_) {}
};

enum class Tool {
    None,
    Resistor,
    Capacitor,
    Inductor,
    Diode,
    Wire,
    VoltageDC,
    VoltageAC,
    GND,    // Add this
    Port    // Add this
};

enum class AnalysisType {
    None,
    Transient,
    ACSweep,
    PhaseSweep
};

struct AnalysisParameters {
    AnalysisType type = AnalysisType::None;

    // Transient analysis
    double maxTimeStep = 0.0;
    double startTime = 0.0;
    double endTime = 0.0;

    // AC Sweep analysis
    double startFreq = 0.0;
    double endFreq = 0.0;
    double maxFreqStep = 0.0;

    // Phase Sweep analysis
    double startPhase = 0.0;
    double endPhase = 0.0;
    int maxSteps = 0;
};

struct WireSegment {
    point start;
    point end;
};

// Add to visual_editor.hpp
class Node {
public:
    point position;
    int id;
    static int next_id;

    Node(point pos) : position(pos), id(next_id++) {}

    bool operator==(const Node& other) const {
        return id == other.id;
    }
};

int Node::next_id = 1; // Start from 1 (0 can be reserved for ground)

struct Placed {
    Tool        type;
    point       pos;
    SDL_Texture* tex;
    int         w, h;
    string name;
    string value;
    Node* node1;  // First connection node
    Node* node2;  // Second connection node
};

static int findDistance(const point& a, const point& b) {
    return static_cast<int>(
            std::sqrt((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y))
    );
}

string promptForValue(SDL_Window* win,
                      SDL_Renderer* ren,
                      TTF_Font* font,
                      int x, int y,
                      const string& title = "Enter value:")
{
    SDL_StartTextInput();
    std::string input;
    bool done = false;
    SDL_Event e;

    // Pre‐compute a background rect for the textbox
    SDL_Rect box{ x, y, 250, 50 };

    while (!done) {
        // 1) Handle events - only process text input events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                done = true;
                break;
            }
            else if (e.type == SDL_TEXTINPUT) {
                input += e.text.text;
            }
            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE && !input.empty()) {
                    input.pop_back();
                }
                else if (e.key.keysym.sym == SDLK_RETURN ||
                         e.key.keysym.sym == SDLK_KP_ENTER)
                {
                    done = true;
                }
                else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    input.clear();
                    done = true;
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button != SDL_BUTTON_LEFT) {
                // Allow right-click to cancel
                input.clear();
                done = true;
            }
        }

        // 2) Clear only the prompt area
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
        SDL_RenderFillRect(ren, &box);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDrawRect(ren, &box);

        // 3) Render title
        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, title.c_str(), SDL_Color{255,255,255,255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(ren, titleSurf);
            SDL_Rect titleRect{ x+5, y+5, titleSurf->w, titleSurf->h };
            SDL_RenderCopy(ren, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_FreeSurface(titleSurf);
        }

        // 4) Render the current input text
        if (!input.empty()) {
            SDL_Surface* surf = TTF_RenderText_Solid(
                    font, input.c_str(), SDL_Color{255,255,255,255}
            );
            if (surf) {
                SDL_Texture* txtTex = SDL_CreateTextureFromSurface(ren, surf);
                SDL_Rect txtDst{ x+5, y+25, surf->w, surf->h };
                SDL_RenderCopy(ren, txtTex, NULL, &txtDst);
                SDL_DestroyTexture(txtTex);
                SDL_FreeSurface(surf);
            }
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_StopTextInput();
    return input;
}
void promptForNameAndValue(SDL_Window* win, SDL_Renderer* ren, TTF_Font* font,
                           int x, int y, Placed& component)
{
    // First prompt for name
    string name = promptForValue(win, ren, font, x, y, "Enter name:");
    if (!name.empty()) {
        component.name = name;

        // Then prompt for value
        string value = promptForValue(win, ren, font, x, y + 60, "Enter value:");
        if (!value.empty()) {
            component.value = value;
        }
    }
}

// Function to show library selection dialog
Tool showLibraryDialog(SDL_Renderer* renderer, TTF_Font* font, const point& mousePos) {
    SDL_Rect dialogRect{ mousePos.x, mousePos.y, 150, 80 };
    SDL_Color bgColor{200, 200, 200, 255};
    SDL_Color textColor{0, 0, 0, 255};

    bool done = false;
    Tool selectedTool = Tool::None;
    SDL_Event e;

    while (!done) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                done = true;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                point p{ e.button.x, e.button.y };

                // Check if click is inside DC button
                SDL_Rect dcButton{ dialogRect.x + 10, dialogRect.y + 30, 60, 20 };
                if (p.x >= dcButton.x && p.x <= dcButton.x + dcButton.w &&
                    p.y >= dcButton.y && p.y <= dcButton.y + dcButton.h) {
                    selectedTool = Tool::VoltageDC;
                    done = true;
                }

                // Check if click is inside AC button
                SDL_Rect acButton{ dialogRect.x + 80, dialogRect.y + 30, 60, 20 };
                if (p.x >= acButton.x && p.x <= acButton.x + acButton.w &&
                    p.y >= acButton.y && p.y <= acButton.y + acButton.h) {
                    selectedTool = Tool::VoltageAC;
                    done = true;
                }

                // Check if click is outside dialog (cancel)
                if (!(p.x >= dialogRect.x && p.x <= dialogRect.x + dialogRect.w &&
                      p.y >= dialogRect.y && p.y <= dialogRect.y + dialogRect.h)) {
                    done = true;
                }
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                done = true;
            }
        }

        // Draw dialog background
        SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_RenderFillRect(renderer, &dialogRect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &dialogRect);

        // Draw title
        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "Voltage Source", textColor);
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_Rect titleRect{ dialogRect.x + 10, dialogRect.y + 5, titleSurf->w, titleSurf->h };
            SDL_RenderCopy(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_FreeSurface(titleSurf);
        }

        // Draw DC button
        SDL_Rect dcButton{ dialogRect.x + 10, dialogRect.y + 30, 60, 20 };
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_RenderFillRect(renderer, &dcButton);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &dcButton);

        SDL_Surface* dcSurf = TTF_RenderText_Solid(font, "DC", textColor);
        if (dcSurf) {
            SDL_Texture* dcTex = SDL_CreateTextureFromSurface(renderer, dcSurf);
            SDL_Rect dcTextRect{ dcButton.x + 20, dcButton.y + 2, dcSurf->w, dcSurf->h };
            SDL_RenderCopy(renderer, dcTex, NULL, &dcTextRect);
            SDL_DestroyTexture(dcTex);
            SDL_FreeSurface(dcSurf);
        }

        // Draw AC button
        SDL_Rect acButton{ dialogRect.x + 80, dialogRect.y + 30, 60, 20 };
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_RenderFillRect(renderer, &acButton);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &acButton);

        SDL_Surface* acSurf = TTF_RenderText_Solid(font, "AC", textColor);
        if (acSurf) {
            SDL_Texture* acTex = SDL_CreateTextureFromSurface(renderer, acSurf);
            SDL_Rect acTextRect{ acButton.x + 20, acButton.y + 2, acSurf->w, acSurf->h };
            SDL_RenderCopy(renderer, acTex, NULL, &acTextRect);
            SDL_DestroyTexture(acTex);
            SDL_FreeSurface(acSurf);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    return selectedTool;
}

// Function to show analysis type selection dialog
AnalysisType showAnalysisTypeDialog(SDL_Renderer* renderer, TTF_Font* font, const point& mousePos) {
    SDL_Rect dialogRect{ mousePos.x, mousePos.y, 200, 130 };
    SDL_Color bgColor{200, 200, 200, 255};
    SDL_Color textColor{0, 0, 0, 255};

    bool done = false;
    AnalysisType selectedAnalysis = AnalysisType::None;
    SDL_Event e;

    while (!done) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                done = true;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                point p{ e.button.x, e.button.y };

                // Check if click is inside Transient button
                SDL_Rect transientButton{ dialogRect.x + 10, dialogRect.y + 30, 180, 20 };
                if (p.x >= transientButton.x && p.x <= transientButton.x + transientButton.w &&
                    p.y >= transientButton.y && p.y <= transientButton.y + transientButton.h) {
                    selectedAnalysis = AnalysisType::Transient;
                    done = true;
                }

                // Check if click is inside AC Sweep button
                SDL_Rect acSweepButton{ dialogRect.x + 10, dialogRect.y + 55, 180, 20 };
                if (p.x >= acSweepButton.x && p.x <= acSweepButton.x + acSweepButton.w &&
                    p.y >= acSweepButton.y && p.y <= acSweepButton.y + acSweepButton.h) {
                    selectedAnalysis = AnalysisType::ACSweep;
                    done = true;
                }

                // Check if click is inside Phase Sweep button
                SDL_Rect phaseSweepButton{ dialogRect.x + 10, dialogRect.y + 80, 180, 20 };
                if (p.x >= phaseSweepButton.x && p.x <= phaseSweepButton.x + phaseSweepButton.w &&
                    p.y >= phaseSweepButton.y && p.y <= phaseSweepButton.y + phaseSweepButton.h) {
                    selectedAnalysis = AnalysisType::PhaseSweep;
                    done = true;
                }

                // Check if click is outside dialog (cancel)
                if (!(p.x >= dialogRect.x && p.x <= dialogRect.x + dialogRect.w &&
                      p.y >= dialogRect.y && p.y <= dialogRect.y + dialogRect.h)) {
                    done = true;
                }
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                done = true;
            }
        }

        // Draw dialog background
        SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_RenderFillRect(renderer, &dialogRect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &dialogRect);

        // Draw title
        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "Select Analysis Type", textColor);
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_Rect titleRect{ dialogRect.x + 10, dialogRect.y + 5, titleSurf->w, titleSurf->h };
            SDL_RenderCopy(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_FreeSurface(titleSurf);
        }

        // Draw Transient button
        SDL_Rect transientButton{ dialogRect.x + 10, dialogRect.y + 30, 180, 20 };
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_RenderFillRect(renderer, &transientButton);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &transientButton);

        SDL_Surface* transientSurf = TTF_RenderText_Solid(font, "Transient Analysis", textColor);
        if (transientSurf) {
            SDL_Texture* transientTex = SDL_CreateTextureFromSurface(renderer, transientSurf);
            SDL_Rect transientTextRect{ transientButton.x + 10, transientButton.y + 2, transientSurf->w, transientSurf->h };
            SDL_RenderCopy(renderer, transientTex, NULL, &transientTextRect);
            SDL_DestroyTexture(transientTex);
            SDL_FreeSurface(transientSurf);
        }

        // Draw AC Sweep button
        SDL_Rect acSweepButton{ dialogRect.x + 10, dialogRect.y + 55, 180, 20 };
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_RenderFillRect(renderer, &acSweepButton);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &acSweepButton);

        SDL_Surface* acSweepSurf = TTF_RenderText_Solid(font, "AC Sweep", textColor);
        if (acSweepSurf) {
            SDL_Texture* acSweepTex = SDL_CreateTextureFromSurface(renderer, acSweepSurf);
            SDL_Rect acSweepTextRect{ acSweepButton.x + 50, acSweepButton.y + 2, acSweepSurf->w, acSweepSurf->h };
            SDL_RenderCopy(renderer, acSweepTex, NULL, &acSweepTextRect);
            SDL_DestroyTexture(acSweepTex);
            SDL_FreeSurface(acSweepSurf);
        }

        // Draw Phase Sweep button
        SDL_Rect phaseSweepButton{ dialogRect.x + 10, dialogRect.y + 80, 180, 20 };
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_RenderFillRect(renderer, &phaseSweepButton);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &phaseSweepButton);

        SDL_Surface* phaseSweepSurf = TTF_RenderText_Solid(font, "Phase Sweep", textColor);
        if (phaseSweepSurf) {
            SDL_Texture* phaseSweepTex = SDL_CreateTextureFromSurface(renderer, phaseSweepSurf);
            SDL_Rect phaseSweepTextRect{ phaseSweepButton.x + 40, phaseSweepButton.y + 2, phaseSweepSurf->w, phaseSweepSurf->h };
            SDL_RenderCopy(renderer, phaseSweepTex, NULL, &phaseSweepTextRect);
            SDL_DestroyTexture(phaseSweepTex);
            SDL_FreeSurface(phaseSweepSurf);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    return selectedAnalysis;
}

// Function to get analysis parameters based on selected analysis type
AnalysisParameters getAnalysisParameters(SDL_Window* window, SDL_Renderer* renderer, TTF_Font* font, AnalysisType analysisType) {
    AnalysisParameters params;
    params.type = analysisType;

    switch (analysisType) {
        case AnalysisType::Transient:
        {
            string maxTimeStepStr = promptForValue(window, renderer, font, 300, 200, "Maximum Timestep:");
            string startTimeStr = promptForValue(window, renderer, font, 300, 260, "Start Time:");
            string endTimeStr = promptForValue(window, renderer, font, 300, 320, "End Time:");

            if (!maxTimeStepStr.empty()) params.maxTimeStep = stod(maxTimeStepStr);
            if (!startTimeStr.empty()) params.startTime = stod(startTimeStr);
            if (!endTimeStr.empty()) params.endTime = stod(endTimeStr);
        }
            break;

        case AnalysisType::ACSweep:
        {
            string startFreqStr = promptForValue(window, renderer, font, 300, 200, "Start Frequency:");
            string endFreqStr = promptForValue(window, renderer, font, 300, 260, "End Frequency:");
            string maxFreqStepStr = promptForValue(window, renderer, font, 300, 320, "Max Frequency Step:");

            if (!startFreqStr.empty()) params.startFreq = stod(startFreqStr);
            if (!endFreqStr.empty()) params.endFreq = stod(endFreqStr);
            if (!maxFreqStepStr.empty()) params.maxFreqStep = stod(maxFreqStepStr);
        }
            break;

        case AnalysisType::PhaseSweep:
        {
            string startPhaseStr = promptForValue(window, renderer, font, 300, 200, "Start Phase (rad):");
            string endPhaseStr = promptForValue(window, renderer, font, 300, 260, "End Phase (rad):");
            string maxStepsStr = promptForValue(window, renderer, font, 300, 320, "Max Steps:");

            if (!startPhaseStr.empty()) params.startPhase = stod(startPhaseStr);
            if (!endPhaseStr.empty()) params.endPhase = stod(endPhaseStr);
            if (!maxStepsStr.empty()) params.maxSteps = stoi(maxStepsStr);
        }
            break;

        case AnalysisType::None:
            break;
    }

    return params;
}

string showSaveDialog(SDL_Window* window, SDL_Renderer* renderer, TTF_Font* font) {
    SDL_StartTextInput();
    string filename;
    bool done = false;
    SDL_Event e;

    // Position the dialog in the center of the screen
    int x = 300, y = 300;
    SDL_Rect box{ x, y, 400, 100 };

    while (!done) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                done = true;
                break;
            }
            else if (e.type == SDL_TEXTINPUT) {
                filename += e.text.text;
            }
            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE && !filename.empty()) {
                    filename.pop_back();
                }
                else if (e.key.keysym.sym == SDLK_RETURN ||
                         e.key.keysym.sym == SDLK_KP_ENTER) {
                    done = true;
                }
                else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    filename.clear();
                    done = true;
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button != SDL_BUTTON_LEFT) {
                filename.clear();
                done = true;
            }
        }

        // Clear only the dialog area
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &box);

        // Render title
        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "Enter filename (with .txt extension):", SDL_Color{0,0,0,255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_Rect titleRect{ x+10, y+10, titleSurf->w, titleSurf->h };
            SDL_RenderCopy(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_FreeSurface(titleSurf);
        }

        // Render the current filename
        if (!filename.empty()) {
            SDL_Surface* surf = TTF_RenderText_Solid(font, filename.c_str(), SDL_Color{0,0,0,255});
            if (surf) {
                SDL_Texture* txtTex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_Rect txtDst{ x+10, y+50, surf->w, surf->h };
                SDL_RenderCopy(renderer, txtTex, NULL, &txtDst);
                SDL_DestroyTexture(txtTex);
                SDL_FreeSurface(surf);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_StopTextInput();
    return filename;
}

// Function to save the schematic to a file
bool saveSchematic(const string& filename, const vector<Placed>& placed,
                   const vector<WireSegment>& wires, const AnalysisParameters& analysis) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not create file " << filename << endl;
        return false;
    }

    // Write components
    for (const auto& comp : placed) {
        string typeStr;
        switch (comp.type) {
            case Tool::Resistor: typeStr = "Resistor"; break;
            case Tool::Capacitor: typeStr = "Capacitor"; break;
            case Tool::Inductor: typeStr = "Inductor"; break;
            case Tool::Diode: typeStr = "Diode"; break;
            case Tool::VoltageDC: typeStr = "VoltageDC"; break;
            case Tool::VoltageAC: typeStr = "VoltageAC"; break;
            case Tool::GND: typeStr = "GND"; break;      // Add this
            case Tool::Port: typeStr = "Port"; break;    // Add this
            default: typeStr = "Unknown";
        }
        file << typeStr << " " << comp.pos.x << " " << comp.pos.y;
        if (!comp.name.empty()) {
            file << " name=" << comp.name;
        }
        if (!comp.value.empty()) {
            file << " value=" << comp.value;
        }
        file << "\n";
    }

    // Write wires
    file << "Wires:\n";
    for (const auto& wire : wires) {
        file << wire.start.x << " " << wire.start.y << " "
             << wire.end.x << " " << wire.end.y << "\n";
    }

    // Write analysis parameters
    file << "Analysis:\n";
    switch (analysis.type) {
        case AnalysisType::Transient:
            file << "Transient " << analysis.maxTimeStep << " "
                 << analysis.startTime << " " << analysis.endTime << "\n";
            break;
        case AnalysisType::ACSweep:
            file << "ACSweep " << analysis.startFreq << " "
                 << analysis.endFreq << " " << analysis.maxFreqStep << "\n";
            break;
        case AnalysisType::PhaseSweep:
            file << "PhaseSweep " << analysis.startPhase << " "
                 << analysis.endPhase << " " << analysis.maxSteps << "\n";
            break;
        case AnalysisType::None:
            file << "None\n";
            break;
    }

    file.close();
    cout << "Schematic saved to " << filename << endl;
    return true;
}




// Returns 0 on success, 1 on failure
int runVisualEditor()
{
    const int SCREEN_WIDTH  = 1000;
    const int SCREEN_HEIGHT =  800;
    const int probeButtonSize = 40;
    const int probeButtonY = 10;


    // 1) Define toolbar buttons
    int toolbarW = 40;
    button newS  (point( 17, 15), 5);
    button openB(point( 35, 15), 5);
    button save (point( 58, 15), 5);
    button runB (point(231, 15), 8);
    button wireB(point(399, 19), 5);
    button GND  (point(419, 19), 5);
    button port (point(440, 17), 5);
    button resB (point(461, 19), 5);
    button capB (point(481, 19), 5);
    button indB (point(502, 18), 5);
    button dioB (point(523, 19), 5);
    button libB (point(544, 19), 5);
    button exitB(point(981, 15),10);

    // 2) State for drawing & placement
    Tool             activeTool = Tool::None;
    point            mousePos{0,0};
    std::vector<Placed> placed;
    std::vector<WireSegment> wires;

    // Wire routing state
    bool isRouting = false;
    point routingStart{0, 0};
    point routingCurrent{0, 0};

    // Help display state
    bool showHelp = false;

    // Analysis state
    AnalysisParameters currentAnalysis;

    // 3) SDL init
    if (SDL_Init(SDL_INIT_VIDEO) < 0
        || !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        std::cerr << "SDL/IMG init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window*   window   = SDL_CreateWindow(
            "SPICE Editor",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SCREEN_WIDTH, SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(
            window, -1, SDL_RENDERER_ACCELERATED
    );
    if (!window || !renderer) {
        std::cerr << "CreateWindow/CreateRenderer failed: "
                  << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }
    if (TTF_Init() < 0) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << "\n";
        // clean up SDL & exit…
    }
    TTF_Font* font = TTF_OpenFont("C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\arial.ttf", 16);
    if (!font) {
        std::cerr << "Failed to load font: " << TTF_GetError() << "\n";
        // clean up…
    }

    // 4) Load workspace background
    SDL_Texture* background = IMG_LoadTexture(
            renderer,
            "C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\pictures\\workspace.png"
    );
    if (!background) {
        std::cerr << "Failed background: "
                  << IMG_GetError() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // 5) Helper to load & scale a PNG
    auto loadScaled = [&](const std::string& path,
                          float scale,
                          int& outW,
                          int& outH)
            -> SDL_Texture*
    {
        SDL_Texture* texSmall = IMG_LoadTexture(renderer, path.c_str());
        if (!texSmall) {
            std::cerr << "Load " << path
                      << " failed: " << IMG_GetError() << "\n";
            return nullptr;
        }
        int w0,h0;
        SDL_QueryTexture(texSmall, nullptr, nullptr, &w0, &h0);
        SDL_Texture* big = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_TARGET,
                int(w0*scale),
                int(h0*scale)
        );
        SDL_SetTextureBlendMode(big, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(renderer, big);
        SDL_RenderCopy(renderer, texSmall, nullptr, nullptr);
        SDL_SetRenderTarget(renderer, nullptr);
        SDL_DestroyTexture(texSmall);
        SDL_QueryTexture(big, nullptr, nullptr, &outW, &outH);
        return big;
    };

    // 6) Load & scale each symbol
    int resW, resH, capW, capH, indW, indH, dioW, dioH, dcW, dcH, acW, acH,gndW, gndH, portW, portH;

    SDL_Texture* gndTex = loadScaled(
            "C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\pictures\\gnd.png", // Update path as needed
            0.02f, gndW, gndH
    );
    SDL_Texture* portTex = loadScaled(
            "C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\pictures\\port.png", // Update path as needed
            0.02f, portW, portH
    );

    SDL_Texture* resistorTex  = loadScaled(
            "C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\pictures\\resistor.png",
            0.4f, resW,  resH
    );
    SDL_Texture* capacitorTex = loadScaled(
            "C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\pictures\\capacitor.png",
            0.1f, capW,  capH
    );
    SDL_Texture* inductorTex  = loadScaled(
            "C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\pictures\\inductor.png",
            0.1f, indW,  indH
    );
    SDL_Texture* diodeTex     = loadScaled(
            "C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\pictures\\diode.png",
            0.2f, dioW,  dioH
    );
    // Load voltage source textures (replace with actual paths)
    SDL_Texture* voltageDCTex = loadScaled(
            "C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\pictures\\voltage_source.png",
            0.2f, dcW, dcH
    );
    SDL_Texture* voltageACTex = loadScaled(
            "C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\pictures\\Sine_Source.png",
            0.2f, acW, acH
    );

    // 7) Main loop
    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    quit = true;
                    break;

                case SDL_MOUSEMOTION:
                    mousePos.x = e.motion.x;
                    mousePos.y = e.motion.y;

                    if (isRouting) {
                        routingCurrent = mousePos;
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == SDL_BUTTON_LEFT &&
                        e.button.clicks == 2)        // ← double-click
                    {
                        point p{ e.button.x, e.button.y };
                        // Hit-test all placed parts
                        for (auto& part : placed) {
                            SDL_Rect r{
                                    part.pos.x - part.w/2,
                                    part.pos.y - part.h/2,
                                    part.w, part.h
                            };
                            SDL_Point mp{ p.x, p.y };
                            if (SDL_PointInRect(&mp, &r)) {
                                // Found the part—prompt for name and value
                                promptForNameAndValue(
                                        window, renderer, font,
                                        r.x, r.y - 35,   // place box just above the part
                                        part
                                );
                                break;  // only edit one
                            }
                        }
                    }

                    if (e.button.button != SDL_BUTTON_LEFT) break;

                    {
                        point p{ e.button.x, e.button.y };

                        // toolbar?
                        if (p.y <= toolbarW) {
                            if      (findDistance(newS.center, p) <= newS.delta)  std::cout<<"New\n";
                            else if (findDistance(openB.center,p) <= openB.delta) std::cout<<"Open\n";
                            else if (findDistance(save.center,p)  <= save.delta) { std::cout<<"Save\n";
                                // Show save dialog and save the schematic
                                string filename = showSaveDialog(window, renderer, font);
                                if (!filename.empty()) {
                                    if (saveSchematic(filename, placed, wires, currentAnalysis)) {
                                        cout << "Schematic saved successfully!" << endl;
                                    } else {
                                        cout << "Failed to save schematic!" << endl;
                                    }
                                }

                            }
                            else if (findDistance(runB.center,p)  <= runB.delta)  {
                                std::cout<<"Run Analysis\n";
                                // Show analysis type selection dialog
                                AnalysisType selectedAnalysis = showAnalysisTypeDialog(renderer, font, mousePos);
                                if (selectedAnalysis != AnalysisType::None) {
                                    currentAnalysis = getAnalysisParameters(window, renderer, font, selectedAnalysis);

                                    // Print analysis parameters (for debugging)
                                    std::cout << "Analysis parameters set:\n";
                                    switch (currentAnalysis.type) {
                                        case AnalysisType::Transient:
                                            std::cout << "Transient Analysis:\n";
                                            std::cout << "  Max Time Step: " << currentAnalysis.maxTimeStep << "\n";
                                            std::cout << "  Start Time: " << currentAnalysis.startTime << "\n";
                                            std::cout << "  End Time: " << currentAnalysis.endTime << "\n";
                                            break;
                                        case AnalysisType::ACSweep:
                                            std::cout << "AC Sweep Analysis:\n";
                                            std::cout << "  Start Freq: " << currentAnalysis.startFreq << "\n";
                                            std::cout << "  End Freq: " << currentAnalysis.endFreq << "\n";
                                            std::cout << "  Max Freq Step: " << currentAnalysis.maxFreqStep << "\n";
                                            break;
                                        case AnalysisType::PhaseSweep:
                                            std::cout << "Phase Sweep Analysis:\n";
                                            std::cout << "  Start Phase: " << currentAnalysis.startPhase << " rad\n";
                                            std::cout << "  End Phase: " << currentAnalysis.endPhase << " rad\n";
                                            std::cout << "  Max Steps: " << currentAnalysis.maxSteps << "\n";
                                            break;
                                        case AnalysisType::None:
                                            break;
                                    }
                                }
                            }
                            else if (findDistance(wireB.center,p) <= wireB.delta) {
                                std::cout<<"Wire tool\n";
                                activeTool = Tool::Wire;
                                isRouting = false; // Reset routing state
                            }

                            else if (findDistance(resB.center,p)  <= resB.delta) {
                                std::cout<<"Resistor tool\n";
                                activeTool = Tool::Resistor;
                                isRouting = false; // Exit routing mode
                            }
                            else if (findDistance(capB.center,p) <= capB.delta) {
                                std::cout<<"Capacitor tool\n";
                                activeTool = Tool::Capacitor;
                                isRouting = false;
                            }
                            else if (findDistance(indB.center,p) <= indB.delta) {
                                std::cout<<"Inductor tool\n";
                                activeTool = Tool::Inductor;
                                isRouting = false;
                            }
                            else if (findDistance(dioB.center,p) <= dioB.delta) {
                                std::cout<<"Diode tool\n";
                                activeTool = Tool::Diode;
                                isRouting = false;
                            }
                            else if (findDistance(libB.center,p) <= libB.delta)  {
                                std::cout<<"Library\n";
                                // Show library dialog to choose voltage source type
                                Tool selected = showLibraryDialog(renderer, font, mousePos);
                                if (selected != Tool::None) {
                                    activeTool = selected;
                                    isRouting = false;
                                }
                            }
                            else if (findDistance(exitB.center,p)<= exitB.delta) quit = true;

                            else if (findDistance(GND.center,p) <= GND.delta) {
                                std::cout << "GND tool\n";
                                activeTool = Tool::GND;
                                isRouting = false;
                            }
                            else if (findDistance(port.center,p) <= port.delta) {
                                std::cout << "Port tool\n";
                                activeTool = Tool::Port;
                                isRouting = false;
                            }
                        }
                            // Wire routing logic
                        else if (activeTool == Tool::Wire) {
                            if (!isRouting) {
                                // First click - start routing at exact mouse position
                                isRouting = true;
                                routingStart = p;
                                routingCurrent = p;
                                std::cout << "Started wire at: " << p.x << ", " << p.y << std::endl;
                            } else {
                                // Second click - finish routing at exact mouse position
                                wires.push_back({routingStart, p});
                                isRouting = false;
                                std::cout << "Finished wire from: " << routingStart.x << " << routingStart.y"
                                        << " to: " << p.x << ", " << p.y << std::endl;
                            }
                        }
                            // place chosen symbol
                        else if (activeTool != Tool::None && activeTool != Tool::Wire) {
                            SDL_Texture* tex = nullptr;
                            int w=0,h=0;
                            switch(activeTool){
                                case Tool::Resistor:  tex = resistorTex;  w=resW; h=resH; break;
                                case Tool::Capacitor: tex = capacitorTex; w=capW; h=capH; break;
                                case Tool::Inductor:  tex = inductorTex;  w=indW; h=indH; break;
                                case Tool::Diode:     tex = diodeTex;     w=dioW; h=dioH; break;
                                case Tool::VoltageDC: tex = voltageDCTex; w=dcW;  h=dcH;  break;
                                case Tool::VoltageAC: tex = voltageACTex; w=acW;  h=acH;  break;
                                case Tool::GND:       tex=gndTex;       w=gndW; h=gndH; break;  // Add this
                                case Tool::Port:      tex=portTex;      w=portW; h=portH; break; // Add this
                                default: ;
                            }
                            placed.push_back({ activeTool, p, tex, w, h });
                            activeTool = Tool::None;
                        }
                    }
                    break;

                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        if (isRouting) {
                            // Cancel routing with ESC key
                            isRouting = false;
                            std::cout << "Wire routing cancelled" << std::endl;
                        } else if (activeTool == Tool::Wire) {
                            // Exit wire tool with ESC key
                            activeTool = Tool::None;
                            std::cout << "Exited wire tool" << std::endl;
                        }
                    }
                        // Add keyboard shortcuts for components
                    else if (e.key.keysym.sym == SDLK_r) {
                        std::cout << "Resistor tool (keyboard shortcut)\n";
                        activeTool = Tool::Resistor;
                        isRouting = false;
                    }
                    else if (e.key.keysym.sym == SDLK_l) {
                        std::cout << "Inductor tool (keyboard shortcut)\n";
                        activeTool = Tool::Inductor;
                        isRouting = false;
                    }
                    else if (e.key.keysym.sym == SDLK_c) {
                        std::cout << "Capacitor tool (keyboard shortcut)\n";
                        activeTool = Tool::Capacitor;
                        isRouting = false;
                    }
                    else if (e.key.keysym.sym == SDLK_d) {
                        std::cout << "Diode tool (keyboard shortcut)\n";
                        activeTool = Tool::Diode;
                        isRouting = false;
                    }
                    else if (e.key.keysym.sym == SDLK_w) {
                        std::cout << "Wire tool (keyboard shortcut)\n";
                        activeTool = Tool::Wire;
                        isRouting = false;
                    }
                    else if (e.key.keysym.sym == SDLK_v) {
                        // Show library dialog for voltage sources
                        Tool selected = showLibraryDialog(renderer, font, mousePos);
                        if (selected != Tool::None) {
                            activeTool = selected;
                            isRouting = false;
                        }
                    }
                    else if (e.key.keysym.sym == SDLK_a) {
                        // Run analysis with keyboard shortcut
                        AnalysisType selectedAnalysis = showAnalysisTypeDialog(renderer, font, mousePos);
                        if (selectedAnalysis != AnalysisType::None) {
                            currentAnalysis = getAnalysisParameters(window, renderer, font, selectedAnalysis);
                            std::cout << "Analysis started via keyboard shortcut\n";
                        }
                    }
                    else if (e.key.keysym.sym == SDLK_h) {
                        // Toggle help display with H key
                        showHelp = !showHelp;
                        std::cout << "Help " << (showHelp ? "enabled" : "disabled") << std::endl;
                    }
                    else if (e.key.keysym.sym == SDLK_g) {
                        std::cout << "GND tool (keyboard shortcut)\n";
                        activeTool = Tool::GND;
                        isRouting = false;
                    }
                    else if (e.key.keysym.sym == SDLK_p) {
                        std::cout << "Port tool (keyboard shortcut)\n";
                        activeTool = Tool::Port;
                        isRouting = false;
                    }
                    break;
            }
        }

        // render
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, background, nullptr, nullptr);

        // Draw all existing wires
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black for wires
        for (const auto& wire : wires) {
            SDL_RenderDrawLine(renderer, wire.start.x, wire.start.y, wire.end.x, wire.end.y);

            // Draw small circles at wire endpoints for better visibility
            const int endpointRadius = 3;
            for (int i = -endpointRadius; i <= endpointRadius; i++) {
                for (int j = -endpointRadius; j <= endpointRadius; j++) {
                    if (i*i + j*j <= endpointRadius*endpointRadius) {
                        SDL_RenderDrawPoint(renderer, wire.start.x + i, wire.start.y + j);
                        SDL_RenderDrawPoint(renderer, wire.end.x + i, wire.end.y + j);
                    }
                }
            }
        }

        // Draw current routing wire (preview)
        if (isRouting) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 128); // Red with transparency for preview
            SDL_RenderDrawLine(renderer, routingStart.x, routingStart.y, routingCurrent.x, routingCurrent.y);

            // Draw circles at both ends of the preview wire
            const int previewRadius = 2;
            for (int i = -previewRadius; i <= previewRadius; i++) {
                for (int j = -previewRadius; j <= previewRadius; j++) {
                    if (i*i + j*j <= previewRadius*previewRadius) {
                        SDL_RenderDrawPoint(renderer, routingStart.x + i, routingStart.y + j);
                        SDL_RenderDrawPoint(renderer, routingCurrent.x + i, routingCurrent.y + j);
                    }
                }
            }
        }

        // draw placed items
        for (auto& it : placed) {
            SDL_Rect dst{
                    it.pos.x - it.w/2,
                    it.pos.y - it.h/2,
                    it.w, it.h
            };
            SDL_RenderCopy(renderer, it.tex, nullptr, &dst);
            if (!it.name.empty()) {
                SDL_Surface* surf = TTF_RenderText_Solid(
                        font, it.name.c_str(),
                        SDL_Color{0,0,0,255}
                );
                SDL_Texture* texV = SDL_CreateTextureFromSurface(
                        renderer, surf
                );
                SDL_Rect valDst{ dst.x, dst.y + dst.h - 30, surf->w, surf->h };
                SDL_FreeSurface(surf);

                SDL_RenderCopy(renderer, texV, NULL, &valDst);
                SDL_DestroyTexture(texV);
            }
            // draw its value, if any
            if (!it.value.empty()) {
                SDL_Surface* surf = TTF_RenderText_Solid(
                        font, it.value.c_str(),
                        SDL_Color{0,0,0,255}
                );
                SDL_Texture* texV = SDL_CreateTextureFromSurface(
                        renderer, surf
                );
                SDL_Rect valDst{ dst.x, dst.y + dst.h - 8, surf->w, surf->h };
                SDL_FreeSurface(surf);

                SDL_RenderCopy(renderer, texV, NULL, &valDst);
                SDL_DestroyTexture(texV);
            }
        }

        // live preview for components
        if (activeTool != Tool::None && activeTool != Tool::Wire) {
            SDL_Texture* tex = nullptr;
            int w=0, h=0;
            switch(activeTool){
                case Tool::Resistor:  tex=resistorTex;  w=resW;  h=resH;  break;
                case Tool::Capacitor: tex=capacitorTex; w=capW; h=capH; break;
                case Tool::Inductor:  tex=inductorTex;  w=indW; h=indH; break;
                case Tool::Diode:     tex=diodeTex;     w=dioW; h=dioH; break;
                case Tool::VoltageDC: tex=voltageDCTex; w=dcW;  h=dcH;  break;
                case Tool::VoltageAC: tex=voltageACTex; w=acW;  h=acH;  break;
                case Tool::GND:       tex = gndTex;       w=gndW; h=gndH; break;  // Add this
                case Tool::Port:      tex = portTex;      w=portW; h=portH; break; // Add this
                default: ;
            }
            SDL_Rect pr{
                    mousePos.x - w/2,
                    mousePos.y - h/2,
                    w, h
            };
            SDL_SetTextureAlphaMod(tex, 180);
            SDL_RenderCopy(renderer, tex, nullptr, &pr);
            SDL_SetTextureAlphaMod(tex, 255);
        }

        // Draw crosshair cursor when in wire mode
        if (activeTool == Tool::Wire) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue crosshair
            SDL_RenderDrawLine(renderer, mousePos.x - 10, mousePos.y, mousePos.x + 10, mousePos.y);
            SDL_RenderDrawLine(renderer, mousePos.x, mousePos.y - 10, mousePos.x, mousePos.y + 10);
        }

        // Display keyboard shortcuts help
        if (showHelp) {
            std::string helpText = "Shortcuts: R=Resistor, C=Capacitor, L=Inductor, D=Diode, W=Wire, V=Voltage Source, A=Run Analysis, H=Toggle Help, ESC=Cancel";
            SDL_Surface* helpSurf = TTF_RenderText_Solid(font, helpText.c_str(), SDL_Color{0,0,0,255});
            if (helpSurf) {
                SDL_Texture* helpTex = SDL_CreateTextureFromSurface(renderer, helpSurf);
                SDL_Rect helpRect{10, SCREEN_HEIGHT - 30, helpSurf->w, helpSurf->h};
                SDL_RenderCopy(renderer, helpTex, NULL, &helpRect);
                SDL_DestroyTexture(helpTex);
                SDL_FreeSurface(helpSurf);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // Cap at ~60 FPS
    }

    // 8) Cleanup all SDL objects
    SDL_DestroyTexture(resistorTex);
    SDL_DestroyTexture(capacitorTex);
    SDL_DestroyTexture(inductorTex);
    SDL_DestroyTexture(diodeTex);
    SDL_DestroyTexture(voltageDCTex);
    SDL_DestroyTexture(voltageACTex);
    SDL_DestroyTexture(gndTex);
    SDL_DestroyTexture(portTex);
    SDL_DestroyTexture(background);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();

    IMG_Quit();
    SDL_Quit();
    return 0;
}