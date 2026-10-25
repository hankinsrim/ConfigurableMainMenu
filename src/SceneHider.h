#pragma once

// Hides parts of the Mist Menu's 3D scene by setting AppCulled and restores the original flags.
namespace SceneHider
{
	// Call once per frame from the Mist Menu; a_hide false restores everything culled, a_launching marks the loading screen before the main menu.
	void Update(RE::MistMenu* a_menu, bool a_hide, bool a_launching);

	void RestoreAll();

	// False while we hid every mesh in the menu scene, because drawing the loaded logo fully culled crashes the renderer.
	bool ShouldDraw();
}
