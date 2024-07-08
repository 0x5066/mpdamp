#include "mpdamp.h"

char time_str[6]; // buffer to hold the formatted string "MM:SS"
char time_str2[6]; // buffer to hold the formatted string "MM:SS"

const char* total_duration_str;

// Complete character set including special characters
const char *charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\"@   0123456789….:()-'!_+\\/[]^&%,=$#ÂÖÄ?* ";

// Mapping indices for each character in the charset
const int charset_indices[] = {
    0, // A
    1, // B
    2, // C
    3, // D
    4, // E
    5, // F
    6, // G
    7, // H
    8, // I
    9, // J
    10, // K
    11, // L
    12, // M
    13, // N
    14, // O
    15, // P
    16, // Q
    17, // R
    18, // S
    19, // T
    20, // U
    21, // V
    22, // W
    23, // X
    24, // Y
    25, // Z
    26, // "
    27, // @
    67, // BLANK
    67, // BLANK
    67, // BLANK
    31, // 0
    32, // 1
    33, // 2
    34, // 3
    35, // 4
    36, // 5
    37, // 6
    38, // 7
    39, // 8
    40, // 9
    41, // …
    42, // .
    43, // :
    44, // (
    43, // )
    46, // -
    47, // '
    46, // !
    49, // _
    50, // +
    51, // backslash
    52, // slash
    53, // [
    52, // ]
    55, // ^
    56, // &
    57, // %
    58, // ,
    59, // =
    60, // $
    61, // #
    62, // Â
    63, // Ö
    64, // Ä
    65, // ?
    66, // *
    67  // BLANK (for remaining undefined characters)
    };

#define CHARF_WIDTH 5
#define CHARF_HEIGHT 6
#define FONT_IMAGE_WIDTH 155

SDL_Texture *pltime_texture = NULL;
SDL_Texture *pltotaltime_texture = NULL;
SDL_Surface *text_surface = NULL;
SDL_Texture *text_texture = NULL;
SDL_Surface *pledit_surface = NULL;
SDL_Texture *pledit = NULL;

bool hasfocus2;
int PL_ResizeX, PL_ResizeY;
int total_seconds;

void load_textbmp(SDL_Renderer *renderer){
    pltime_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 32, 6);
    SDL_SetTextureBlendMode(pltime_texture, SDL_BLENDMODE_BLEND);

    pltotaltime_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 90, 6);
    SDL_SetTextureBlendMode(pltotaltime_texture, SDL_BLENDMODE_BLEND);

    text_surface = IMG_Load("text.bmp");
    text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
    SDL_FreeSurface(text_surface);

    pledit_surface = IMG_Load("pledit.bmp");
    pledit = SDL_CreateTextureFromSurface(renderer, pledit_surface);
    SDL_FreeSurface(pledit_surface);
}

void draw_pltime(SDL_Renderer *renderer, SDL_Texture *font_texture, SDL_Texture *texture, const char *text, int x, int y) {
    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color (R, G, B, A)
    SDL_RenderClear(renderer);

    const char *numbers = nullptr;

    if (res == 3) {
        if (blinking == 0) {
            numbers = "      ";
        } else if (blinking >= 60) {
            numbers = text;
        }
    } else if (res == 1) {
        numbers = text;
    }

    if (numbers != nullptr && (res == 1 || res == 3)) {
        for (const char *c = numbers; *c != '\0'; ++c) {
            char upper_char = std::toupper(*c);  // Convert character to uppercase

            // Skip rendering but add padding for ':'
            if (upper_char == ':') {
                x += 3; // Adjust padding as needed
                continue;
            }

            // Find the character in the charset and get its index
            const char *char_pos = strchr(charset, upper_char);
            int index;
            if (char_pos) {
                index = char_pos - charset;
            } else {
                // If character is not found, use the index for space (or blank)
                index = 67;  // Assuming the last index (67) is a space or blank
            }

            int col = charset_indices[index] % (FONT_IMAGE_WIDTH / CHARF_WIDTH);
            int row = charset_indices[index] / (FONT_IMAGE_WIDTH / CHARF_WIDTH);

            // Debug output
            // std::cout << "Character: " << upper_char 
            //           << ", Index: " << index 
            //           << ", Col: " << col 
            //           << ", Row: " << row 
            //           << std::endl;

            SDL_Rect src_rect = { col * CHARF_WIDTH, row * CHARF_HEIGHT, CHARF_WIDTH, CHARF_HEIGHT };
            SDL_Rect dst_rect = { x, y, CHARF_WIDTH, CHARF_HEIGHT };
            SDL_RenderCopy(renderer, font_texture, &src_rect, &dst_rect);
            x += CHARF_WIDTH;
        }
    }

    SDL_SetRenderTarget(renderer, NULL);
}

void draw_pltotaltime(SDL_Renderer *renderer, SDL_Texture *font_texture, SDL_Texture *texture, const char *text, int x, int y) {
    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color (R, G, B, A)
    SDL_RenderClear(renderer);

    for (const char *c = text; *c != '\0'; ++c) {
        char upper_char = std::toupper(*c);  // Convert character to uppercase

        // Find the character in the charset and get its index
        const char *char_pos = strchr(charset, upper_char);
        int index;
        if (char_pos) {
            index = char_pos - charset;
        } else {
            // If character is not found, use the index for space (or blank)
            index = 67;  // Assuming the last index (67) is a space or blank
        }

        int col = charset_indices[index] % (FONT_IMAGE_WIDTH / CHARF_WIDTH);
        int row = charset_indices[index] / (FONT_IMAGE_WIDTH / CHARF_WIDTH);

        // Debug output
        // std::cout << "Character: " << upper_char 
        //           << ", Index: " << index 
        //           << ", Col: " << col 
        //           << ", Row: " << row 
        //           << std::endl;

        SDL_Rect src_rect = { col * CHARF_WIDTH, row * CHARF_HEIGHT, CHARF_WIDTH, CHARF_HEIGHT };
        SDL_Rect dst_rect = { x, y, CHARF_WIDTH, CHARF_HEIGHT };
        SDL_RenderCopy(renderer, font_texture, &src_rect, &dst_rect);
        x += CHARF_WIDTH;
    }

    SDL_SetRenderTarget(renderer, NULL);
}

const char* combine_duration(float total_time, const char* total_duration_str) {
    static char combined_str[32];
    if (res == 1 || res == 3){
        total_seconds = static_cast<int>(total_time);
    }
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    sprintf(combined_str, "%d:%02d/%s", minutes, seconds, total_duration_str);
    return combined_str;
}

void draw_playlisteditor(SDL_Window *Window, SDL_Renderer *renderer) {
    SDL_RenderClear(renderer);

    draw_pltime(renderer, text_texture, pltime_texture, time_str, 4, 0);
    draw_pltotaltime(renderer, text_texture, pltotaltime_texture, combine_duration(total_time, total_duration_str), 0, 0);

    //std::cout << time_str << std::endl;

    hasfocus2 = HasWindowFocus(Window);
    int f = 0;

    if (!hasfocus2){
        f = 21;
    } else {
        f = 0;
    }

    int Width, Height;
    SDL_GetWindowSize(Window, &Width, &Height);

    PL_ResizeX = Width - 19;
    PL_ResizeY = Height - 19;

    int bottom_y = Height - 38;

    SDL_Rect src_rect_pledit_topleft = {0, f, 25, 20};
    SDL_Rect src_rect_pledit_topstretchybit = {127, f, 25, 20};
    SDL_Rect src_rect_pledit_title = {26, f, 100, 20};
    SDL_Rect src_rect_pledit_topright = {153, f, 25, 20};

    SDL_Rect src_rect_pledit_leftstretchybit = {0, 42, 25, 29};
    SDL_Rect src_rect_pledit_bottomleft = {0, 72, 125, 38};
    SDL_Rect src_rect_pledit_bottomright = {126, 72, 150, 38};
    SDL_Rect src_rect_pledit_rightstretchybit = {26, 42, 25, 29};

    SDL_Rect src_rect_pledit_bottomstretchybit = {179, 0, 25, 38};

    SDL_Rect src_rect_pledit_time = {0, 0, 32, 6};
    SDL_Rect src_rect_pledit_ttime = {0, 0, 90, 6};

    SDL_Rect dst_rect_pledit_topleft = {0, 0, 25, 20};
    
    // Center the title rectangle horizontally
    int title_x = (Width - 100) / 2;
    SDL_Rect dst_rect_pledit_title = {title_x, 0, 100, 20};
    
    SDL_Rect dst_rect_pledit_topright = {-25 + Width, 0, 25, 20};
    SDL_Rect dst_rect_pledit_leftstretchybit = {0, 20, 25, 29};
    SDL_Rect dst_rect_pledit_bottomleft = {0, bottom_y, 125, 38};
    SDL_Rect dst_rect_pledit_bottomright = {Width - 150, bottom_y, 150, 38};
    SDL_Rect dst_rect_pledit_rightstretchybit = {Width - 25, 20, 25, 29};

    SDL_Rect dst_rect_pledit_time = {Width - 86, Height - 15, 32, 6};
    SDL_Rect dst_rect_pledit_ttime = {Width - 143, Height - 28, 90, 6};

    // Tile the top stretchy bits around the title
    int top_stretchy_width = 25;
    int num_tiles_left = (title_x - 25) / top_stretchy_width;
    int num_tiles_right = (Width - (title_x + 100) - 25) / top_stretchy_width;

    // Render left tiles
    for (int i = 0; i <= num_tiles_left; ++i) {
        SDL_Rect dst_rect_pledit_topstretchybit = {25 + i * top_stretchy_width, 0, top_stretchy_width, 20};
        SDL_RenderCopy(renderer, pledit, &src_rect_pledit_topstretchybit, &dst_rect_pledit_topstretchybit);
    }

    // Render right tiles
    for (int i = 0; i <= num_tiles_right; ++i) {
        SDL_Rect dst_rect_pledit_topstretchybit2 = {title_x + 100 + i * top_stretchy_width, 0, top_stretchy_width, 20};
        SDL_RenderCopy(renderer, pledit, &src_rect_pledit_topstretchybit, &dst_rect_pledit_topstretchybit2);
    }

    // Tile the left stretchy bits around the bottom left and right rectangles
    int left_stretchy_height = 29;
    int num_tiles_top = (bottom_y - 20) / left_stretchy_height;

    // Render top tiles on the left
    for (int i = 0; i <= num_tiles_top; ++i) {
        SDL_Rect dst_rect_pledit_leftstretchybit = {0, 20 + i * left_stretchy_height, 25, left_stretchy_height};
        SDL_RenderCopy(renderer, pledit, &src_rect_pledit_leftstretchybit, &dst_rect_pledit_leftstretchybit);
    }

    // Render top tiles on the right
    for (int i = 0; i <= num_tiles_top; ++i) {
        SDL_Rect dst_rect_pledit_rightstretchybit = {Width - 25, 20 + i * left_stretchy_height, 25, left_stretchy_height};
        SDL_RenderCopy(renderer, pledit, &src_rect_pledit_rightstretchybit, &dst_rect_pledit_rightstretchybit);
    }

    // Calculate the horizontal tiling of the bottom stretchy bits
    int bottom_stretchy_width = 25;
    int space_between = Width - 150 - 125;
    int num_bottom_tiles = space_between / bottom_stretchy_width;

    // Render bottom stretchy bits between bottom left and bottom right
    for (int i = 0; i <= num_bottom_tiles; ++i) {
        SDL_Rect dst_rect_pledit_bottomstretchybit = {125 + i * bottom_stretchy_width, bottom_y, bottom_stretchy_width, 38};
        SDL_RenderCopy(renderer, pledit, &src_rect_pledit_bottomstretchybit, &dst_rect_pledit_bottomstretchybit);
    }

    SDL_RenderCopy(renderer, pledit, &src_rect_pledit_topleft, &dst_rect_pledit_topleft);
    SDL_RenderCopy(renderer, pledit, &src_rect_pledit_title, &dst_rect_pledit_title);
    SDL_RenderCopy(renderer, pledit, &src_rect_pledit_topright, &dst_rect_pledit_topright);
    SDL_RenderCopy(renderer, pledit, &src_rect_pledit_bottomleft, &dst_rect_pledit_bottomleft);
    SDL_RenderCopy(renderer, pledit, &src_rect_pledit_bottomright, &dst_rect_pledit_bottomright);

    SDL_RenderCopy(renderer, pltime_texture, &src_rect_pledit_time, &dst_rect_pledit_time);
    SDL_RenderCopy(renderer, pltotaltime_texture, &src_rect_pledit_ttime, &dst_rect_pledit_ttime);

    SDL_RenderPresent(renderer);
}
