#define _CRT_SECURE_NO_DEPRECATE
#include <iostream>
#include <fstream> 
#include <windows.h>
#include <string>
#include <vector>
#include <map>

using namespace std;

bool FileExists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);
  return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

int main()
{
    std::map<int, std::vector<std::string>> modDLLs;

    //Cube world is obviously required
    if (!FileExists("Cube.exe")) {
        printf("Cube World not found.\n");
        Sleep(1000);
        return 1;
    }

    FILE* file = fopen("Cube.exe", "rb");
    fseek(file, 0, SEEK_END);
    int fileSize = ftell(file);
    fclose(file);

    const int CUBE_SIZE = 3885568;
    if (fileSize != CUBE_SIZE) {
        printf("Cube World was found, but it is not version 0.1.1. Please update your game.\n");
        printf("Press enter to exit.\n");
        cin.ignore();
        return 1;
    }



    string CALLBACKMANAGER_PATH = "CallbackManager.dll";
    //The callback manager is required.
    if (!FileExists(CALLBACKMANAGER_PATH.c_str())) {
        printf("Callback manager not found.\n");
        Sleep(1000);
        return 1;
    }

    std::map<string, int> priorityListData;
    modDLLs.insert({ -1, {CALLBACKMANAGER_PATH} });
    const char MOD_PATH[] = "Mods\\*.dll";
    const char MOD_PRIORITY_LIST_PATH[] = "ModPriorityList.txt";
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    CreateDirectory("Mods", NULL);

    //Create game in suspended state
    printf("Starting Cube.exe...\n\n");
    if (!CreateProcess(NULL,
                  (char*)"Cube.exe",
                  NULL,
                  NULL,
                  true,
                  CREATE_SUSPENDED,
                  NULL,
                  NULL,
                  &si,
                  &pi))
    {
        printf("Failed to create process: %lu", GetLastError());
        return 1;
    }
    else {
        printf("Cube.exe was successfully started.\n\n");
    }

    boolean priorityListFileExist = FileExists(MOD_PRIORITY_LIST_PATH);
    // Load priorityFile content into priorityListData
    if (priorityListFileExist) {
        ifstream file;
        string line;
        file.open(MOD_PRIORITY_LIST_PATH);
        while (getline(file, line)) {
            // line exemple -> "mod.dll"=0
            if (line.substr(0, 2) == string("##")) continue; // Comment
            int endOfName = line.find_last_of("\"");
            string modName = line.substr(1, endOfName-1);
            int modPriority = stoi(line.substr(endOfName+2));
            priorityListData.insert({ modName, modPriority});// modPriority });
        }
        file.close();
    } else {
        printf("ModPriorityList file not found.\n");
        printf("Default priority values will be used.\n\n");
    }

    //Find mods
    HANDLE hFind;
    WIN32_FIND_DATA data;

    hFind = FindFirstFile(MOD_PATH, &data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            string modFilePath = std::string("Mods\\") + data.cFileName;
            int modPriority = 1;
            if (priorityListData.find(modFilePath) == priorityListData.end()) {
                priorityListData.insert({ modFilePath, modPriority });
                printf("file %s not found\n", modFilePath.c_str());
            }
            modPriority = priorityListData.at(modFilePath);
            if (modDLLs.find(modPriority) == modDLLs.end()) {
                modDLLs.insert({ modPriority, {} });
            }
            modDLLs.at(modPriority).push_back(modFilePath);
        } while (FindNextFile(hFind, &data));
        FindClose(hFind);
    }

    // Save ModPriorityList file
    ofstream priorityListFile;
    priorityListFile.open(MOD_PRIORITY_LIST_PATH);
    priorityListFile << "## This file contain the path of all the mods detected by the ModLoader\n";
    priorityListFile << "## Each mod is ordered using a priority value. Mods with value of 0 load before mods with value of 1\n";
    priorityListFile << "## 1 is the default value used by mods.\n";
    priorityListFile << "## If a mod need to access another mod you may want it to be higher than the one it is accessing\n";

    //Inject DLLs ordered by priority
    vector <HANDLE> threads;
    for (auto const& modVectors : modDLLs){
        int I_DLLPriority = modVectors.first;
        vector<string> V_DLLVector = modVectors.second;
        for (string S_DLLName : V_DLLVector) {
            printf("Loading mod with priority %d : '%s'\n", I_DLLPriority, S_DLLName.c_str());

            // Save the mod in ModPriorityList file
            if (S_DLLName != CALLBACKMANAGER_PATH) {
                priorityListFile << "\"" << S_DLLName << "\"=" << to_string(I_DLLPriority) << "\n";
            }

            LPVOID load_library = (LPVOID)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "LoadLibraryA");
            LPVOID remote_string = (LPVOID)VirtualAllocEx(pi.hProcess, NULL, strlen(S_DLLName.c_str()) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            WriteProcessMemory(pi.hProcess, remote_string, S_DLLName.c_str(), strlen(S_DLLName.c_str()) + 1, NULL);

            HANDLE thread = CreateRemoteThread(pi.hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)load_library, remote_string, CREATE_SUSPENDED, NULL);
            threads.push_back(thread);
            ResumeThread(thread);
            Sleep(10); // wait a bit to ensure mod is initialised (hacky i know)
        }
    }
    priorityListFile.close();

    ResumeThread(pi.hThread);
    CloseHandle(pi.hProcess);

    printf("\nAll available mods have been loaded.\n");
    Sleep(1500);
//
//
//    WaitForSingleObject(pi.hThread, INFINITE);
//    for (HANDLE thread : threads){
//        CloseHandle(thread);
//    }
    return 0;
}
