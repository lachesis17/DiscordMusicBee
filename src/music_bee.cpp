#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QFont>
#include <QVBoxLayout>
#include <QIcon>
#include <QTimer>
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QEvent>
#include <cstring>
#include <string>
#include <windows.h>
#include <comdef.h>
#include "discord_rpc.h"

// app id from https://discord.com/developers/applications/
std::string DISCORD_APP_ID = std::getenv("DISCORD_APP_ID");
const char *DISCORD_DETAILS = "";
const char *DISCORD_STATE = "";
const char *DISCORD_LARGE_IMAGE_KEY = "something";       // https://discord.com/developers/applications/DISCORD_APP_ID/rich-presence/assets/something.png
const char *DISCORD_LARGE_IMAGE_TEXT = "something-else"; // https://discord.com/developers/applications/DISCORD_APP_ID/rich-presence/assets/something-else.png

struct MusicInfo
{
    std::string artist;
    std::string title;
    std::string album;
    bool isPlaying = false;
};

// Function to get current MusicBee track info
MusicInfo getMusicBeeInfo()
{
    MusicInfo info;
    HWND musicBeeWindow = NULL;

    // Try to find any window with "MusicBee" in title
    musicBeeWindow = FindWindow(NULL, L"MusicBee");
    if (!musicBeeWindow)
    {
        // Try common MusicBee window class names
        const wchar_t *classNames[] = {
            L"WindowsForms10.Window.8.app.0.2bf8098_r11_ad1",
            L"MusicBee",
            L"WindowsForms10.Window.8.app.0.141b42a_r6_ad1",
            NULL};

        for (int i = 0; classNames[i] != NULL; i++)
        {
            musicBeeWindow = FindWindow(classNames[i], NULL);
            if (musicBeeWindow)
                break;
        }
    }

    // Search all windows for MusicBee in title
    if (!musicBeeWindow)
    {
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
                    {
                        wchar_t windowTitle[512];
                        GetWindowText(hwnd, windowTitle, 512);
                        std::wstring title(windowTitle);

                        if (title.find(L"MusicBee") != std::wstring::npos)
                        {
                            *(HWND *)lParam = hwnd;
                            return FALSE;
                        }
                        return TRUE; },
                    (LPARAM)&musicBeeWindow);
    }

    if (!musicBeeWindow)
    {
        return info; // MusicBee not found
    }

    // Get window title
    wchar_t windowTitle[512];
    GetWindowText(musicBeeWindow, windowTitle, 512);
    std::wstring title(windowTitle);

    if (title.find(L" - ") != std::wstring::npos)
    {
        // "Artist - Title - MusicBee"
        size_t firstDash = title.find(L" - ");
        size_t lastDash = title.rfind(L" - MusicBee");

        if (firstDash != std::wstring::npos && lastDash != std::wstring::npos && firstDash < lastDash)
        {
            std::wstring artist = title.substr(0, firstDash);
            std::wstring song = title.substr(firstDash + 3, lastDash - firstDash - 3);

            info.artist = std::string(artist.begin(), artist.end());
            info.title = std::string(song.begin(), song.end());
            info.isPlaying = true;
        }
        // "Title - Artist - MusicBee" (reversed)
        else if (lastDash != std::wstring::npos)
        {
            std::wstring trackInfo = title.substr(0, lastDash);
            size_t dashPos = trackInfo.find(L" - ");
            if (dashPos != std::wstring::npos)
            {
                std::wstring song = trackInfo.substr(0, dashPos);
                std::wstring artist = trackInfo.substr(dashPos + 3);

                info.title = std::string(song.begin(), song.end());
                info.artist = std::string(artist.begin(), artist.end());
                info.isPlaying = true;
            }
        }
    }
    // (not playing)
    else if (title == L"MusicBee")
    {
        info.isPlaying = false;
    }

    return info;
}

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow() : QMainWindow(), discordTimer(new QTimer(this))
    {
        setupUI();
        setupTrayIcon();
        pollDiscord();
    }

private:
    QTimer *discordTimer;
    QLabel *songLabel;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;

    void setupUI()
    {
        setWindowTitle("DiscordMusicBee");
        setWindowIcon(QIcon(":/icon.ico"));

        auto *centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        songLabel = new QLabel("DiscordMusicBee 🎧\nwaiting for song...", this);
        QFont font = songLabel->font();
        font.setPointSize(24);
        songLabel->setFont(font);
        songLabel->setAlignment(Qt::AlignCenter);

        auto *layout = new QVBoxLayout(centralWidget);
        layout->addWidget(songLabel);

        resize(600, 200);
    }

    void setupTrayIcon()
    {
        trayIcon = new QSystemTrayIcon(this);
        trayIcon->setIcon(QIcon(":/icon.ico"));
        trayIcon->setToolTip("DiscordMusicBee - MusicBee Discord Integration");
        trayMenu = new QMenu(this);

        QAction *showAction = new QAction("Show", this);
        connect(showAction, &QAction::triggered, [this]()
                {
            showNormal();
            raise();
            activateWindow(); });

        QAction *quitAction = new QAction("Quit", this);
        connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

        trayMenu->addAction(showAction);
        trayMenu->addSeparator();
        trayMenu->addAction(quitAction);

        trayIcon->setContextMenu(trayMenu);

        connect(trayIcon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason)
                {
            if (reason == QSystemTrayIcon::DoubleClick) {
                showNormal();
                raise();
                activateWindow();
            } });

        trayIcon->show();
    }

    void closeEvent(QCloseEvent *event) override
    {
        if (trayIcon->isVisible())
        {
            hide();
            event->ignore();
        }
    }

    void changeEvent(QEvent *event) override
    {
        if (event->type() == QEvent::WindowStateChange)
        {
            if (isMinimized())
            {
                hide();
                event->ignore();
            }
        }
        QMainWindow::changeEvent(event);
    }

    void pollDiscord()
    {
        connect(discordTimer, &QTimer::timeout, [this]()
                {
            updateDiscordPresence();
            Discord_RunCallbacks(); });
        discordTimer->start(500); // poll this so discord prescence updates
    }

    void updateDiscordPresence()
    {
        MusicInfo music = getMusicBeeInfo();

        DiscordRichPresence discordPresence;
        memset(&discordPresence, 0, sizeof(discordPresence));

        if (music.isPlaying && !music.title.empty())
        {
            // Create new strings each time to avoid static issues
            std::string details = "♪ " + music.title;
            std::string state = "by " + music.artist;

            discordPresence.details = details.c_str();
            discordPresence.state = state.c_str();
            discordPresence.largeImageKey = "music";
            discordPresence.largeImageText = "Listening to music";

            QString labelText = QString::fromStdString("🎧 " + music.title + "\n🎶 " + music.artist);
            songLabel->setText(labelText);
        }
        else
        {
            discordPresence.details = DISCORD_DETAILS;
            discordPresence.state = DISCORD_STATE;
            discordPresence.largeImageKey = DISCORD_LARGE_IMAGE_KEY;
            discordPresence.largeImageText = DISCORD_LARGE_IMAGE_TEXT;
            songLabel->setText("DiscordMusicBee 🎧\nwaiting for song...");
        }

        Discord_UpdatePresence(&discordPresence);
    }
};

void setupDiscord()
{
    DiscordEventHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    Discord_Initialize(DISCORD_APP_ID.c_str(), &handlers, 1, NULL);

    // Initial presence update will happen in pollDiscord timer
}

int main(int argc, char *argv[])
{
    setupDiscord();

    QApplication app(argc, argv);
    MainWindow window;
    window.show();

    int result = app.exec();
    Discord_Shutdown();
    return result;
}
