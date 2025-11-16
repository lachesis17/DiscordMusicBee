#pragma once

#include <string>
#include <windows.h>

// IPC Commands
enum class MBCommand : WPARAM
{
    GetPlayState = 109,
    GetFileTag = 142,
    GetArtwork = 145,
    GetArtworkUrl = 146,
    GetDownloadedArtwork = 147,
    FreeLRESULT = 900,
    Probe = 999
};

// PlayState
enum class MBPlayState : int
{
    Undefined = 0,
    Loading = 1,
    Playing = 3,
    Paused = 6,
    Stopped = 7
};

// MetaDataType
enum class MBMetaDataType : LPARAM
{
    TrackTitle = 65,
    Album = 30,
    Artist = 32
};

class MusicBeeIPC
{
public:
    MusicBeeIPC() : ipcWindow(nullptr) {}
    ~MusicBeeIPC() = default;

    bool Connect();
    void Disconnect();
    bool IsConnected() const;

    MBPlayState GetPlayState();
    std::string GetFileTag(MBMetaDataType tagType);
    std::string GetArtwork();

private:
    HWND ipcWindow;

    std::wstring GetMMFName(unsigned short mmfId) const;
    std::string ReadStringFromSharedMemory(LRESULT lr);
    void FreeSharedMemory(LRESULT lr);
    std::string TryGetArtworkCommand(MBCommand command);
};
