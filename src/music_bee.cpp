#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QTimer>
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QEvent>
#include <QPixmap>
#include <QFile>
#include <QByteArray>
#include <QPainter>
#include <QPainterPath>
#include <QBitmap>
#include <cstring>
#include <string>
#include <windows.h>
#include <comdef.h>
#include "discord_rpc.h"
#include "musicbee_ipc.h"

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
    std::string artworkPath;
    bool isPlaying = false;
};

static MusicBeeIPC ipcClient;

MusicInfo getMusicBeeInfo()
{
    MusicInfo info;

    if (!ipcClient.IsConnected())
    {
        if (!ipcClient.Connect())
        {
            return info;
        }
    }

    // play state
    MBPlayState playState = ipcClient.GetPlayState();
    info.isPlaying = (playState == MBPlayState::Playing);

    if (info.isPlaying)
    {
        // track metadata
        info.title = ipcClient.GetFileTag(MBMetaDataType::TrackTitle);
        info.artist = ipcClient.GetFileTag(MBMetaDataType::Artist);
        info.album = ipcClient.GetFileTag(MBMetaDataType::Album);
        info.artworkPath = ipcClient.GetArtwork();
    }

    return info;
}

QPixmap createRoundedPixmap(const QPixmap &source, int radius)
{
    if (source.isNull())
        return QPixmap();

    QPixmap rounded(source.size());
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(rounded.rect(), radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, source);

    return rounded;
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
    QLabel *artworkLabel;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    std::string currentDetails;
    std::string currentState;

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

        artworkLabel = new QLabel(this);
        artworkLabel->setFixedSize(150, 150);
        artworkLabel->setAlignment(Qt::AlignCenter);
        artworkLabel->setScaledContents(false);
        artworkLabel->setStyleSheet("QLabel { border-radius: 10px; }");

        auto *layout = new QVBoxLayout(centralWidget);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(5);
        layout->addWidget(songLabel);
        layout->addWidget(artworkLabel);
        layout->setAlignment(artworkLabel, Qt::AlignCenter);

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
            updatePlayingState(discordPresence, music);
        }
        else
        {
            updateIdleState(discordPresence);
        }

        Discord_UpdatePresence(&discordPresence);
    }

    void updatePlayingState(DiscordRichPresence &presence, const MusicInfo &music)
    {
        // update discord presence
        currentDetails = "♪ " + music.title;
        currentState = "by " + music.artist;
        presence.details = currentDetails.c_str();
        presence.state = currentState.c_str();
        presence.largeImageKey = "music";
        presence.largeImageText = "Listening to music";

        // update UI
        QString labelText = QString("🎧 %1 - %2")
                                .arg(QString::fromStdString(music.title))
                                .arg(QString::fromStdString(music.artist));
        songLabel->setText(labelText);
        updateArtwork(music.artworkPath);
    }

    void updateIdleState(DiscordRichPresence &presence)
    {
        presence.details = DISCORD_DETAILS;
        presence.state = DISCORD_STATE;
        presence.largeImageKey = DISCORD_LARGE_IMAGE_KEY;
        presence.largeImageText = DISCORD_LARGE_IMAGE_TEXT;

        songLabel->setText("DiscordMusicBee 🎧\nwaiting for song...");
        artworkLabel->clear();
    }

    void updateArtwork(const std::string &artworkPath)
    {
        if (artworkPath.empty())
        {
            artworkLabel->clear();
            return;
        }

        QString artworkData = QString::fromStdString(artworkPath);

        // is base64
        if (artworkData.length() <= 150 || !artworkData.contains("/"))
        {
            artworkLabel->clear();
            return;
        }

        QByteArray imageData = QByteArray::fromBase64(artworkData.toUtf8());
        QPixmap artwork;

        if (artwork.loadFromData(imageData))
        {
            QPixmap scaled = artwork.scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPixmap rounded = createRoundedPixmap(scaled, 10);
            artworkLabel->setPixmap(rounded);
        }
        else
        {
            artworkLabel->clear();
        }
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
