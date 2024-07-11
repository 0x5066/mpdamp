#ifndef MPDAMP_H
#define MPDAMP_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_syswm.h>  // Include for SDL_SysWMinfo
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <mpd/client.h>
#include <mpd/stats.h>
#include <fftw3.h>
#include <math.h>
#include <iostream>
#include <string>
#include <sstream>
#include <codecvt>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <vector>

extern bool hasfocus;
extern bool hasfocus2;

extern int PL_ResizeX;
extern int PL_ResizeY;
extern int res;
extern int blinking;
extern float total_time;

extern const char* total_duration_str;

extern std::vector<std::string> songs;
extern std::string song_title;

void draw_text(SDL_Renderer *renderer, SDL_Texture *font_texture, SDL_Texture *texture, const wchar_t *text, int x, int y);
void draw_playlisteditor(SDL_Window *Window, SDL_Renderer *renderer);
void init_tex(SDL_Renderer *renderer, SDL_Renderer *renderer2, int WIDTH, int HEIGHT, int mult);
void load_textbmp(SDL_Renderer *renderer);
void draw_pltime(SDL_Renderer *renderer, SDL_Texture *font_texture, SDL_Texture *texture, const wchar_t *text, int x, int y);
bool HasWindowFocus(SDL_Window* window);
void retrieve_songs(struct mpd_connection *conn, std::vector<std::string> &songs);
void render_text(SDL_Renderer *renderer, SDL_Texture *texture, TTF_Font *font, const char *text, int x, int y, SDL_Color color);

extern SDL_Texture *master_texture;
extern SDL_Texture *vis_texture;
extern SDL_Texture *spec_texture;
extern SDL_Texture *num_texture;
extern SDL_Texture *text_texture;
extern SDL_Texture *bitratenum_texture;
extern SDL_Texture *khznum_texture;
extern SDL_Texture *progress_texture;
extern SDL_Texture *titleb_texture;
extern SDL_Texture *clutt_texture;
extern SDL_Texture *mons_texture;
extern SDL_Texture *cbutt_texture;
extern SDL_Texture *vol_texture;
extern SDL_Texture *shuf_texture;

extern TTF_Font *font;

extern char time_str[6];
extern char time_str2[6];

#endif