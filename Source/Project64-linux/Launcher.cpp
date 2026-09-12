#include "Launcher.h"

#include "LinuxConfig.h"
#include <Project64-input-sdl/InputConfig.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QEventLoop>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
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
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

#include <SDL.h>
#include <SDL_syswm.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
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

const char * N64ButtonLabel(size_t index)
{
    static constexpr std::array<const char *, static_cast<size_t>(pj64::input::N64Button::Count)> labels = {{
        "A Button", "B Button", "Z Trigger", "Start", "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right",
        "C Button Up", "C Button Down", "C Button Left", "C Button Right", "L Trigger", "R Trigger",
    }};
    return labels[index];
}

QString AxisLabel(SDL_GameControllerAxis axis, bool inverted)
{
    const char * name = "Unassigned";
    switch (axis)
    {
    case SDL_CONTROLLER_AXIS_LEFTX: name = "Left Stick X"; break;
    case SDL_CONTROLLER_AXIS_LEFTY: name = "Left Stick Y"; break;
    case SDL_CONTROLLER_AXIS_RIGHTX: name = "Right Stick X"; break;
    case SDL_CONTROLLER_AXIS_RIGHTY: name = "Right Stick Y"; break;
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT: name = "Left Trigger"; break;
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: name = "Right Trigger"; break;
    default: break;
    }
    return QString(name) + (inverted ? " (inverted)" : "");
}

QString GamepadBindingLabel(const pj64::input::GamepadBinding & binding)
{
    using Kind = pj64::input::GamepadBinding::Kind;
    if (binding.kind == Kind::Button)
    {
        return Text(pj64::input::BindingName(binding));
    }
    if (binding.kind == Kind::PositiveAxis || binding.kind == Kind::NegativeAxis)
    {
        return AxisLabel(static_cast<SDL_GameControllerAxis>(binding.value), false) +
            (binding.kind == Kind::PositiveAxis ? " +" : " -");
    }
    return "Unassigned";
}

class SettingsDialog final : public QDialog
{
public:
    SettingsDialog(
        LinuxConfig & config,
        pj64::input::InputConfig & input,
        const std::string & frontendConfigPath,
        const std::string & inputConfigPath,
        SettingsPage page) :
        m_Config(config),
        m_Input(input),
        m_FrontendConfigPath(frontendConfigPath),
        m_InputConfigPath(inputConfigPath),
        m_GamepadBindings(input.gamepad),
        m_AnalogXValue(input.analogX),
        m_AnalogYValue(input.analogY)
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
        switch (page)
        {
        case SettingsPage::General: tabs->setCurrentIndex(0); break;
        case SettingsPage::Video: tabs->setCurrentIndex(1); break;
        case SettingsPage::Audio: tabs->setCurrentIndex(2); break;
        case SettingsPage::Input: tabs->setCurrentIndex(4); break;
        case SettingsPage::Directories: tabs->setCurrentIndex(5); break;
        }
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

        m_CaptureTimer.setInterval(16);
        connect(&m_CaptureTimer, &QTimer::timeout, this, [this]() { PollGamepadCapture(); });
        m_RumbleTimer.setSingleShot(true);
        connect(&m_RumbleTimer, &QTimer::timeout, this, [this]() { StopRumbleTest(); });
    }

    ~SettingsDialog() override
    {
        StopCapture(false);
        StopRumbleTest();
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

        for (size_t index = 0; index < m_Input.gamepad.size(); index++)
        {
            auto * row = new QWidget;
            auto * layout = new QHBoxLayout(row);
            layout->setContentsMargins(0, 0, 0, 0);
            auto * binding = new QPushButton(GamepadBindingLabel(m_GamepadBindings[index]));
            binding->setMinimumWidth(220);
            connect(binding, &QPushButton::clicked, this, [this, index]() { BeginCapture(CaptureKind::Button, index); });
            layout->addWidget(binding, 1);
            auto * clear = new QPushButton("Clear");
            connect(clear, &QPushButton::clicked, this, [this, index]() {
                StopCapture(false);
                m_GamepadBindings[index] = {};
                UpdateGamepadButton(index);
            });
            layout->addWidget(clear);
            form->addRow(N64ButtonLabel(index), row);
            m_Gamepad.push_back(binding);
        }

        m_AnalogX = new QPushButton(AxisLabel(m_AnalogXValue, m_Input.invertAnalogX));
        connect(m_AnalogX, &QPushButton::clicked, this, [this]() { BeginCapture(CaptureKind::AnalogX, 0); });
        form->addRow("Analog X (move right)", m_AnalogX);
        m_AnalogY = new QPushButton(AxisLabel(m_AnalogYValue, m_Input.invertAnalogY));
        connect(m_AnalogY, &QPushButton::clicked, this, [this]() { BeginCapture(CaptureKind::AnalogY, 0); });
        form->addRow("Analog Y (move up)", m_AnalogY);
        m_InvertAnalogX = new QCheckBox("Invert analog X");
        m_InvertAnalogX->setChecked(m_Input.invertAnalogX);
        connect(m_InvertAnalogX, &QCheckBox::toggled, this, [this](bool checked) {
            m_AnalogX->setText(AxisLabel(m_AnalogXValue, checked));
        });
        form->addRow(m_InvertAnalogX);
        m_InvertAnalogY = new QCheckBox("Invert analog Y");
        m_InvertAnalogY->setChecked(m_Input.invertAnalogY);
        connect(m_InvertAnalogY, &QCheckBox::toggled, this, [this](bool checked) {
            m_AnalogY->setText(AxisLabel(m_AnalogYValue, checked));
        });
        form->addRow(m_InvertAnalogY);
        m_Deadzone = new QSpinBox;
        m_Deadzone->setRange(0, 20000);
        m_Deadzone->setValue(m_Input.deadzone);
        form->addRow("Deadzone", m_Deadzone);
        m_Sensitivity = new QSpinBox;
        m_Sensitivity->setRange(1, 127);
        m_Sensitivity->setValue(m_Input.sensitivity);
        form->addRow("Analog sensitivity", m_Sensitivity);

        m_ControllerPak = new QComboBox;
        m_ControllerPak->addItem("None", static_cast<int>(pj64::input::ControllerPak::NoPak));
        m_ControllerPak->addItem("Memory Pak", static_cast<int>(pj64::input::ControllerPak::Mempak));
        m_ControllerPak->addItem("Rumble Pak", static_cast<int>(pj64::input::ControllerPak::RumblePak));
        m_ControllerPak->setCurrentIndex(m_ControllerPak->findData(static_cast<int>(m_Input.controllerPak)));
        form->addRow("Controller accessory", m_ControllerPak);

        auto * testRumble = new QPushButton("Test Rumble");
        connect(testRumble, &QPushButton::clicked, this, [this]() { TestRumble(); });
        form->addRow(testRumble);

        auto * defaults = new QPushButton("Restore gamepad defaults");
        connect(defaults, &QPushButton::clicked, this, [this]() {
            StopCapture(false);
            const pj64::input::InputConfig defaults;
            for (size_t index = 0; index < m_Gamepad.size(); index++)
            {
                m_GamepadBindings[index] = defaults.gamepad[index];
                UpdateGamepadButton(index);
            }
            m_AnalogXValue = defaults.analogX;
            m_AnalogYValue = defaults.analogY;
            m_InvertAnalogX->setChecked(defaults.invertAnalogX);
            m_InvertAnalogY->setChecked(defaults.invertAnalogY);
            m_AnalogX->setText(AxisLabel(m_AnalogXValue, defaults.invertAnalogX));
            m_AnalogY->setText(AxisLabel(m_AnalogYValue, defaults.invertAnalogY));
            m_Deadzone->setValue(defaults.deadzone);
            m_Sensitivity->setValue(defaults.sensitivity);
            m_ControllerPak->setCurrentIndex(m_ControllerPak->findData(static_cast<int>(defaults.controllerPak)));
        });
        form->addRow(defaults);
        auto * instructions = new QLabel(
            "Click a binding, then press a controller button or move an axis. "
            "Changes are applied to a running game as soon as Save is pressed.");
        instructions->setWordWrap(true);
        form->addRow(instructions);
        return Scrollable(contents);
    }

    enum class CaptureKind
    {
        Inactive,
        Button,
        AnalogX,
        AnalogY,
    };

    struct CaptureController
    {
        SDL_GameController * controller = nullptr;
        std::string guid;
        std::array<Uint8, SDL_CONTROLLER_BUTTON_MAX> buttons{};
        std::array<Sint16, SDL_CONTROLLER_AXIS_MAX> axes{};
    };

    void UpdateGamepadButton(size_t index)
    {
        m_Gamepad[index]->setText(GamepadBindingLabel(m_GamepadBindings[index]));
    }

    void CloseCaptureControllers()
    {
        for (CaptureController & device : m_CaptureControllers)
        {
            SDL_GameControllerClose(device.controller);
        }
        m_CaptureControllers.clear();
    }

    bool OpenCaptureControllers()
    {
        CloseCaptureControllers();
        SDL_GameControllerUpdate();
        const std::string selectedGuid = Text(m_Controller->currentData().toString());
        for (int index = 0; index < SDL_NumJoysticks(); index++)
        {
            if (!SDL_IsGameController(index))
            {
                continue;
            }
            char guidText[64] = {};
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guidText, sizeof(guidText));
            if (!selectedGuid.empty() && selectedGuid != guidText)
            {
                continue;
            }
            SDL_GameController * controller = SDL_GameControllerOpen(index);
            if (controller == nullptr)
            {
                continue;
            }
            CaptureController device;
            device.controller = controller;
            device.guid = guidText;
            for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++)
            {
                device.buttons[static_cast<size_t>(button)] = SDL_GameControllerGetButton(
                    controller, static_cast<SDL_GameControllerButton>(button));
            }
            for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++)
            {
                device.axes[static_cast<size_t>(axis)] = SDL_GameControllerGetAxis(
                    controller, static_cast<SDL_GameControllerAxis>(axis));
            }
            m_CaptureControllers.push_back(std::move(device));
        }
        if (m_CaptureControllers.empty())
        {
            m_Status->setText("No compatible SDL game controller is connected.");
            return false;
        }
        return true;
    }

    void BeginCapture(CaptureKind kind, size_t index)
    {
        StopRumbleTest();
        StopCapture(true);
        if (!OpenCaptureControllers())
        {
            return;
        }
        m_CaptureKind = kind;
        m_CaptureIndex = index;
        if (kind == CaptureKind::Button)
        {
            m_Gamepad[index]->setText("Press a button or move an axis…");
        }
        else if (kind == CaptureKind::AnalogX)
        {
            m_AnalogX->setText("Move the desired stick to the right…");
        }
        else
        {
            m_AnalogY->setText("Move the desired stick upward…");
        }
        m_Status->setText("Waiting for controller input. Return the sticks to neutral before moving one.");
        m_CaptureTimer.start();
    }

    void StopCapture(bool restoreLabel)
    {
        m_CaptureTimer.stop();
        if (restoreLabel)
        {
            if (m_CaptureKind == CaptureKind::Button && m_CaptureIndex < m_Gamepad.size())
            {
                UpdateGamepadButton(m_CaptureIndex);
            }
            else if (m_CaptureKind == CaptureKind::AnalogX && m_AnalogX != nullptr)
            {
                m_AnalogX->setText(AxisLabel(m_AnalogXValue, m_InvertAnalogX->isChecked()));
            }
            else if (m_CaptureKind == CaptureKind::AnalogY && m_AnalogY != nullptr)
            {
                m_AnalogY->setText(AxisLabel(m_AnalogYValue, m_InvertAnalogY->isChecked()));
            }
        }
        m_CaptureKind = CaptureKind::Inactive;
        CloseCaptureControllers();
    }

    void SelectCapturedController(const std::string & guid)
    {
        const int index = m_Controller->findData(Text(guid));
        if (index >= 0)
        {
            m_Controller->setCurrentIndex(index);
        }
    }

    void AcceptBinding(const CaptureController & device, const pj64::input::GamepadBinding & binding)
    {
        const CaptureKind kind = m_CaptureKind;
        const size_t index = m_CaptureIndex;
        SelectCapturedController(device.guid);
        if (kind == CaptureKind::Button)
        {
            m_GamepadBindings[index] = binding;
        }
        else
        {
            const auto axis = static_cast<SDL_GameControllerAxis>(binding.value);
            const bool inverted = binding.kind == pj64::input::GamepadBinding::Kind::NegativeAxis;
            if (kind == CaptureKind::AnalogX)
            {
                m_AnalogXValue = axis;
                m_InvertAnalogX->setChecked(inverted);
            }
            else
            {
                m_AnalogYValue = axis;
                m_InvertAnalogY->setChecked(inverted);
            }
        }
        StopCapture(false);
        if (kind == CaptureKind::Button)
        {
            UpdateGamepadButton(index);
            m_Status->setText("Bound " + QString(N64ButtonLabel(index)) + " to " + GamepadBindingLabel(binding) + ".");
        }
        else if (kind == CaptureKind::AnalogX)
        {
            m_AnalogX->setText(AxisLabel(m_AnalogXValue, m_InvertAnalogX->isChecked()));
            m_Status->setText("Analog X axis captured.");
        }
        else
        {
            m_AnalogY->setText(AxisLabel(m_AnalogYValue, m_InvertAnalogY->isChecked()));
            m_Status->setText("Analog Y axis captured.");
        }
    }

    void PollGamepadCapture()
    {
        SDL_GameControllerUpdate();
        const int threshold = std::max(16000, m_Deadzone->value() + 4000);
        for (CaptureController & device : m_CaptureControllers)
        {
            if (m_CaptureKind == CaptureKind::Button)
            {
                for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++)
                {
                    const Uint8 pressed = SDL_GameControllerGetButton(
                        device.controller, static_cast<SDL_GameControllerButton>(button));
                    const bool newlyPressed = pressed != 0 && device.buttons[static_cast<size_t>(button)] == 0;
                    device.buttons[static_cast<size_t>(button)] = pressed;
                    if (newlyPressed)
                    {
                        AcceptBinding(device, {pj64::input::GamepadBinding::Kind::Button, button});
                        return;
                    }
                }
            }
            const int axisLimit = m_CaptureKind == CaptureKind::Button
                ? SDL_CONTROLLER_AXIS_MAX
                : SDL_CONTROLLER_AXIS_TRIGGERLEFT;
            for (int axis = 0; axis < axisLimit; axis++)
            {
                const Sint16 value = SDL_GameControllerGetAxis(
                    device.controller, static_cast<SDL_GameControllerAxis>(axis));
                const Sint16 previous = device.axes[static_cast<size_t>(axis)];
                device.axes[static_cast<size_t>(axis)] = value;
                if (std::abs(static_cast<int>(value)) <= threshold ||
                    std::abs(static_cast<int>(previous)) > threshold)
                {
                    continue;
                }
                const auto kind = value > 0
                    ? pj64::input::GamepadBinding::Kind::PositiveAxis
                    : pj64::input::GamepadBinding::Kind::NegativeAxis;
                AcceptBinding(device, {kind, axis});
                return;
            }
        }
    }

    void TestRumble()
    {
        StopCapture(true);
        StopRumbleTest();
        const std::string selectedGuid = Text(m_Controller->currentData().toString());
        for (int index = 0; index < SDL_NumJoysticks(); index++)
        {
            if (!SDL_IsGameController(index))
            {
                continue;
            }
            char guidText[64] = {};
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guidText, sizeof(guidText));
            if (!selectedGuid.empty() && selectedGuid != guidText)
            {
                continue;
            }
            m_RumbleController = SDL_GameControllerOpen(index);
            if (m_RumbleController != nullptr)
            {
                break;
            }
        }
        if (m_RumbleController == nullptr)
        {
            m_Status->setText("No compatible SDL game controller is connected.");
            return;
        }
        if (SDL_GameControllerRumble(m_RumbleController, 0xFFFF, 0xFFFF, 500) != 0)
        {
            m_Status->setText("This controller or driver does not expose rumble through SDL.");
            StopRumbleTest();
            return;
        }
        m_Status->setText("Rumble test started. Select Rumble Pak to enable it in games.");
        m_RumbleTimer.start(550);
    }

    void StopRumbleTest()
    {
        m_RumbleTimer.stop();
        if (m_RumbleController != nullptr)
        {
            SDL_GameControllerRumble(m_RumbleController, 0, 0, 0);
            SDL_GameControllerClose(m_RumbleController);
            m_RumbleController = nullptr;
        }
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
        for (size_t index = 0; index < m_Gamepad.size(); index++)
        {
            m_Input.gamepad[index] = m_GamepadBindings[index];
        }
        m_Input.controllerGuid = Text(m_Controller->currentData().toString());
        m_Input.analogX = m_AnalogXValue;
        m_Input.analogY = m_AnalogYValue;
        m_Input.invertAnalogX = m_InvertAnalogX->isChecked();
        m_Input.invertAnalogY = m_InvertAnalogY->isChecked();
        m_Input.deadzone = m_Deadzone->value();
        m_Input.sensitivity = m_Sensitivity->value();
        m_Input.controllerPak = static_cast<pj64::input::ControllerPak>(m_ControllerPak->currentData().toInt());
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
    std::vector<QPushButton *> m_Gamepad;
    std::array<pj64::input::GamepadBinding, static_cast<size_t>(pj64::input::N64Button::Count)> m_GamepadBindings;
    QPushButton * m_AnalogX = nullptr;
    QPushButton * m_AnalogY = nullptr;
    SDL_GameControllerAxis m_AnalogXValue;
    SDL_GameControllerAxis m_AnalogYValue;
    QCheckBox * m_InvertAnalogX = nullptr;
    QCheckBox * m_InvertAnalogY = nullptr;
    QSpinBox * m_Deadzone = nullptr;
    QSpinBox * m_Sensitivity = nullptr;
    QComboBox * m_ControllerPak = nullptr;
    QLineEdit * m_SaveDirectory = nullptr;
    QLineEdit * m_StateDirectory = nullptr;
    QLineEdit * m_ScreenshotDirectory = nullptr;
    QLineEdit * m_TextureDirectory = nullptr;
    QLabel * m_Status = nullptr;
    QTimer m_CaptureTimer;
    QTimer m_RumbleTimer;
    CaptureKind m_CaptureKind = CaptureKind::Inactive;
    size_t m_CaptureIndex = 0;
    std::vector<CaptureController> m_CaptureControllers;
    SDL_GameController * m_RumbleController = nullptr;
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
    const std::string & inputConfigPath,
    SettingsPage page)
{
    SettingsDialog dialog(config, input, frontendConfigPath, inputConfigPath, page);
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

        window.menuBar()->setNativeMenuBar(false);

        auto addRuntimeCommand = [this](QMenu * menu, const char * label, RuntimeCommand value, const char * shortcut) {
            QAction * action = menu->addAction(label);
            QObject::connect(action, &QAction::triggered, &window, [this, value]() { command = value; });
            if (shortcut[0] != '\0')
            {
                action->setShortcut(QKeySequence(shortcut));
            }
            action->setEnabled(false);
            runtimeActions.push_back(action);
            return action;
        };

        QMenu * file = window.menuBar()->addMenu("&File");
        openAction = file->addAction(window.style()->standardIcon(QStyle::SP_DialogOpenButton), "&Open ROM…");
        openAction->setShortcut(QKeySequence("Ctrl+O"));
        QObject::connect(openAction, &QAction::triggered, &window, [this]() { ChooseRom(true); });
        startAction = file->addAction(window.style()->standardIcon(QStyle::SP_MediaPlay), "Start &Emulation");
        QObject::connect(startAction, &QAction::triggered, &window, [this]() { FinishLaunch(); });
        recentMenu = file->addMenu("Recent ROM");
        RefreshRecentMenu();
        file->addSeparator();
        QAction * quitAction = file->addAction("E&xit");
        quitAction->setShortcut(QKeySequence("Alt+F4"));
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
        QMenu * reset = system->addMenu("&Reset");
        resetAction = addRuntimeCommand(reset, "&Soft Reset", RuntimeCommand::SoftReset, "F8");
        resetAction->setIcon(window.style()->standardIcon(QStyle::SP_BrowserReload));
        addRuntimeCommand(reset, "&Hard Reset", RuntimeCommand::HardReset, "Shift+F8");
        pauseAction = addRuntimeCommand(system, "&Pause", RuntimeCommand::PauseResume, "F2");
        pauseAction->setIcon(window.style()->standardIcon(QStyle::SP_MediaPause));
        addRuntimeCommand(system, "Capture Screenshot", RuntimeCommand::Screenshot, "F12");
        system->addSeparator();
        speedAction = addRuntimeCommand(system, "Limit FPS", RuntimeCommand::ToggleSpeedLimit, "F9");
        speedAction->setCheckable(true);
        system->addSeparator();
        addRuntimeCommand(system, "&Save State", RuntimeCommand::SaveState, "F5");
        addRuntimeCommand(system, "&Load State", RuntimeCommand::LoadState, "F7");
        system->menuAction()->setVisible(false);
        systemMenuAction = system->menuAction();

        QMenu * options = window.menuBar()->addMenu("&Options");
        fullscreenAction = addRuntimeCommand(options, "&Fullscreen", RuntimeCommand::ToggleFullscreen, "F11");
        fullscreenAction->setCheckable(true);
        fullscreenAction->setIcon(window.style()->standardIcon(QStyle::SP_TitleBarMaxButton));
        options->addSeparator();
        auto addSettingsAction = [this, options](const char * label, SettingsPage page, RuntimeCommand value) {
            QAction * action = options->addAction(label);
            QObject::connect(action, &QAction::triggered, &window, [this, page, value]() {
                if (attached)
                {
                    command = value;
                }
                else
                {
                    ShowSettings(config, input, frontendConfigPath, inputConfigPath, page);
                }
            });
            return action;
        };
        addSettingsAction("&Graphics Settings…", SettingsPage::Video, RuntimeCommand::VideoSettings);
        addSettingsAction("&Audio Settings…", SettingsPage::Audio, RuntimeCommand::AudioSettings);
        inputSettingsAction = addSettingsAction("&Input Settings…", SettingsPage::Input, RuntimeCommand::InputSettings);
        inputSettingsAction->setIcon(window.style()->standardIcon(QStyle::SP_ComputerIcon));
        options->addSeparator();
        settingsAction = addSettingsAction("Configura&tion…", SettingsPage::General, RuntimeCommand::Settings);
        settingsAction->setIcon(window.style()->standardIcon(QStyle::SP_FileDialogDetailedView));

        QMenu * help = window.menuBar()->addMenu("&Help");
        QAction * websiteAction = help->addAction("&Website");
        QObject::connect(websiteAction, &QAction::triggered, &window, []() {
            QDesktopServices::openUrl(QUrl("https://www.pj64-emu.com/"));
        });
        QAction * discordAction = help->addAction("&Discord");
        QObject::connect(discordAction, &QAction::triggered, &window, []() {
            QDesktopServices::openUrl(QUrl("https://discord.gg/Cg3zquF"));
        });
        help->addSeparator();
        QAction * aboutAction = help->addAction("&About Project64-EM");
        QObject::connect(aboutAction, &QAction::triggered, &window, [this]() {
            QMessageBox::about(
                &window,
                "About Project64-EM",
                "Project64-EM for OoTMM multiplayer\n\nLinux Qt frontend · OoTMM client release r4");
        });

        QToolBar * toolbar = window.addToolBar("Main");
        toolbar->setMovable(false);
        toolbar->setFloatable(false);
        toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        toolbar->addAction(openAction);
        toolbar->addAction(startAction);
        toolbar->addSeparator();
        toolbar->addAction(pauseAction);
        toolbar->addAction(resetAction);
        toolbar->addAction(fullscreenAction);
        toolbar->addSeparator();
        toolbar->addAction(inputSettingsAction);
        toolbar->addAction(settingsAction);

        BuildHome();
        window.resize(900, 620);
        window.show();
    }

    void BuildHome()
    {
        auto * home = new QWidget;
        auto * layout = new QVBoxLayout(home);
        layout->setContentsMargins(12, 12, 12, 12);
        auto * heading = new QLabel("ROM Browser");
        QFont headingFont = heading->font();
        headingFont.setBold(true);
        headingFont.setPointSize(headingFont.pointSize() + 2);
        heading->setFont(headingFont);
        layout->addWidget(heading);

        auto * romGroup = new QGroupBox("Selected ROM");
        auto * romLayout = new QVBoxLayout(romGroup);
        auto * romRow = new QHBoxLayout;
        romEdit = new QLineEdit(Text(config.lastRom));
        romEdit->setPlaceholderText("Choose a .z64, .n64, .v64, .zip, or .7z file");
        romRow->addWidget(romEdit, 1);
        auto * browse = new QPushButton("Browse…");
        QObject::connect(browse, &QPushButton::clicked, &window, [this]() { ChooseRom(false); });
        romRow->addWidget(browse);
        romLayout->addLayout(romRow);

        auto * recent = new QTreeWidget;
        recent->setHeaderLabels({"Good Name", "File"});
        recent->setRootIsDecorated(false);
        recent->setAlternatingRowColors(true);
        recent->setSelectionMode(QAbstractItemView::SingleSelection);
        recent->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        recent->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        for (const std::string & path : config.recentRoms)
        {
            auto * item = new QTreeWidgetItem(recent);
            item->setText(0, Text(std::filesystem::path(path).stem().string()));
            item->setText(1, Text(path));
            item->setData(0, Qt::UserRole, Text(path));
        }
        QObject::connect(recent, &QTreeWidget::itemSelectionChanged, &window, [this, recent]() {
            const QList<QTreeWidgetItem *> selected = recent->selectedItems();
            if (!selected.empty())
            {
                romEdit->setText(selected.front()->data(0, Qt::UserRole).toString());
            }
        });
        QObject::connect(recent, &QTreeWidget::itemDoubleClicked, &window, [this](QTreeWidgetItem *, int) {
            FinishLaunch();
        });
        layout->addWidget(recent, 1);

        auto * start = new QPushButton(window.style()->standardIcon(QStyle::SP_MediaPlay), "Start Emulation");
        start->setDefault(true);
        start->setMinimumHeight(38);
        QObject::connect(start, &QPushButton::clicked, &window, [this]() { FinishLaunch(); });
        romLayout->addWidget(start);
        layout->addWidget(romGroup);

        auto * note = new QLabel(
            "The emulator exposes its multiplayer IPC socket automatically. "
            "Start the OoTMM multiclient r4 after the game is running.");
        note->setWordWrap(true);
        layout->addWidget(note);
        window.setCentralWidget(home);
        window.statusBar()->showMessage("Ready");
    }

    void ChooseRom(bool startAfterSelection)
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
            if (startAfterSelection)
            {
                FinishLaunch();
            }
        }
    }

    void RefreshRecentMenu()
    {
        recentMenu->clear();
        if (config.recentRoms.empty())
        {
            QAction * empty = recentMenu->addAction("(Empty)");
            empty->setEnabled(false);
            return;
        }
        for (const std::string & path : config.recentRoms)
        {
            QAction * action = recentMenu->addAction(Text(std::filesystem::path(path).filename().string()));
            action->setToolTip(Text(path));
            QObject::connect(action, &QAction::triggered, &window, [this, path]() {
                if (romEdit != nullptr)
                {
                    romEdit->setText(Text(path));
                    FinishLaunch();
                }
            });
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
        RefreshRecentMenu();
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

    void SynchronizeRenderSize()
    {
        if (container == nullptr)
        {
            return;
        }
        const qreal scale = container->devicePixelRatioF();
        const int width = std::max(1, static_cast<int>(std::lround(container->width() * scale)));
        const int height = std::max(1, static_cast<int>(std::lround(container->height() * scale)));
        drawableWidth.store(static_cast<uint32_t>(width), std::memory_order_relaxed);
        drawableHeight.store(static_cast<uint32_t>(height), std::memory_order_relaxed);
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
    QAction * resetAction = nullptr;
    QAction * pauseAction = nullptr;
    QAction * speedAction = nullptr;
    QAction * fullscreenAction = nullptr;
    QAction * inputSettingsAction = nullptr;
    QAction * settingsAction = nullptr;
    QAction * systemMenuAction = nullptr;
    QMenu * recentMenu = nullptr;
    std::vector<QAction *> runtimeActions;
    std::function<void()> launcherFinished;
    RuntimeCommand command = RuntimeCommand::Idle;
    bool embedded = false;
    bool open = true;
    bool attached = false;
    bool launchAccepted = false;
    bool fullscreen = false;
    std::atomic_uint32_t drawableWidth{0};
    std::atomic_uint32_t drawableHeight{0};
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
    const QWidget * previousCentralWidget = m_Implementation->window.centralWidget();
    const int chromeHeight = previousCentralWidget != nullptr
        ? m_Implementation->window.height() - previousCentralWidget->height()
        : 0;

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
    m_Implementation->recentMenu->setEnabled(false);
    for (QAction * action : m_Implementation->runtimeActions)
    {
        action->setEnabled(true);
    }
    m_Implementation->systemMenuAction->setVisible(true);
    m_Implementation->attached = true;
    m_Implementation->window.resize(width, height + chromeHeight);
    m_Implementation->window.show();
    QApplication::processEvents();
    m_Implementation->SynchronizeRenderSize();
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
    m_Implementation->SynchronizeRenderSize();
}

void RuntimeWindow::GetDrawableSize(uint32_t & width, uint32_t & height) const
{
    width = m_Implementation->drawableWidth.load(std::memory_order_relaxed);
    height = m_Implementation->drawableHeight.load(std::memory_order_relaxed);
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
    m_Implementation->fullscreenAction->setChecked(
        m_Implementation->embedded ? m_Implementation->window.isFullScreen() : m_Implementation->fullscreen);
}

void RuntimeWindow::SetPaused(bool paused)
{
    m_Implementation->pauseAction->setText(paused ? "Resume" : "Pause");
    SetStatus(paused ? "Emulation paused" : "Emulation running");
}

void RuntimeWindow::SetSpeedLimited(bool limited)
{
    m_Implementation->speedAction->setChecked(limited);
}

void RuntimeWindow::SetStatus(const std::string & text)
{
    m_Implementation->window.statusBar()->showMessage(Text(text), 4000);
}
