#include "Interface.h"
#include <SDL2/SDL.h>
#include <system_error>

void RunGui()
{
	if (SDL_Init(SDL_INIT_EVERYTHING))
		throw std::runtime_error(SDL_GetError());

	auto window = SDL_CreateWindow("Astropress", 10, 10, 800, 600, SDL_WINDOW_RESIZABLE);
	if (!window)
		throw std::runtime_error(SDL_GetError());
	
	auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	if (!renderer)
		throw std::runtime_error(SDL_GetError());

	for (int i = 0;; i++)
	{
		if (SDL_SetRenderDrawColor(renderer, i % 255, i / 255 % 255, i / 255 / 255 % 255, 255))
			throw std::runtime_error(SDL_GetError());
		if (SDL_RenderDrawLine(renderer, 0, 0, 100, 100))
			throw std::runtime_error(SDL_GetError());
		SDL_RenderPresent(renderer);
		SDL_PumpEvents();
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			
		}
		const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
		if (keyboard[SDL_SCANCODE_ESCAPE])
			break;
	}

	SDL_DestroyWindow(window);
	SDL_Quit();
}
