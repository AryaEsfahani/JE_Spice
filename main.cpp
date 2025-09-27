#include "bits/stdc++.h"
#include "fstream"
#include "external/ghc/filesystem.hpp"
#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"
#include "SDL2/SDL_ttf.h"
#include <complex>
#include "schematic editor.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif
// by Aria Esfahani  aria.esfahani.ee.sharif@gmail.com
using namespace std::complex_literals;
using namespace std;
namespace fs = ghc::filesystem;
const int GRID_SIZE = 40;
const int WORKSPACE_WIDTH = 1200;
const int WORKSPACE_HEIGHT = 800;
const int COMPONENT_WIDTH = 80;
const int COMPONENT_HEIGHT = 40;

enum ComponentType {
    COMPONENT_NONE,
    COMPONENT_RESISTOR,
    COMPONENT_CAPACITOR,
    COMPONENT_INDUCTOR,
    COMPONENT_DIODE,
    COMPONENT_VOLTAGE_SOURCE,
    COMPONENT_GROUND,
    COMPONENT_PHASE_SOURCE,
    COMPONENT_WIRE,
    COMPONENT_JUNCTION
};
enum ProbeType {
    PROBE_NONE,
    PROBE_VOLTAGE,
    PROBE_CURRENT,
    PROBE_POWER
};
int selectedNode = -1;
int wireStartNode = -1;
bool isDrawingWire = false;
struct GraphicalNode {
    int x, y;
    string name;
    bool connected;
};

struct GraphicalComponent {
    ComponentType type;
    string name;
    string value;
    int x, y;  // Center position of the component
    bool selected;
    int node1, node2;  // Indices of connected nodes
};


// Add these global variables for the schematic editor
vector<GraphicalNode> graphicalNodes;
vector<GraphicalComponent> graphicalComponents;
ComponentType currentTool = COMPONENT_NONE;

int dragStartX = 0, dragStartY = 0;
bool isDragging = false;
SDL_Texture* resistorTexture = nullptr;
SDL_Texture* capacitorTexture = nullptr;
SDL_Texture* inductorTexture = nullptr;
SDL_Texture* diodeTexture = nullptr;
SDL_Texture* voltageSourceTexture = nullptr;
SDL_Texture* groundTexture = nullptr;
SDL_Texture* phaseSourceTexture = nullptr;
SDL_Texture* workspaceTexture = nullptr;
SDL_Texture* nameValueTexture = nullptr;
// --------------------------
// Plot Data Structures
// --------------------------
struct SignalConfig {
    string name;
    SDL_Color color;
    bool userChanged;
    double scaleFactor;  // For unit conversion
    string unit;
};

struct PlotData {
    string analysisType;  // "TRAN", "DC", "AC"
    vector<double> xValues; // Time or Frequency values
    vector<vector<double>> yValues; // Each inner vector is a signal's data
    vector<SignalConfig> signals;
    double maxTimestep = 0.01;
    bool showLegend = true;

    // New fields for storing full solution data
    vector<string> allNodeNames;            // Names of all non-ground nodes
    vector<vector<double>> nodeVoltages;    // Node voltages at each time step
    vector<vector<double>> sourceCurrents;  // Voltage source currents
};

// Global plot storage
unordered_map<string, PlotData> plotsMap;
PlotData currentPlot;

// --------------------------
// Node Class (for nodal analysis)
// --------------------------
class node {
public:
    string name;
    node(const string &n) : name(n) {}
};

// Global node library: maps node names to their node objects.
unordered_map<string, node*> nodesMap;

// Helper function that returns a pointer to the node with the given name.
// If not present, a new node is created and stored.
node* getOrCreateNode(const string &nodeName) {
    if(nodesMap.count(nodeName))
        return nodesMap[nodeName];
    else {
        node* newNode = new node(nodeName);
        nodesMap[nodeName] = newNode;
        return newNode;
    }
}

// --------------------------
// Component Classes
// --------------------------
class component {
public:
    string name;
    node* n1;  // pointer to first node
    node* n2;  // pointer to second node
    double value;
    virtual ~component() {}
};
class WirelessVoltageSource : public component {
public:
    string ipAddress;
    int port;
    bool isServer;
    double voltageValue;

    WirelessVoltageSource() : component(), port(8080), isServer(false), voltageValue(0.0) {}

    void updateFromNetwork() {
#ifdef _WIN32
        if (isServer) {
            runServer();
        } else {
            voltageValue = runClient();
            value = voltageValue;
        }
#endif
    }

private:
#ifdef _WIN32
    void runServer() {
        WSADATA wsaData;
        SOCKET serverSocket, clientSocket;
        sockaddr_in serverAddr, clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        // Initialize Winsock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "WSAStartup failed." << endl;
            return;
        }

        // Create socket
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET) {
            cerr << "Socket creation failed." << endl;
            WSACleanup();
            return;
        }

        // Setup server address
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        // Bind socket
        if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "Bind failed." << endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }

        // Listen for connections
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            cerr << "Listen failed." << endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }

        cout << "Server listening on port " << port << endl;

        // Accept connection
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "Accept failed." << endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }

        // Send voltage value
        string voltageStr = to_string(voltageValue);
        send(clientSocket, voltageStr.c_str(), voltageStr.size(), 0);

        // Cleanup
        closesocket(clientSocket);
        closesocket(serverSocket);
        WSACleanup();
    }

    double runClient() {
        WSADATA wsaData;
        SOCKET clientSocket;
        sockaddr_in serverAddr;

        // Initialize Winsock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "WSAStartup failed." << endl;
            return 0.0;
        }

        // Create socket
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "Socket creation failed." << endl;
            WSACleanup();
            return 0.0;
        }

        // Setup server address
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(ipAddress.c_str());
        serverAddr.sin_port = htons(port);

        // Connect to server
        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "Connect failed." << endl;
            closesocket(clientSocket);
            WSACleanup();
            return 0.0;
        }

        // Receive voltage value
        char buffer[1024];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived == SOCKET_ERROR) {
            cerr << "Recv failed." << endl;
            closesocket(clientSocket);
            WSACleanup();
            return 0.0;
        }

        buffer[bytesReceived] = '\0';
        double receivedVoltage = stod(buffer);

        // Cleanup
        closesocket(clientSocket);
        WSACleanup();

        return receivedVoltage;
    }
#endif
};

class resistor : public component {
    // Additional resistor-specific details may be added here.
};

class capacitor : public component {
    // Additional capacitor-specific details may be added here.
};

class inductor : public component {
    // Additional inductor-specific details may be added here.
};

class diode : public component {
public:
    string model;  // Diode model (e.g., "D" for standard, "Z" for Zener).
};

class ground : public component {
public:
    // For ground, we store just one node pointer.
    node* n;  // the node that is grounded
};

// --------------------------
// Independent and Dependent Sources
// --------------------------

// Independent Voltage Source (for DC analysis) and for sinusoidal sources.
class vsource : public component {
public:
    double dcValue;      // For DC analysis
    bool isSinusoidal;   // True if the source is sinusoidal
    double Voffset;      // DC offset (used for DC analysis if sinusoidal)
    double Vamplitude;   // Wave amplitude (for SIN sources)
    double Frequency;    // Frequency in Hz (for SIN sources)

    // Add these for AC analysis
    double ACMagnitude;  // Magnitude for AC analysis
    double ACPhase;      // Phase for AC analysis (in degrees)

    vsource() : ACMagnitude(0.0), ACPhase(0.0) {} // Initialize AC parameters
};

// Independent Current Source.
class currentSource : public component {
public:
    double dcCurrent;   // DC current value (in amperes)
};
class phaseVoltageSource : public component {
public:
    double baseFrequency;  // ω_base in rad/s (but we'll store in Hz for consistency)
    double amplitude;      // Amplitude of the cosine wave
    double phaseOffset;    // φ in radians

    phaseVoltageSource() : baseFrequency(1000.0), amplitude(1.0), phaseOffset(0.0) {}
};

// --------------------------
// Parsing Functions
// --------------------------
double parseResistanceValue(const string &valStr, bool &isValid) {
    int idx = 0;
    while (idx < (int)valStr.size() &&
           (isdigit(valStr[idx]) || valStr[idx]=='+' || valStr[idx]=='-' ||
            valStr[idx]=='.' || valStr[idx]=='e' || valStr[idx]=='E')) {
        idx++;
    }
    string numStr = valStr.substr(0, idx);
    string unitStr = valStr.substr(idx);
    double multiplier = 1.0;
    if(unitStr == "" || unitStr == "Ω" || unitStr == "ohm" || unitStr == "Ohm" ||
       unitStr == "ohms" || unitStr == "Ohms")
        multiplier = 1.0;
    else if(unitStr == "k" || unitStr == "kΩ" || unitStr == "K" || unitStr == "KΩ")
        multiplier = 1e3;
    else if(unitStr == "M" || unitStr == "Meg" || unitStr == "MΩ" || unitStr == "MegΩ")
        multiplier = 1e6;
    else {
        isValid = false;
        return 0;
    }
    try {
        double value = stod(numStr);
        isValid = true;
        return value * multiplier;
    } catch (...) {
        isValid = false;
        return 0;
    }
}

double parseCapacitanceValue(const string &valStr, bool &isValid) {
    int idx = 0;
    while (idx < (int)valStr.size() &&
           (isdigit(valStr[idx]) || valStr[idx]=='+' || valStr[idx]=='-' ||
            valStr[idx]=='.' || valStr[idx]=='e' || valStr[idx]=='E')) {
        idx++;
    }
    string numStr = valStr.substr(0, idx);
    string unitStr = valStr.substr(idx);
    double multiplier = 1.0;
    if(unitStr == "" || unitStr == "F" || unitStr == "f")
        multiplier = 1.0;
    else if(unitStr == "u" || unitStr == "uF" || unitStr == "μF" || unitStr == "UF")
        multiplier = 1e-6;
    else if(unitStr == "n" || unitStr == "nF" || unitStr == "NF")
        multiplier = 1e-9;
    else if(unitStr == "p" || unitStr == "pF" || unitStr == "PF")
        multiplier = 1e-12;
    else {
        isValid = false;
        return 0;
    }
    try {
        double value = stod(numStr);
        isValid = true;
        return value * multiplier;
    } catch (...) {
        isValid = false;
        return 0;
    }
}

double parseInductanceValue(const string &valStr, bool &isValid) {
    int idx = 0;
    while (idx < (int)valStr.size() &&
           (isdigit(valStr[idx]) || valStr[idx]=='+' || valStr[idx]=='-' ||
            valStr[idx]=='.' || valStr[idx]=='e' || valStr[idx]=='E')) {
        idx++;
    }
    string numStr = valStr.substr(0, idx);
    string unitStr = valStr.substr(idx);
    double multiplier = 1.0;
    if(unitStr == "" || unitStr == "H" || unitStr == "h")
        multiplier = 1.0;
    else if(unitStr == "m" || unitStr == "mH" || unitStr == "mh")
        multiplier = 1e-3;
    else if(unitStr == "u" || unitStr == "uH" || unitStr == "μH" || unitStr == "UH")
        multiplier = 1e-6;
    else {
        isValid = false;
        return 0;
    }
    try {
        double value = stod(numStr);
        isValid = true;
        return value * multiplier;
    } catch (...) {
        isValid = false;
        return 0;
    }
}

// --------------------------
// Linear System Solver (Gaussian Elimination)
// --------------------------
vector<double> solveLinearSystem(vector<vector<double>> A, vector<double> b) {
    int n = A.size();
    for (int i = 0; i < n; i++) {
        int pivot = i;
        for (int r = i+1; r < n; r++){
            if(fabs(A[r][i]) > fabs(A[pivot][i]))
                pivot = r;
        }
        if(fabs(A[pivot][i]) < 1e-12)
            throw runtime_error("Singular matrix encountered in DC analysis.");
        swap(A[i], A[pivot]);
        swap(b[i], b[pivot]);
        double factor = A[i][i];
        for (int j = i; j < n; j++)
            A[i][j] /= factor;
        b[i] /= factor;
        for (int r = i+1; r < n; r++){
            double mult = A[r][i];
            for (int j = i; j < n; j++)
                A[r][j] -= mult * A[i][j];
            b[r] -= mult * b[i];
        }
    }
    vector<double> x(n, 0.0);
    for (int i = n-1; i>=0; i--){
        x[i] = b[i];
        for (int j = i+1; j < n; j++){
            x[i] -= A[i][j] * x[j];
        }
    }
    return x;
}


// --------------------------
// Helper Functions for Plot Configuration
// --------------------------
SignalConfig createSignalConfig(const string& name, const string& unit = "") {
    static vector<SDL_Color> defaultColors = {
            {255, 0, 0, 255},     // Red
            {0, 0, 255, 255},     // Blue
            {0, 180, 0, 255},     // Green
            {180, 0, 180, 255},   // Purple
            {0, 180, 180, 255},   // Cyan
            {180, 180, 0, 255},   // Yellow
            {255, 128, 0, 255},   // Orange
            {128, 0, 255, 255}    // Violet
    };

    static int colorIndex = 0;

    SignalConfig config;
    config.name = name;
    config.color = defaultColors[colorIndex];
    config.userChanged = false;
    config.unit = unit;

    // Set scale factor based on unit
    if(unit == "mV") config.scaleFactor = 1000.0;
    else if(unit == "μV") config.scaleFactor = 1000000.0;
    else if(unit == "mA") config.scaleFactor = 1000.0;
    else if(unit == "μA") config.scaleFactor = 1000000.0;
    else config.scaleFactor = 1.0;

    // Cycle to next color
    colorIndex = (colorIndex + 1) % defaultColors.size();

    return config;
}
void updateWirelessVoltageSources(const vector<component*>& circuit) {
    for (auto comp : circuit) {
        WirelessVoltageSource* wvs = dynamic_cast<WirelessVoltageSource*>(const_cast<component*>(comp));
        if (wvs) {
            wvs->updateFromNetwork();
        }
    }
}

// --------------------------
// DC Analysis using MNA
// --------------------------
void performDCAnalysis(const string &sweepSource, double startVal, double endVal, double incr,
                       const vector<string> &measVars, const vector<component*> &circuit) {
    // Reset plot data
    updateWirelessVoltageSources(circuit);
    currentPlot = PlotData();
    currentPlot.analysisType = "DC";


    // Create signal configurations
    for(const auto& var : measVars) {
        string unit = "";
        if(var.find("V(") != string::npos) unit = "V";
        else if(var.find("I(") != string::npos) unit = "A";
        currentPlot.signals.push_back(createSignalConfig(var, unit));
    }

    // Initialize yValues storage (one vector per signal)
    currentPlot.yValues.resize(measVars.size());

    // Identify ground node from the circuit by scanning for a ground element.
    string groundName = "";
    for(auto comp : circuit) {
        ground* g = dynamic_cast<ground*>(comp);
        if(g && g->n) {
            groundName = g->n->name;
            break;
        }
    }
    if(groundName == ""){
        cout << "ERROR: No ground node defined. Please add a ground element." << endl;
        return;
    }

    // --- Determine the sweep source (voltage or resistor) ---
    bool sweepIsVoltage = false, sweepIsResistor = false;
    vsource* sweepVsrc = nullptr;
    resistor* sweepRes = nullptr;
    for(auto comp : circuit) {
        if(comp->name == sweepSource) {
            if(vsource* vs = dynamic_cast<vsource*>(comp)) {
                sweepIsVoltage = true;
                sweepVsrc = vs;
            } else if(resistor* r = dynamic_cast<resistor*>(comp)) {
                sweepIsResistor = true;
                sweepRes = r;
            }
        }
    }
    if(!sweepIsVoltage && !sweepIsResistor) {
        cout << "ERROR: Sweep source " << sweepSource << " not found or not supported for sweeping" << endl;
        return;
    }

    // --- For resistor sweep, store its original value ---
    double origResValue = 0.0;
    if(sweepIsResistor)
        origResValue = sweepRes->value;

    // Build node index for all nodes except the ground node.
    map<string, int> nodeIndex;
    int idx = 0;
    for(auto &p : nodesMap) {
        if(p.first != groundName) {
            nodeIndex[p.first] = idx;
            idx++;
        }
    }
    int N = nodeIndex.size();
    vector<vsource*> vSources;
    for(auto comp : circuit) {
        if(vsource* vs = dynamic_cast<vsource*>(comp))
            vSources.push_back(vs);
    }
    int M = vSources.size();
    int total = N + M;

    // Sweep the parameter.
    for(double sweepVal = startVal; sweepVal <= endVal + 1e-12; sweepVal += incr) {
        if(sweepIsVoltage)
            sweepVsrc->dcValue = sweepVal;  // update voltage source value
        if(sweepIsResistor)
            sweepRes->value = sweepVal;     // update resistor value

        // Build MNA system: A x = b.
        vector<vector<double>> A(total, vector<double>(total, 0.0));
        vector<double> b(total, 0.0);

        // Add resistor contributions.
        for(auto comp : circuit) {
            resistor* r = dynamic_cast<resistor*>(comp);
            if(r) {
                double G = 1.0 / r->value;
                bool n1Ground = (r->n1->name == groundName);
                bool n2Ground = (r->n2->name == groundName);
                if(!n1Ground) {
                    int i = nodeIndex[r->n1->name];
                    A[i][i] += G;
                }
                if(!n2Ground) {
                    int j = nodeIndex[r->n2->name];
                    A[j][j] += G;
                }
                if(!n1Ground && !n2Ground) {
                    int i = nodeIndex[r->n1->name];
                    int j = nodeIndex[r->n2->name];
                    A[i][j] -= G;
                    A[j][i] -= G;
                }
            }
        }

        // Add voltage source contributions.
        int vsIndex = 0;
        for(auto vs : vSources) {
            int extraRow = N + vsIndex;
            if(vs->n1->name != groundName) {
                int i = nodeIndex[vs->n1->name];
                A[i][extraRow] += 1;
                A[extraRow][i] += 1;
            }
            if(vs->n2->name != groundName) {
                int j = nodeIndex[vs->n2->name];
                A[j][extraRow] -= 1;
                A[extraRow][j] -= 1;
            }
            b[extraRow] = vs->dcValue;
            vsIndex++;
        }

        // Solve the system A x = b.
        vector<double> sol;
        try {
            sol = solveLinearSystem(A, b);
        } catch(runtime_error &e) {
            cout << "ERROR in DC Analysis: " << e.what() << endl;
            continue;
        }

        cout << "DC Sweep: " << sweepSource << " = " << sweepVal;
        currentPlot.xValues.push_back(sweepVal);

        for(int i = 0; i < measVars.size(); i++) {
            const string& var = measVars[i];
            double value = 0.0;

            if(var.substr(0,2) == "V(") {
                size_t pos = var.find(")");
                if(pos == string::npos) continue;
                string nodeName = var.substr(2, pos-2);
                if(nodeIndex.find(nodeName) == nodeIndex.end()) {
                    cout << " | ERROR: Node " << nodeName << " not found";
                    continue;
                }
                value = (nodeName == groundName) ? 0.0 : sol[nodeIndex[nodeName]];
                cout << " | " << var << " = " << value;
            }
            else if(var.substr(0,2) == "I(") {
                size_t pos = var.find(")");
                if(pos == string::npos) continue;
                string compName = var.substr(2, pos-2);
                bool found = false;
                vsIndex = 0;
                for(auto vs : vSources) {
                    if(vs->name == compName) {
                        value = sol[N + vsIndex];
                        found = true;
                        break;
                    }
                    vsIndex++;
                }
                if(!found) {
                    for(auto comp : circuit) {
                        resistor* r = dynamic_cast<resistor*>(comp);
                        if(r && r->name == compName) {
                            double v1 = (r->n1->name == groundName) ? 0.0 : sol[nodeIndex[r->n1->name]];
                            double v2 = (r->n2->name == groundName) ? 0.0 : sol[nodeIndex[r->n2->name]];
                            value = (v1 - v2) / r->value;
                            found = true;
                            break;
                        }
                    }
                }
                if(!found) {
                    cout << " | ERROR: Component " << compName << " not found";
                    continue;
                }
                cout << " | " << var << " = " << value;
            }
            else {
                cout << " | ERROR: Unknown variable " << var;
            }

            currentPlot.yValues[i].push_back(value);
        }
        cout << endl;
    }

    // Restore original resistor value if swept.
    if(sweepIsResistor)
        sweepRes->value = origResValue;
}

// --------------------------
// Transient Analysis
// --------------------------
void performTRANAnalysis(double Tstep, double Tstop, double Tstart,
                         const vector<string> &measVars, const vector<component*> &circuit) {
    // Reset plot data
    currentPlot = PlotData();
    currentPlot.analysisType = "TRAN";

    // Create signal configurations
    for(const auto& var : measVars) {
        string unit = "";
        if(var.find("V(") != string::npos) unit = "V";
        else if(var.find("I(") != string::npos) unit = "A";
        currentPlot.signals.push_back(createSignalConfig(var, unit));
    }

    // Initialize yValues storage (one vector per signal)
    currentPlot.yValues.resize(measVars.size());

    // Identify ground node
    string groundName = "";
    for(auto comp : circuit) {
        ground* g = dynamic_cast<ground*>(comp);
        if(g && g->n) {
            groundName = g->n->name;
            break;
        }
    }
    if(groundName == ""){
        cout << "ERROR: No ground node defined. Please add a ground element." << endl;
        return;
    }

    // Build node indices (excluding ground)
    map<string,int> nodeIndex;
    int idx = 0;
    for(auto &p : nodesMap) {
        if(p.first != groundName) {
            nodeIndex[p.first] = idx;
            idx++;
        }
    }
    int N = nodeIndex.size();

    // Store node names for later use
    currentPlot.allNodeNames.resize(N);
    for(const auto& p : nodeIndex) {
        currentPlot.allNodeNames[p.second] = p.first;
    }

    // Collect voltage sources and inductors
    vector<vsource*> vSources;
    vector<inductor*> indList;
    for(auto comp : circuit) {
        if(vsource* vs = dynamic_cast<vsource*>(comp))
            vSources.push_back(vs);
        if(inductor* ind = dynamic_cast<inductor*>(comp))
            indList.push_back(ind);
    }
    int M = vSources.size();
    int K = indList.size();
    int total = N + M + K; // Total unknowns: node voltages + Vsource currents + inductor currents

    // Initial conditions
    vector<double> Vprev(N, 0.0); // Node voltages
    unordered_map<string, double> IprevMap; // Inductor currents
    for(auto ind : indList) {
        IprevMap[ind->name] = 0.0;
    }

    // Time-stepping loop
    for(double t = Tstart; t <= Tstop + 1e-12; t += Tstep) {
        vector<vector<double>> A(total, vector<double>(total, 0.0));
        vector<double> b(total, 0.0);

        // --- Resistor stamps ---
        for(auto comp : circuit) {
            if(resistor* r = dynamic_cast<resistor*>(comp)) {
                double G = 1.0 / r->value;
                bool n1Ground = (r->n1->name == groundName);
                bool n2Ground = (r->n2->name == groundName);

                if(!n1Ground) {
                    int i = nodeIndex[r->n1->name];
                    A[i][i] += G;
                }
                if(!n2Ground) {
                    int j = nodeIndex[r->n2->name];
                    A[j][j] += G;
                }
                if(!n1Ground && !n2Ground) {
                    int i = nodeIndex[r->n1->name];
                    int j = nodeIndex[r->n2->name];
                    A[i][j] -= G;
                    A[j][i] -= G;
                }
            }
        }

        // --- Capacitor stamps (Backward Euler) ---
        for(auto comp : circuit) {
            if(capacitor* c = dynamic_cast<capacitor*>(comp)) {
                double Gcap = c->value / Tstep;
                bool n1Ground = (c->n1->name == groundName);
                bool n2Ground = (c->n2->name == groundName);

                if(!n1Ground) {
                    int i = nodeIndex[c->n1->name];
                    A[i][i] += Gcap;
                    b[i] += Gcap * Vprev[i];
                }
                if(!n2Ground) {
                    int j = nodeIndex[c->n2->name];
                    A[j][j] += Gcap;
                    b[j] -= Gcap * Vprev[j];
                }
                if(!n1Ground && !n2Ground) {
                    int i = nodeIndex[c->n1->name];
                    int j = nodeIndex[c->n2->name];
                    A[i][j] -= Gcap;
                    A[j][i] -= Gcap;
                    b[i] += Gcap * Vprev[j];
                    b[j] -= Gcap * Vprev[i];
                }
            }
        }

        // --- Voltage source stamps ---
        for(int vsIdx = 0; vsIdx < vSources.size(); vsIdx++) {
            vsource* vs = vSources[vsIdx];
            int row = N + vsIdx;
            if(vs->n1->name != groundName) {
                int i = nodeIndex[vs->n1->name];
                A[i][row] += 1;
                A[row][i] += 1;
            }
            if(vs->n2->name != groundName) {
                int j = nodeIndex[vs->n2->name];
                A[j][row] -= 1;
                A[row][j] -= 1;
            }
            // Compute the instantaneous value for sinusoidal sources:
            double vsValue;
            if(vs->isSinusoidal) {
                vsValue = vs->Voffset + vs->Vamplitude * sin(2 * M_PI * vs->Frequency * t);
            } else {
                vsValue = vs->dcValue;
            }
            b[row] = vsValue;
        }


        // --- Inductor stamps (Backward Euler) ---
        for(int indIdx = 0; indIdx < indList.size(); indIdx++) {
            inductor* ind = indList[indIdx];
            int row = N + M + indIdx;
            double Rind = ind->value / Tstep;
            double Iprev = IprevMap[ind->name];

            if(ind->n1->name != groundName) {
                int i = nodeIndex[ind->n1->name];
                A[i][row] += 1;  // +I_L in KCL
                A[row][i] += 1;  // +Vn1 in KVL
            }
            if(ind->n2->name != groundName) {
                int j = nodeIndex[ind->n2->name];
                A[j][row] -= 1;  // -I_L in KCL
                A[row][j] -= 1;  // -Vn2 in KVL
            }

            // KVL equation: Vn1 - Vn2 = L/Tstep*(I_new - Iprev)
            A[row][row] = -Rind;  // -L/Tstep*I_new
            b[row] = -Rind * Iprev;  // -L/Tstep*Iprev
        }

        // Solve the system
        vector<double> sol;
        try {
            sol = solveLinearSystem(A, b);
        } catch(runtime_error &e) {
            cout << "ERROR in transient analysis at time t = " << t << ": " << e.what() << endl;
            continue;
        }

        // Store node voltages for this time step
        vector<double> currentVoltages(N);
        for(int i = 0; i < N; i++) {
            currentVoltages[i] = sol[i];
        }
        currentPlot.nodeVoltages.push_back(currentVoltages);

        // Store source currents for this time step
        vector<double> currentSourceCurrents(M);
        for(int i = 0; i < M; i++) {
            currentSourceCurrents[i] = sol[N + i];
        }
        currentPlot.sourceCurrents.push_back(currentSourceCurrents);

        // Output results
        cout << "t = " << t;
        currentPlot.xValues.push_back(t);

        for(int i = 0; i < measVars.size(); i++) {
            const string& var = measVars[i];
            double value = 0.0;

            if(var.substr(0,2) == "V(") {
                size_t endPos = var.find(")");
                if(endPos == string::npos) {
                    cout << " | ERROR: Invalid variable syntax: " << var;
                    continue;
                }
                string nodeName = var.substr(2, endPos-2);
                if(nodeIndex.find(nodeName) == nodeIndex.end()) {
                    cout << " | ERROR: Node " << nodeName << " not found in circuit";
                    value = 0.0;
                } else {
                    value = sol[nodeIndex[nodeName]];
                }
                cout << " | " << var << " = " << value;
            }
            else if(var.substr(0,2) == "I(") {
                size_t endPos = var.find(")");
                if(endPos == string::npos) {
                    cout << " | ERROR: Invalid variable syntax: " << var;
                    continue;
                }
                string compName = var.substr(2, endPos-2);
                bool found = false;

                // Check voltage sources
                for(int vsIdx = 0; vsIdx < vSources.size(); vsIdx++) {
                    if(vSources[vsIdx]->name == compName) {
                        value = sol[N + vsIdx];
                        found = true;
                        break;
                    }
                }

                // Check inductors
                if(!found) {
                    for(int indIdx = 0; indIdx < indList.size(); indIdx++) {
                        if(indList[indIdx]->name == compName) {
                            value = sol[N + M + indIdx];
                            found = true;
                            break;
                        }
                    }
                }

                // Check resistors
                if(!found) {
                    for(auto comp : circuit) {
                        if(resistor* r = dynamic_cast<resistor*>(comp)) {
                            if(r->name == compName) {
                                double v1 = (r->n1->name == groundName) ? 0.0 : sol[nodeIndex[r->n1->name]];
                                double v2 = (r->n2->name == groundName) ? 0.0 : sol[nodeIndex[r->n2->name]];
                                value = (v1 - v2) / r->value;
                                found = true;
                                break;
                            }
                        }
                    }
                }

                if(found) {
                    cout << " | " << var << " = " << value;
                } else {
                    cout << " | ERROR: Component " << compName << " not found";
                }
            }

            currentPlot.yValues[i].push_back(value);
        }
        cout << endl;

        // Update previous values for next time step
        for(auto &entry : nodeIndex) {
            Vprev[entry.second] = sol[entry.second];
        }
        for(int indIdx = 0; indIdx < indList.size(); indIdx++) {
            IprevMap[indList[indIdx]->name] = sol[N + M + indIdx];
        }
    }
}

// --------------------------
// Plotting Functions
// --------------------------
void renderText(SDL_Renderer* renderer, TTF_Font* font, const string& text,
                int x, int y, SDL_Color color, bool vertical = false) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    if(!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if(!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dstRect;
    dstRect.x = x;
    dstRect.y = y;
    dstRect.w = surface->w;
    dstRect.h = surface->h;

    if(vertical) {
        SDL_Point center = {surface->w/2, surface->h/2};
        SDL_RenderCopyEx(renderer, texture, NULL, &dstRect, 90, &center, SDL_FLIP_NONE);
    } else {
        SDL_RenderCopy(renderer, texture, NULL, &dstRect);
    }

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void showPlot(PlotData& plotData) {
    // Initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << endl;
        return;
    }

    if(TTF_Init() == -1) {
        cerr << "TTF could not initialize! TTF_Error: " << TTF_GetError() << endl;
        SDL_Quit();
        return;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow("Circuit Simulator Plot",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          1200, 800,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if(!window) {
        cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << endl;
        TTF_Quit();
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(!renderer) {
        cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return;
    }

    // Load font
    TTF_Font* font = TTF_OpenFont("arial.ttf", 14);
    if(!font) {
        // Try fallback fonts
        font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 14);
        if(!font) {
            font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSans.ttf", 14);
            if(!font) {
                cerr << "Failed to load font: " << TTF_GetError() << endl;
            }
        }
    }

    // Color definitions
    const SDL_Color GRID_COLOR = {200, 200, 200, 255};
    const SDL_Color AXIS_COLOR = {0, 0, 0, 255};
    const SDL_Color TEXT_COLOR = {0, 0, 0, 255};
    const SDL_Color CURSOR1_COLOR = {255, 0, 0, 255};    // Red
    const SDL_Color CURSOR2_COLOR = {0, 0, 255, 255};    // Blue

    // Cursor variables
    bool cursorActive = true;
    bool doubleCursorMode = true;
    int activeCursor = 1;

    // Cursor positions
    double cursor1X = 0.0, cursor1Y = 0.0;
    int cursor1ScreenX = 100, cursor1ScreenY = 400;
    double cursor2X = 0.0, cursor2Y = 0.0;
    int cursor2ScreenX = 300, cursor2ScreenY = 400;

    // Plot dimension variables
    int MARGIN = 70;
    int PLOT_WIDTH = 0, PLOT_HEIGHT = 0;

    // Scaling variables
    double zoomFactorX = 1.0;
    double zoomFactorY = 1.0;
    double panOffsetX = 0.0;
    double panOffsetY = 0.0;
    bool isPanning = false;
    int panStartX = 0, panStartY = 0;
    double panStartOffsetX = 0.0, panStartOffsetY = 0.0;

    // Data range variables
    double dataMinX = 0.0, dataMaxX = 0.0, dataMinY = 0.0, dataMaxY = 0.0;
    double viewMinX = 0.0, viewMaxX = 0.0, viewMinY = 0.0, viewMaxY = 0.0;

    // Auto zoom flag
    bool autoZoom = true;

    // Predefined distinct colors
    vector<SDL_Color> defaultColors = {
            {255, 0, 0, 255},     // Red
            {0, 0, 255, 255},     // Blue
            {0, 180, 0, 255},     // Green
            {180, 0, 180, 255},   // Purple
            {0, 180, 180, 255},   // Cyan
            {180, 180, 0, 255},   // Yellow
            {255, 128, 0, 255},   // Orange
            {128, 0, 255, 255}    // Violet
    };

    // Initialize signal colors if not set
    for(int i = 0; i < plotData.signals.size(); i++) {
        if(!plotData.signals[i].userChanged) {
            plotData.signals[i].color = defaultColors[i % defaultColors.size()];
        }
    }

    // Main plot loop
    bool quit = false;
    bool needsRedraw = true;
    vector<SDL_Rect> legendRects;

    while(!quit) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                quit = true;
            }
            else if(event.type == SDL_KEYDOWN) {
                if(event.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                }
                else if(event.key.keysym.sym == SDLK_l) {
                    plotData.showLegend = !plotData.showLegend;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_c) {
                    cursorActive = !cursorActive;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_d) {
                    doubleCursorMode = !doubleCursorMode;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_1) {
                    activeCursor = 1;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_2) {
                    activeCursor = 2;
                    needsRedraw = true;
                }
                    // Zoom controls
                else if(event.key.keysym.sym == SDLK_EQUALS || event.key.keysym.sym == SDLK_PLUS) {
                    // Zoom in
                    zoomFactorX *= 1.2;
                    zoomFactorY *= 1.2;
                    autoZoom = false;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_MINUS) {
                    // Zoom out
                    zoomFactorX /= 1.2;
                    zoomFactorY /= 1.2;
                    autoZoom = false;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_LEFT) {
                    // Pan left
                    panOffsetX -= (viewMaxX - viewMinX) * 0.1;
                    autoZoom = false;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_RIGHT) {
                    // Pan right
                    panOffsetX += (viewMaxX - viewMinX) * 0.1;
                    autoZoom = false;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_UP) {
                    // Pan up
                    panOffsetY += (viewMaxY - viewMinY) * 0.1;
                    autoZoom = false;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_DOWN) {
                    // Pan down
                    panOffsetY -= (viewMaxY - viewMinY) * 0.1;
                    autoZoom = false;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_r) {
                    // Reset zoom and pan
                    zoomFactorX = 1.0;
                    zoomFactorY = 1.0;
                    panOffsetX = 0.0;
                    panOffsetY = 0.0;
                    autoZoom = false;
                    needsRedraw = true;
                }
                else if(event.key.keysym.sym == SDLK_a) {
                    // Toggle auto zoom
                    autoZoom = !autoZoom;
                    if(autoZoom) {
                        // Reset zoom and pan when enabling auto zoom
                        zoomFactorX = 1.0;
                        zoomFactorY = 1.0;
                        panOffsetX = 0.0;
                        panOffsetY = 0.0;
                    }
                    needsRedraw = true;
                }
            }
            else if(event.type == SDL_MOUSEBUTTONDOWN) {
                if(event.button.button == SDL_BUTTON_LEFT) {
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;

                    // Check if clicked on legend color box
                    for(int i = 0; i < legendRects.size(); i++) {
                        SDL_Rect rect = legendRects[i];
                        if(mouseX >= rect.x && mouseX <= rect.x + rect.w &&
                           mouseY >= rect.y && mouseY <= rect.y + rect.h) {
                            // Cycle to next color
                            SDL_Color current = plotData.signals[i].color;
                            int currentIndex = -1;

                            // Find current color in default colors
                            for(int j = 0; j < defaultColors.size(); j++) {
                                if(defaultColors[j].r == current.r &&
                                   defaultColors[j].g == current.g &&
                                   defaultColors[j].b == current.b) {
                                    currentIndex = j;
                                    break;
                                }
                            }

                            // Set next color
                            int nextIndex = (currentIndex + 1) % defaultColors.size();
                            plotData.signals[i].color = defaultColors[nextIndex];
                            plotData.signals[i].userChanged = true;
                            needsRedraw = true;
                            break;
                        }
                    }

                    // Check if clicked within plot area (excluding legend)
                    if(mouseX >= MARGIN && mouseX <= MARGIN + PLOT_WIDTH &&
                       mouseY >= MARGIN && mouseY <= MARGIN + PLOT_HEIGHT) {

                        // Convert screen coordinates to data coordinates
                        if(activeCursor == 1) {
                            cursor1ScreenX = mouseX;
                            cursor1ScreenY = mouseY;
                            cursor1X = viewMinX + (mouseX - MARGIN) * (viewMaxX - viewMinX) / PLOT_WIDTH;
                            cursor1Y = viewMinY + (MARGIN + PLOT_HEIGHT - mouseY) * (viewMaxY - viewMinY) / PLOT_HEIGHT;
                        } else {
                            cursor2ScreenX = mouseX;
                            cursor2ScreenY = mouseY;
                            cursor2X = viewMinX + (mouseX - MARGIN) * (viewMaxX - viewMinX) / PLOT_WIDTH;
                            cursor2Y = viewMinY + (MARGIN + PLOT_HEIGHT - mouseY) * (viewMaxY - viewMinY) / PLOT_HEIGHT;
                        }
                        cursorActive = true;
                        autoZoom = false;
                        needsRedraw = true;
                    }
                }
                else if(event.button.button == SDL_BUTTON_RIGHT) {
                    // Start panning with right mouse button
                    isPanning = true;
                    panStartX = event.button.x;
                    panStartY = event.button.y;
                    panStartOffsetX = panOffsetX;
                    panStartOffsetY = panOffsetY;
                    autoZoom = false;
                }
            }
            else if(event.type == SDL_MOUSEBUTTONUP) {
                if(event.button.button == SDL_BUTTON_RIGHT) {
                    // Stop panning
                    isPanning = false;
                }
            }
            else if(event.type == SDL_MOUSEMOTION) {
                if(isPanning) {
                    // Calculate pan distance and update offset
                    int dx = event.motion.x - panStartX;
                    int dy = event.motion.y - panStartY;

                    // Convert pixel distance to data distance
                    double dataDx = dx * (viewMaxX - viewMinX) / PLOT_WIDTH;
                    double dataDy = dy * (viewMaxY - viewMinY) / PLOT_HEIGHT;

                    panOffsetX = panStartOffsetX - dataDx;
                    panOffsetY = panStartOffsetY + dataDy;

                    autoZoom = false;
                    needsRedraw = true;
                }
            }
            else if(event.type == SDL_MOUSEWHEEL) {
                // Zoom with mouse wheel
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);

                // Check if mouse is over plot area
                if(mouseX >= MARGIN && mouseX <= MARGIN + PLOT_WIDTH &&
                   mouseY >= MARGIN && mouseY <= MARGIN + PLOT_HEIGHT) {

                    // Calculate mouse position in data coordinates
                    double mouseDataX = viewMinX + (mouseX - MARGIN) * (viewMaxX - viewMinX) / PLOT_WIDTH;
                    double mouseDataY = viewMinY + (MARGIN + PLOT_HEIGHT - mouseY) * (viewMaxY - viewMinY) / PLOT_HEIGHT;

                    // Zoom factor
                    double zoomChange = (event.wheel.y > 0) ? 1.2 : 1/1.2;

                    // Apply zoom
                    zoomFactorX *= zoomChange;
                    zoomFactorY *= zoomChange;

                    // Adjust pan offset to zoom toward mouse position
                    panOffsetX = mouseDataX - (mouseDataX - panOffsetX) * zoomChange;
                    panOffsetY = mouseDataY - (mouseDataY - panOffsetY) * zoomChange;

                    autoZoom = false;
                    needsRedraw = true;
                }
            }
            else if(event.type == SDL_WINDOWEVENT) {
                if(event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    needsRedraw = true;
                }
            }
        }

        if(needsRedraw) {
            // Clear screen
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderClear(renderer);

            int width, height;
            SDL_GetWindowSize(window, &width, &height);
            PLOT_WIDTH = width - 2 * MARGIN;
            PLOT_HEIGHT = height - 2 * MARGIN;

            // Calculate data ranges
            if(plotData.xValues.empty() || plotData.yValues.empty() || plotData.signals.empty()) {
                if(font) {
                    renderText(renderer, font, "No data to plot", width/2, height/2, TEXT_COLOR);
                }
                SDL_RenderPresent(renderer);
                continue;
            }

            // Calculate full data range
            dataMinX = *min_element(plotData.xValues.begin(), plotData.xValues.end());
            dataMaxX = *max_element(plotData.xValues.begin(), plotData.xValues.end());
            dataMinY = numeric_limits<double>::max();
            dataMaxY = numeric_limits<double>::min();

            // Find min/max values for all signals
            for(int s = 0; s < plotData.signals.size(); s++) {
                for(double val : plotData.yValues[s]) {
                    double scaledVal = val * plotData.signals[s].scaleFactor;
                    if(scaledVal < dataMinY) dataMinY = scaledVal;
                    if(scaledVal > dataMaxY) dataMaxY = scaledVal;
                }
            }

            // Handle case where all values are the same
            if(dataMinY == dataMaxY) {
                dataMinY -= 1.0;
                dataMaxY += 1.0;
            }

            // Auto zoom adjustment
            if(autoZoom) {
                // Calculate exact range needed for signals
                double signalMinY = numeric_limits<double>::max();
                double signalMaxY = numeric_limits<double>::min();

                for(int s = 0; s < plotData.signals.size(); s++) {
                    for(double val : plotData.yValues[s]) {
                        double scaledVal = val * plotData.signals[s].scaleFactor;
                        if(scaledVal < signalMinY) signalMinY = scaledVal;
                        if(scaledVal > signalMaxY) signalMaxY = scaledVal;
                    }
                }

                // Add a small margin (2%) to avoid clipping
                double yMargin = (signalMaxY - signalMinY) * 0.02;
                if(yMargin == 0) yMargin = 0.1; // Handle case where all values are the same

                dataMinY = signalMinY - yMargin;
                dataMaxY = signalMaxY + yMargin;

                // Reset zoom and pan for auto zoom
                zoomFactorX = 1.0;
                zoomFactorY = 1.0;
                panOffsetX = 0.0;
                panOffsetY = 0.0;
            } else {
                // Add 10% padding to Y range when not in auto zoom
                double yRange = dataMaxY - dataMinY;
                if(yRange == 0) yRange = 1; // Avoid division by zero
                dataMinY -= yRange * 0.1;
                dataMaxY += yRange * 0.1;
            }

            // Calculate view range based on zoom and pan
            double dataRangeX = dataMaxX - dataMinX;
            double dataRangeY = dataMaxY - dataMinY;

            viewMinX = dataMinX + panOffsetX;
            viewMaxX = viewMinX + dataRangeX / zoomFactorX;

            viewMinY = dataMinY + panOffsetY;
            viewMaxY = viewMinY + dataRangeY / zoomFactorY;

            // Draw grid
            SDL_SetRenderDrawColor(renderer, GRID_COLOR.r, GRID_COLOR.g, GRID_COLOR.b, GRID_COLOR.a);

            // Calculate grid spacing based on view range
            double xRange = viewMaxX - viewMinX;
            double yRangeView = viewMaxY - viewMinY;

            // Determine appropriate grid divisions
            double xGridSize = pow(10, floor(log10(xRange))) / 2;
            double yGridSize = pow(10, floor(log10(yRangeView))) / 2;

            // Adjust grid size to have reasonable number of divisions
            while(xRange / xGridSize > 20) xGridSize *= 2;
            while(xRange / xGridSize < 5) xGridSize /= 2;

            while(yRangeView / yGridSize > 20) yGridSize *= 2;
            while(yRangeView / yGridSize < 5) yGridSize /= 2;

            // Draw vertical grid lines
            double firstGridX = ceil(viewMinX / xGridSize) * xGridSize;
            for(double x = firstGridX; x <= viewMaxX; x += xGridSize) {
                int screenX = MARGIN + static_cast<int>((x - viewMinX) / (viewMaxX - viewMinX) * PLOT_WIDTH);
                SDL_RenderDrawLine(renderer, screenX, MARGIN, screenX, MARGIN + PLOT_HEIGHT);
            }

            // Draw horizontal grid lines
            double firstGridY = ceil(viewMinY / yGridSize) * yGridSize;
            for(double y = firstGridY; y <= viewMaxY; y += yGridSize) {
                int screenY = MARGIN + PLOT_HEIGHT - static_cast<int>((y - viewMinY) / (viewMaxY - viewMinY) * PLOT_HEIGHT);
                SDL_RenderDrawLine(renderer, MARGIN, screenY, MARGIN + PLOT_WIDTH, screenY);
            }

            // Draw axes
            SDL_SetRenderDrawColor(renderer, AXIS_COLOR.r, AXIS_COLOR.g, AXIS_COLOR.b, AXIS_COLOR.a);
            SDL_RenderDrawLine(renderer, MARGIN, MARGIN + PLOT_HEIGHT, MARGIN + PLOT_WIDTH, MARGIN + PLOT_HEIGHT); // X-axis
            SDL_RenderDrawLine(renderer, MARGIN, MARGIN, MARGIN, MARGIN + PLOT_HEIGHT); // Y-axis

            // Draw axis numbers and units
            if(font) {
                // Draw X-axis numbers
                for(double x = firstGridX; x <= viewMaxX; x += xGridSize) {
                    string label = to_string(x);
                    // Format number to remove extra zeros
                    size_t pos = label.find('.');
                    if(pos != string::npos) {
                        // Trim trailing zeros
                        label = label.substr(0, label.find_last_not_of('0') + 1);
                        // Remove decimal point if no fractional part
                        if(label.back() == '.') {
                            label.pop_back();
                        }
                    }

                    int xPos = MARGIN + static_cast<int>((x - viewMinX) / (viewMaxX - viewMinX) * PLOT_WIDTH) - 15;
                    int yPos = MARGIN + PLOT_HEIGHT + 10;
                    renderText(renderer, font, label, xPos, yPos, TEXT_COLOR);
                }

                // Draw Y-axis numbers
                for(double y = firstGridY; y <= viewMaxY; y += yGridSize) {
                    string label = to_string(y);
                    // Format number to remove extra zeros
                    size_t pos = label.find('.');
                    if(pos != string::npos) {
                        // Trim trailing zeros
                        label = label.substr(0, label.find_last_not_of('0') + 1);
                        // Remove decimal point if no fractional part
                        if(label.back() == '.') {
                            label.pop_back();
                        }
                    }

                    int xPos = MARGIN - 50;
                    int yPos = MARGIN + PLOT_HEIGHT - static_cast<int>((y - viewMinY) / (viewMaxY - viewMinY) * PLOT_HEIGHT) - 7;
                    renderText(renderer, font, label, xPos, yPos, TEXT_COLOR);
                }

                // Draw axis labels
                string xLabel = (plotData.analysisType == "TRAN") ? "Time (s)" : "Voltage (V)";
                string yLabel = "Amplitude";

                if(plotData.signals.size() > 0 && !plotData.signals[0].unit.empty()) {
                    yLabel += " (" + plotData.signals[0].unit + ")";
                }

                renderText(renderer, font, xLabel, MARGIN + PLOT_WIDTH/2, MARGIN + PLOT_HEIGHT + 40, TEXT_COLOR);
                renderText(renderer, font, yLabel, MARGIN - 60, MARGIN + PLOT_HEIGHT/2, TEXT_COLOR, true);

                // Title
                string title = plotData.analysisType + " Analysis";
                renderText(renderer, font, title, MARGIN + PLOT_WIDTH/2, 20, TEXT_COLOR);

                // Zoom info
                string zoomInfo = "Zoom: X" + to_string(zoomFactorX).substr(0, 4) +
                                  " Y" + to_string(zoomFactorY).substr(0, 4);
                renderText(renderer, font, zoomInfo, width - 150, 20, TEXT_COLOR);

                // Auto zoom status
                string autoZoomStatus = "Auto Zoom: " + string(autoZoom ? "ON" : "OFF");
                renderText(renderer, font, autoZoomStatus, width - 150, 40, autoZoom ? SDL_Color{0, 180, 0, 255} : SDL_Color{180, 0, 0, 255});
            }

            // Draw signals
            for(int s = 0; s < plotData.signals.size(); s++) {
                SDL_Color color = plotData.signals[s].color;
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

                // Draw the signal
                for(size_t i = 1; i < plotData.xValues.size(); i++) {
                    double x0 = plotData.xValues[i-1];
                    double y0 = plotData.yValues[s][i-1] * plotData.signals[s].scaleFactor;
                    double x1 = plotData.xValues[i];
                    double y1 = plotData.yValues[s][i] * plotData.signals[s].scaleFactor;

                    // Skip points outside view
                    if((x0 < viewMinX && x1 < viewMinX) || (x0 > viewMaxX && x1 > viewMaxX)) {
                        continue;
                    }

                    int sx0 = MARGIN + static_cast<int>((x0 - viewMinX) / (viewMaxX - viewMinX) * PLOT_WIDTH);
                    int sy0 = MARGIN + PLOT_HEIGHT - static_cast<int>((y0 - viewMinY) / (viewMaxY - viewMinY) * PLOT_HEIGHT);
                    int sx1 = MARGIN + static_cast<int>((x1 - viewMinX) / (viewMaxX - viewMinX) * PLOT_WIDTH);
                    int sy1 = MARGIN + PLOT_HEIGHT - static_cast<int>((y1 - viewMinY) / (viewMaxY - viewMinY) * PLOT_HEIGHT);

                    SDL_RenderDrawLine(renderer, sx0, sy0, sx1, sy1);
                }
            }

            // Draw cursors if active
            if(cursorActive) {
                // Draw cursor 1
                SDL_SetRenderDrawColor(renderer, CURSOR1_COLOR.r, CURSOR1_COLOR.g, CURSOR1_COLOR.b, CURSOR1_COLOR.a);
                SDL_RenderDrawLine(renderer, cursor1ScreenX, MARGIN, cursor1ScreenX, MARGIN + PLOT_HEIGHT);
                SDL_RenderDrawLine(renderer, MARGIN, cursor1ScreenY, MARGIN + PLOT_WIDTH, cursor1ScreenY);

                // Draw cursor 1 coordinate display
                string coordText1 = "Cursor1: (" + to_string(cursor1X) + ", " + to_string(cursor1Y) + ")";

                // Format numbers to remove unnecessary precision
                size_t decimalPos = coordText1.find('.');
                if(decimalPos != string::npos) {
                    size_t endPos = min(coordText1.size(), decimalPos + 5);
                    coordText1 = coordText1.substr(0, endPos);
                }

                // Draw text background for cursor 1
                SDL_Surface* textSurface1 = TTF_RenderText_Solid(font, coordText1.c_str(), CURSOR1_COLOR);
                if(textSurface1) {
                    SDL_Texture* textTexture1 = SDL_CreateTextureFromSurface(renderer, textSurface1);
                    SDL_Rect textRect1 = {cursor1ScreenX + 10, cursor1ScreenY - 20, textSurface1->w, textSurface1->h};

                    // Draw background rectangle
                    SDL_Rect bgRect1 = {textRect1.x - 5, textRect1.y - 2, textRect1.w + 10, textRect1.h + 4};
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
                    SDL_RenderFillRect(renderer, &bgRect1);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderDrawRect(renderer, &bgRect1);

                    // Draw text
                    SDL_RenderCopy(renderer, textTexture1, NULL, &textRect1);

                    SDL_FreeSurface(textSurface1);
                    SDL_DestroyTexture(textTexture1);
                }

                // Draw cursor 2 if in double cursor mode
                if(doubleCursorMode) {
                    SDL_SetRenderDrawColor(renderer, CURSOR2_COLOR.r, CURSOR2_COLOR.g, CURSOR2_COLOR.b, CURSOR2_COLOR.a);
                    SDL_RenderDrawLine(renderer, cursor2ScreenX, MARGIN, cursor2ScreenX, MARGIN + PLOT_HEIGHT);
                    SDL_RenderDrawLine(renderer, MARGIN, cursor2ScreenY, MARGIN + PLOT_WIDTH, cursor2ScreenY);

                    // Draw cursor 2 coordinate display
                    string coordText2 = "Cursor2: (" + to_string(cursor2X) + ", " + to_string(cursor2Y) + ")";

                    // Format numbers to remove unnecessary precision
                    decimalPos = coordText2.find('.');
                    if(decimalPos != string::npos) {
                        size_t endPos = min(coordText2.size(), decimalPos + 5);
                        coordText2 = coordText2.substr(0, endPos);
                    }

                    // Draw text background for cursor 2
                    SDL_Surface* textSurface2 = TTF_RenderText_Solid(font, coordText2.c_str(), CURSOR2_COLOR);
                    if(textSurface2) {
                        SDL_Texture* textTexture2 = SDL_CreateTextureFromSurface(renderer, textSurface2);
                        SDL_Rect textRect2 = {cursor2ScreenX + 10, cursor2ScreenY - 20, textSurface2->w, textSurface2->h};

                        // Draw background rectangle
                        SDL_Rect bgRect2 = {textRect2.x - 5, textRect2.y - 2, textRect2.w + 10, textRect2.h + 4};
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
                        SDL_RenderFillRect(renderer, &bgRect2);
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                        SDL_RenderDrawRect(renderer, &bgRect2);

                        // Draw text
                        SDL_RenderCopy(renderer, textTexture2, NULL, &textRect2);

                        SDL_FreeSurface(textSurface2);
                        SDL_DestroyTexture(textTexture2);
                    }

                    // Calculate and display differences
                    double deltaX = cursor2X - cursor1X;
                    double deltaY = cursor2Y - cursor1Y;
                    double slope = (deltaX != 0) ? deltaY / deltaX : 0;

                    string diffText = "ΔX: " + to_string(deltaX);
                    string diffText2 = "ΔY: " + to_string(deltaY);
                    string slopeText = "Slope: " + to_string(slope);

                    // Format numbers
                    decimalPos = diffText.find('.');
                    if(decimalPos != string::npos) {
                        size_t endPos = min(diffText.size(), decimalPos + 5);
                        diffText = diffText.substr(0, endPos);
                    }

                    decimalPos = diffText2.find('.');
                    if(decimalPos != string::npos) {
                        size_t endPos = min(diffText2.size(), decimalPos + 5);
                        diffText2 = diffText2.substr(0, endPos);
                    }

                    decimalPos = slopeText.find('.');
                    if(decimalPos != string::npos) {
                        size_t endPos = min(slopeText.size(), decimalPos + 5);
                        slopeText = slopeText.substr(0, endPos);
                    }

                    // Draw difference info box
                    int infoX = MARGIN + 10;
                    int infoY = MARGIN + 10;

                    SDL_Surface* diffSurface = TTF_RenderText_Solid(font, diffText.c_str(), TEXT_COLOR);
                    SDL_Surface* diffSurface2 = TTF_RenderText_Solid(font, diffText2.c_str(), TEXT_COLOR);
                    SDL_Surface* slopeSurface = TTF_RenderText_Solid(font, slopeText.c_str(), TEXT_COLOR);

                    if(diffSurface && diffSurface2 && slopeSurface) {
                        int boxWidth = max(max(diffSurface->w, diffSurface2->w), slopeSurface->w) + 20;
                        int boxHeight = diffSurface->h + diffSurface2->h + slopeSurface->h + 30;

                        SDL_Rect infoBox = {infoX, infoY, boxWidth, boxHeight};
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
                        SDL_RenderFillRect(renderer, &infoBox);
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                        SDL_RenderDrawRect(renderer, &infoBox);

                        SDL_Texture* diffTexture = SDL_CreateTextureFromSurface(renderer, diffSurface);
                        SDL_Texture* diffTexture2 = SDL_CreateTextureFromSurface(renderer, diffSurface2);
                        SDL_Texture* slopeTexture = SDL_CreateTextureFromSurface(renderer, slopeSurface);

                        SDL_Rect diffRect = {infoX + 10, infoY + 10, diffSurface->w, diffSurface->h};
                        SDL_Rect diffRect2 = {infoX + 10, infoY + 10 + diffSurface->h + 5, diffSurface2->w, diffSurface2->h};
                        SDL_Rect slopeRect = {infoX + 10, infoY + 10 + diffSurface->h + diffSurface2->h + 10, slopeSurface->w, slopeSurface->h};

                        SDL_RenderCopy(renderer, diffTexture, NULL, &diffRect);
                        SDL_RenderCopy(renderer, diffTexture2, NULL, &diffRect2);
                        SDL_RenderCopy(renderer, slopeTexture, NULL, &slopeRect);

                        SDL_FreeSurface(diffSurface);
                        SDL_FreeSurface(diffSurface2);
                        SDL_FreeSurface(slopeSurface);
                        SDL_DestroyTexture(diffTexture);
                        SDL_DestroyTexture(diffTexture2);
                        SDL_DestroyTexture(slopeTexture);
                    }
                }
            }

            // Draw legend if enabled
            if(plotData.showLegend && font) {
                const int LEGEND_WIDTH = 200;
                const int LEGEND_HEIGHT = 30 + plotData.signals.size() * 25;
                const int LEGEND_MARGIN = 10;
                const int COLOR_BOX_SIZE = 15;

                int legendX = MARGIN + PLOT_WIDTH - LEGEND_WIDTH - LEGEND_MARGIN;
                int legendY = MARGIN + LEGEND_MARGIN;

                // Clear legend rectangles for click detection
                legendRects.clear();

                // Draw legend background
                SDL_Rect legendBg = {legendX - 5, legendY - 5, LEGEND_WIDTH + 10, LEGEND_HEIGHT + 10};
                SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
                SDL_RenderFillRect(renderer, &legendBg);
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &legendBg);

                // Draw legend title
                renderText(renderer, font, "Signals", legendX + LEGEND_WIDTH/2, legendY, TEXT_COLOR);

                // Draw each signal in legend
                for(int i = 0; i < plotData.signals.size(); i++) {
                    int itemY = legendY + 25 + i * 25;

                    // Draw color box (clickable)
                    SDL_Rect colorRect = {legendX, itemY, COLOR_BOX_SIZE, COLOR_BOX_SIZE};
                    SDL_SetRenderDrawColor(renderer,
                                           plotData.signals[i].color.r,
                                           plotData.signals[i].color.g,
                                           plotData.signals[i].color.b,
                                           255);
                    SDL_RenderFillRect(renderer, &colorRect);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderDrawRect(renderer, &colorRect);

                    // Store for click detection
                    legendRects.push_back(colorRect);

                    // Draw signal name
                    string label = plotData.signals[i].name;
                    if(!plotData.signals[i].unit.empty()) {
                        label += " (" + plotData.signals[i].unit + ")";
                    }
                    renderText(renderer, font, label,
                               legendX + COLOR_BOX_SIZE + 10, itemY + COLOR_BOX_SIZE/2 - 7, TEXT_COLOR);
                }
            }

            // Draw mode info
            if(font) {
                string modeText = "Mode: " + string(doubleCursorMode ? "Double Cursor" : "Single Cursor");
                if(doubleCursorMode) {
                    modeText += " (Active: " + to_string(activeCursor) + ")";
                }
                modeText += " - Press 'd' to toggle, '1'/'2' to select cursor";
                renderText(renderer, font, modeText, MARGIN, height - 20, TEXT_COLOR);

                // Draw zoom controls info
                string zoomControls = "Zoom: +/- keys, Mouse Wheel | Pan: Arrow keys, Right-click drag | Reset: 'r' | Auto Zoom: 'a'";
                renderText(renderer, font, zoomControls, MARGIN, height - 40, TEXT_COLOR);
            }

            SDL_RenderPresent(renderer);
            needsRedraw = false;
        }

        SDL_Delay(10);
    }

    // Cleanup
    if(font) TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

// --------------------------
// File I/O Functions
// --------------------------
void loadCircuitFromFile(const string &filename, vector<component*> &circuit) {
    ifstream infile(filename);
    if(!infile) {
        cerr << "Error: Could not open file " << filename << endl;
        return;
    }

    string line;
    while(getline(infile, line)) {
        // Skip empty lines or lines starting with '#' (as comments)
        if(line.empty() || line[0] == '#')
            continue;

        stringstream ss(line);
        vector<string> tokens;
        string token;
        while(ss >> token)
            tokens.push_back(token);
        // Ground element expected format: "GND <nodeName>"
        if(!tokens.empty() && tokens[0] == "GND") {
            if(tokens.size() != 2) {
                cout << "Skipping invalid ground line: " << line << endl;
                continue;
            }
            string nodeName = tokens[1];
            // Ensure we don't already have a ground at this node.
            bool duplicate = false;
            for(auto comp: circuit) {
                ground* g = dynamic_cast<ground*>(comp);
                if(g && g->n && g->n->name == nodeName) {
                    duplicate = true;
                    break;
                }
            }
            if(duplicate) {
                cout << "Ground already exists at node " << nodeName << endl;
            } else {
                ground* g = new ground();
                g->name = "GND";
                g->n = getOrCreateNode(nodeName);
                circuit.push_back(g);
                cout << "Ground added at node " << nodeName << endl;
            }
            continue;
        }

        if(tokens.size() < 5) {
            cout << "Skipping invalid line: " << line << endl;
            continue;
        }

        // The tokens are:
        // token[0] = element type e.g., "V", "R"
        // token[1] = name, e.g., "V1", "R1"
        // token[2] = node1, e.g., "1"
        // token[3] = node2, e.g., "0"
        // token[4] = value, e.g., "5" or "1000"
        string elemType = tokens[0];
        string elemName = tokens[1];
        string node1 = tokens[2];
        string node2 = tokens[3];
        string valueStr = tokens[4];

        // Depending on element type, create the proper element.
        // For example, we can support voltage sources and resistors for now:
        if(elemType == "V") {
            // Create a DC voltage source.
            double dcVal = stod(valueStr);  // no unit conversion is provided here, so assume value is in volts.
            vsource* vs = new vsource();
            vs->name = elemName;
            vs->dcValue = dcVal;
            vs->value = dcVal;
            vs->isSinusoidal = false;
            vs->n1 = getOrCreateNode(node1);
            vs->n2 = getOrCreateNode(node2);
            circuit.push_back(vs);
            cout << "Voltage Source " << elemName << " added between " << node1 << " and " << node2
                 << " with DC value " << dcVal << " V" << endl;
        }
        else if(elemType == "R") {
            bool ok;
            double resValue = parseResistanceValue(valueStr, ok);
            if(!ok) {
                cout << "Error parsing resistor value on line: " << line << endl;
                continue;
            }
            resistor* r = new resistor();
            r->name = elemName;
            r->value = resValue;
            r->n1 = getOrCreateNode(node1);
            r->n2 = getOrCreateNode(node2);
            circuit.push_back(r);
            cout << "Resistor " << elemName << " added between " << node1 << " and " << node2
                 << " with value " << resValue << " ohms" << endl;
        }
        else if (elemType == "WV") {
            WirelessVoltageSource* wvs = new WirelessVoltageSource();
            wvs->name = elemName;
            wvs->n1 = getOrCreateNode(node1);
            wvs->n2 = getOrCreateNode(node2);

            if (tokens.size() >= 6) {
                wvs->isServer = (tokens[4] == "SERVER");
                wvs->ipAddress = tokens[5];
                if (tokens.size() >= 7) wvs->port = stoi(tokens[6]);
                if (tokens.size() >= 8) wvs->voltageValue = stod(tokens[7]);
            }

            wvs->value = wvs->voltageValue;
            circuit.push_back(wvs);
            cout << "Wireless Voltage Source " << elemName << " added between "
                 << node1 << " and " << node2 << endl;
        }
        else if(elemType == "C") {
            // Create a capacitor.
            bool ok;
            double capValue = parseCapacitanceValue(valueStr, ok);
            if(!ok) {
                cout << "Error parsing capacitor value on line: " << line << endl;
                continue;
            }
            capacitor* c = new capacitor();
            c->name = elemName;
            c->value = capValue;
            c->n1 = getOrCreateNode(node1);
            c->n2 = getOrCreateNode(node2);
            circuit.push_back(c);
            cout << "Capacitor " << elemName
                 << " added between " << node1 << " and " << node2
                 << " with value " << capValue << " F" << endl;
        }
        else if(elemType == "L") {
            // Create an inductor.
            bool ok;
            double indValue = parseInductanceValue(valueStr, ok);
            if(!ok) {
                cout << "Error parsing inductor value on line: " << line << endl;
                continue;
            }
            inductor* l = new inductor();
            l->name = elemName;
            l->value = indValue;
            l->n1 = getOrCreateNode(node1);
            l->n2 = getOrCreateNode(node2);
            circuit.push_back(l);
            cout << "Inductor " << elemName
                 << " added between " << node1 << " and " << node2
                 << " with value " << indValue << " H" << endl;
        }
        else if(elemType == "V" && valueStr.rfind("SIN(", 0) == 0) {
            // Create a sinusoidal voltage source.
            // valueStr is like "SIN(Voffset Vamplitude Frequency)"
            string sinParams = valueStr.substr(4, valueStr.size() - 5);
            stringstream ssSin(sinParams);
            double Voffset, Vamplitude, Frequency;
            if(!(ssSin >> Voffset >> Vamplitude >> Frequency)) {
                cout << "Error parsing SIN parameters on line: " << line << endl;
                continue;
            }
            vsource* vs = new vsource();
            vs->name = elemName;
            vs->isSinusoidal = true;
            vs->Voffset = Voffset;
            vs->Vamplitude = Vamplitude;
            vs->Frequency = Frequency;
            vs->dcValue = Voffset;       // offset for DC analysis / initial condition
            vs->value = Vamplitude;      // amplitude
            vs->n1 = getOrCreateNode(node1);
            vs->n2 = getOrCreateNode(node2);
            circuit.push_back(vs);
            cout << "Sinusoidal Voltage Source " << elemName
                 << " added between " << node1 << " and " << node2
                 << " with SIN(" << Voffset << " " << Vamplitude
                 << " " << Frequency << ")" << endl;
        }

        else {
            cout << "Skipping unsupported element type: " << elemType << endl;
        }
    }
}

bool showExistingSchematics(const string& schematicsDir, vector<component*> &circuit) {
    vector<fs::directory_entry> files;

    // List all files in the given schematics folder.
    for (const auto& entry : fs::directory_iterator(schematicsDir)) {
        if(entry.is_regular_file())
            files.push_back(entry);
    }

    if(files.empty()){
        cout << "No schematics found in " << schematicsDir << endl;
        return 0;
    }

    // Display the list with an associated number.
    cout << "choose existing schematic:" << endl;
    for (size_t i = 0; i < files.size(); i++) {
        cout << i+1 << " - " << files[i].path().filename().string() << endl;
    }

    while(true) {
        cout << "choose existing schematic: ";
        string userChoice;
        getline(cin, userChoice);

        // Handle return command
        if(userChoice == "return") {
            cout << "Returning to main menu." << endl;
            return 0;
        }

        // Handle numeric selection
        try {
            int choice = stoi(userChoice);
            if(choice < 1 || choice > static_cast<int>(files.size())){
                cout << "-Error : Inappropriate input" << endl;
                continue;
            }

            string selectedFile = files[choice-1].path().string();
            cout << "Netlist spice content:" << endl;

            ifstream infile(selectedFile);
            if(!infile) {
                cout << "Error opening file: " << selectedFile << endl;
                continue;
            }

            string line;
            while(getline(infile, line)){
                cout << line << endl;
            }
            infile.close();

            // Ask if user wants to load after viewing
            cout << "Load this schematic? (yes/no): ";
            string loadChoice;
            getline(cin, loadChoice);
            if(loadChoice == "yes") {
                loadCircuitFromFile(selectedFile, circuit);
                return 1;
            }
        }
        catch(...) {
            cout << "Invalid input. Please enter a number or 'return'." << endl;
        }
    }
}

void saveCircuitToFile(const string &filename, const vector<component*> &circuit) {
    ofstream outfile(filename);
    if(!outfile) {
        cout << "Error: Could not open file " << filename << " for writing." << endl;
        return;
    }
    // For each component, output a line in the following format:
    // <type> <name> <node1> <node2> <value>
    for(auto comp : circuit) {
        // Output ground elements only if you wish; typically the netlist format omits ground because it is assumed "0".
        if(dynamic_cast<ground*>(comp)) {
            // For example, you might write: GND <nodeName>
            ground* g = dynamic_cast<ground*>(comp);
            outfile << "GND " << g->n->name << "\n";
        }
        else if(resistor* r = dynamic_cast<resistor*>(comp)) {
            outfile << "R " << r->name << " " << r->n1->name << " "
                    << r->n2->name << " " << r->value << "\n";
        }
            // Capacitor
        else if(capacitor* c = dynamic_cast<capacitor*>(comp)) {
            outfile << "C " << c->name << " " << c->n1->name << " "
                    << c->n2->name << " " << c->value << "\n";
        }
            // Inductor
        else if(inductor* l = dynamic_cast<inductor*>(comp)) {
            outfile << "L " << l->name << " " << l->n1->name << " "
                    << l->n2->name << " " << l->value << "\n";
        }
        else if(vsource* vs = dynamic_cast<vsource*>(comp)) {
            if(vs->isSinusoidal) {
                // Output using SIN(...) notation
                outfile << "V " << vs->name << " " << vs->n1->name << " "
                        << vs->n2->name << " SIN("
                        << vs->Voffset << " "
                        << vs->Vamplitude << " "
                        << vs->Frequency << ")\n";
            } else {
                outfile << "V " << vs->name << " " << vs->n1->name << " "
                        << vs->n2->name << " " << vs->dcValue << "\n";
            }
        }
        // You can similarly add output for capacitors (C), inductors (L), diodes (D), etc.
    }
    outfile.close();
    cout << "Circuit saved to file: " << filename << endl;
}

struct MathOperation {
    string resultName;
    string operation; // "add", "sub", "mul", "div"
    vector<string> operands;
    vector<double> constants;
};

// Add this to your global variables
vector<MathOperation> mathOperations;

// Add these helper functions
vector<double> performMathOperation(const MathOperation& op, const PlotData& plotData) {
    vector<double> result;

    if (op.operation == "add" || op.operation == "sub") {
        // Find the operands in the plot data
        int idx1 = -1, idx2 = -1;
        for (int i = 0; i < plotData.signals.size(); i++) {
            if (plotData.signals[i].name == op.operands[0]) idx1 = i;
            if (plotData.signals[i].name == op.operands[1]) idx2 = i;
        }

        if (idx1 == -1 || idx2 == -1 ||
            plotData.yValues[idx1].size() != plotData.yValues[idx2].size()) {
            return result;
        }

        // Perform addition or subtraction
        for (size_t i = 0; i < plotData.yValues[idx1].size(); i++) {
            double val1 = plotData.yValues[idx1][i] * plotData.signals[idx1].scaleFactor;
            double val2 = plotData.yValues[idx2][i] * plotData.signals[idx2].scaleFactor;

            if (op.operation == "add") {
                result.push_back(val1 + val2);
            } else {
                result.push_back(val1 - val2);
            }
        }
    }
    else if (op.operation == "mul" || op.operation == "div") {
        // Find the operand in the plot data
        int idx = -1;
        for (int i = 0; i < plotData.signals.size(); i++) {
            if (plotData.signals[i].name == op.operands[0]) idx = i;
        }

        if (idx == -1 || op.constants.empty()) {
            return result;
        }

        double constant = op.constants[0];

        // Perform multiplication or division
        for (size_t i = 0; i < plotData.yValues[idx].size(); i++) {
            double val = plotData.yValues[idx][i] * plotData.signals[idx].scaleFactor;

            if (op.operation == "mul") {
                result.push_back(val * constant);
            } else {
                if (constant != 0) {
                    result.push_back(val / constant);
                } else {
                    result.push_back(0); // Avoid division by zero
                }
            }
        }
    }

    return result;
}

void updateMathSignals(PlotData& plotData) {
    // Remove previously calculated math signals
    vector<SignalConfig> newSignals;
    vector<vector<double>> newYValues;

    for (int i = 0; i < plotData.signals.size(); i++) {
        bool isMathSignal = false;
        for (const auto& op : mathOperations) {
            if (plotData.signals[i].name == op.resultName) {
                isMathSignal = true;
                break;
            }
        }

        if (!isMathSignal) {
            newSignals.push_back(plotData.signals[i]);
            newYValues.push_back(plotData.yValues[i]);
        }
    }

    // Add updated math signals
    for (const auto& op : mathOperations) {
        vector<double> result = performMathOperation(op, plotData);
        if (!result.empty()) {
            SignalConfig newSig = createSignalConfig(op.resultName, "");
            newSignals.push_back(newSig);
            newYValues.push_back(result);
        }
    }

    plotData.signals = newSignals;
    plotData.yValues = newYValues;
}
struct ACConfig {
    double startFreq;
    double endFreq;
    int points;
    vector<string> measVars;
};
vector<complex<double>> solveComplexSystem(vector<vector<complex<double>>> A, vector<complex<double>> b) {
    int n = A.size();
    for (int i = 0; i < n; i++) {
        int pivot = i;
        for (int r = i+1; r < n; r++){
            if(abs(A[r][i]) > abs(A[pivot][i]))
                pivot = r;
        }
        if(abs(A[pivot][i]) < 1e-12)
            throw runtime_error("Singular matrix encountered in AC analysis.");
        swap(A[i], A[pivot]);
        swap(b[i], b[pivot]);
        complex<double> factor = A[i][i];
        for (int j = i; j < n; j++)
            A[i][j] /= factor;
        b[i] /= factor;
        for (int r = i+1; r < n; r++){
            complex<double> mult = A[r][i];
            for (int j = i; j < n; j++)
                A[r][j] -= mult * A[i][j];
            b[r] -= mult * b[i];
        }
    }
    vector<complex<double>> x(n, 0.0);
    for (int i = n-1; i>=0; i--){
        x[i] = b[i];
        for (int j = i+1; j < n; j++){
            x[i] -= A[i][j] * x[j];
        }
    }
    return x;
}

// --------------------------
// AC Analysis Function
// --------------------------
void performACAnalysis(double startFreq, double endFreq, int points,
                       const vector<string> &measVars, const vector<component*> &circuit) {
    // Reset plot data
    currentPlot = PlotData();
    currentPlot.analysisType = "AC";

    // Create signal configurations (magnitude and phase)
    for(const auto& var : measVars) {
        string unit = "";
        if(var.find("V(") != string::npos) unit = "V";
        else if(var.find("I(") != string::npos) unit = "A";

        currentPlot.signals.push_back(createSignalConfig(var + " Magnitude", unit));
        currentPlot.signals.push_back(createSignalConfig(var + " Phase", "°"));
    }

    // Initialize yValues storage (two vectors per signal: magnitude and phase)
    currentPlot.yValues.resize(measVars.size() * 2);

    // Identify ground node
    string groundName = "";
    for(auto comp : circuit) {
        ground* g = dynamic_cast<ground*>(comp);
        if(g && g->n) {
            groundName = g->n->name;
            break;
        }
    }
    if(groundName == ""){
        cout << "ERROR: No ground node defined. Please add a ground element." << endl;
        return;
    }

    // Build node indices (excluding ground)
    map<string,int> nodeIndex;
    int idx = 0;
    for(auto &p : nodesMap) {
        if(p.first != groundName) {
            nodeIndex[p.first] = idx;
            idx++;
        }
    }
    int N = nodeIndex.size();

    // Collect voltage sources
    vector<vsource*> vSources;
    for(auto comp : circuit) {
        if(vsource* vs = dynamic_cast<vsource*>(comp))
            vSources.push_back(vs);
    }
    int M = vSources.size();
    int total = N + M;

    // AC source (for AC analysis, we need at least one AC source)
    complex<double> acSourceValue = 1.0; // Default 1V magnitude, 0 phase
    bool hasACSource = false;

    // Find AC source
    for(auto vs : vSources) {
        // For AC analysis, we consider the source value as complex (magnitude and phase)
        // You might want to extend your vsource class to include AC specific parameters
        acSourceValue = vs->Vamplitude; // Use amplitude as magnitude
        hasACSource = true;
        break;
    }

    if(!hasACSource) {
        cout << "WARNING: No AC source found. Using default 1V source." << endl;
    }

    // Calculate frequency points (logarithmic scale)
    vector<double> frequencies;
    if(points == 1) {
        frequencies.push_back(startFreq);
    } else {
        double logStart = log10(startFreq);
        double logEnd = log10(endFreq);
        double logStep = (logEnd - logStart) / (points - 1);

        for(int i = 0; i < points; i++) {
            double freq = pow(10, logStart + i * logStep);
            frequencies.push_back(freq);
        }
    }

    // Perform AC analysis at each frequency point
    for(double freq : frequencies) {
        double omega = 2 * M_PI * freq;

        // Build MNA system with complex numbers: A x = b
        vector<vector<complex<double>>> A(total, vector<complex<double>>(total, 0.0));
        vector<complex<double>> b(total, 0.0);

        // Add resistor contributions (real values)
        for(auto comp : circuit) {
            resistor* r = dynamic_cast<resistor*>(comp);
            if(r) {
                double G = 1.0 / r->value;
                bool n1Ground = (r->n1->name == groundName);
                bool n2Ground = (r->n2->name == groundName);

                if(!n1Ground) {
                    int i = nodeIndex[r->n1->name];
                    A[i][i] += G;
                }
                if(!n2Ground) {
                    int j = nodeIndex[r->n2->name];
                    A[j][j] += G;
                }
                if(!n1Ground && !n2Ground) {
                    int i = nodeIndex[r->n1->name];
                    int j = nodeIndex[r->n2->name];
                    A[i][j] -= G;
                    A[j][i] -= G;
                }
            }
        }

        // Add capacitor contributions (imaginary values)
        for(auto comp : circuit) {
            capacitor* c = dynamic_cast<capacitor*>(comp);
            if(c) {
                complex<double> Yc = 1i * omega * c->value;
                bool n1Ground = (c->n1->name == groundName);
                bool n2Ground = (c->n2->name == groundName);

                if(!n1Ground) {
                    int i = nodeIndex[c->n1->name];
                    A[i][i] += Yc;
                }
                if(!n2Ground) {
                    int j = nodeIndex[c->n2->name];
                    A[j][j] += Yc;
                }
                if(!n1Ground && !n2Ground) {
                    int i = nodeIndex[c->n1->name];
                    int j = nodeIndex[c->n2->name];
                    A[i][j] -= Yc;
                    A[j][i] -= Yc;
                }
            }
        }

        // Add inductor contributions (imaginary values)
        for(auto comp : circuit) {
            inductor* l = dynamic_cast<inductor*>(comp);
            if(l) {
                complex<double> Yl = 1.0 / (1i * omega * l->value);
                bool n1Ground = (l->n1->name == groundName);
                bool n2Ground = (l->n2->name == groundName);

                if(!n1Ground) {
                    int i = nodeIndex[l->n1->name];
                    A[i][i] += Yl;
                }
                if(!n2Ground) {
                    int j = nodeIndex[l->n2->name];
                    A[j][j] += Yl;
                }
                if(!n1Ground && !n2Ground) {
                    int i = nodeIndex[l->n1->name];
                    int j = nodeIndex[l->n2->name];
                    A[i][j] -= Yl;
                    A[j][i] -= Yl;
                }
            }
        }

        // Add voltage source contributions
        int vsIndex = 0;
        for(auto vs : vSources) {
            int extraRow = N + vsIndex;
            if(vs->n1->name != groundName) {
                int i = nodeIndex[vs->n1->name];
                A[i][extraRow] += 1.0;
                A[extraRow][i] += 1.0;
            }
            if(vs->n2->name != groundName) {
                int j = nodeIndex[vs->n2->name];
                A[j][extraRow] -= 1.0;
                A[extraRow][j] -= 1.0;
            }

            // Set source value (complex)
            complex<double> sourceValue;
            if (vs->ACMagnitude != 0.0) {

                sourceValue = std::polar(vs->ACMagnitude, vs->ACPhase * M_PI / 180.0);
            } else if (vs->isSinusoidal) {

                sourceValue = vs->Vamplitude;
            } else {

                sourceValue = 0.0;
            }
            b[extraRow] = sourceValue;
            vsIndex++;
        }

        // Solve the complex system A x = b
        vector<complex<double>> sol;
        try {
            sol = solveComplexSystem(A, b);
        } catch(runtime_error &e) {
            cout << "ERROR in AC Analysis at frequency " << freq << " Hz: " << e.what() << endl;
            continue;
        }

        cout << "AC Analysis: f = " << freq << " Hz";
        currentPlot.xValues.push_back(freq);

        // Process measurement variables
        for(int i = 0; i < measVars.size(); i++) {
            const string& var = measVars[i];
            complex<double> value = 0.0;

            if(var.substr(0,2) == "V(") {
                size_t pos = var.find(")");
                if(pos == string::npos) continue;
                string nodeName = var.substr(2, pos-2);
                if(nodeIndex.find(nodeName) == nodeIndex.end()) {
                    cout << " | ERROR: Node " << nodeName << " not found";
                    continue;
                }
                value = (nodeName == groundName) ? 0.0 : sol[nodeIndex[nodeName]];
            }
            else if(var.substr(0,2) == "I(") {
                size_t pos = var.find(")");
                if(pos == string::npos) continue;
                string compName = var.substr(2, pos-2);
                bool found = false;

                // Check voltage sources
                for(int vsIdx = 0; vsIdx < vSources.size(); vsIdx++) {
                    if(vSources[vsIdx]->name == compName) {
                        value = sol[N + vsIdx];
                        found = true;
                        break;
                    }
                }

                if(!found) {
                    cout << " | ERROR: Component " << compName << " not found or not a voltage source";
                    continue;
                }
            }
            else {
                cout << " | ERROR: Unknown variable " << var;
                continue;
            }

            // Store magnitude and phase
            double magnitude = abs(value);
            double phase = arg(value) * 180.0 / M_PI; // Convert to degrees

            cout << " | " << var << " = " << magnitude << " with phase of " << phase << " degree ";

            currentPlot.yValues[i*2].push_back(magnitude);     // Magnitude
            currentPlot.yValues[i*2 + 1].push_back(phase);     // Phase
        }
        cout << endl;
    }
}
// --------------------------
// Phase Analysis Function
// --------------------------
void performPhaseAnalysis(double startPhase, double endPhase, int points, double baseFrequency,
                          const vector<string> &measVars, const vector<component*> &circuit) {
    // Reset plot data
    currentPlot = PlotData();
    currentPlot.analysisType = "PHASE";

    // Create signal configurations
    for(const auto& var : measVars) {
        string unit = "";
        if(var.find("V(") != string::npos) unit = "V";
        else if(var.find("I(") != string::npos) unit = "A";
        currentPlot.signals.push_back(createSignalConfig(var, unit));
    }

    // Initialize yValues storage
    currentPlot.yValues.resize(measVars.size());

    // Identify ground node
    string groundName = "";
    for(auto comp : circuit) {
        ground* g = dynamic_cast<ground*>(comp);
        if(g && g->n) {
            groundName = g->n->name;
            break;
        }
    }
    if(groundName == ""){
        cout << "ERROR: No ground node defined. Please add a ground element." << endl;
        return;
    }

    // Build node indices (excluding ground)
    map<string,int> nodeIndex;
    int idx = 0;
    for(auto &p : nodesMap) {
        if(p.first != groundName) {
            nodeIndex[p.first] = idx;
            idx++;
        }
    }
    int N = nodeIndex.size();

    // Collect phase voltage sources
    vector<phaseVoltageSource*> phaseSources;
    for(auto comp : circuit) {
        if(phaseVoltageSource* pvs = dynamic_cast<phaseVoltageSource*>(comp))
            phaseSources.push_back(pvs);
    }

    // Collect regular voltage sources for MNA
    vector<vsource*> vSources;
    for(auto comp : circuit) {
        if(vsource* vs = dynamic_cast<vsource*>(comp))
            vSources.push_back(vs);
    }
    int M = vSources.size() + phaseSources.size();
    int total = N + M;

    // Check if we have at least one phase voltage source
    if(phaseSources.empty()) {
        cout << "ERROR: No phase voltage source found for phase analysis." << endl;
        return;
    }

    // Use the first phase voltage source as the phase-varying source
    phaseVoltageSource* phaseSource = phaseSources[0];
    double originalPhaseOffset = phaseSource->phaseOffset;

    // Calculate phase points
    vector<double> phases;
    if(points == 1) {
        phases.push_back(startPhase);
    } else {
        double phaseStep = (endPhase - startPhase) / (points - 1);
        for(int i = 0; i < points; i++) {
            double phase = startPhase + i * phaseStep;
            phases.push_back(phase);
        }
    }

    // Perform phase analysis at each phase point
    for(double phase : phases) {
        // Set the phase offset of the source
        phaseSource->phaseOffset = phase;

        double omega = 2 * M_PI * phaseSource->baseFrequency;

        // Build MNA system with complex numbers: A x = b
        vector<vector<complex<double>>> A(total, vector<complex<double>>(total, 0.0));
        vector<complex<double>> b(total, 0.0);

        // Add resistor contributions (real values)
        for(auto comp : circuit) {
            resistor* r = dynamic_cast<resistor*>(comp);
            if(r) {
                double G = 1.0 / r->value;
                bool n1Ground = (r->n1->name == groundName);
                bool n2Ground = (r->n2->name == groundName);

                if(!n1Ground) {
                    int i = nodeIndex[r->n1->name];
                    A[i][i] += G;
                }
                if(!n2Ground) {
                    int j = nodeIndex[r->n2->name];
                    A[j][j] += G;
                }
                if(!n1Ground && !n2Ground) {
                    int i = nodeIndex[r->n1->name];
                    int j = nodeIndex[r->n2->name];
                    A[i][j] -= G;
                    A[j][i] -= G;
                }
            }
        }

        // Add capacitor contributions (imaginary values)
        for(auto comp : circuit) {
            capacitor* c = dynamic_cast<capacitor*>(comp);
            if(c) {
                complex<double> Yc = 1i * omega * c->value;
                bool n1Ground = (c->n1->name == groundName);
                bool n2Ground = (c->n2->name == groundName);

                if(!n1Ground) {
                    int i = nodeIndex[c->n1->name];
                    A[i][i] += Yc;
                }
                if(!n2Ground) {
                    int j = nodeIndex[c->n2->name];
                    A[j][j] += Yc;
                }
                if(!n1Ground && !n2Ground) {
                    int i = nodeIndex[c->n1->name];
                    int j = nodeIndex[c->n2->name];
                    A[i][j] -= Yc;
                    A[j][i] -= Yc;
                }
            }
        }

        // Add inductor contributions (imaginary values)
        for(auto comp : circuit) {
            inductor* l = dynamic_cast<inductor*>(comp);
            if(l) {
                complex<double> Yl = 1.0 / (1i * omega * l->value);
                bool n1Ground = (l->n1->name == groundName);
                bool n2Ground = (l->n2->name == groundName);

                if(!n1Ground) {
                    int i = nodeIndex[l->n1->name];
                    A[i][i] += Yl;
                }
                if(!n2Ground) {
                    int j = nodeIndex[l->n2->name];
                    A[j][j] += Yl;
                }
                if(!n1Ground && !n2Ground) {
                    int i = nodeIndex[l->n1->name];
                    int j = nodeIndex[l->n2->name];
                    A[i][j] -= Yl;
                    A[j][i] -= Yl;
                }
            }
        }

        // Add voltage source contributions
        int vsIndex = 0;

        // First add regular voltage sources
        for(auto vs : vSources) {
            int extraRow = N + vsIndex;
            if(vs->n1->name != groundName) {
                int i = nodeIndex[vs->n1->name];
                A[i][extraRow] += 1.0;
                A[extraRow][i] += 1.0;
            }
            if(vs->n2->name != groundName) {
                int j = nodeIndex[vs->n2->name];
                A[j][extraRow] -= 1.0;
                A[extraRow][j] -= 1.0;
            }

            // Set source value (complex)
            complex<double> sourceValue;
            if(vs->ACMagnitude != 0.0) {
                sourceValue = std::polar(vs->ACMagnitude, vs->ACPhase * M_PI / 180.0);
            } else if(vs->isSinusoidal) {
                sourceValue = vs->Vamplitude;
            } else {
                sourceValue = 0.0;
            }
            b[extraRow] = sourceValue;
            vsIndex++;
        }

        // Then add phase voltage sources
        for(auto pvs : phaseSources) {
            int extraRow = N + vsIndex;
            if(pvs->n1->name != groundName) {
                int i = nodeIndex[pvs->n1->name];
                A[i][extraRow] += 1.0;
                A[extraRow][i] += 1.0;
            }
            if(pvs->n2->name != groundName) {
                int j = nodeIndex[pvs->n2->name];
                A[j][extraRow] -= 1.0;
                A[extraRow][j] -= 1.0;
            }

            // Set source value using cos(ωt + φ) = Re[e^{j(ωt + φ)}] = Re[e^{jωt} * e^{jφ}]
            // For AC analysis, we use the complex amplitude: amplitude * e^{jφ}
            complex<double> sourceValue = std::polar(pvs->amplitude, pvs->phaseOffset);
            b[extraRow] = sourceValue;
            vsIndex++;
        }

        // Solve the complex system A x = b
        vector<complex<double>> sol;
        try {
            sol = solveComplexSystem(A, b);
        } catch(runtime_error &e) {
            cout << "ERROR in Phase Analysis at phase " << phase << " rad: " << e.what() << endl;
            continue;
        }

        cout << "Phase Analysis: Phase = " << phase << " rad";
        currentPlot.xValues.push_back(phase);

        // Process measurement variables
        for(int i = 0; i < measVars.size(); i++) {
            const string& var = measVars[i];
            complex<double> value = 0.0;

            if(var.substr(0,2) == "V(") {
                size_t pos = var.find(")");
                if(pos == string::npos) continue;
                string nodeName = var.substr(2, pos-2);
                if(nodeIndex.find(nodeName) == nodeIndex.end()) {
                    cout << " | ERROR: Node " << nodeName << " not found";
                    continue;
                }
                value = (nodeName == groundName) ? 0.0 : sol[nodeIndex[nodeName]];
            }
            else if(var.substr(0,2) == "I(") {
                size_t pos = var.find(")");
                if(pos == string::npos) continue;
                string compName = var.substr(2, pos-2);
                bool found = false;

                // Check regular voltage sources
                for(int vsIdx = 0; vsIdx < vSources.size(); vsIdx++) {
                    if(vSources[vsIdx]->name == compName) {
                        value = sol[N + vsIdx];
                        found = true;
                        break;
                    }
                }

                // Check phase voltage sources
                if(!found) {
                    for(int pvsIdx = 0; pvsIdx < phaseSources.size(); pvsIdx++) {
                        if(phaseSources[pvsIdx]->name == compName) {
                            value = sol[N + vSources.size() + pvsIdx];
                            found = true;
                            break;
                        }
                    }
                }

                if(!found) {
                    cout << " | ERROR: Component " << compName << " not found or not a voltage source";
                    continue;
                }
            }
            else {
                cout << " | ERROR: Unknown variable " << var;
                continue;
            }

            // Store magnitude
            double magnitude = abs(value);

            cout << " | " << var << " = " << magnitude;

            currentPlot.yValues[i].push_back(magnitude);
        }
        cout << endl;
    }

    // Restore original phase offset of the source
    phaseSource->phaseOffset = originalPhaseOffset;
}
SDL_Texture* loadTexture(SDL_Renderer* renderer, const string& path) {
    SDL_Texture* texture = nullptr;
    SDL_Surface* surface = IMG_Load(path.c_str());
    if(surface) {
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
    return texture;
}

int showMenu(const std::string& imagePath)
{
    // 1) Create window
    SDL_Window* menuWindow = SDL_CreateWindow(
            "Menu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            800, 600, SDL_WINDOW_SHOWN
    );
    if (!menuWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateWindow: %s", SDL_GetError());
        return -1;
    }

    // 2) Create renderer
    SDL_Renderer* menuRenderer = SDL_CreateRenderer(
            menuWindow, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!menuRenderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer: %s", SDL_GetError());
        SDL_DestroyWindow(menuWindow);
        return -1;
    }

    // 3) Load menu texture
    SDL_Texture* menuTex = IMG_LoadTexture(menuRenderer, imagePath.c_str());
    if (!menuTex) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IMG_LoadTexture: %s", IMG_GetError());
        SDL_DestroyRenderer(menuRenderer);
        SDL_DestroyWindow(menuWindow);
        return -1;
    }

    // 4) Define button hit-rects
    std::vector<SDL_Rect> btnRects = {
            {  72, 180, 654,  76 },
            {  71, 282, 656,  77 },
            {  72, 383, 655,  78 },
            {  74, 486, 655,  76 }
    };

    int hoverIndex = -1;

    // 5) Initial draw
    SDL_RenderClear(menuRenderer);
    SDL_RenderCopy(menuRenderer, menuTex, nullptr, nullptr);
    SDL_RenderPresent(menuRenderer);

    // 6) Event loop
    SDL_Event e;
    while (SDL_WaitEvent(&e)) {
        bool redraw = false;

        if (e.type == SDL_QUIT) {
            hoverIndex = -1;
            break;
        }

        if (e.type == SDL_MOUSEMOTION) {
            SDL_Point p{ e.motion.x, e.motion.y };
            int newHover = -1;
            for (int i = 0; i < (int)btnRects.size(); ++i) {
                if (SDL_PointInRect(&p, &btnRects[i])) {
                    newHover = i;
                    break;
                }
            }
            if (newHover != hoverIndex) {
                hoverIndex = newHover;
                redraw = true;
            }
        }

        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            SDL_Point p{ e.button.x, e.button.y };
            for (int i = 0; i < (int)btnRects.size(); ++i) {
                if (SDL_PointInRect(&p, &btnRects[i])) {
                    hoverIndex = i;
                    break;
                }
            }
            break;  // exit loop on click or outside
        }

        if (redraw) {
            SDL_RenderClear(menuRenderer);
            SDL_RenderCopy(menuRenderer, menuTex, nullptr, nullptr);

            if (hoverIndex >= 0) {
                SDL_SetRenderDrawBlendMode(menuRenderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(menuRenderer, 255, 255, 255, 100);
                SDL_RenderFillRect(menuRenderer, &btnRects[hoverIndex]);
                SDL_SetRenderDrawBlendMode(menuRenderer, SDL_BLENDMODE_NONE);
            }

            SDL_RenderPresent(menuRenderer);
        }
    }

    // 7) Cleanup
    SDL_DestroyTexture(menuTex);
    SDL_DestroyRenderer(menuRenderer);
    SDL_DestroyWindow(menuWindow);

    return hoverIndex;  // -1 if quit, 0..3 for the button clicked
}

bool showSplash(const std::string& imgPath,
                int width       = 800,
                int height      = 600,
                int displayTime = 2000)
{
    // 1) Init SDL + IMG
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << "\n";
        SDL_Quit();
        return false;
    }

    // 2) Create window + renderer
    SDL_Window*  win = SDL_CreateWindow(
            "Splash",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_BORDERLESS
    );
    if (!win) {
        std::cerr << "CreateWindow failed: " << SDL_GetError() << "\n";
        IMG_Quit();
        SDL_Quit();
        return false;
    }

    SDL_Renderer* ren = SDL_CreateRenderer(
            win, -1, SDL_RENDERER_ACCELERATED
    );
    if (!ren) {
        std::cerr << "CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(win);
        IMG_Quit();
        SDL_Quit();
        return false;
    }

    // 3) Load texture and show
    SDL_Texture* tex = IMG_LoadTexture(ren, imgPath.c_str());
    if (tex) {
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        SDL_Delay(displayTime);
        SDL_DestroyTexture(tex);
    } else {
        std::cerr << "IMG_LoadTexture failed: " << IMG_GetError() << "\n";
    }

    // 4) Clean up
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    IMG_Quit();
    SDL_Quit();
    return true;
}
struct Probe {
    ProbeType type;
    string targetName;
    SDL_Color color;
};


vector<Probe> activeProbes;
unordered_map<string, vector<double>> probeData;


void processProbeCommand(const vector<string>& words) {
    if (words.size() != 3) {
        cout << "Usage: .prob [V|I|P] <name>" << endl;
        return;
    }

    string probeType = words[1];
    string targetName = words[2];
    ProbeType type;

    if (probeType == "V") type = PROBE_VOLTAGE;
    else if (probeType == "I") type = PROBE_CURRENT;
    else if (probeType == "P") type = PROBE_POWER;
    else {
        cout << "Invalid probe type. Use V, I, or P." << endl;
        return;
    }


    bool exists = false;
    if (type == PROBE_VOLTAGE) {
        for (const auto& node : graphicalNodes) {
            if (node.name == targetName) {
                exists = true;
                break;
            }
        }
    } else {
        for (const auto& comp : graphicalComponents) {
            if (comp.name == targetName) {
                exists = true;
                break;
            }
        }
    }

    if (!exists) {
        cout << "Target '" << targetName << "' not found." << endl;
        return;
    }


    Probe newProbe;
    newProbe.type = type;
    newProbe.targetName = targetName;
    newProbe.color = {static_cast<Uint8>(rand() % 256),
                      static_cast<Uint8>(rand() % 256),
                      static_cast<Uint8>(rand() % 256), 255};

    activeProbes.push_back(newProbe);
    cout << "Added " << probeType << " probe for " << targetName << endl;
}


void calculateProbeValues() {
    probeData.clear();

    for (const auto& probe : activeProbes) {
        vector<double> values;

        if (probe.type == PROBE_VOLTAGE) {

            for (const auto& voltage : currentPlot.nodeVoltages) {

                double value = 0.0;

                values.push_back(value);
            }
        }
        else if (probe.type == PROBE_CURRENT) {

            for (const auto& current : currentPlot.sourceCurrents) {

                double value = 0.0;

                values.push_back(value);
            }
        }
        else if (probe.type == PROBE_POWER) {

            for (size_t i = 0; i < currentPlot.nodeVoltages.size(); i++) {

                double voltage = 0.0;
                double current = 0.0;
                double value = voltage * current;
                values.push_back(value);
            }
        }

        probeData[probe.targetName] = values;
    }
}


void addProbesToPlot() {
    for (const auto& probe : activeProbes) {
        string signalName;
        string unit;

        if (probe.type == PROBE_VOLTAGE) {
            signalName = "V(" + probe.targetName + ")";
            unit = "V";
        }
        else if (probe.type == PROBE_CURRENT) {
            signalName = "I(" + probe.targetName + ")";
            unit = "A";
        }
        else if (probe.type == PROBE_POWER) {
            signalName = "P(" + probe.targetName + ")";
            unit = "W";
        }

        SignalConfig config = createSignalConfig(signalName, unit);
        config.color = probe.color;

        currentPlot.signals.push_back(config);
        currentPlot.yValues.push_back(probeData[probe.targetName]);
    }
}



// --------------------------
// Main Program - Command Processing
// --------------------------
int SDL_main(int argc, char* argv[]){
    string mainFolder = "C:\\Users\\Pardis\\Desktop\\OOP_Pro2_v3\\";

    // --------------------------------------------------
    // 1) Splash Screen (unchanged)
    // --------------------------------------------------
    string splashPath = mainFolder + "pictures\\Splash.png";
    showSplash(splashPath);
    // --------------------------------------------------

    // --------------------------------------------------
    // 2) Show menu
    // --------------------------------------------------
    string menuPath = mainFolder + "pictures\\menu.png";
    int selected = showMenu(menuPath);

    //schem editor
    if (selected == 0) {
        if (runVisualEditor() != 0) {
            std::cerr << "Visual SPICE failed\n";
            return 1;
        }
    }
    else if (selected == 3) {
        return 0;
    }

    // UI part
    vector<component*> circuit;  // Global circuit vector
    string schematicsFolder = mainFolder+ "schematics";


    while (true) {
        // Check for the show schematics command.
        cout << "Enter command (-show existing schematics or NewFile or -show schematic editor)";
        string input;
        getline(cin, input);
        stringstream ss(input);
        vector<string> words;
        string token;
        while (ss >> token)
            words.push_back(token);

        bool enterSchemtic = 0;
        // If user types exactly "-show existing schematics"
        if(words.size() >= 3 && words[0] == "-show" && words[1] == "existing" && words[2] == "schematics") {
            enterSchemtic = showExistingSchematics(schematicsFolder, circuit);
        }
        else if(words.size() == 1 && words[0] == "NewFile"){
            cout << "Creating new file, dont forget to save " << endl;
            enterSchemtic =1;
        }
        else if(words.size() >= 3 && words[0] == "-show" && words[1] == "schematic" && words[2] == "editor") {
            enterSchemtic = 1;
        }
        else if(words.size() == 1 && words[0] == "exit"){
            cout<<"exit program"<<endl;
            break;
        }
        else {
            cout<<"invalid input"<<endl;
            continue;
        }
        if(enterSchemtic == 0){
            continue;
        }
        while (true) {
            string input;
            getline(cin, input);
            stringstream ss(input);
            vector<string> words;
            string token;
            while (ss >> token)
                words.push_back(token);
            if (words.size() == 0)
                continue;
            if (words[0] == "end")
                break;
            // ... inside your main() command processing loop:
            //save file
            if (words.size() >= 2 && words[0] == "-save" && words[1] == "circuit") {
                cout << "Enter filename to save the circuit: ";
                string saveFile;
                getline(cin, saveFile);
                saveCircuitToFile(saveFile, circuit);
                continue;
            }

            // --- .nodes Command ---
            if (words[0] == ".nodes") {
                if (nodesMap.empty()) {
                    cout << "Available nodes: None" << endl;
                } else {
                    cout << "Available nodes: ";
                    bool first = true;
                    for (auto &p: nodesMap) {
                        if (!first)
                            cout << ", ";
                        cout << p.first;
                        first = false;
                    }
                    cout << endl;
                }
                continue;
            }

            // --- .rename Command ---
            if (words[0] == ".rename") {
                if (words.size() != 4 || words[1] != "node") {
                    cout << "ERROR: Invalid syntax - correct format: .rename node <old_name> <new_name>" << endl;
                    continue;
                }
                string oldName = words[2], newName = words[3];
                if (nodesMap.find(oldName) == nodesMap.end()) {
                    cout << "ERROR: Node " << oldName << " not found in circuit" << endl;
                    cout << "Cause: The node doesn't exist." << endl;
                    cout << "Solutions: Verify the node name; use .nodes to list available nodes." << endl;
                    continue;
                }
                if (nodesMap.find(newName) != nodesMap.end()) {
                    cout << "ERROR: Node name " << newName << " already exists" << endl;
                    cout << "Choose a new name; use .nodes to check existing names" << endl;
                    continue;
                }
                node *nd = nodesMap[oldName];
                nodesMap.erase(oldName);
                nd->name = newName;
                nodesMap[newName] = nd;
                cout << "SUCCESS: Node renamed from " << oldName << " to " << newName << endl;
                continue;
            }

            // --- .print Command ---
            if (words[0] == ".print") {
                if (words.size() < 2) {
                    cout << "Syntax error in command" << endl;
                    continue;
                }
                string analysisType = words[1];
                // Transient Analysis:
                if (analysisType == "TRAN") {
                    // Expect at least two parameters for Tstep and Tstop. Optional Tstart may follow.
                    if (words.size() < 4) {
                        cout << "Syntax error in command" << endl;
                        continue;
                    }
                    // Parse Tstep and Tstop using stod() (assuming user enters numbers, not units).
                    double Tstep = stod(words[2]);
                    double Tstop = stod(words[3]);
                    double Tstart = 0.0;
                    if (words.size() >= 5)
                        Tstart = stod(words[4]);
                    vector<string> variables;
                    // Variables start from the next token (if any)
                    for (size_t i = (words.size() >= 6 ? 5 : 4); i < words.size(); i++) {
                        variables.push_back(words[i]);
                    }
                    cout << "Simulating TRAN analysis with Tstep = " << Tstep
                         << ", Tstop = " << Tstop << ", Tstart = " << Tstart << endl;
                    cout << "Monitoring variables: ";
                    for (auto v: variables)
                        cout << v << " ";
                    cout << endl;
                    try {
                        performTRANAnalysis(Tstep, Tstop, Tstart, variables, circuit);
                        plotsMap["TRAN_" + to_string(time(nullptr))] = currentPlot;
                    } catch (exception &e) {
                        cout << "Error in Transient Analysis: " << e.what() << endl;
                    }
                }
                else if (analysisType == "AC") {
                    if (words.size() < 6) {
                        cout << "Syntax error in command" << endl;
                        continue;
                    }

                    double startFreq = stod(words[2]);
                    double endFreq = stod(words[3]);
                    int points = stoi(words[4]);

                    vector<string> variables;
                    for (int i = 5; i < words.size(); i++) {
                        variables.push_back(words[i]);
                    }

                    if (variables.empty()) {
                        cout << "Syntax error in command" << endl;
                        continue;
                    }

                    cout << "Simulating AC analysis from " << startFreq << " Hz to "
                         << endFreq << " Hz with " << points << " points" << endl;
                    cout << "Monitoring variables: ";
                    for (auto v: variables)
                        cout << v << " ";
                    cout << endl;

                    try {
                        performACAnalysis(startFreq, endFreq, points, variables, circuit);
                        plotsMap["AC_" + to_string(time(nullptr))] = currentPlot;
                    } catch (exception &e) {
                        cout << "Error in AC Analysis: " << e.what() << endl;
                    }
                    continue;
                }

                else if (analysisType == "PHASE") {
                    if (words.size() < 7) {
                        cout << "Syntax error in command. Use: .print PHASE <start_phase> <end_phase> <points> <base_freq> <variables...>" << endl;
                        continue;
                    }

                    try {
                        double startPhase = stod(words[2]);
                        double endPhase = stod(words[3]);
                        int points = stoi(words[4]);
                        double baseFrequency = stod(words[5]);

                        vector<string> variables;
                        for (int i = 6; i < words.size(); i++) {
                            variables.push_back(words[i]);
                        }

                        if (variables.empty()) {
                            cout << "Syntax error in command. No variables specified." << endl;
                            continue;
                        }

                        cout << "Simulating Phase analysis from " << startPhase << " rad to "
                             << endPhase << " rad with " << points << " points" << endl;
                        cout << "Base frequency: " << baseFrequency << " Hz" << endl;
                        cout << "Monitoring variables: ";
                        for (auto v: variables)
                            cout << v << " ";
                        cout << endl;

                        performPhaseAnalysis(startPhase, endPhase, points, baseFrequency, variables, circuit);
                        plotsMap["PHASE_" + to_string(time(nullptr))] = currentPlot;
                    } catch (exception &e) {
                        cout << "Error in Phase Analysis: " << e.what() << endl;
                        cout << "Please check your parameters. Usage: .print PHASE <start_phase> <end_phase> <points> <base_freq> <variables...>" << endl;
                    }
                    continue;
                }

                    // DC Analysis:
                else if (analysisType == "DC") {
                    if (words.size() < 7) {
                        cout << "Syntax error in command" << endl;
                        continue;
                    }
                    string sourceName = words[2];
                    string startValue = words[3];
                    string endValue = words[4];
                    string increment = words[5];
                    vector<string> variables;
                    for (int i = 6; i < words.size(); i++) {
                        variables.push_back(words[i]);
                    }
                    if (variables.empty()) {
                        cout << "Syntax error in command" << endl;
                        continue;
                    }
                    bool err = false;
                    for (auto var: variables) {
                        if (var.substr(0, 2) == "V(") {
                            size_t pos = var.find(")");
                            if (pos == string::npos) {
                                cout << "Syntax error in command" << endl;
                                err = true;
                                break;
                            }
                            string nodeName = var.substr(2, pos - 2);
                            if (nodesMap.find(nodeName) == nodesMap.end()) {
                                cout << "Node " << nodeName << " not found in circuit" << endl;
                                err = true;
                                break;
                            }
                        } else if (var.substr(0, 2) == "I(") {
                            size_t pos = var.find(")");
                            if (pos == string::npos) {
                                cout << "Syntax error in command" << endl;
                                err = true;
                                break;
                            }
                            string compName = var.substr(2, pos - 2);
                            bool found = false;
                            for (auto comp: circuit) {
                                if (comp->name == compName) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                cout << "Component " << compName << " not found in circuit" << endl;
                                err = true;
                                break;
                            }
                        } else {
                            cout << "Syntax error in command" << endl;
                            err = true;
                            break;
                        }
                    }
                    if (err)
                        continue;
                    cout << "Simulating DC analysis with source: " << sourceName
                         << ", start = " << startValue << ", end = " << endValue
                         << ", increment = " << increment << endl;
                    cout << "Monitoring variables: ";
                    for (auto v: variables)
                        cout << v << " ";
                    cout << endl;

                    // Parse the sweep parameters using parseResistanceValue.
                    bool validParam;
                    double sVal = parseResistanceValue(startValue, validParam);
                    if (!validParam) {
                        cout << "Error parsing start value: " << startValue << endl;
                        continue;
                    }
                    double eVal = parseResistanceValue(endValue, validParam);
                    if (!validParam) {
                        cout << "Error parsing end value: " << endValue << endl;
                        continue;
                    }
                    double incVal = parseResistanceValue(increment, validParam);
                    if (!validParam) {
                        cout << "Error parsing increment: " << increment << endl;
                        continue;
                    }

                    try {
                        performDCAnalysis(sourceName, sVal, eVal, incVal, variables, circuit);
                        plotsMap["DC_" + to_string(time(nullptr))] = currentPlot;
                    } catch (exception &e) {
                        cout << "Error in DC Analysis: " << e.what() << endl;
                    }
                } else {
                    cout << "Syntax error in command" << endl;
                    continue;
                }
                continue;
            }
                // --- .plot Command ---
            else if (words[0] == ".plot") {
                if (currentPlot.xValues.empty()) {
                    cout << "No plot data available. Run an analysis first." << endl;
                } else {
                    calculateProbeValues();
                    addProbesToPlot();
                    showPlot(currentPlot);
                }
                continue;
            }
            else if (words[0] == ".prob") {
                processProbeCommand(words);
                calculateProbeValues();
                addProbesToPlot();
                showPlot(currentPlot);
            }
                // --- .unit Command ---
            else if (words[0] == ".unit") {
                if (words.size() < 3) {
                    cout << "Usage: .unit <signal_name> <unit>" << endl;
                    cout << "Available units: V, mV, μV, A, mA, μA" << endl;
                    continue;
                }

                string signalName = words[1];
                string unit = words[2];

                bool found = false;
                for (auto& signal : currentPlot.signals) {
                    if (signal.name == signalName) {
                        signal.unit = unit;

                        // Set scale factor
                        if(unit == "mV") signal.scaleFactor = 1000.0;
                        else if(unit == "μV") signal.scaleFactor = 1000000.0;
                        else if(unit == "mA") signal.scaleFactor = 1000.0;
                        else if(unit == "μA") signal.scaleFactor = 1000000.0;
                        else signal.scaleFactor = 1.0;

                        cout << "Unit for " << signalName << " set to " << unit << endl;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    cout << "Signal " << signalName << " not found in current plot" << endl;
                }
                continue;
            }
            else if (words[0] == ".math") {
                if (words.size() < 2) {
                    cout << "Usage: .math [add|sub|mul|div|list|remove]" << endl;
                    continue;
                }

                string mathCommand = words[1];

                if (mathCommand == "list") {
                    if (mathOperations.empty()) {
                        cout << "No mathematical operations defined." << endl;
                    } else {
                        cout << "Mathematical operations:" << endl;
                        for (int i = 0; i < mathOperations.size(); i++) {
                            const auto& op = mathOperations[i];
                            cout << i+1 << ". " << op.resultName << " = ";

                            if (op.operation == "add" || op.operation == "sub") {
                                cout << op.operands[0] << " " << (op.operation == "add" ? "+" : "-") << " " << op.operands[1];
                            } else if (op.operation == "mul" || op.operation == "div") {
                                cout << op.operands[0] << " " << (op.operation == "mul" ? "×" : "÷") << " " << op.constants[0];
                            }
                            cout << endl;
                        }
                    }
                }
                else if (mathCommand == "remove") {
                    if (words.size() < 3) {
                        cout << "Usage: .math remove <index>" << endl;
                        continue;
                    }

                    try {
                        int index = stoi(words[2]) - 1;
                        if (index >= 0 && index < mathOperations.size()) {
                            mathOperations.erase(mathOperations.begin() + index);
                            cout << "Operation removed successfully." << endl;
                            updateMathSignals(currentPlot);
                        } else {
                            cout << "Invalid index." << endl;
                        }
                    } catch (...) {
                        cout << "Invalid index." << endl;
                    }
                }
                else if (mathCommand == "add" || mathCommand == "sub") {
                    if (words.size() < 5) {
                        cout << "Usage: .math " << mathCommand << " <result_name> <signal1> <signal2>" << endl;
                        continue;
                    }

                    string resultName = words[2];
                    string signal1 = words[3];
                    string signal2 = words[4];

                    // Check if signals exist
                    bool found1 = false, found2 = false;
                    for (const auto& sig : currentPlot.signals) {
                        if (sig.name == signal1) found1 = true;
                        if (sig.name == signal2) found2 = true;
                    }

                    if (!found1 || !found2) {
                        cout << "One or both signals not found." << endl;
                        continue;
                    }

                    // Check if result name already exists
                    for (const auto& op : mathOperations) {
                        if (op.resultName == resultName) {
                            cout << "Result name already exists." << endl;
                            continue;
                        }
                    }

                    MathOperation newOp;
                    newOp.resultName = resultName;
                    newOp.operation = mathCommand;
                    newOp.operands = {signal1, signal2};

                    mathOperations.push_back(newOp);
                    updateMathSignals(currentPlot);
                    cout << "Operation added successfully." << endl;
                }
                else if (mathCommand == "mul" || mathCommand == "div") {
                    if (words.size() < 5) {
                        cout << "Usage: .math " << mathCommand << " <result_name> <signal> <constant>" << endl;
                        continue;
                    }

                    string resultName = words[2];
                    string signal = words[3];
                    string constantStr = words[4];

                    // Check if signal exists
                    bool found = false;
                    for (const auto& sig : currentPlot.signals) {
                        if (sig.name == signal) found = true;
                    }

                    if (!found) {
                        cout << "Signal not found." << endl;
                        continue;
                    }

                    // Parse constant
                    try {
                        double constant = stod(constantStr);

                        // Check if result name already exists
                        for (const auto& op : mathOperations) {
                            if (op.resultName == resultName) {
                                cout << "Result name already exists." << endl;
                                continue;
                            }
                        }

                        MathOperation newOp;
                        newOp.resultName = resultName;
                        newOp.operation = mathCommand;
                        newOp.operands = {signal};
                        newOp.constants = {constant};

                        mathOperations.push_back(newOp);
                        updateMathSignals(currentPlot);
                        cout << "Operation added successfully." << endl;
                    } catch (...) {
                        cout << "Invalid constant." << endl;
                    }
                }
                else {
                    cout << "Unknown math command: " << mathCommand << endl;
                }
            }
                // --- .addsignal Command ---
            else if (words[0] == ".addsignal") {
                if (words.size() != 2) {
                    cout << "Usage: .addsignal V(node_name)" << endl;
                    continue;
                }

                string signalExpr = words[1];
                size_t startPos = signalExpr.find('(');
                size_t endPos = signalExpr.find(')');

                if (startPos == string::npos || endPos == string::npos || endPos <= startPos) {
                    cout << "Invalid signal format. Use: V(node_name)" << endl;
                    continue;
                }

                string nodeName = signalExpr.substr(startPos+1, endPos-startPos-1);

                // Check if node exists
                bool nodeExists = false;
                for (const auto& name : currentPlot.allNodeNames) {
                    if (name == nodeName) {
                        nodeExists = true;
                        break;
                    }
                }

                if (!nodeExists) {
                    cout << "Node " << nodeName << " not found in circuit or not calculated" << endl;
                    cout << "Available nodes: ";
                    for (const auto& name : currentPlot.allNodeNames) {
                        cout << name << " ";
                    }
                    cout << endl;
                    continue;
                }

                // Find node index
                int nodeIdx = -1;
                for (int i = 0; i < currentPlot.allNodeNames.size(); i++) {
                    if (currentPlot.allNodeNames[i] == nodeName) {
                        nodeIdx = i;
                        break;
                    }
                }

                if (nodeIdx == -1) {
                    cout << "Internal error: Node index not found" << endl;
                    continue;
                }

                // Create new signal data
                vector<double> nodeVoltages;
                for (const auto& voltages : currentPlot.nodeVoltages) {
                    nodeVoltages.push_back(voltages[nodeIdx]);
                }

                // Add to plot data
                string signalName = "V(" + nodeName + ")";
                SignalConfig newConfig = createSignalConfig(signalName, "V");
                currentPlot.signals.push_back(newConfig);
                currentPlot.yValues.push_back(nodeVoltages);

                cout << "Added signal: " << signalName << endl;
                continue;
            }
            else if (words[0] == ".list") {
                string filter = "";
                if (words.size() == 2)
                    filter = words[1];
                if (circuit.empty()) {
                    cout << "No circuit elements available." << endl;
                } else {
                    for (auto comp: circuit) {
                        bool match = true;
                        if (!filter.empty()) {
                            if (filter == "R" || filter == "r") {
                                if (comp->name[0] != 'R') match = false;
                            } else if (filter == "C" || filter == "c") {
                                if (comp->name[0] != 'C') match = false;
                            } else if (filter == "L" || filter == "l") {
                                if (comp->name[0] != 'L') match = false;
                            } else if (filter == "D" || filter == "d") {
                                if (comp->name[0] != 'D') match = false;
                            } else if (filter == "V" || filter == "v") {
                                if (comp->name[0] != 'V') match = false;
                            } else if (filter == "G" || filter == "GND") {
                                if (dynamic_cast<ground *>(comp) == nullptr)
                                    match = false;
                            } else {
                                cout << "Error: Invalid component type filter" << endl;
                                match = false;
                                break;
                            }
                        }
                        if (match) {
                            if (dynamic_cast<ground *>(comp) != nullptr) {
                                ground *g = dynamic_cast<ground *>(comp);
                                cout << "Ground at node " << g->n->name << endl;
                            } else {
                                char type = comp->name[0];
                                if (type == 'R') {
                                    cout << "Resistor " << comp->name << ": between "
                                         << comp->n1->name << " and " << comp->n2->name
                                         << ", value = " << comp->value << " ohms" << endl;
                                } else if (type == 'C') {
                                    cout << "Capacitor " << comp->name << ": between "
                                         << comp->n1->name << " and " << comp->n2->name
                                         << ", value = " << comp->value << " F" << endl;
                                } else if (type == 'L') {
                                    cout << "Inductor " << comp->name << ": between "
                                         << comp->n1->name << " and " << comp->n2->name
                                         << ", value = " << comp->value << " H" << endl;
                                } else if (type == 'D') {
                                    diode *d = dynamic_cast<diode *>(comp);
                                    cout << "Diode " << comp->name << ": between "
                                         << comp->n1->name << " and " << comp->n2->name
                                         << ", model = " << d->model << endl;
                                } else if (type == 'V') {
                                    vsource *vs = dynamic_cast<vsource *>(comp);
                                    cout << "Voltage Source " << comp->name << ": between "
                                         << comp->n1->name << " and " << comp->n2->name
                                         << ", DC value = " << vs->dcValue << " V";
                                    if (vs->isSinusoidal) {
                                        cout << ", SIN( offset = " << vs->Voffset
                                             << ", amplitude = " << vs->Vamplitude
                                             << ", freq = " << vs->Frequency << " )";
                                    }
                                    cout << endl;
                                }
                            }
                        }
                    }
                }
                continue;
            } else if (words[0] == "add") {
                if (words.size() < 2) {
                    cout << "Error: Syntax error" << endl;
                    continue;
                }
                string elementName = words[1];
                // Allow: GND, R, C, L, D, VoltageSource, CurrentSource, and sinusoidal sources starting with V.
                if (elementName != "GND" && elementName[0] != 'R' && elementName[0] != 'C' &&
                    elementName[0] != 'L' && elementName[0] != 'D' &&
                    elementName.substr(0, 13) != "VoltageSource" &&
                    elementName.substr(0, 13) != "CurrentSource" &&
                    elementName[0] != 'V') {
                    cout << "Error: Element " << elementName << " not found in library" << endl;
                    continue;
                }
                // --- Ground Handling ---
                if (elementName == "GND") {
                    if (words.size() != 3) {
                        cout << "Error: Syntax error" << endl;
                        continue;
                    }
                    string nodeName = words[2];
                    bool duplicate = false;
                    for (auto comp: circuit) {
                        ground *g = dynamic_cast<ground *>(comp);
                        if (g && g->n && g->n->name == nodeName) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (duplicate) {
                        cout << "Error: Ground already exists at node " << nodeName << endl;
                        continue;
                    }
                    ground *g = new ground();
                    g->name = "GND";
                    g->n = getOrCreateNode(nodeName);
                    circuit.push_back(g);
                    cout << "Ground added at node " << nodeName << endl;
                    continue;
                }
                // --- Independent Sources Handling ---
                if (elementName.substr(0, 13) == "VoltageSource") {
                    if (words.size() != 5) {
                        cout << "Error: Syntax error" << endl;
                        continue;
                    }
                    double dcVal = 0.0;
                    try {
                        dcVal = stod(words[4]);
                    } catch (...) {
                        cout << "Error: Syntax error" << endl;
                        continue;
                    }
                    vsource *vs = new vsource();
                    vs->name = elementName;
                    vs->dcValue = dcVal;
                    vs->value = dcVal;
                    vs->isSinusoidal = false;
                    vs->n1 = getOrCreateNode(words[2]);
                    vs->n2 = getOrCreateNode(words[3]);
                    circuit.push_back(vs);
                    cout << "Voltage Source " << elementName << " added between "
                         << words[2] << " and " << words[3] << " with DC value " << dcVal << " V" << endl;
                    continue;
                } else if (elementName.substr(0, 13) == "CurrentSource") {
                    if (words.size() != 5) {
                        cout << "Error: Syntax error" << endl;
                        continue;
                    }
                    double iVal = 0.0;
                    try {
                        iVal = stod(words[4]);
                    } catch (...) {
                        cout << "Error: Syntax error" << endl;
                        continue;
                    }
                    currentSource *cs = new currentSource();
                    cs->name = elementName;
                    cs->value = iVal;
                    cs->n1 = getOrCreateNode(words[2]);
                    cs->n2 = getOrCreateNode(words[3]);
                    cs->dcCurrent = iVal;
                    circuit.push_back(cs);
                    cout << "Current Source " << elementName << " added between "
                         << words[2] << " and " << words[3] << " with DC current " << iVal << " A" << endl;
                    continue;
                }
                if (elementName[0] == 'V' && words[4].substr(0, 4) == "SIN(") {
                    // Combine tokens starting from index 4 until a token ending with ')' is encountered.
                    string sinExpr = "";
                    for (size_t i = 4; i < words.size(); i++) {
                        sinExpr += words[i] + " ";
                        if (!words[i].empty() && words[i].back() == ')')
                            break;
                    }
                    // Trim trailing space.
                    if (!sinExpr.empty() && sinExpr.back() == ' ')
                        sinExpr.pop_back();

                    // Check if the sinExpr ends with a ')'
                    if (sinExpr.empty() || sinExpr.back() != ')') {
                        cout << "Error: Syntax error in SIN expression" << endl;
                        continue;
                    }
                    sinExpr.pop_back();  // Remove the trailing ')'
                    size_t pos = sinExpr.find("(");
                    if (pos == string::npos) {
                        cout << "Error: Syntax error in SIN expression" << endl;
                        continue;
                    }
                    string params = sinExpr.substr(pos + 1);
                    stringstream sinSS(params);
                    double Voffset, Vampl, freq;
                    if (!(sinSS >> Voffset >> Vampl >> freq)) {
                        cout << "Error: Syntax error in SIN parameters" << endl;
                        continue;
                    }
                    vsource *vs = new vsource();
                    vs->name = elementName;
                    vs->n1 = getOrCreateNode(words[2]);
                    vs->n2 = getOrCreateNode(words[3]);
                    vs->isSinusoidal = true;
                    vs->Voffset = Voffset;
                    vs->Vamplitude = Vampl;
                    vs->Frequency = freq;
                    vs->dcValue = Voffset;  // For transient analysis, we use the DC offset.
                    vs->value = Vampl;
                    circuit.push_back(vs);
                    cout << "Sinusoidal Voltage Source " << elementName << " added between "
                         << words[2] << " and " << words[3] << ", with offset = " << Voffset
                         << " V, amplitude = " << Vampl << " V, frequency = " << freq << " Hz" << endl;
                    continue;
                }

                else if (elementName[0] == 'V' && words.size() >= 8 && words[4] == "PHASE") {
                    double amplitude, baseFreq, phase;
                    try {
                        amplitude = stod(words[5]);
                        baseFreq = stod(words[6]);
                        phase = stod(words[7]);
                    } catch (...) {
                        cout << "Error: Invalid phase voltage source parameters" << endl;
                        continue;
                    }

                    phaseVoltageSource* pvs = new phaseVoltageSource();
                    pvs->name = elementName;
                    pvs->n1 = getOrCreateNode(words[2]);
                    pvs->n2 = getOrCreateNode(words[3]);
                    pvs->amplitude = amplitude;
                    pvs->baseFrequency = baseFreq;
                    pvs->phaseOffset = phase;
                    circuit.push_back(pvs);

                    cout << "Phase Voltage Source " << elementName << " added between "
                         << words[2] << " and " << words[3] << " with amplitude " << amplitude
                         << " V, base frequency " << baseFreq << " Hz, and phase offset " << phase << " rad" << endl;
                    continue;
                }
                else if (elementName[0] == 'V' && words[4] == "AC") {
                    if (words.size() != 7) {
                        cout << "Error: Syntax error for AC source. Use: add V<name> <node+> <node-> AC <magnitude> <phase>" << endl;
                        continue;
                    }

                    double acMagnitude, acPhase;
                    try {
                        acMagnitude = stod(words[5]);
                        acPhase = stod(words[6]);
                    } catch (...) {
                        cout << "Error: Invalid AC parameters" << endl;
                        continue;
                    }

                    vsource* vs = new vsource();
                    vs->name = elementName;
                    vs->n1 = getOrCreateNode(words[2]);
                    vs->n2 = getOrCreateNode(words[3]);
                    vs->isSinusoidal = false;
                    vs->ACMagnitude = acMagnitude;
                    vs->ACPhase = acPhase;
                    circuit.push_back(vs);

                    cout << "AC Voltage Source " << elementName << " added between "
                         << words[2] << " and " << words[3] << " with magnitude " << acMagnitude
                         << " V and phase " << acPhase << " degrees" << endl;
                }

                // --- Other Circuit Elements ---
                if (words.size() != 5) {
                    cout << "Error: Syntax error" << endl;
                    continue;
                }
                string compName = words[1];
                if (compName.empty()) {
                    cout << "Error: Syntax error" << endl;
                    continue;
                }
                char type = compName[0]; // 'R', 'C', 'L', or 'D'
                bool duplicate = false;
                for (auto comp: circuit) {
                    if (comp->name == compName) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    if (type == 'R')
                        cout << "Error: Resistor " << compName << " already exists in the circuit" << endl;
                    else if (type == 'C')
                        cout << "Error: Capacitor " << compName << " already exists in the circuit" << endl;
                    else if (type == 'L')
                        cout << "Error: Inductor " << compName << " already exists in the circuit" << endl;
                    else if (type == 'D')
                        cout << "Error: Diode " << compName << " already exists in the circuit" << endl;
                    continue;
                }
                string node1Str = words[2];
                string node2Str = words[3];
                string valueStr = words[4];
                bool validValue;
                if (type == 'R') {
                    double resValue = parseResistanceValue(valueStr, validValue);
                    if (!validValue) {
                        cout << "Error: Syntax error" << endl;
                        continue;
                    }
                    if (resValue <= 0) {
                        cout << "Error: Resistance cannot be zero or negative" << endl;
                        continue;
                    }
                    resistor *r = new resistor();
                    r->name = compName;
                    r->value = resValue;
                    r->n1 = getOrCreateNode(node1Str);
                    r->n2 = getOrCreateNode(node2Str);
                    circuit.push_back(r);
                    cout << "Resistor " << compName << " added between "
                         << node1Str << " and " << node2Str << " with value " << resValue << " ohms" << endl;
                } else if (type == 'C') {
                    double capValue = parseCapacitanceValue(valueStr, validValue);
                    if (!validValue) {
                        cout << "Error: Syntax error" << endl;
                        continue;
                    }
                    if (capValue <= 0) {
                        cout << "Error: Capacitance cannot be zero or negative" << endl;
                        continue;
                    }
                    capacitor *c = new capacitor();
                    c->name = compName;
                    c->value = capValue;
                    c->n1 = getOrCreateNode(node1Str);
                    c->n2 = getOrCreateNode(node2Str);
                    circuit.push_back(c);
                    cout << "Capacitor " << compName << " added between "
                         << node1Str << " and " << node2Str << " with value " << capValue << " F" << endl;
                } else if (type == 'L') {
                    double indValue = parseInductanceValue(valueStr, validValue);
                    if (!validValue) {
                        cout << "Error: Syntax error" << endl;
                        continue;
                    }
                    if (indValue <= 0) {
                        cout << "Error: Inductance cannot be zero or negative" << endl;
                        continue;
                    }
                    inductor *l = new inductor();
                    l->name = compName;
                    l->value = indValue;
                    l->n1 = getOrCreateNode(node1Str);
                    l->n2 = getOrCreateNode(node2Str);
                    circuit.push_back(l);
                    cout << "Inductor " << compName << " added between "
                         << node1Str << " and " << node2Str << " with value " << indValue << " H" << endl;
                } else if (type == 'D') {
                    string model = valueStr;
                    if (model != "D" && model != "Z") {
                        cout << "Error: Model " << model << " not found in library" << endl;
                        continue;
                    }
                    diode *d = new diode();
                    d->name = compName;
                    d->model = model;
                    d->n1 = getOrCreateNode(node1Str);
                    d->n2 = getOrCreateNode(node2Str);
                    circuit.push_back(d);
                    cout << "Diode " << compName << " added between "
                         << node1Str << " and " << node2Str << " with model " << model << endl;
                } else {
                    cout << "Error: Element " << compName << " not found in library" << endl;
                    continue;
                }
            } else if (words[0] == "delete") {
                if (words.size() >= 2 && words[1] == "GND") {
                    if (words.size() != 3) {
                        cout << "Error: Syntax error" << endl;
                        continue;
                    }
                    string nodeName = words[2];
                    bool found = false;
                    for (auto it = circuit.begin(); it != circuit.end(); ++it) {
                        ground *g = dynamic_cast<ground *>(*it);
                        if (g && g->n && g->n->name == nodeName) {
                            delete *it;
                            circuit.erase(it);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        cout << "Node does not exist" << endl;
                        continue;
                    }
                    cout << "Ground at node " << nodeName << " deleted" << endl;
                    continue;
                }
                if (words.size() != 2) {
                    cout << "Error: Syntax error" << endl;
                    continue;
                }
                string compName = words[1];
                if (compName.empty() || (compName[0] != 'R' && compName[0] != 'C' &&
                                         compName[0] != 'L' && compName[0] != 'D' &&
                                         compName.substr(0, 1) != "V" &&
                                         compName.substr(0, 13) != "VoltageSource" &&
                                         compName.substr(0, 13) != "CurrentSource")) {
                    cout << "Error: Element " << compName << " not found in library" << endl;
                    continue;
                }
                bool found = false;
                for (auto it = circuit.begin(); it != circuit.end(); ++it) {
                    if ((*it)->name == compName) {
                        delete *it;
                        circuit.erase(it);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (compName[0] == 'R')
                        cout << "Error: Cannot delete resistor; component not found" << endl;
                    else if (compName[0] == 'C')
                        cout << "Error: Cannot delete capacitor; component not found" << endl;
                    else if (compName[0] == 'L')
                        cout << "Error: Cannot delete inductor; component not found" << endl;
                    else if (compName[0] == 'D')
                        cout << "Error: Cannot delete diode; component not found" << endl;
                    else
                        cout << "Error: Cannot delete source; component not found" << endl;
                    continue;
                }
                if (compName[0] == 'R')
                    cout << "Resistor " << compName << " deleted" << endl;
                else if (compName[0] == 'C')
                    cout << "Capacitor " << compName << " deleted" << endl;
                else if (compName[0] == 'L')
                    cout << "Inductor " << compName << " deleted" << endl;
                else if (compName[0] == 'D')
                    cout << "Diode " << compName << " deleted" << endl;
                else
                    cout << "Source " << compName << " deleted" << endl;
            } else {
                cout << "Error: Syntax error" << endl;
            }
        }

        // Clean up any remaining dynamically allocated components.
        for (auto comp: circuit) {
            delete comp;
        }
        // Clean up all dynamically allocated nodes.
        for (auto &p: nodesMap) {
            delete p.second;
        }
    }
    return 0;
}