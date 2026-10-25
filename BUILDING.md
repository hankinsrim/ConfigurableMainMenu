# Building Configurable Main Menu

Requirements: Visual Studio 2022 or newer with *Desktop development with C++* (includes CMake,
Ninja and vcpkg), and Git.

## Building the plugin

```
git clone --recursive https://github.com/hankinsrim/ConfigurableMainMenu.git
cd ConfigurableMainMenu
powershell -ExecutionPolicy Bypass -File .\build.ps1
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Zip
```

* `build.ps1` enters the VS developer environment, uses `%VCPKG_ROOT%` if set (otherwise VS's
  bundled vcpkg), configures with the `release` preset and builds. The first build compiles
  the vcpkg dependencies and CommonLibSSE-NG, which takes a while.
* `-Zip` also writes `build\ConfigurableMainMenu-<major.minor>.zip`, a minimal FOMOD whose
  `fomod\info.xml` carries the name and version for mod managers that read it.
* `-Preset debug` builds the debug configuration.
* `-Lint` compiles the plugin with MSVC Code Analysis (`/analyze`) and then runs VS's
  `clang-tidy.exe` over `src\` with the checks in `.clang-tidy`. The next build without `-Lint`
  switches the analysis off again.
* Manual equivalent from a *Developer PowerShell for VS*: `cmake --preset release` then
  `cmake --build --preset release`.

The mod-manager-ready output is staged in `dist\`:

```
dist\
└── SKSE\
    └── Plugins\
        ├── MainMenuToggles.dll
        ├── MainMenuToggles.ini
        └── MainMenuToggles.pdb
```

Sources are formatted with `.clang-format` (VS ships `clang-format.exe` under
`VC\Tools\Llvm\x64\bin`).

## How it works

* Hooks: `AdvanceMovie` (vtable slot 5, via Address Library) of the Main Menu, the Mist Menu
  and the LoadWaitSpinner, plus the Mist Menu's `PostDisplay` (slot 6) right before it draws.
  Each applies its state every frame, so hiding survives the SWF re-showing things.
* 3D: The background belongs to the Mist Menu. The scene root is
  `UI3DSceneManager::shadowSceneNode` (available from the first launch frame) plus the roots of
  its model pointers, all validated against known vtables under SEH. The smoke model's particle
  systems (`NiParticleSystem "PArray01"` in `meshes\interface\intmenufogparticles.nif`) and the
  children of every `Logo` node are culled, and while every mesh in the scene is culled the
  Mist Menu's draw is skipped, because rendering the loaded logo fully culled crashes the game.
  Original `AppCulled` values are restored exactly.
* Dragon models: The main menu loads its logo through two
  `lea rcx, "interface/logo/logo01ae.nif"` instructions (AE IDs 52135 +0x12 and 416420 +0x1B),
  which are pointed at a copy of the chosen path in trampoline memory. The launch screen reads
  its path from a `BSFixedString` (AE ID 406372), which is reassigned. Both are verified
  against the vanilla bytes and string first and skipped with a warning otherwise.
* UI: `_visible` (not `_alpha`, which detaches clips from their tweens) through
  `GFxValue::SetMember`. `VersionText` is restored with the SWF's own rule, everything else to
  the value seen before hiding. The news panel is hidden through its two text fields, because
  the game's `SetMotd` call re-shows `MessageOfTheDay_mc` itself.
* Events: `MenuOpenCloseEvent` ("Main Menu") ends the launch phase, turns the logic on and off
  and restores on close; a `BSInputDeviceManager` sink handles the hotkey.
