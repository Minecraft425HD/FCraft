#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <random>
#include <chrono>
#include <iostream>
#include <sstream>

// ---------------------------------------------------------------------------
// WorldSettings -- mirrors Minecraft's world-creation / options screen
// ---------------------------------------------------------------------------
struct WorldSettings
{
	std::string worldName      = "New World";
	uint32_t    seed           = 0;    // 0 means "generate randomly"
	int         renderDistance = 8;    // chunks in each direction
	float       mouseSensitivity = 1.0f;
	bool        fullscreen     = false;
	int         fovDegrees     = 70;   // Minecraft default
};

// ---------------------------------------------------------------------------
// MenuState
// ---------------------------------------------------------------------------
enum class MenuState
{
	MAIN,         // title screen: Spielen / Optionen / Beenden
	WORLD_CREATE, // enter world name + seed
	OPTIONS,      // render distance, sensitivity, FOV, fullscreen
	IN_GAME,      // no menu -- game is running
	PAUSE         // ESC pressed while in-game
};

// ---------------------------------------------------------------------------
// MainMenu
// ---------------------------------------------------------------------------
class MainMenu
{
public:
	MenuState    state    = MenuState::MAIN;
	WorldSettings settings;

	// ------------------------------------------------------------------
	// Query helpers
	// ------------------------------------------------------------------
	bool isInGame() const { return state == MenuState::IN_GAME; }
	WorldSettings& getSettings() { return settings; }

	// ------------------------------------------------------------------
	// Fill settings.seed with a random value (system-clock seeded mt19937)
	// ------------------------------------------------------------------
	void generateRandomSeed()
	{
		auto now = std::chrono::system_clock::now().time_since_epoch().count();
		std::mt19937 rng(static_cast<uint32_t>(now));
		settings.seed = rng();
	}

	// ------------------------------------------------------------------
	// render() -- called every frame while !isInGame().
	// Implemented as a simple console-driven keyboard UI rendered to
	// stdout.  No external GUI library required.
	// The function is non-blocking: it only redraws + reacts when a key
	// state changes (edge-detected via prevKeys).
	// ------------------------------------------------------------------
	void render(GLFWwindow* window)
	{
		switch (state)
		{
			case MenuState::MAIN:         renderMain(window);        break;
			case MenuState::WORLD_CREATE: renderWorldCreate(window); break;
			case MenuState::OPTIONS:      renderOptions(window);     break;
			case MenuState::PAUSE:        renderPause(window);       break;
			default: break;
		}
	}

private:
	// ---- edge-detection helpers ----------------------------------------
	bool prevUp    = false;
	bool prevDown  = false;
	bool prevEnter = false;
	bool prevEsc   = false;
	bool prevLeft  = false;
	bool prevRight = false;

	// Which menu item is highlighted in the current screen
	int cursor = 0;

	// Simple string input buffer for text fields
	std::string inputBuffer;
	bool editingName = false;
	bool editingSeed = false;

	// ---- key edge helpers ----------------------------------------------
	bool edgeUp(GLFWwindow* w)
	{
		bool cur = glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS;
		bool edge = cur && !prevUp;
		prevUp = cur;
		return edge;
	}
	bool edgeDown(GLFWwindow* w)
	{
		bool cur = glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS;
		bool edge = cur && !prevDown;
		prevDown = cur;
		return edge;
	}
	bool edgeEnter(GLFWwindow* w)
	{
		bool cur = glfwGetKey(w, GLFW_KEY_ENTER) == GLFW_PRESS;
		bool edge = cur && !prevEnter;
		prevEnter = cur;
		return edge;
	}
	bool edgeEsc(GLFWwindow* w)
	{
		bool cur = glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS;
		bool edge = cur && !prevEsc;
		prevEsc = cur;
		return edge;
	}
	bool edgeLeft(GLFWwindow* w)
	{
		bool cur = glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS;
		bool edge = cur && !prevLeft;
		prevLeft = cur;
		return edge;
	}
	bool edgeRight(GLFWwindow* w)
	{
		bool cur = glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS;
		bool edge = cur && !prevRight;
		prevRight = cur;
		return edge;
	}

	// ---- MAIN screen ---------------------------------------------------
	// Items: 0=Spielen  1=Optionen  2=Beenden
	void renderMain(GLFWwindow* w)
	{
		const int ITEMS = 3;
		if (edgeDown(w)) cursor = (cursor + 1) % ITEMS;
		if (edgeUp(w))   cursor = (cursor - 1 + ITEMS) % ITEMS;

		if (edgeEnter(w))
		{
			switch (cursor)
			{
				case 0: // Spielen -> Welt erstellen
					cursor = 0;
					state  = MenuState::WORLD_CREATE;
					break;
				case 1: // Optionen
					cursor = 0;
					state  = MenuState::OPTIONS;
					break;
				case 2: // Beenden
					glfwSetWindowShouldClose(w, GLFW_TRUE);
					break;
			}
		}

		printScreen(
			"=== FCraft ===\n"
			"[Pfeil-Tasten navigieren, Enter bestaetigen]\n\n",
			{"Spielen", "Optionen", "Beenden"}, cursor);
	}

	// ---- WORLD_CREATE screen -------------------------------------------
	// Items: 0=Weltname  1=Seed  2=Starten  3=Zurueck
	void renderWorldCreate(GLFWwindow* w)
	{
		const int ITEMS = 4;
		if (edgeDown(w)) { cursor = (cursor + 1) % ITEMS; inputBuffer.clear(); editingName = false; editingSeed = false; }
		if (edgeUp(w))   { cursor = (cursor - 1 + ITEMS) % ITEMS; inputBuffer.clear(); editingName = false; editingSeed = false; }

		if (edgeEnter(w))
		{
			switch (cursor)
			{
				case 0: // Weltname bearbeiten
					editingName = !editingName;
					editingSeed = false;
					if (editingName) inputBuffer = settings.worldName;
					else             settings.worldName = inputBuffer.empty() ? "New World" : inputBuffer;
					break;
				case 1: // Seed bearbeiten
					editingSeed = !editingSeed;
					editingName = false;
					if (editingSeed) inputBuffer = settings.seed ? std::to_string(settings.seed) : "";
					else
					{
						if (inputBuffer.empty())
							generateRandomSeed();
						else
						{
							try { settings.seed = static_cast<uint32_t>(std::stoul(inputBuffer)); }
							catch (...) { generateRandomSeed(); }
						}
					}
					break;
				case 2: // Starten
					if (settings.seed == 0) generateRandomSeed();
					state = MenuState::IN_GAME;
					break;
				case 3: // Zurueck
					cursor = 0;
					state  = MenuState::MAIN;
					break;
			}
		}

		std::ostringstream oss;
		oss << "=== Neue Welt ===\n"
			<< "[Pfeil-Tasten, Enter zum Bearbeiten/Bestaetigen]\n\n"
			<< (cursor==0 ? "> " : "  ") << "Weltname  : " << (editingName ? "[" + inputBuffer + "_]" : settings.worldName) << "\n"
			<< (cursor==1 ? "> " : "  ") << "Seed      : " << (editingSeed ? "[" + inputBuffer + "_]" : (settings.seed ? std::to_string(settings.seed) : "(zufaellig)")) << "\n"
			<< (cursor==2 ? "> " : "  ") << "Welt starten\n"
			<< (cursor==3 ? "> " : "  ") << "Zurueck\n";
		std::cout << "\033[2J\033[H" << oss.str() << std::flush;
	}

	// ---- OPTIONS screen ------------------------------------------------
	// Items: 0=RenderDist  1=Sensitivity  2=FOV  3=Fullscreen  4=Zurueck
	void renderOptions(GLFWwindow* w)
	{
		const int ITEMS = 5;
		if (edgeDown(w)) cursor = (cursor + 1) % ITEMS;
		if (edgeUp(w))   cursor = (cursor - 1 + ITEMS) % ITEMS;

		if (edgeLeft(w))
		{
			switch (cursor)
			{
				case 0: settings.renderDistance    = std::max(1,  settings.renderDistance - 1); break;
				case 1: settings.mouseSensitivity  = std::max(0.1f, settings.mouseSensitivity - 0.1f); break;
				case 2: settings.fovDegrees        = std::max(60, settings.fovDegrees - 5); break;
				case 3: settings.fullscreen        = !settings.fullscreen; break;
			}
		}
		if (edgeRight(w))
		{
			switch (cursor)
			{
				case 0: settings.renderDistance    = std::min(32, settings.renderDistance + 1); break;
				case 1: settings.mouseSensitivity  = std::min(5.0f, settings.mouseSensitivity + 0.1f); break;
				case 2: settings.fovDegrees        = std::min(110, settings.fovDegrees + 5); break;
				case 3: settings.fullscreen        = !settings.fullscreen; break;
			}
		}

		if (edgeEnter(w) && cursor == 4)
		{
			cursor = 0;
			state  = (state == MenuState::OPTIONS) ? MenuState::MAIN : MenuState::PAUSE;
			// If we came from PAUSE we must go back there, but since we always
			// come via PAUSE->OPTIONS or MAIN->OPTIONS, just go MAIN for now.
			state = MenuState::MAIN;
		}

		std::ostringstream oss;
		oss << "=== Optionen ===\n"
			<< "[Pfeil hoch/runter = navigieren, links/rechts = Wert aendern]\n\n"
			<< (cursor==0 ? "> " : "  ") << "Sichtweite    : " << settings.renderDistance << " Chunks  (1-32)\n"
			<< (cursor==1 ? "> " : "  ") << "Maussensitiv. : " << settings.mouseSensitivity << "  (0.1-5.0)\n"
			<< (cursor==2 ? "> " : "  ") << "Sichtfeld (FOV): " << settings.fovDegrees << "  (60-110)\n"
			<< (cursor==3 ? "> " : "  ") << "Vollbild       : " << (settings.fullscreen ? "An" : "Aus") << "\n"
			<< (cursor==4 ? "> " : "  ") << "Zurueck\n";
		std::cout << "\033[2J\033[H" << oss.str() << std::flush;
	}

	// ---- PAUSE screen --------------------------------------------------
	// Items: 0=Weiterspielen  1=Optionen  2=Hauptmenue
	void renderPause(GLFWwindow* w)
	{
		const int ITEMS = 3;
		if (edgeDown(w)) cursor = (cursor + 1) % ITEMS;
		if (edgeUp(w))   cursor = (cursor - 1 + ITEMS) % ITEMS;

		if (edgeEnter(w))
		{
			switch (cursor)
			{
				case 0: state = MenuState::IN_GAME; break;
				case 1: cursor = 0; state = MenuState::OPTIONS; break;
				case 2: cursor = 0; state = MenuState::MAIN;    break;
			}
		}

		printScreen(
			"=== Pause ===\n"
			"[Pfeil-Tasten navigieren, Enter bestaetigen]\n\n",
			{"Weiterspielen", "Optionen", "Hauptmenue"}, cursor);
	}

	// ---- generic list printer ------------------------------------------
	void printScreen(const std::string& header,
	                 std::initializer_list<const char*> items,
	                 int selected)
	{
		std::ostringstream oss;
		oss << header;
		int i = 0;
		for (auto item : items)
			oss << (i++ == selected ? "> " : "  ") << item << "\n";
		std::cout << "\033[2J\033[H" << oss.str() << std::flush;
	}
};
