#include <Windows.h>
#include <iostream>
#include <filesystem>

#include "Utils.hpp"
#include "Loader.hpp"

STargetAssembly TargetAssembly = {};

auto InjectAssembly(STargetAssembly* Asm) -> bool
{
    const auto Path = std::filesystem::current_path().append("MonoBleedingEdge/EmbedRuntime/mono-2.0-bdwgc.dll");

    if (!std::filesystem::exists(Path))
    {
        std::cout << "Path does not exist: " << Path.string() << std::endl;
        return false;
    }

    try
    {
        auto Loader = EQU8AssemblyLoader::Create(Path.string(), std::cout);

        return
            Loader &&
            Loader->Load(Asm->ModulePath, Asm->Method);
    }
    catch (const LoaderException& Ex)
    {
        std::cout << "Error encountered: " << Ex.What() << std::endl;
        return false;
    }
}

__declspec(dllexport) auto CALLBACK InjAsm(int Code, WPARAM WParam, LPARAM LParam) -> LRESULT
{
    MSG* Msg = reinterpret_cast<MSG*>(LParam);

    switch (Msg->message)
    {
        case RequestLoad:
        {
            std::cout << "Exec RequestLoad" << std::endl;

            if (ScopedHandle<&OpenFileMappingA, &CloseHandle> Mapping{ FILE_MAP_READ, FALSE, "TargetAssembly" })
            {
                if (ScopedHandle<&MapViewOfFile, &UnmapViewOfFile> MappingMem{ Mapping, FILE_MAP_READ, 0, 0, sizeof(TargetAssembly) })
                {
                    if (InjectAssembly(reinterpret_cast<STargetAssembly*>(*MappingMem)))
                    {
                        std::cout << "Assembly laoded" << std::endl;
                        return 1;
                    }

                    std::cout << "Failed to inject" << std::endl;
                    return 0;
                }

                std::cout << "Unable to map view of file" << std::endl;
                return 0;
            }
            
            std::cout << "Unable to open file mapping" << std::endl;
            return 0;
        }

        case RequestUnhook:
        {
            std::cout << "Received unhook message" << std::endl;

            if (Msg->lParam)
            {
                auto Hook = reinterpret_cast<HHOOK>(Msg->lParam);

                if (UnhookWindowsHookEx(Hook))
                {
                    return 1;
                }

                std::cout << "Unhook error: " << GetLastError() << std::endl;
                return 0;
            }
        }
    }

	return CallNextHookEx(0, Code, WParam, LParam);
}