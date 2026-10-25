#pragma once

// Hides display objects in startmenu.swf and loadwaitspinner.swf by setting _visible=false.
namespace UIHider
{
	// Call once per frame from the Main Menu after its movie advanced.
	void Update(RE::GFxMovieView* a_movie, bool a_hide);

	// Call once per frame from the Main Menu to filter and reorder its buttons by MenuButtons.
	void UpdateMenuButtons(RE::GFxMovieView* a_movie);

	// Call once per frame from the LoadWaitSpinner menu.
	void UpdateSpinner(RE::GFxMovieView* a_movie, bool a_hide);

	// Main Menu closed: restore the cursor and forget per-movie state.
	void Reset();
}
