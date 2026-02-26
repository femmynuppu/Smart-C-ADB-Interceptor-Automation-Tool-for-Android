#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>
#include <regex>
#include <windows.h>
#include <atomic>
#include <conio.h>

using namespace std;

// ==========================================
// 🎀 APPLICATION CONFIGURATION
// ==========================================
string AppPackage = "com.proximabeta.mf.uamo";
string AppActivity = "com.epicgames.ue4.GameActivity";
string UserId = "0";

// RADAR STATUS (Atomic for thread safety)
atomic<bool> isWaitingForKey(false);
atomic<bool> isWaitingForOption(false);
atomic<bool> triggerRelog(false);
atomic<bool> triggerUnbindFailed(false); // FITUR BARU: Deteksi Unbind

// SAVED DATA
string savedScriptPath = "";
string savedKey = "";
string savedOption = "";

// Get current directory path for config
string GetConfigPath() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    string path(buffer);
    return path.substr(0, path.find_last_of("\\/")) + "\\femmy_data.txt";
}

void LoadConfig() {
    ifstream infile(GetConfigPath());
    if (infile.good()) {
        string line;
        getline(infile, line);

        // FIX WARNING C4267: Tambahkan (int)
        int firstPipe = (int)line.find('|');
        int secondPipe = (int)line.find('|', firstPipe + 1);

        if (firstPipe != string::npos) {
            savedScriptPath = line.substr(0, firstPipe);
            if (secondPipe != string::npos) {
                savedKey = line.substr(firstPipe + 1, secondPipe - firstPipe - 1);
                savedOption = line.substr(secondPipe + 1);
            }
            else {
                savedKey = line.substr(firstPipe + 1);
            }
        }
        else {
            savedScriptPath = line;
        }
    }
}

void SaveConfig() {
    ofstream outfile(GetConfigPath());
    outfile << savedScriptPath << "|" << savedKey << "|" << savedOption;
}

// Function to execute ADB commands and read output
string RunAdb(string args) {
    string cmd = "adb " + args;
    char buffer[128];
    string result = "";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return "ERROR";
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    }
    catch (...) {
        _pclose(pipe);
        throw;
    }
    _pclose(pipe);
    return result;
}

// Background Radar Thread
void RadarThread(HANDLE hRead, HANDLE hProcess) {
    char buffer[1024];
    DWORD bytesRead;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        string output(buffer);
        cout << output;

        // Convert to lowercase for checking
        string lower = output;
        for (auto& c : lower) c = tolower(c);

        // FITUR BARU: Deteksi "Unbind Failed"
        if (lower.find("解绑失败") != string::npos) {
            triggerUnbindFailed = true;
            TerminateProcess(hProcess, 0); // Bunuh script segera
            break;
        }

        // AUTO RELOG TRIGGER (Network errors, timeouts)
        if (lower.find("网络错误") != string::npos || lower.find("timeout") != string::npos ||
            lower.find("超时") != string::npos || lower.find("连接失败") != string::npos) {
            triggerRelog = true;
            TerminateProcess(hProcess, 0); // Bunuh script untuk relog
            break;
        }

        if (lower.find("卡密") != string::npos || lower.find("card") != string::npos || lower.find("key") != string::npos) {
            isWaitingForKey = true;
            isWaitingForOption = false;
        }
        else if (lower.find("请选择") != string::npos || lower.find("number") != string::npos || lower.find("选择") != string::npos) {
            isWaitingForKey = false;
            isWaitingForOption = true;
        }
    }
}

void RunInjector() {
    isWaitingForKey = false;
    isWaitingForOption = false;
    bool autoKeySent = false;
    bool autoOptionSent = false;

    string scriptFileName = savedScriptPath.substr(savedScriptPath.find_last_of("\\/") + 1);
    string targetPath = "/data/local/tmp/" + scriptFileName;

    system("cls");
    cout << "========================================================" << endl;
    cout << " (◕‿◕✿) FEMMY FLASHER - C++ FAST AUTO EDITION (★^O^★)" << endl;
    cout << "========================================================" << endl;
    cout << "[INFO] Target Script : " << scriptFileName << endl;

    cout << "[~] Searching for your device, Senpai..." << endl;
    RunAdb("wait-for-device");

    // 2. Auto Detect Profile
    cout << "[~] Tracking game installation path automatically..." << endl;
    string users = RunAdb("shell pm list users");
    smatch m;
    regex e("\\{(\\d+):");
    bool found = false;
    string searchStr = users;
    while (regex_search(searchStr, m, e)) {
        string foundId = m[1];
        if (stoi(foundId) >= 10) {
            string pathOut = RunAdb("shell pm path --user " + foundId + " " + AppPackage);
            if (pathOut.find("package:") != string::npos) {
                UserId = foundId;
                found = true;
                break;
            }
        }
        searchStr = m.suffix().str();
    }
    if (!found) cout << "[!] Game found in Main Space (User 0)." << endl;
    else cout << "[~] BINGO! Game found in Work Profile: User " << UserId << endl;

    // 3. Cleanup & Push
    cout << "[~] Force stopping game & clearing cache..." << endl;
    RunAdb("shell am force-stop --user " + UserId + " " + AppPackage);
    RunAdb("shell su -c \"rm -rf /data/user/" + UserId + "/" + AppPackage + "/cache/*\"");

    cout << "[~] Cleaning up old '" << scriptFileName << "' tasks from memory..." << endl;
    RunAdb("shell su -c \"pkill -f '" + scriptFileName + "'\"");

    cout << "[~] Injecting new script to Android..." << endl;
    RunAdb("push \"" + savedScriptPath + "\" \"" + targetPath + "\"");
    RunAdb("shell chmod 755 \"" + targetPath + "\"");

    cout << "\n✿✿✿✿✿✿✿✿✿ STARTING INJECTION ✿✿✿✿✿✿✿✿✿" << endl;

    // 4. Setup Pipes & Process
    HANDLE hStdInRead, hStdInWrite;
    HANDLE hStdOutRead, hStdOutWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0);
    CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdInRead;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdOutWrite;

    PROCESS_INFORMATION pi = { 0 };
    string adbCmd = "adb shell su -c \"sh '" + targetPath + "'\"";

    if (CreateProcessA(NULL, (LPSTR)adbCmd.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        thread radar(RadarThread, hStdOutRead, pi.hProcess);
        radar.detach();

        // 5. High-Speed Interceptor
        while (WaitForSingleObject(pi.hProcess, 50) == WAIT_TIMEOUT) {
            // Berhenti jika mendeteksi error jaringan atau unbind
            if (triggerRelog || triggerUnbindFailed) break;

            // --- AUTO FILL KEY ---
            if (isWaitingForKey && !autoKeySent && !savedKey.empty()) {
                isWaitingForKey = false;
                autoKeySent = true;
                cout << "\n[C++ SYSTEM] >> Waking up Arena Breakout to background..." << endl;
                RunAdb("shell am start --user " + UserId + " -n " + AppPackage + "/" + AppActivity);

                cout << "[C++ SYSTEM] >> Auto-injecting saved Key..." << endl;
                string inputStr = savedKey + "\n";
                DWORD bytesWritten;

                // FIX WARNING C4267: Tambahkan (DWORD)
                WriteFile(hStdInWrite, inputStr.c_str(), (DWORD)inputStr.length(), &bytesWritten, NULL);
                continue;
            }

            // --- AUTO FILL OPTION ---
            if (isWaitingForOption && !autoOptionSent && !savedOption.empty()) {
                isWaitingForOption = false;
                autoOptionSent = true;
                cout << "\n[C++ SYSTEM] >> Waking up Arena Breakout to background..." << endl;
                RunAdb("shell am start --user " + UserId + " -n " + AppPackage + "/" + AppActivity);

                cout << "[C++ SYSTEM] >> Auto-selecting saved Menu Option (" << savedOption << ")..." << endl;
                string inputStr = savedOption + "\n";
                DWORD bytesWritten;

                // FIX WARNING C4267: Tambahkan (DWORD)
                WriteFile(hStdInWrite, inputStr.c_str(), (DWORD)inputStr.length(), &bytesWritten, NULL);
                continue;
            }

            // --- MANUAL INPUT ---
            if (_kbhit()) {
                string input;
                getline(cin, input);
                if (!input.empty()) {
                    bool wasKey = isWaitingForKey;
                    bool wasOpt = isWaitingForOption;
                    isWaitingForKey = false;
                    isWaitingForOption = false;

                    // GAME FIRST LOGIC
                    if (wasKey || wasOpt || isdigit(input[0])) {
                        cout << "\n[C++ SYSTEM] >> Preparing game in background..." << endl;
                        RunAdb("shell am start --user " + UserId + " -n " + AppPackage + "/" + AppActivity);
                    }

                    string inputStr = input + "\n";
                    DWORD bytesWritten;

                    // FIX WARNING C4267: Tambahkan (DWORD)
                    WriteFile(hStdInWrite, inputStr.c_str(), (DWORD)inputStr.length(), &bytesWritten, NULL);

                    if (wasKey) {
                        savedKey = input;
                        SaveConfig();
                        autoKeySent = true;
                        cout << "[C++ SYSTEM] >> Key saved & Injected!" << endl;
                    }
                    else if (wasOpt || isdigit(input[0])) {
                        savedOption = input;
                        SaveConfig();
                        autoOptionSent = true;
                        cout << "[C++ SYSTEM] >> Option saved & Executed!" << endl;
                    }
                }
            }
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // Jangan tampilkan ini kalau kita mau relog/reset key
    if (!triggerRelog && !triggerUnbindFailed) {
        cout << "\n✿✿✿✿✿✿✿✿✿ INJECTION COMPLETE ✿✿✿✿✿✿✿✿✿" << endl;
        cout << "[~] Sweeping injection traces from Android..." << endl;
        RunAdb("shell rm -f \"" + targetPath + "\"");
    }
}

int main(int argc, char* argv[]) {
    // UTF-8 Console encoding to display Chinese text properly
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    LoadConfig();

    if (argc > 1) {
        savedScriptPath = argv[1];
        string cleanArg = savedScriptPath;
        if (!cleanArg.empty() && cleanArg.front() == '"') cleanArg.erase(0, 1);
        if (!cleanArg.empty() && cleanArg.back() == '"') cleanArg.pop_back();
        savedScriptPath = cleanArg;

        savedKey = "";
        savedOption = "";
        SaveConfig();
    }
    else if (!savedScriptPath.empty()) {
        ifstream f(savedScriptPath.c_str());
        if (f.good()) {
            cout << "========================================================" << endl;
            cout << " (◕‿◕✿) FEMMY REMEMBERS YOU, SENPAI!" << endl;
            cout << " Script : " << savedScriptPath << endl;
            if (!savedKey.empty()) cout << " Key    : " << savedKey << endl;
            if (!savedOption.empty()) cout << " Menu   : " << savedOption << endl;
            cout << "========================================================\n" << endl;

            cout << "Use this configuration? [ENTER/Y = Yes, N = Setup New]: ";
            string choice;
            getline(cin, choice);
            if (choice == "N" || choice == "n") {
                savedScriptPath = "";
                savedKey = "";
                savedOption = "";
                remove(GetConfigPath().c_str());
            }
        }
        else {
            savedScriptPath = "";
        }
    }

    if (savedScriptPath.empty()) {
        cout << "\n[INFO] Waiting for a new script..." << endl;
        cout << "Please CLOSE this window, then Drag & Drop your NEW .sh file onto the .exe icon!" << endl;
        system("pause");
        return 0;
    }

    bool loop = true;
    while (loop) {
        triggerRelog = false;
        triggerUnbindFailed = false;
        RunInjector();

        // LOGIKA BARU: Jika terjadi "解绑失败" (Unbind Failed)
        if (triggerUnbindFailed) {
            cout << "\n[!] Unbind Failed (解绑失败) Detected!" << endl;
            cout << "[!] Old Key is no longer valid. Deleting saved key..." << endl;
            savedKey = ""; // Mengosongkan Key yang salah
            SaveConfig();  // Simpan konfigurasi baru yang kosong

            cout << "[!] Restarting in 3 seconds to let you enter a NEW KEY..." << endl;
            this_thread::sleep_for(chrono::seconds(3));
            continue; // Ulangi dari awal
        }

        if (triggerRelog) {
            cout << "\n[!] Connection or Timeout error detected!" << endl;
            cout << "[!] Femmy is Auto-Relogging in 3 seconds..." << endl;
            this_thread::sleep_for(chrono::seconds(3));
            continue;
        }

        cout << "\n========================================================" << endl;
        cout << " (づ｡◕‿‿◕｡)づ ALL DONE, SENPAI! DEVICE IS CLEAN!" << endl;
        cout << "========================================================" << endl;
        cout << "Do you want to RERUN (Play Again)? [ENTER/Y = Yes, N = Exit]: ";
        string r;
        getline(cin, r);
        if (r == "n" || r == "N") loop = false;
    }

    return 0;
}
