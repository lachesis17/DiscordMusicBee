#include "musicbee_ipc.h"
#include <vector>

#define WM_USER 0x0400
constexpr size_t CSHARP_LONG_SIZE = 8; // C# long is 64-bit, not 32-bit like C++ long on Windows

namespace
{
    constexpr unsigned short MMF_ID_MASK = 0xFFFF;
    constexpr int OFFSET_SHIFT = 16;

    class ScopedHandle
    {
        HANDLE handle;

    public:
        explicit ScopedHandle(HANDLE h) : handle(h) {}
        ~ScopedHandle()
        {
            if (handle && handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }

        operator HANDLE() const { return handle; }
        bool IsValid() const { return handle && handle != INVALID_HANDLE_VALUE; }

        ScopedHandle(const ScopedHandle &) = delete;
        ScopedHandle &operator=(const ScopedHandle &) = delete;
    };

    class ScopedMapView
    {
        LPVOID view;

    public:
        explicit ScopedMapView(LPVOID v) : view(v) {}
        ~ScopedMapView()
        {
            if (view)
                UnmapViewOfFile(view);
        }

        operator LPVOID() const { return view; }
        bool IsValid() const { return view != nullptr; }

        ScopedMapView(const ScopedMapView &) = delete;
        ScopedMapView &operator=(const ScopedMapView &) = delete;
    };
}

bool MusicBeeIPC::Connect()
{
    ipcWindow = FindWindowW(nullptr, L"MusicBee IPC Interface");
    if (!ipcWindow)
        return false;

    // Test connection with Probe command (should return 1 for NoError)
    LRESULT result = SendMessageW(ipcWindow, WM_USER, static_cast<WPARAM>(MBCommand::Probe), 0);
    return result == 1;
}

void MusicBeeIPC::Disconnect()
{
    ipcWindow = nullptr;
}

bool MusicBeeIPC::IsConnected() const
{
    return ipcWindow != nullptr && IsWindow(ipcWindow);
}

MBPlayState MusicBeeIPC::GetPlayState()
{
    if (!IsConnected())
        return MBPlayState::Undefined;

    LRESULT result = SendMessageW(ipcWindow, WM_USER, static_cast<WPARAM>(MBCommand::GetPlayState), 0);
    return static_cast<MBPlayState>(result);
}

std::string MusicBeeIPC::GetFileTag(MBMetaDataType tagType)
{
    if (!IsConnected())
        return "";

    LRESULT lr = SendMessageW(ipcWindow, WM_USER, static_cast<WPARAM>(MBCommand::GetFileTag), static_cast<LPARAM>(tagType));
    if (lr == 0)
        return "";

    std::string result = ReadStringFromSharedMemory(lr);
    FreeSharedMemory(lr);
    return result;
}

std::string MusicBeeIPC::GetArtwork()
{
    if (!IsConnected())
        return "";

    // Try commands in priority order: embedded → external → URL
    std::string result = TryGetArtworkCommand(MBCommand::GetDownloadedArtwork);
    if (!result.empty())
        return result;

    result = TryGetArtworkCommand(MBCommand::GetArtwork);
    if (!result.empty())
        return result;

    return TryGetArtworkCommand(MBCommand::GetArtworkUrl);
}

std::wstring MusicBeeIPC::GetMMFName(unsigned short mmfId) const
{
    return L"mbipc_mmf_" + std::to_wstring(mmfId);
}

std::string MusicBeeIPC::TryGetArtworkCommand(MBCommand command)
{
    LRESULT lr = SendMessageW(ipcWindow, WM_USER, static_cast<WPARAM>(command), 0);
    if (lr == 0)
        return "";

    std::string result = ReadStringFromSharedMemory(lr);
    FreeSharedMemory(lr);
    return result;
}

std::string MusicBeeIPC::ReadStringFromSharedMemory(LRESULT lr)
{
    // LRESULT encoding: low 2 bytes = MMF ID, high 2 bytes = offset
    unsigned short mmfId = lr & MMF_ID_MASK;
    unsigned short offset = (lr >> OFFSET_SHIFT) & MMF_ID_MASK;

    // Open memory-mapped file
    std::wstring mmfName = GetMMFName(mmfId);
    ScopedHandle hMapFile(OpenFileMappingW(FILE_MAP_READ, FALSE, mmfName.c_str()));
    if (!hMapFile.IsValid())
        return "";

    // Map view of file
    ScopedMapView pBuf(MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0));
    if (!pBuf.IsValid())
        return "";

    // Data format: [CSHARP_LONG_SIZE capacity] [int32 byteCount] [UTF-16 LE string]
    char *dataPtr = static_cast<char *>(static_cast<LPVOID>(pBuf)) + offset + CSHARP_LONG_SIZE;
    int byteCount = *reinterpret_cast<int *>(dataPtr);
    dataPtr += sizeof(int);

    // Read UTF-16 LE string
    std::wstring wstr(reinterpret_cast<wchar_t *>(dataPtr), byteCount / sizeof(wchar_t));

    // Convert to UTF-8
    if (wstr.empty())
        return "";

    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::vector<char> utf8Buffer(utf8Length);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, utf8Buffer.data(), utf8Length, nullptr, nullptr);

    return std::string(utf8Buffer.data());
}

void MusicBeeIPC::FreeSharedMemory(LRESULT lr)
{
    if (IsConnected() && lr != 0)
    {
        SendMessageW(ipcWindow, WM_USER, static_cast<WPARAM>(MBCommand::FreeLRESULT), lr);
    }
}
