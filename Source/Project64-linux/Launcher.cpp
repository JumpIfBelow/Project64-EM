#include "Launcher.h"

#include "LinuxConfig.h"
#include <Project64-input-sdl/InputConfig.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QEventLoop>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <SDL.h>
#include <SDL_syswm.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace
{
QString Text(const std::string & value)
{
    return QString::fromUtf8(value.c_str());
}

std::string Text(const QString & value)
{
    return value.toUtf8().constData();
}

QWidget * Scrollable(QWidget * contents)
{
    auto * scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(contents);
    return scroll;
}

QComboBox * StringCombo(const std::vector<std::pair<const char *, int>> & values, int selected)
{
    auto * combo = new QComboBox;
    for (const auto & value : values)
    {
        combo->addItem(value.first, value.second);
    }
    const int index = combo->findData(selected);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    return combo;
}

struct BindingOption
{
    const char * name;
    pj64::input::GamepadBinding binding;
};

const std::vector<BindingOption> & BindingOptions()
{
    using Binding = pj64::input::GamepadBinding;
    static const std::vector<BindingOption> values = {
        {"None", {}},
        {"A", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_A}},
        {"B", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_B}},
        {"X", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_X}},
        {"Y", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_Y}},
        {"Back", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_BACK}},
        {"Guide", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_GUIDE}},
        {"Start", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_START}},
        {"Left stick click", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_LEFTSTICK}},
        {"Right stick click", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_RIGHTSTICK}},
        {"Left shoulder", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_LEFTSHOULDER}},
        {"Right shoulder", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}},
        {"D-pad up", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_UP}},
        {"D-pad down", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_DOWN}},
        {"D-pad left", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_LEFT}},
        {"D-pad right", {Binding::Kind::Button, SDL_CONTROLLER_BUTTON_DPAD_RIGHT}},
        {"Left X +", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_LEFTX}},
        {"Left X -", {Binding::Kind::NegativeAxis, SDL_CONTROLLER_AXIS_LEFTX}},
        {"Left Y +", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_LEFTY}},
        {"Left Y -", {Binding::Kind::NegativeAxis, SDL_CONTROLLER_AXIS_LEFTY}},
        {"Right X +", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_RIGHTX}},
        {"Right X -", {Binding::Kind::NegativeAxis, SDL_CONTROLLER_AXIS_RIGHTX}},
        {"Right Y +", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_RIGHTY}},
        {"Right Y -", {Binding::Kind::NegativeAxis, SDL_CONTROLLER_AXIS_RIGHTY}},
        {"Left trigger", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_TRIGGERLEFT}},
        {"Right trigger", {Binding::Kind::PositiveAxis, SDL_CONTROLLER_AXIS_TRIGGERRIGHT}},
    };
    return values;
}

bool SameBinding(const pj64::input::GamepadBinding & left, const pj64::input::GamepadBinding & right)
{
    return left.kind == right.kind && left.value == right.value;
}

class SettingsDialog final : public QDialog
{
public:
    SettingsDialog(
        LinuxConfig & config,
        pj64::input::InputConfig & input,
        const std::string & frontendConfigPath,
        const std::string & inputConfigPath) :
        m_Config(config),
        m_Input(input),
        m_FrontendConfigPath(frontendConfigPath),
        m_InputConfigPath(inputConfigPath)
    {
        setWindowTitle("Project64-EM Settings");
        setMinimumSize(760, 620);
        resize(920, 700);

        auto * layout = new QVBoxLayout(this);
        auto * tabs = new QTabWidget;
        tabs->addTab(BuildGeneralPage(), "General");
        tabs->addTab(BuildVideoPage(), "Video");
        tabs->addTab(BuildAudioPage(), "Audio");
        tabs->addTab(BuildKeyboardPage(), "Keyboard");
        tabs->addTab(BuildGamepadPage(), "Gamepad");
        tabs->addTab(BuildDirectoriesPage(), "Directories");
        layout->addWidget(tabs, 1);

        m_Status = new QLabel;
        m_Status->setWordWrap(true);
        layout->addWidget(m_Status);

        auto * buttons = new QHBoxLayout;
        buttons->addStretch();
        auto * save = new QPushButton("Save");
        save->setDefault(true);
        connect(save, &QPushButton::clicked, this, [this]() { Save(); });
        buttons->addWidget(save);
        auto * cancel = new QPushButton("Cancel");
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        buttons->addWidget(cancel);
        layout->addLayout(buttons);
    }

private:
    QWidget * BuildGeneralPage()
    {
        auto * page = new QWidget;
        auto * form = new QFormLayout(page);
        m_Fullscreen = new QCheckBox("Start in fullscreen");
        m_Fullscreen->setChecked(m_Config.fullscreen);
        form->addRow(m_Fullscreen);
        m_WindowWidth = new QSpinBox;
        m_WindowWidth->setRange(320, 7680);
        m_WindowWidth->setValue(m_Config.windowWidth);
        form->addRow("Window width", m_WindowWidth);
        m_WindowHeight = new QSpinBox;
        m_WindowHeight->setRange(240, 4320);
        m_WindowHeight->setValue(m_Config.windowHeight);
        form->addRow("Window height", m_WindowHeight);
        m_LimitFps = new QCheckBox("Limit emulation speed");
        m_LimitFps->setChecked(m_Config.limitFps);
        form->addRow(m_LimitFps);
        auto * help = new QLabel(
            "Runtime controls are also available from the menu while a game is running. "
            "Keyboard shortcuts: F2 pause, F5 save, F7 load, F8 reset, F9 speed limit, "
            "F11 fullscreen, F12 screenshot, Escape exit.");
        help->setWordWrap(true);
        form->addRow(help);
        return page;
    }

    QWidget * BuildVideoPage()
    {
        auto * page = new QWidget;
        auto * form = new QFormLayout(page);
        m_Vsync = new QCheckBox("Vertical synchronization");
        m_Vsync->setChecked(m_Config.vsync);
        form->addRow(m_Vsync);
        m_Aspect = StringCombo({{"4:3", 0}, {"16:9", 1}, {"Stretch", 2}, {"Original", 3}}, m_Config.aspectRatio);
        form->addRow("Aspect ratio", m_Aspect);
        m_Filtering = StringCombo({{"Automatic", 0}, {"Force bilinear", 1}, {"Force point sampled", 2}}, m_Config.filtering);
        form->addRow("N64 texture filtering", m_Filtering);
        m_TextureFilter = StringCombo({
            {"None", 0x00}, {"Smooth 1", 0x01}, {"Smooth 2", 0x02},
            {"Smooth 3", 0x03}, {"Smooth 4", 0x04}, {"Sharp 1", 0x10}, {"Sharp 2", 0x20}},
            m_Config.textureFilter);
        form->addRow("Texture enhancement filter", m_TextureFilter);
        m_HighResolutionTextures = new QCheckBox("Load high-resolution texture packs");
        m_HighResolutionTextures->setChecked(m_Config.highResolutionTextures);
        form->addRow(m_HighResolutionTextures);
        m_AnisotropicFiltering = new QCheckBox("Anisotropic filtering");
        m_AnisotropicFiltering->setChecked(m_Config.anisotropicFiltering);
        form->addRow(m_AnisotropicFiltering);
        auto * note = new QLabel("The bundled Project64 OpenGL video plugin is used on Linux.");
        note->setWordWrap(true);
        form->addRow(note);
        return page;
    }

    QWidget * BuildAudioPage()
    {
        auto * page = new QWidget;
        auto * form = new QFormLayout(page);
        m_AudioEnabled = new QCheckBox("Enable audio");
        m_AudioEnabled->setChecked(m_Config.audioEnabled);
        form->addRow(m_AudioEnabled);
        m_AudioVolume = new QSpinBox;
        m_AudioVolume->setRange(0, 100);
        m_AudioVolume->setSuffix("%");
        m_AudioVolume->setValue(m_Config.audioVolume);
        form->addRow("Volume", m_AudioVolume);
        m_AudioDevice = new QComboBox;
        m_AudioDevice->addItem("System default", QString());
        for (int index = 0; index < SDL_GetNumAudioDevices(0); index++)
        {
            const char * name = SDL_GetAudioDeviceName(index, 0);
            if (name != nullptr)
            {
                m_AudioDevice->addItem(name, name);
            }
        }
        const int selected = m_AudioDevice->findData(Text(m_Config.audioDevice));
        m_AudioDevice->setCurrentIndex(selected >= 0 ? selected : 0);
        form->addRow("Output device", m_AudioDevice);
        return page;
    }

    QWidget * BuildKeyboardPage()
    {
        auto * contents = new QWidget;
        auto * grid = new QGridLayout(contents);
        for (size_t index = 0; index < m_Input.keyboard.size(); index++)
        {
            auto * combo = new QComboBox;
            combo->addItem("Unassigned", SDL_SCANCODE_UNKNOWN);
            for (int scanCode = 1; scanCode < SDL_NUM_SCANCODES; scanCode++)
            {
                const char * name = SDL_GetScancodeName(static_cast<SDL_Scancode>(scanCode));
                if (name != nullptr && name[0] != '\0')
                {
                    combo->addItem(name, scanCode);
                }
            }
            const int selected = combo->findData(static_cast<int>(m_Input.keyboard[index]));
            combo->setCurrentIndex(selected >= 0 ? selected : 0);
            grid->addWidget(new QLabel(pj64::input::KeyboardActionName(static_cast<pj64::input::KeyboardAction>(index))), static_cast<int>(index), 0);
            grid->addWidget(combo, static_cast<int>(index), 1);
            m_Keyboard.push_back(combo);
        }
        auto * defaults = new QPushButton("Restore keyboard defaults");
        connect(defaults, &QPushButton::clicked, this, [this]() {
            const pj64::input::InputConfig defaults;
            for (size_t index = 0; index < m_Keyboard.size(); index++)
            {
                m_Keyboard[index]->setCurrentIndex(m_Keyboard[index]->findData(static_cast<int>(defaults.keyboard[index])));
            }
        });
        grid->addWidget(defaults, static_cast<int>(m_Input.keyboard.size()), 0, 1, 2);
        grid->setColumnStretch(1, 1);
        return Scrollable(contents);
    }

    QWidget * BuildGamepadPage()
    {
        auto * contents = new QWidget;
        auto * form = new QFormLayout(contents);
        m_Controller = new QComboBox;
        m_Controller->addItem("Any controller", QString());
        for (int index = 0; index < SDL_NumJoysticks(); index++)
        {
            if (!SDL_IsGameController(index))
            {
                continue;
            }
            char guid[64] = {};
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
            const char * name = SDL_GameControllerNameForIndex(index);
            m_Controller->addItem(name != nullptr ? name : "Unnamed controller", QString::fromLatin1(guid));
        }
        const int controller = m_Controller->findData(Text(m_Input.controllerGuid));
        m_Controller->setCurrentIndex(controller >= 0 ? controller : 0);
        form->addRow("Preferred controller", m_Controller);

        const auto & bindings = BindingOptions();
        for (size_t index = 0; index < m_Input.gamepad.size(); index++)
        {
            auto * combo = new QComboBox;
            int selected = 0;
            for (size_t option = 0; option < bindings.size(); option++)
            {
                combo->addItem(bindings[option].name, static_cast<int>(option));
                if (SameBinding(bindings[option].binding, m_Input.gamepad[index]))
                {
                    selected = static_cast<int>(option);
                }
            }
            combo->setCurrentIndex(selected);
            form->addRow(pj64::input::N64ButtonName(static_cast<pj64::input::N64Button>(index)), combo);
            m_Gamepad.push_back(combo);
        }

        const std::vector<std::pair<const char *, int>> axes = {
            {"Left X", SDL_CONTROLLER_AXIS_LEFTX}, {"Left Y", SDL_CONTROLLER_AXIS_LEFTY},
            {"Right X", SDL_CONTROLLER_AXIS_RIGHTX}, {"Right Y", SDL_CONTROLLER_AXIS_RIGHTY}};
        m_AnalogX = StringCombo(axes, m_Input.analogX);
        m_AnalogY = StringCombo(axes, m_Input.analogY);
        form->addRow("Analog X axis", m_AnalogX);
        form->addRow("Analog Y axis", m_AnalogY);
        m_InvertAnalogX = new QCheckBox("Invert analog X");
        m_InvertAnalogX->setChecked(m_Input.invertAnalogX);
        form->addRow(m_InvertAnalogX);
        m_InvertAnalogY = new QCheckBox("Invert analog Y");
        m_InvertAnalogY->setChecked(m_Input.invertAnalogY);
        form->addRow(m_InvertAnalogY);
        m_Deadzone = new QSpinBox;
        m_Deadzone->setRange(0, 20000);
        m_Deadzone->setValue(m_Input.deadzone);
        form->addRow("Deadzone", m_Deadzone);
        m_Sensitivity = new QSpinBox;
        m_Sensitivity->setRange(1, 127);
        m_Sensitivity->setValue(m_Input.sensitivity);
        form->addRow("Analog sensitivity", m_Sensitivity);
        auto * defaults = new QPushButton("Restore gamepad defaults");
        connect(defaults, &QPushButton::clicked, this, [this]() {
            const pj64::input::InputConfig defaults;
            const auto & bindings = BindingOptions();
            for (size_t index = 0; index < m_Gamepad.size(); index++)
            {
                for (size_t option = 0; option < bindings.size(); option++)
                {
                    if (SameBinding(bindings[option].binding, defaults.gamepad[index]))
                    {
                        m_Gamepad[index]->setCurrentIndex(static_cast<int>(option));
                        break;
                    }
                }
            }
            m_AnalogX->setCurrentIndex(m_AnalogX->findData(defaults.analogX));
            m_AnalogY->setCurrentIndex(m_AnalogY->findData(defaults.analogY));
            m_InvertAnalogX->setChecked(defaults.invertAnalogX);
            m_InvertAnalogY->setChecked(defaults.invertAnalogY);
            m_Deadzone->setValue(defaults.deadzone);
            m_Sensitivity->setValue(defaults.sensitivity);
        });
        form->addRow(defaults);
        return Scrollable(contents);
    }

    QWidget * DirectoryRow(QLineEdit *& edit, const std::string & value)
    {
        auto * row = new QWidget;
        auto * layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        edit = new QLineEdit(Text(value));
        layout->addWidget(edit, 1);
        auto * browse = new QPushButton("Browse…");
        layout->addWidget(browse);
        connect(browse, &QPushButton::clicked, this, [this, edit]() {
            const QString path = QFileDialog::getExistingDirectory(this, "Choose directory", edit->text());
            if (!path.isEmpty())
            {
                edit->setText(path);
            }
        });
        return row;
    }

    QWidget * BuildDirectoriesPage()
    {
        auto * page = new QWidget;
        auto * form = new QFormLayout(page);
        auto * note = new QLabel("Leave a directory empty to use the portable folder beside the executable.");
        note->setWordWrap(true);
        form->addRow(note);
        form->addRow("Native saves", DirectoryRow(m_SaveDirectory, m_Config.saveDirectory));
        form->addRow("Save states", DirectoryRow(m_StateDirectory, m_Config.stateDirectory));
        form->addRow("Screenshots", DirectoryRow(m_ScreenshotDirectory, m_Config.screenshotDirectory));
        form->addRow("Texture packs", DirectoryRow(m_TextureDirectory, m_Config.textureDirectory));
        return page;
    }

    void Apply()
    {
        m_Config.fullscreen = m_Fullscreen->isChecked();
        m_Config.windowWidth = m_WindowWidth->value();
        m_Config.windowHeight = m_WindowHeight->value();
        m_Config.limitFps = m_LimitFps->isChecked();
        m_Config.vsync = m_Vsync->isChecked();
        m_Config.aspectRatio = m_Aspect->currentData().toInt();
        m_Config.filtering = m_Filtering->currentData().toInt();
        m_Config.textureFilter = m_TextureFilter->currentData().toInt();
        m_Config.highResolutionTextures = m_HighResolutionTextures->isChecked();
        m_Config.anisotropicFiltering = m_AnisotropicFiltering->isChecked();
        m_Config.audioEnabled = m_AudioEnabled->isChecked();
        m_Config.audioVolume = m_AudioVolume->value();
        m_Config.audioDevice = Text(m_AudioDevice->currentData().toString());
        m_Config.saveDirectory = Text(m_SaveDirectory->text());
        m_Config.stateDirectory = Text(m_StateDirectory->text());
        m_Config.screenshotDirectory = Text(m_ScreenshotDirectory->text());
        m_Config.textureDirectory = Text(m_TextureDirectory->text());
        for (size_t index = 0; index < m_Keyboard.size(); index++)
        {
            m_Input.keyboard[index] = static_cast<SDL_Scancode>(m_Keyboard[index]->currentData().toInt());
        }
        const auto & bindings = BindingOptions();
        for (size_t index = 0; index < m_Gamepad.size(); index++)
        {
            m_Input.gamepad[index] = bindings[static_cast<size_t>(m_Gamepad[index]->currentData().toInt())].binding;
        }
        m_Input.controllerGuid = Text(m_Controller->currentData().toString());
        m_Input.analogX = static_cast<SDL_GameControllerAxis>(m_AnalogX->currentData().toInt());
        m_Input.analogY = static_cast<SDL_GameControllerAxis>(m_AnalogY->currentData().toInt());
        m_Input.invertAnalogX = m_InvertAnalogX->isChecked();
        m_Input.invertAnalogY = m_InvertAnalogY->isChecked();
        m_Input.deadzone = m_Deadzone->value();
        m_Input.sensitivity = m_Sensitivity->value();
    }

    void Save()
    {
        Apply();
        if (!m_Config.Save(m_FrontendConfigPath) || !m_Input.Save(m_InputConfigPath))
        {
            m_Status->setText("Unable to save one or more configuration files.");
            return;
        }
        accept();
    }

    LinuxConfig & m_Config;
    pj64::input::InputConfig & m_Input;
    std::string m_FrontendConfigPath;
    std::string m_InputConfigPath;
    QCheckBox * m_Fullscreen = nullptr;
    QSpinBox * m_WindowWidth = nullptr;
    QSpinBox * m_WindowHeight = nullptr;
    QCheckBox * m_LimitFps = nullptr;
    QCheckBox * m_Vsync = nullptr;
    QComboBox * m_Aspect = nullptr;
    QComboBox * m_Filtering = nullptr;
    QComboBox * m_TextureFilter = nullptr;
    QCheckBox * m_HighResolutionTextures = nullptr;
    QCheckBox * m_AnisotropicFiltering = nullptr;
    QCheckBox * m_AudioEnabled = nullptr;
    QSpinBox * m_AudioVolume = nullptr;
    QComboBox * m_AudioDevice = nullptr;
    std::vector<QComboBox *> m_Keyboard;
    QComboBox * m_Controller = nullptr;
    std::vector<QComboBox *> m_Gamepad;
    QComboBox * m_AnalogX = nullptr;
    QComboBox * m_AnalogY = nullptr;
    QCheckBox * m_InvertAnalogX = nullptr;
    QCheckBox * m_InvertAnalogY = nullptr;
    QSpinBox * m_Deadzone = nullptr;
    QSpinBox * m_Sensitivity = nullptr;
    QLineEdit * m_SaveDirectory = nullptr;
    QLineEdit * m_StateDirectory = nullptr;
    QLineEdit * m_ScreenshotDirectory = nullptr;
    QLineEdit * m_TextureDirectory = nullptr;
    QLabel * m_Status = nullptr;
};

class EmulatorMainWindow final : public QMainWindow
{
public:
    std::function<void()> closeRequested;

protected:
    void closeEvent(QCloseEvent * event) override
    {
        if (closeRequested)
        {
            closeRequested();
        }
        event->accept();
    }
};
}

bool ShowSettings(
    LinuxConfig & config,
    pj64::input::InputConfig & input,
    const std::string & frontendConfigPath,
    const std::string & inputConfigPath)
{
    SettingsDialog dialog(config, input, frontendConfigPath, inputConfigPath);
    return dialog.exec() == QDialog::Accepted;
}

struct RuntimeWindow::Implementation
{
    Implementation(
        LinuxConfig & frontendConfig,
        pj64::input::InputConfig & controllerConfig,
        std::string frontendPath,
        std::string inputPath) :
        config(frontendConfig),
        input(controllerConfig),
        frontendConfigPath(std::move(frontendPath)),
        inputConfigPath(std::move(inputPath))
    {
        window.setWindowTitle("Project64-EM");
        window.setMinimumSize(320, 240);
        window.closeRequested = [this]() {
            open = false;
            if (attached)
            {
                command = RuntimeCommand::Quit;
            }
            if (launcherFinished)
            {
                launcherFinished();
            }
        };

        auto addRuntimeCommand = [this](QMenu * menu, const char * label, RuntimeCommand value) {
            QAction * action = menu->addAction(label);
            QObject::connect(action, &QAction::triggered, &window, [this, value]() { command = value; });
            action->setEnabled(false);
            runtimeActions.push_back(action);
            return action;
        };

        QMenu * file = window.menuBar()->addMenu("&File");
        openAction = file->addAction("Open ROM…");
        QObject::connect(openAction, &QAction::triggered, &window, [this]() { ChooseRom(); });
        startAction = file->addAction("Start Game");
        QObject::connect(startAction, &QAction::triggered, &window, [this]() { FinishLaunch(); });
        file->addSeparator();
        QAction * quitAction = file->addAction("Quit");
        QObject::connect(quitAction, &QAction::triggered, &window, [this]() {
            open = false;
            if (attached)
            {
                command = RuntimeCommand::Quit;
            }
            if (launcherFinished)
            {
                launcherFinished();
            }
        });

        QMenu * system = window.menuBar()->addMenu("&System");
        addRuntimeCommand(system, "Soft Reset", RuntimeCommand::SoftReset);
        pauseAction = addRuntimeCommand(system, "Pause", RuntimeCommand::PauseResume);
        addRuntimeCommand(system, "Take Screenshot", RuntimeCommand::Screenshot);
        system->addSeparator();
        speedAction = addRuntimeCommand(system, "Disable Speed Limit", RuntimeCommand::ToggleSpeedLimit);
        system->addSeparator();
        addRuntimeCommand(system, "Save State", RuntimeCommand::SaveState);
        addRuntimeCommand(system, "Load State", RuntimeCommand::LoadState);

        QMenu * options = window.menuBar()->addMenu("&Options");
        addRuntimeCommand(options, "Toggle Fullscreen", RuntimeCommand::ToggleFullscreen);
        options->addSeparator();
        QAction * settingsAction = options->addAction("Settings…");
        QObject::connect(settingsAction, &QAction::triggered, &window, [this]() {
            if (attached)
            {
                command = RuntimeCommand::Settings;
            }
            else
            {
                ShowSettings(config, input, frontendConfigPath, inputConfigPath);
            }
        });

        QMenu * help = window.menuBar()->addMenu("&Help");
        QAction * aboutAction = help->addAction("About Project64-EM");
        QObject::connect(aboutAction, &QAction::triggered, &window, [this]() {
            QMessageBox::about(
                &window,
                "About Project64-EM",
                "Project64-EM for OoTMM multiplayer\n\nLinux Qt frontend · OoTMM client release r4");
        });

        BuildHome();
        window.resize(900, 620);
        window.show();
    }

    void BuildHome()
    {
        auto * home = new QWidget;
        auto * layout = new QVBoxLayout(home);
        layout->setContentsMargins(48, 36, 48, 36);
        auto * heading = new QLabel("<h1>Project64-EM</h1><p>Open an OoTMM ROM to begin.</p>");
        heading->setTextFormat(Qt::RichText);
        layout->addWidget(heading);

        auto * romGroup = new QGroupBox("Game");
        auto * romLayout = new QVBoxLayout(romGroup);
        auto * romRow = new QHBoxLayout;
        romEdit = new QLineEdit(Text(config.lastRom));
        romEdit->setPlaceholderText("Choose a .z64, .n64, .v64, .zip, or .7z file");
        romRow->addWidget(romEdit, 1);
        auto * browse = new QPushButton("Browse…");
        QObject::connect(browse, &QPushButton::clicked, &window, [this]() { ChooseRom(); });
        romRow->addWidget(browse);
        romLayout->addLayout(romRow);

        if (!config.recentRoms.empty())
        {
            auto * recent = new QComboBox;
            recent->addItem("Recent games");
            for (const std::string & path : config.recentRoms)
            {
                recent->addItem(Text(std::filesystem::path(path).filename().string()), Text(path));
            }
            QObject::connect(recent, &QComboBox::currentIndexChanged, &window, [this, recent](int index) {
                if (index > 0)
                {
                    romEdit->setText(recent->itemData(index).toString());
                }
            });
            romLayout->addWidget(recent);
        }

        auto * start = new QPushButton("Start Game");
        start->setDefault(true);
        start->setMinimumHeight(38);
        QObject::connect(start, &QPushButton::clicked, &window, [this]() { FinishLaunch(); });
        romLayout->addWidget(start);
        layout->addWidget(romGroup);
        layout->addStretch();

        auto * note = new QLabel(
            "The emulator exposes its multiplayer IPC socket automatically. "
            "Start the OoTMM multiclient r4 after the game is running.");
        note->setWordWrap(true);
        layout->addWidget(note);
        window.setCentralWidget(home);
        window.statusBar()->showMessage("Ready");
    }

    void ChooseRom()
    {
        const QString current = romEdit != nullptr ? romEdit->text() : Text(config.lastRom);
        const QString path = QFileDialog::getOpenFileName(
            &window,
            "Open ROM",
            current,
            "Nintendo 64 ROMs (*.z64 *.n64 *.v64 *.zip *.7z);;All files (*)");
        if (!path.isEmpty() && romEdit != nullptr)
        {
            romEdit->setText(path);
        }
    }

    void FinishLaunch()
    {
        if (romEdit == nullptr)
        {
            return;
        }
        const std::string path = Text(romEdit->text());
        if (!std::filesystem::is_regular_file(path))
        {
            window.statusBar()->showMessage("Choose an existing ROM before starting", 5000);
            return;
        }
        config.AddRecentRom(path);
        if (!config.Save(frontendConfigPath) || !input.Save(inputConfigPath))
        {
            window.statusBar()->showMessage("Unable to save one or more configuration files", 5000);
            return;
        }
        launchAccepted = true;
        if (launcherFinished)
        {
            launcherFinished();
        }
    }

    LinuxConfig & config;
    pj64::input::InputConfig & input;
    std::string frontendConfigPath;
    std::string inputConfigPath;
    SDL_Window * sdlWindow = nullptr;
    EmulatorMainWindow window;
    QWindow * foreignWindow = nullptr;
    QWidget * container = nullptr;
    QLineEdit * romEdit = nullptr;
    QAction * openAction = nullptr;
    QAction * startAction = nullptr;
    QAction * pauseAction = nullptr;
    QAction * speedAction = nullptr;
    std::vector<QAction *> runtimeActions;
    std::function<void()> launcherFinished;
    RuntimeCommand command = RuntimeCommand::Idle;
    bool embedded = false;
    bool open = true;
    bool attached = false;
    bool launchAccepted = false;
    bool fullscreen = false;
};

RuntimeWindow::RuntimeWindow(
    LinuxConfig & config,
    pj64::input::InputConfig & input,
    const std::string & frontendConfigPath,
    const std::string & inputConfigPath) :
    m_Implementation(std::make_unique<Implementation>(config, input, frontendConfigPath, inputConfigPath))
{
}

RuntimeWindow::~RuntimeWindow() = default;

LauncherResult RuntimeWindow::SelectRom(volatile std::sig_atomic_t * stopSignal)
{
    QEventLoop loop;
    QTimer signalTimer;
    m_Implementation->launcherFinished = [&loop]() { loop.quit(); };
    QObject::connect(&signalTimer, &QTimer::timeout, &loop, [&loop, stopSignal]() {
        if (stopSignal != nullptr && *stopSignal != 0)
        {
            loop.quit();
        }
    });
    signalTimer.start(25);
    loop.exec();
    m_Implementation->launcherFinished = {};
    return m_Implementation->launchAccepted && (stopSignal == nullptr || *stopSignal == 0)
        ? LauncherResult::Launch
        : LauncherResult::Quit;
}

bool RuntimeWindow::AttachRenderWindow(SDL_Window * renderWindow, const std::string & title, int width, int height)
{
    m_Implementation->sdlWindow = renderWindow;
    m_Implementation->window.setWindowTitle(Text(title));

    SDL_SysWMinfo windowInfo;
    SDL_VERSION(&windowInfo.version);
    if (SDL_GetWindowWMInfo(renderWindow, &windowInfo) == SDL_TRUE)
    {
#if defined(SDL_VIDEO_DRIVER_X11)
        if (windowInfo.subsystem == SDL_SYSWM_X11 && QGuiApplication::platformName() == "xcb")
        {
            m_Implementation->foreignWindow = QWindow::fromWinId(static_cast<WId>(windowInfo.info.x11.window));
        }
#endif
    }

    if (m_Implementation->foreignWindow != nullptr)
    {
        m_Implementation->container = QWidget::createWindowContainer(m_Implementation->foreignWindow, &m_Implementation->window);
        m_Implementation->container->setFocusPolicy(Qt::StrongFocus);
        m_Implementation->container->setMinimumSize(320, 240);
        m_Implementation->window.setCentralWidget(m_Implementation->container);
        m_Implementation->embedded = true;
        m_Implementation->window.statusBar()->showMessage("Emulation running");
    }
    else
    {
        auto * message = new QLabel(
            "The render surface could not be embedded on this display server. "
            "The game is open in a separate SDL window; controls remain available here.");
        message->setAlignment(Qt::AlignCenter);
        message->setWordWrap(true);
        m_Implementation->window.setCentralWidget(message);
        SDL_ShowWindow(renderWindow);
        m_Implementation->window.statusBar()->showMessage("Using a separate render window");
    }

    m_Implementation->romEdit = nullptr;
    m_Implementation->openAction->setEnabled(false);
    m_Implementation->startAction->setEnabled(false);
    for (QAction * action : m_Implementation->runtimeActions)
    {
        action->setEnabled(true);
    }
    m_Implementation->attached = true;
    m_Implementation->window.resize(width, height + m_Implementation->window.menuBar()->sizeHint().height());
    m_Implementation->window.show();
    QApplication::processEvents();
    if (m_Implementation->container != nullptr)
    {
        m_Implementation->container->setFocus();
    }
    return m_Implementation->embedded;
}

bool RuntimeWindow::IsEmbedded() const
{
    return m_Implementation->embedded;
}

bool RuntimeWindow::IsOpen() const
{
    return m_Implementation->open;
}

void RuntimeWindow::ProcessEvents()
{
    QApplication::processEvents(QEventLoop::AllEvents, 2);
}

RuntimeCommand RuntimeWindow::TakeCommand()
{
    const RuntimeCommand result = m_Implementation->command;
    m_Implementation->command = RuntimeCommand::Idle;
    return result;
}

void RuntimeWindow::ToggleFullscreen()
{
    if (!m_Implementation->embedded && m_Implementation->sdlWindow != nullptr)
    {
        m_Implementation->fullscreen = !m_Implementation->fullscreen;
        SDL_SetWindowFullscreen(
            m_Implementation->sdlWindow,
            m_Implementation->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }
    else if (m_Implementation->window.isFullScreen())
    {
        m_Implementation->window.showNormal();
    }
    else
    {
        m_Implementation->window.showFullScreen();
    }
}

void RuntimeWindow::SetPaused(bool paused)
{
    m_Implementation->pauseAction->setText(paused ? "Resume" : "Pause");
    SetStatus(paused ? "Emulation paused" : "Emulation running");
}

void RuntimeWindow::SetSpeedLimited(bool limited)
{
    m_Implementation->speedAction->setText(limited ? "Disable Speed Limit" : "Enable Speed Limit");
}

void RuntimeWindow::SetStatus(const std::string & text)
{
    m_Implementation->window.statusBar()->showMessage(Text(text), 4000);
}
