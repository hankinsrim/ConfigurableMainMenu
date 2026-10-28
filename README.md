# Configurable Main Menu

An SKSE plugin for Skyrim Special Edition / Anniversary Edition that lets you choose, via INI
file, which parts of the main menu are visible. This mod was developed so I can record just the
smoke and intro music for a video of mine.

No files are replaced. 3D nodes are hidden with the `AppCulled` flag and Flash clips with
`_visible = false` at runtime, and everything is restored when the main menu closes. Menu
replacers are tolerated: anything that can't be found is skipped and logged.

Supported runtimes: 1.5.x, 1.6.x and 1.7.x in one DLL via CommonLibSSE-NG and Address Library,
built and checked against 1.7.104.

This mod has had zero impact on load times. BUT, I have my loading/menu locked to 60fps, as I
would recommend to anyone for load time reasons. I could see a small chance at a sub-second
impact to load time above 60fps, but just don't do that :).

This code was written with LLM assistance.

Requirements: [SKSE64](https://skse.silverlock.org/) and
[Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444).

## Notable Files

* Config: `SKSE\Plugins\MainMenuToggles.ini`
* Log: `Documents\My Games\Skyrim Special Edition\SKSE\MainMenuToggles.log`

## INI options

All `Show*` options default to `true` (vanilla), and `false` hides the element. With
`HideOnStartup=true` elements are hidden from game launch (the loading screen before the main
menu) and every time the main menu opens, and `ToggleKey` switches between hidden and vanilla
while the main menu is open.

* **`[General]`**
  * `HideOnStartup` (`true`): hide from game launch and on each main menu visit. With `false`
    (and a `ToggleKey`) you start vanilla and toggle in.
  * `ToggleKey` (`0`): DirectInput scan code, decimal or `0x..`, that toggles hiding while the
    main menu is open. `0` = off, F11=87, F12=88, Home=199, Insert=210.
  * `EnableLogging` (`true`): write `MainMenuToggles.log` with the loaded settings, warnings
    and errors. `false` writes no log file.
* **`[Visibility]`**
  * `MenuButtons` (empty): comma-separated buttons to keep, in display order, from `Continue`,
    `New`, `Load`, `Creations`, `Credits` and `Quit`, for example
    `MenuButtons=Continue, New, Load, Quit`. Buttons the game doesn't offer (such as `Continue`
    without saves) are skipped. Empty keeps the vanilla list. Unlike the `Show*` options it is
    always applied, even when `ToggleKey` switches to the vanilla view.
  * `ShowMenuButtons`: `Menu_mc.MainListHolder`, the main button list with its selection arrow
    and the anniversary upsell banner, plus menu text that only shows briefly or rarely on PC:
    the "Press [START]" prompt (an unnamed clip at timeline depth 75), `VersionText`, the
    Bethesda.net login hints (`BottomButtons_mc`) and "Loading extra content."
    (`LoadingContentMessage`).
  * `ShowSmoke`: the rising smoke, `NiParticleSystem "PArray01"` in
    `meshes\interface\intmenufogparticles.nif`.
  * `ShowDragonMenuLogo`: the silver dragon on the main menu (`logo01ae.nif`).
  * `DragonMenuLogo` (empty): which dragon model the main menu loads, `Classic` (`logo.nif`)
    or `Anniversary` (`logo01ae.nif`, the vanilla choice).
  * `ShowDragonLoadingLogo`: the silver dragon on the loading screen before the main menu
    (`logo.nif`). Every `Logo` node first seen before the main menu opens counts as the
    loading logo. When both screens use the same model, the game keeps one dragon for both,
    so it follows `ShowDragonLoadingLogo` until the main menu opens and `ShowDragonMenuLogo`
    after.
  * `DragonLoadingLogo` (empty): which dragon model the loading screen loads, `Classic`
    (`logo.nif`, the vanilla choice) or `Anniversary` (`logo01ae.nif`). Both dragon model
    options need Skyrim 1.6 or newer.
  * `ShowLoadingSpinner`: the bottom-right spinner, `_root.Menu_mc` of
    `interface\loadwaitspinner.swf`, on the launch screen and main menu.
  * `ShowMouseCursor`: the mouse cursor, only while the main menu is open.
  * `ShowBethesdaLogo`: `Menu_mc.Logo_mc`, bottom left.
  * `ShowMOTD`: the "CREATION CLUB NEWS" panel, `Menu_mc.MessageOfTheDay_mc` (its `Header_tf`
    and `Motd_tf` text fields).

Unknown option names are reported as warnings in the log.

Hidden buttons keep their focus and input handling, so **keyboard navigation still works**
while they are invisible. The mouse cannot click buttons it can't see.

## License

Configurable Main Menu, Copyright (C) 2026 hankinsrim

This program is free software: you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
the GNU General Public License for more details (`LICENSE`, or
<https://www.gnu.org/licenses/gpl-3.0.html>).

Source: <https://github.com/hankinsrim/ConfigurableMainMenu>

It statically links [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng), which
is GPL-3.0-or-later with exceptions (see `extern/CommonLibSSE-NG/EXCEPTIONS.md`).
