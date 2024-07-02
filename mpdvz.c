#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h> // for read, close
#include <signal.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <mpd/client.h>
#include <fftw3.h>
#include <math.h>
#include <iostream>
#include <string>    // For std::string
#include <sstream>   // For std::wstringstream
#include <codecvt>   // For std::codecvt_utf8_utf16

#define M_PI 3.14159265358979323846

#define WIDTH 275
#define HEIGHT 116
#define BSZ 768

int VisMode = 0;
int res = 0;
int ampstatus, bitrate, sample_rate, volume;
bool repeat, shuffle;

const int mult = 2;

float total_time = 0;

bool isDragging = false;
int mouseX, mouseY;
unsigned int current_song_id = 0;

SDL_Texture *masterTexture = NULL;

FILE *ptr_file;
char shutdown = 0;

void handle_sigint(int signal) {
    shutdown = 1;
}

typedef struct {
    Uint8 r, g, b, a;
} Color;

int last_y = 0;
int top = 0, bottom = 0;

Color colors[] = {
    {0, 0, 0, 255},        // color 0 = black
    {24, 33, 41, 255},     // color 1 = grey for dots
    {239, 49, 16, 255},    // color 2 = top of spec
    {206, 41, 16, 255},    // 3
    {214, 90, 0, 255},     // 4
    {214, 102, 0, 255},    // 5
    {214, 115, 0, 255},    // 6
    {198, 123, 8, 255},    // 7
    {222, 165, 24, 255},   // 8
    {214, 181, 33, 255},   // 9
    {189, 222, 41, 255},   // 10
    {148, 222, 33, 255},   // 11
    {41, 206, 16, 255},    // 12
    {50, 190, 16, 255},    // 13
    {57, 181, 16, 255},    // 14
    {49, 156, 8, 255},     // 15
    {41, 148, 0, 255},     // 16
    {24, 132, 8, 255},     // 17 = bottom of spec
    {255, 255, 255, 255},  // 18 = osc 1
    {214, 214, 222, 255},  // 19 = osc 2 (slightly dimmer)
    {181, 189, 189, 255},  // 20 = osc 3
    {160, 170, 175, 255},  // 21 = osc 4
    {148, 156, 165, 255},  // 22 = osc 5
    {150, 150, 150, 255}   // 23 = analyzer peak dots
};

Color* osccolors(const Color* colors) {
    static Color osc_colors[16];

    osc_colors[0] = colors[21];
    osc_colors[1] = colors[21];
    osc_colors[2] = colors[20];
    osc_colors[3] = colors[20];
    osc_colors[4] = colors[19];
    osc_colors[5] = colors[19];
    osc_colors[6] = colors[18];
    osc_colors[7] = colors[18];
    osc_colors[8] = colors[19];
    osc_colors[9] = colors[19];
    osc_colors[10] = colors[20];
    osc_colors[11] = colors[20];
    osc_colors[12] = colors[21];
    osc_colors[13] = colors[21];
    osc_colors[14] = colors[22];
    osc_colors[15] = colors[22];

    return osc_colors;
}

void drawRect(SDL_Renderer *renderer, int x, int y, int zoom, Color color) {
    SDL_Rect rect = {x * zoom, y * zoom, zoom, zoom};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

void clearRenderer(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    for (int x = 0; x < 75; ++x) {
        for (int y = 0; y < 16; ++y) {
            if (x % 2 == 1 || y % 2 == 0) {
                drawRect(renderer, x, y, 1, colors[0]);
            }
            else {
                drawRect(renderer, x, y, 1, colors[1]);
            }
        }
    }
}

void render_to_master(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *target_rect) {
    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    // Render to texture here (e.g., render_waveform and render_spec functions)
    SDL_SetRenderTarget(renderer, NULL); // Reset render target
    SDL_RenderCopy(renderer, texture, NULL, target_rect); // Render master texture to window
    SDL_RenderPresent(renderer);
}

// C-weighting filter coefficients calculation
double c_weighting(double freq) {
    double f2 = freq * freq;
    // Adjust these constants to change the weighting curve
    double f2_plus_20_6_squared = f2 + 40.6 * 40.6;  // Low-frequency pole
    double f2_plus_12200_squared = f2 + 2200 * 2200;  // High-frequency pole

    // Original constant is 1.0072; lower it to reduce overall high-frequency gain
    double constant = 1.2;  // Adjust this constant to decrease high frequency gain
    
    double r = (f2 * (f2 + 1.94e5)) / (f2_plus_20_6_squared * f2_plus_12200_squared);
    return r * constant;
}

void render_vis(SDL_Renderer *renderer, SDL_Texture *texture, short *buffer, double *fft_data, int length, Color *osc_colors) {
    SDL_SetRenderTarget(renderer, texture);
    clearRenderer(renderer);

    double sample[BSZ];
    int bufferSize = length; // Assuming the length is passed as the size of buffer
    int targetSize = 75;
    int lower = 8192;
    
    // Calculate C-weighting for each FFT bin frequency
    double c_weighting_factors[bufferSize / 2];
    for (int i = 0; i < bufferSize / 2; ++i) {
        double freq = (double)i * 44100 / bufferSize;
        c_weighting_factors[i] = c_weighting(freq);
    }

    // Apply C-weighting to the FFT data
    double weighted_buffer[bufferSize / 2];
    for (int i = 0; i < bufferSize / 2; ++i) {
        weighted_buffer[i] = fft_data[i] * c_weighting_factors[i];
    }

    // Calculate the maximum frequency index (half the buffer size for real-valued FFT)
    int maxFreqIndex = bufferSize / 2;

    // Logarithmic scale factor
    double logMaxFreqIndex = log10(maxFreqIndex);
    double logMinFreqIndex = 0;  // log10(1) == 0
    
        for (int x = 0; x < 75; x++) {
            int buffer_index = (x * length) / 75;
            int y = (((buffer[buffer_index / 2] + 32768 + 1024) * 2) * 16 / 65536) - 9;

            y = y < 0 ? 0 : (y > 16 - 1 ? 16 - 1 : y);

            if (x == 0) {
                last_y = y;
            }

            top = y;
            bottom = last_y;
            last_y = y;

            if (bottom < top) {
                int temp = bottom;
                bottom = top;
                top = temp + 1;
            }

            int color_index = (top) % 16;
            Color scope_color = osc_colors[color_index];

            if (VisMode == 1){
                for (int dy = top; dy <= bottom; dy++) {
                    drawRect(renderer, x, dy, 1, scope_color);
                }
            }
        }

    for (int x = 0; x < targetSize; x++) {
        // Calculate the logarithmic index with intensity adjustment
        double logScaledIndex = logMinFreqIndex + (logMaxFreqIndex - logMinFreqIndex) * x / (targetSize - 1);
        double scaledIndex = pow(10, logScaledIndex / 1.0f);

        int index1 = (int)floor(scaledIndex);
        int index2 = (int)ceil(scaledIndex);

        if (index1 >= maxFreqIndex) {
            index1 = maxFreqIndex - 1;
        }
        if (index2 >= maxFreqIndex) {
            index2 = maxFreqIndex - 1;
        }

        if (index1 == index2) {
            // No interpolation needed, exact match
            sample[x] = weighted_buffer[index1] / lower;
        } else {
            // Perform linear interpolation
            double frac2 = scaledIndex - index1;
            double frac1 = 1.0 - frac2;
            sample[x] = (frac1 * weighted_buffer[index1] + frac2 * weighted_buffer[index2]) / lower;
        }
    }

    static int sapeaks[BSZ];
    static char safalloff[BSZ];
    int sadata2[BSZ];
    static float sadata3[BSZ];
    bool sa_thick = true;

        for (int x = 0; x < 75; x++) {
        
        // WHY?! WHY DO I HAVE TO DO THIS?! NOWHERE IS THIS IN THE DECOMPILE
        int i = ((i = x & 0xfffffffc) < 72) ? i : 71; // Limiting i to prevent out of bounds access
            if (sa_thick == true) {
                    // this used to be unoptimized and came straight out of ghidra
                    // here's what that looked like
                    /* uVar12 =  (int)((u_int)*(byte *)(i + 3 + sadata) +
                                    (u_int)*(byte *)(i + 2 + sadata) +
                                    (u_int)*(byte *)(i + 1 + sadata) +
                                    (u_int)*(byte *)(i + sadata)) >> 2; */

                    int uVar12 = (int)((sample[i+3] + sample[i+2] + sample[i+1] + sample[i]) / 4);

                    // shove the data from uVar12 into sadata2
                    sadata2[x] = uVar12;
            } else if (sa_thick == false) { // just copy sadata to sadata2
                sadata2[x] = (int)sample[x];
            }

        safalloff[x] = safalloff[x] - 32 / 16.0f;

        // okay this is really funny
        // somehow the internal vis data for winamp/wacup can just, wrap around
        // but here it didnt, until i saw my rect drawing *under* its intended area
        // and i just figured out that winamp's vis box just wraps that around
        // this is really funny to me
        if (sadata2[x] < 0) {
            sadata2[x] = sadata2[x] + 127;
        }
        if (sadata2[x] >= 15) {
            sadata2[x] = 15;
        }

        if (safalloff[x] <= sadata2[x]) {
            safalloff[x] = sadata2[x];
        }

    if (sapeaks[x] <= (int)(safalloff[x] * 256)) {
        sapeaks[x] = safalloff[x] * 256;
        sadata3[x] = 3.0f;
    }

    int intValue2 = -(sapeaks[x]/256) + 15;

    sapeaks[x] -= (int)sadata3[x];
    sadata3[x] *= 1.05f;
    if (sapeaks[x] < 0) 
    {
        sapeaks[x] = 0;
    }
    if (VisMode == 0){
        if ((x == i + 3 && x < 72) && sa_thick) {
            // skip rendering
            } else {
                for (int dy = -safalloff[x] + 16; dy <= 17; ++dy) {
                    int color_index = dy + 2; // Assuming dy starts from 0
                    Color scope_color = colors[color_index];
                    drawRect(renderer, x, dy, 1, scope_color); // Assuming drawRect function draws a single pixel
                }            
            }

        if ((x == i + 3 && x < 72) && sa_thick) {
            // skip rendering
            } else {
                if (intValue2 < 15) {
                    Color peaksColor = colors[23];
                    drawRect(renderer, x, intValue2, 1, peaksColor); // Assuming drawRect function draws a single pixel
                }
            }
        }
    }

    SDL_SetRenderTarget(renderer, NULL);
}

#define CHAR_WIDTH 9
#define CHAR_HEIGHT 13

void draw_numtext(SDL_Renderer *renderer, SDL_Texture *font_texture, SDL_Texture *texture, const char *text, int x, int y) {
    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color (R, G, B, A)
    SDL_RenderClear(renderer);

    if (res == 1 || res == 3){
        for (const char *c = text; *c != '\0'; ++c) {
            int char_index = -1;
            if (*c >= '0' && *c <= '9') {
                char_index = *c - '0';
            } else if (*c == '-') {
                char_index = 10;
            } else if (*c == ':') {
                char_index = 11;
            } else if (*c == ' ') {
                char_index = 11;
            }

            if (char_index != -1) {
                SDL_Rect src_rect = { char_index * CHAR_WIDTH, 0, CHAR_WIDTH, CHAR_HEIGHT };
                SDL_Rect dst_rect = { x, y, CHAR_WIDTH, CHAR_HEIGHT };
                SDL_RenderCopy(renderer, font_texture, &src_rect, &dst_rect);
                if (*c == ':'){
                    x -= 6;  // Increment x position for next character
                }
                x += CHAR_WIDTH + 3;  // Increment x position for next character
            }
        }
    }
    
    SDL_SetRenderTarget(renderer, NULL);
}

#define CHARF_WIDTH 5
#define CHARF_HEIGHT 6
#define FONT_IMAGE_WIDTH 155

void draw_text(SDL_Renderer *renderer, SDL_Texture *font_texture, SDL_Texture *texture, const wchar_t *text, int x, int y) {
    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color (R, G, B, A)
    SDL_RenderClear(renderer);

    // Complete character set including special characters
    const wchar_t *charset = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ\"@   0123456789….:()-'!_+\\/[]^&%,=$#ÂÖÄ?* ";

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
        43, // =
        44, // (
        45, // )
        46, // -
        47, // '
        48, // !
        49, // _
        50, // +
        51, // \
        52, // /
        53, // [
        54, // ]
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

    for (const wchar_t *c = text; *c != '\0'; ++c) {
        char upper_char = std::toupper(*c);  // Convert character to uppercase

        // Find the character in the charset and get its index
        const wchar_t *char_pos = wcschr(charset, upper_char);
        if (char_pos) {
            int index = char_pos - charset;
            int col = charset_indices[index] % (FONT_IMAGE_WIDTH / CHARF_WIDTH);
            int row = charset_indices[index] / (FONT_IMAGE_WIDTH / CHARF_WIDTH);

            // Debug output
            //printf("Character: %lc, Index: %d, Col: %d, Row: %d\n", upper_char, index, col, row);

            SDL_Rect src_rect = { col * CHARF_WIDTH, row * CHARF_HEIGHT, CHARF_WIDTH, CHARF_HEIGHT };
            SDL_Rect dst_rect = { x, y, CHARF_WIDTH, CHARF_HEIGHT };
            SDL_RenderCopy(renderer, font_texture, &src_rect, &dst_rect);
            x += CHARF_WIDTH;
        } else {
            // Handle characters not found in the charset (should not happen if charset is complete)
            continue;
        }
    }

    SDL_SetRenderTarget(renderer, NULL);
}

void draw_progress_bar(SDL_Renderer *renderer, SDL_Texture *progress_texture, SDL_Texture *posbar_texture, int seek_x) {
    SDL_SetRenderTarget(renderer, progress_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color (R, G, B, A)
    SDL_RenderClear(renderer);

    // Draw the posbar background
    SDL_Rect src_bg = { 0, 0, 248, 10 };
    SDL_Rect dst_bg = { 0, 0, 248, 10 };
    SDL_RenderCopy(renderer, posbar_texture, &src_bg, &dst_bg);

    if ((res == 1 && total_time != 0.000000) || (res == 3 && total_time != 0.000000)){
        // Draw the seek button
        SDL_Rect seek_src_rect;
        if (isDragging){
            seek_src_rect = { 278, 0, 29, 10 };
        } else {
            seek_src_rect = { 248, 0, 29, 10 };        
        }
        SDL_Rect seek_dst_rect = { seek_x, 0, 29, 10 };
        SDL_RenderCopy(renderer, posbar_texture, &seek_src_rect, &seek_dst_rect);
    }

    SDL_SetRenderTarget(renderer, NULL);
}

void draw_titlebar(SDL_Renderer *renderer, SDL_Texture *titleb_texture, SDL_Texture *titlebar_texture, bool focus) {
    SDL_SetRenderTarget(renderer, titleb_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color (R, G, B, A)
    SDL_RenderClear(renderer);

     // Draw the posbar background
    SDL_Rect src_bg;
    if (focus) {
        src_bg = { 27, 0, WIDTH, 14 };        
    } else {
        src_bg = { 27, 15, WIDTH, 14 }; 
    }
    SDL_Rect dst_bg = { 0, 0, WIDTH, 14 };
    SDL_RenderCopy(renderer, titlebar_texture, &src_bg, &dst_bg);

    SDL_SetRenderTarget(renderer, NULL);
}

void monoster(SDL_Renderer *renderer, SDL_Texture *mons_texture, SDL_Texture *monoster_texture, int channels) {
    SDL_SetRenderTarget(renderer, mons_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color (R, G, B, A)
    SDL_RenderClear(renderer);

    // Draw the posbar background
    SDL_Rect stereo, mono;
    if (channels == 2) {
        mono = { 29, 12, 29, 12 };
        stereo = { 0, 0, 29, 12 };        
    } else if (channels == 1) {
        mono = { 29, 0, 29, 12 };
        stereo = { 0, 12, 29, 12 }; 
    } else {
        mono = { 29, 12, 29, 12 };
        stereo = { 0, 12, 29, 12 }; 
    }
    SDL_Rect dst_stereo = { 27, 0, 29, 12 };
    SDL_Rect dst_mono = { 0, 0, 29, 12 };
    SDL_RenderCopy(renderer, monoster_texture, &mono, &dst_mono);
    SDL_RenderCopy(renderer, monoster_texture, &stereo, &dst_stereo);

    SDL_SetRenderTarget(renderer, NULL);
}

const int POSBAR_WIDTH = 248;
const int SEEK_BUTTON_WIDTH = 29;

void handleMouseEvent(SDL_Event &e, int &seek_x, float &elapsed_time, float total_time, mpd_connection *conn) {
    int posbar_x = 16 * mult;
    int posbar_y = 72 * mult;
    int posbar_w = 248 * mult;
    int posbar_h = 10 * mult;

    if ((res == 1 && total_time != 0.000000) || (res == 3 && total_time != 0.000000)){
        if (e.type == SDL_MOUSEBUTTONDOWN) {
            SDL_GetMouseState(&mouseX, &mouseY);
            if ((mouseX / mult) >= posbar_x && (mouseX / mult) <= posbar_x + posbar_w && mouseY >= posbar_y && mouseY <= posbar_y + posbar_h) {
                isDragging = true;
                seek_x = (mouseX / mult) - posbar_x - ((SEEK_BUTTON_WIDTH / mult) / 2);  // Adjust for button width
                if (seek_x < 0) seek_x = 0;
                if (seek_x > posbar_w - (SEEK_BUTTON_WIDTH / mult)) seek_x = posbar_w - (SEEK_BUTTON_WIDTH / mult);  // Adjust for button width
            }
        }

        if (e.type == SDL_MOUSEMOTION) {
            if (isDragging) {
                SDL_GetMouseState(&mouseX, &mouseY);
                seek_x = (mouseX / mult) - posbar_x - ((SEEK_BUTTON_WIDTH / mult) / 2);  // Adjust for button width
                if (seek_x < 0) seek_x = 0;
                if (seek_x > posbar_w - (SEEK_BUTTON_WIDTH / mult)) seek_x = posbar_w - (SEEK_BUTTON_WIDTH / mult);  // Adjust for button width
            }
        }

        if (e.type == SDL_MOUSEBUTTONUP) {
            if (isDragging) {
                isDragging = false;
                int pos = seek_x;
                float progress = static_cast<float>(pos) / (POSBAR_WIDTH - SEEK_BUTTON_WIDTH);
                elapsed_time = static_cast<int>(progress * total_time);
                // Send command to MPD to set the time
                if (mpd_send_seek_id(conn, current_song_id, elapsed_time)) {
                    mpd_response_finish(conn);
                }
            }
        }
    }
}

void updateSeekPosition(int &seek_x, float elapsed_time, float total_time) {
    if (!isDragging) {
        float progress = (total_time > 0) ? static_cast<float>(elapsed_time) / total_time : 0;
        seek_x = static_cast<int>(progress * (POSBAR_WIDTH - SEEK_BUTTON_WIDTH));
    }
}

unsigned int getCurrentSongID(mpd_connection *conn) {
    struct mpd_status *status = mpd_run_status(conn);
    unsigned int song_id = mpd_status_get_song_id(status);
    mpd_status_free(status);
    return song_id;
}

void send_mpd_command(struct mpd_connection *conn, const char *command) {
    if (strcmp(command, "previous") == 0) {
        mpd_run_previous(conn);
    } else if (strcmp(command, "play") == 0) {
        mpd_run_play(conn); // how do you restart playback?
    } else if (strcmp(command, "pause") == 0) {
        if (ampstatus == 3){
            mpd_run_pause(conn, false);
        } else {
            mpd_run_pause(conn, true);
        }

    } else if (strcmp(command, "stop") == 0) {
        mpd_run_stop(conn);
    } else if (strcmp(command, "next") == 0) {
        mpd_run_next(conn);
    }
    //std::cout << command << std::endl;
}

void cbuttons(SDL_Event &e, SDL_Renderer *renderer, SDL_Texture *cbutt_texture, SDL_Texture *cbuttons_texture, struct mpd_connection *conn) {
    // Set render target to cbutt_texture
    SDL_SetRenderTarget(renderer, cbutt_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color
    SDL_RenderClear(renderer);

    // Button source rectangles
    SDL_Rect buttons[5];
    for (int i = 0; i < 5; ++i) {
        buttons[i] = { 23 * i, 0, 23, 18 };
    }

    // Button destination rectangles
    SDL_Rect dst_buttons[5];
    for (int i = 0; i < 5; ++i) {
        dst_buttons[i] = { 23 * i, 0, 23, 18 };
    }

    // Offset for button area on the screen
    const int buttonAreaX = 16 * mult;
    const int buttonAreaY = 88 * mult;

    // Handle events
    if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
        int x = e.button.x - buttonAreaX;
        int y = e.button.y - buttonAreaY;

        if (e.type == SDL_MOUSEBUTTONDOWN) {
            //std::cout << "down" << std::endl;

            // Check which button was clicked
            for (int i = 0; i < 5; ++i) {
                if (x >= dst_buttons[i].x * mult && x < (dst_buttons[i].x + dst_buttons[i].w) * mult &&
                    y >= dst_buttons[i].y * mult && y < (dst_buttons[i].y + dst_buttons[i].h) * mult) {
                    // Offset the y-coordinate of the button source rectangle to show the clicked state
                    buttons[i].y = 18;
                    break;
                }
            }
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            //std::cout << "up" << std::endl;

            // Check which button was clicked
            for (int i = 0; i < 5; ++i) {
                if (x >= dst_buttons[i].x * mult && x < (dst_buttons[i].x + dst_buttons[i].w) * mult &&
                    y >= dst_buttons[i].y * mult && y < (dst_buttons[i].y + dst_buttons[i].h) * mult) {
                    // Reset the y-coordinate of the button source rectangle to the unclicked state
                    buttons[i].y = 0;

                    // Send the appropriate command to MPD
                    const char *commands[] = { "previous", "play", "pause", "stop", "next" };
                    send_mpd_command(conn, commands[i]);
                    break;
                }
            }
        }
    }

    // Render the buttons
    for (int i = 0; i < 5; ++i) {
        SDL_RenderCopy(renderer, cbuttons_texture, &buttons[i], &dst_buttons[i]);
    }

    // Reset render target
    SDL_SetRenderTarget(renderer, NULL);
}

void render_volume(SDL_Renderer *renderer, SDL_Texture *vol_texture, SDL_Texture *volume_texture/*, struct mpd_connection *conn*/) {
    // Set render target to cbutt_texture
    SDL_SetRenderTarget(renderer, vol_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color
    SDL_RenderClear(renderer);

    // Map volume (0-100) to bar index (0-28)
    int barIndex = volume * 27 / 100;
    int barbtnIndex = volume * (68 - 14) / 100;

    // Source rectangle for the volume bar
    SDL_Rect src_vol = { 0, barIndex * 15, 68, 15 };
    SDL_Rect src_vol_btn = { 15, 422, 14, 11 };

    // Destination rectangle for rendering (you can adjust the position as needed)
    SDL_Rect dst_vol = { 0, 0, 68, 15 };
    SDL_Rect dst_vol_btn = { barbtnIndex, 1, 14, 11 };

    // Render the volume bar
    SDL_RenderCopy(renderer, volume_texture, &src_vol, &dst_vol);
    SDL_RenderCopy(renderer, volume_texture, &src_vol_btn, &dst_vol_btn);
    // Reset render target
    SDL_SetRenderTarget(renderer, NULL);
}

void format_time(unsigned int time, char *buffer, int width) {
    unsigned int minutes = time / 60;
    unsigned int seconds = time % 60;
    sprintf(buffer, "%02u:%02u", minutes, seconds);

    // Calculate the starting position for right alignment
    int text_width = strlen(buffer);  // Calculate the width of the rendered text
    int start_x = width - text_width; // Calculate the starting position for right alignment

    // Shift the buffer content to the right
    memmove(buffer + start_x, buffer, text_width + 1); // +1 to include the null terminator

    // Fill the beginning with spaces
    memset(buffer, ' ', start_x);

    // Ensure null-termination
    buffer[width] = '\0';
}

/* int res = SendMessage(hwnd_winamp,WM_WA_IPC,0,IPC_ISPLAYING);
** This is sent to retrieve the current playback state of Winamp.
** If it returns 1, Winamp is playing.
** If it returns 3, Winamp is paused.
** If it returns 0, Winamp is not playing.
*/

int print_status(enum mpd_state state) {
    switch (state) {
        case MPD_STATE_PLAY:
            res = 1;
            return 1;
            break;
        case MPD_STATE_PAUSE:
            res = 3;
            return 3;
            break;
        case MPD_STATE_STOP:
            res = 0;
            return 0;
            break;
        default:
            res = 4;
            return 4;
            break;
    }
}

double hann(int n, int N) {
    return 0.5 * (1 - cos(2 * M_PI * n / (N - 1)));
}

bool HasWindowFocus(SDL_Window* window)
{
    uint32_t flags = SDL_GetWindowFlags(window);
    // We *don't* want to check mouse focus:
    // SDL_WINDOW_INPUT_FOCUS - input is going to the window
    // SDL_WINDOW_MOUSE_FOCUS - mouse is hovered over the window, regardless of window focus
    return (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
}

#define MOUSE_GRAB_PADDING 10

SDL_HitTestResult HitTestCallback(SDL_Window *Window, const SDL_Point *Area, void *Data)
{
    int Width, Height;
    SDL_GetWindowSize(Window, &Width, &Height);

    // Draggable area
    int draggableWidth = Width * mult;
    int draggableHeight = 14 * mult;

    if(Area->y < MOUSE_GRAB_PADDING)
    {
        if(Area->x < MOUSE_GRAB_PADDING)
        {
            return SDL_HITTEST_RESIZE_TOPLEFT;
        }
        else if(Area->x > Width - MOUSE_GRAB_PADDING)
        {
            return SDL_HITTEST_RESIZE_TOPRIGHT;
        }
        else
        {
            return SDL_HITTEST_RESIZE_TOP;
        }
    }
    else if(Area->y > Height - MOUSE_GRAB_PADDING)
    {
        if(Area->x < MOUSE_GRAB_PADDING)
        {
            return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        }
        else if(Area->x > Width - MOUSE_GRAB_PADDING)
        {
            return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        }
        else
        {
            return SDL_HITTEST_RESIZE_BOTTOM;
        }
    }
    else if(Area->x < MOUSE_GRAB_PADDING)
    {
        return SDL_HITTEST_RESIZE_LEFT;
    }
    else if(Area->x > Width - MOUSE_GRAB_PADDING)
    {
        return SDL_HITTEST_RESIZE_RIGHT;
    }
    else if(Area->y < draggableHeight && Area->x < draggableWidth)
    {
        return SDL_HITTEST_DRAGGABLE;
    }

    return SDL_HITTEST_NORMAL; // Return normal if not in draggable area
}

int main(int argc, char *argv[]) {

    SDL_Window *window = SDL_CreateWindow("MPDamp",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WIDTH*mult, HEIGHT*mult, SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture *master_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIDTH*mult, HEIGHT*mult);
    SDL_Texture *vis_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 75, 16);
    SDL_Texture *spec_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 75, 16);
    SDL_Texture *num_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 63, 13);
    SDL_SetTextureBlendMode(num_texture, SDL_BLENDMODE_BLEND);  // Enable alpha blending for transparency
    SDL_Texture *text_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 154, 6);
    SDL_Texture *bitratenum_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 15, 6);
    SDL_Texture *khznum_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 10, 6);
    SDL_Texture *progress_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 248, 10);
    SDL_Texture *titleb_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIDTH, 14);
    SDL_Texture *clutt_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 8, 43);
    SDL_Texture *mons_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 56, 12);
    SDL_Texture *cbutt_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 114, 18);
    SDL_Texture *vol_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 68, 13);

    // Load images and create textures
    SDL_Surface *image_surface = IMG_Load("main.bmp");
    SDL_Texture *image_texture = SDL_CreateTextureFromSurface(renderer, image_surface);
    SDL_FreeSurface(image_surface);

    SDL_Surface *numfont_surface = IMG_Load("numbers.bmp");
    SDL_Texture *numfont_texture = SDL_CreateTextureFromSurface(renderer, numfont_surface);
    SDL_FreeSurface(numfont_surface);

    SDL_Surface *font_surface = IMG_Load("text.bmp");
    SDL_Texture *font_texture = SDL_CreateTextureFromSurface(renderer, font_surface);
    SDL_FreeSurface(font_surface);

    SDL_Surface *play_surface = IMG_Load("playpaus.bmp");
    SDL_Texture *playpaus_texture = SDL_CreateTextureFromSurface(renderer, play_surface);
    SDL_FreeSurface(play_surface);

    SDL_Surface *bmp = IMG_Load("posbar.bmp");
    SDL_Texture *posbar_texture = SDL_CreateTextureFromSurface(renderer, bmp);
    SDL_FreeSurface(bmp);

    SDL_Surface *titlebar_surface = IMG_Load("titlebar.bmp");
    SDL_Texture *titlebar_texture = SDL_CreateTextureFromSurface(renderer, titlebar_surface);
    SDL_FreeSurface(titlebar_surface);

    SDL_Surface *monoster_surface = IMG_Load("monoster.bmp");
    SDL_Texture *monoster_texture = SDL_CreateTextureFromSurface(renderer, monoster_surface);
    SDL_FreeSurface(monoster_surface);

    SDL_Surface *cbuttons_surface = IMG_Load("cbuttons.bmp");
    SDL_Texture *cbuttons_texture = SDL_CreateTextureFromSurface(renderer, cbuttons_surface);
    SDL_FreeSurface(cbuttons_surface);

    SDL_Surface *volume_surface = IMG_Load("volume.bmp");
    SDL_Texture *volume_texture = SDL_CreateTextureFromSurface(renderer, volume_surface);
    SDL_FreeSurface(volume_surface);

    SDL_SetWindowHitTest(window, HitTestCallback, 0);

    float time = 0;
    int seek_x = 0;
    int channels;
    char time_str[6]; // buffer to hold the formatted string "MM:SS"
    bool hasfocus;
    struct mpd_connection *conn = mpd_connection_new(NULL, 0, 0);
    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
        fprintf(stderr, "Error connecting to MPD: %s\n", mpd_connection_get_error_message(conn));
        mpd_connection_free(conn);
        return 1;
    }

    struct mpd_status *status = mpd_run_status(conn);

    volume = mpd_status_get_volume(status);
    repeat = mpd_status_get_repeat(status);
    shuffle = mpd_status_get_random(status);

    Color* osc_colors = osccolors(colors);

    // Open the FIFO file
    char *filename = "/tmp/mpd.fifo";
    if (argc > 1) {
        filename = argv[1];
    }
    int fd = open(filename, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        fprintf(stderr, "Error opening FIFO file: %s\n", strerror(errno));
        return 1;
    }

    signal(SIGINT, handle_sigint);

    fftw_complex *in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * BSZ);
    fftw_complex *out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * BSZ);
    fftw_plan p = fftw_plan_dft_1d(BSZ, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    SDL_Rect src_rect = {0, 0, 0, 0}; // Source rect for image

    short buf[BSZ];
    double fft_result[BSZ];
    int shutdown = 0;
    enum mpd_state state = mpd_status_get_state(status);
    ssize_t read_count;
    while (!shutdown) {
        if (res == 1 || res == 3) {
            read_count = read(fd, buf, sizeof(short) * BSZ);
        } else {
            memset(buf, 0, sizeof(buf));
            read_count = BSZ; // Simulate full buffer of zeros
        }

        if (read_count > 0) {
            for (int i = 0; i < BSZ; i++) {
                double window = hann(i, BSZ);
                in[i][0] = buf[i] * window; // Apply Hann window to real part
                in[i][1] = 0.0;             // Imaginary part remains 0
            }
            fftw_execute(p);

            for (int i = 0; i < BSZ; i++) {
                fft_result[i] = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]); // Magnitude
            }
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                shutdown = 1;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {  // Check for left mouse button
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;

                    SDL_Rect clickableRect = {24 * mult, 43 * mult, 76 * mult, 16 * mult};

                    if (mouseX >= clickableRect.x && mouseX <= clickableRect.x + clickableRect.w &&
                        mouseY >= clickableRect.y && mouseY <= clickableRect.y + clickableRect.h) {
                        VisMode = (VisMode + 1) % 3;  // Increment VisMode and wrap around using modulo
                    }
                }
            }
            handleMouseEvent(event, seek_x, time, total_time, conn);
            cbuttons(event, renderer, cbutt_texture, cbuttons_texture, conn);
        }

        struct mpd_song *current_song = mpd_run_current_song(conn);
        std::string song_title;
        std::string bitrate_info;
        std::string sample_rate_info;

        hasfocus = HasWindowFocus(window);

        if (current_song != NULL) {
            const char *title = mpd_song_get_tag(current_song, MPD_TAG_TITLE, 0);
            const char *artist = mpd_song_get_tag(current_song, MPD_TAG_ARTIST, 0);
            const char *album = mpd_song_get_tag(current_song, MPD_TAG_ALBUM, 0);
            //const char *track = mpd_song_get_tag(current_song, MPD_TAG_TRACK, 0);
            
            // Fetching bitrate from status
            bitrate = mpd_status_get_kbit_rate(status);

            // Fetching sample rate from audio format
            const struct mpd_audio_format *audio_format = mpd_status_get_audio_format(status);
            sample_rate = (audio_format != NULL) ? audio_format->sample_rate : 0;
            channels = (audio_format != NULL) ? audio_format->channels : 0;

            if (title && artist && album) {
                song_title = std::to_string(current_song_id) + ". " + std::string(artist) + " - " + std::string(title) + " (" + std::string(album) + ")";
            } else {
                song_title = std::to_string(current_song_id) + ". " + std::string(title);
            }

            if (bitrate > 0) {
                bitrate_info = std::to_string(bitrate);
            } else {
                if (res == 1 || res == 3){
                    bitrate_info = "0";
                }
            }

            if (sample_rate > 0) {
                sample_rate_info = std::to_string(sample_rate / 1000.0);
            } else {
                if (res == 1 || res == 3){
                    sample_rate_info = "0";
                }
            }

            mpd_song_free(current_song);
        } /* else {
            song_title = "No current song";
        } */

        //printf(bitrate_info.c_str());
        current_song_id = getCurrentSongID(conn);

        std::wstringstream wide_stream;
        wide_stream << std::wstring(song_title.begin(), song_title.end()); // Convert std::string
        std::wstringstream wide_bit;
        wide_bit << std::wstring(bitrate_info.begin(), bitrate_info.end()); // Convert std::string
        std::wstringstream wide_khz;
        wide_khz << std::wstring(sample_rate_info.begin(), sample_rate_info.end()); // Convert std::string

        ampstatus = print_status(state);
        // volume = mpd_status_get_volume(status);
        // why's this turn the volume bar into nothing? why's putting this
        // return bogus values like -696923407, -696923274?

        //std::cout << volume << std::endl;

        // Set source rect based on ampstatus
        switch (ampstatus) {
            case 1:
                src_rect.x = 0;
                src_rect.y = 0;
                src_rect.w = 9;
                src_rect.h = 9;
                break;
            case 3:
                src_rect.x = 9;
                src_rect.y = 0;
                src_rect.w = 9;
                src_rect.h = 9;
                break;
            case 0:
                src_rect.x = 18;
                src_rect.y = 0;
                src_rect.w = 9;
                src_rect.h = 9;
                break;
            default:
                break;
        }

        format_time(time, time_str, sizeof(time_str));

        render_vis(renderer, vis_texture, buf, fft_result, BSZ, osc_colors);
        draw_numtext(renderer, numfont_texture, num_texture, time_str, 0, 0);
        draw_text(renderer, font_texture, text_texture, wide_stream.str().c_str(), 0, 0);
        draw_text(renderer, font_texture, bitratenum_texture, wide_bit.str().c_str(), 0, 0);
        draw_text(renderer, font_texture, khznum_texture, wide_khz.str().c_str(), 0, 0);
        updateSeekPosition(seek_x, time, total_time);
        draw_progress_bar(renderer, progress_texture, posbar_texture, seek_x);
        draw_titlebar(renderer, titleb_texture, titlebar_texture, hasfocus);
        monoster(renderer, mons_texture, monoster_texture, channels);
        render_volume(renderer, vol_texture, volume_texture);
        //printf(song_title.c_str());

        SDL_SetRenderTarget(renderer, master_texture);
        SDL_RenderClear(renderer);

        int posplay;
        if (ampstatus == 1){
            posplay = 26;
        } else {
            posplay = 28;
        }

        SDL_Rect dst_rect_vis = {24*mult, 43*mult, 75*mult, 16*mult};
        SDL_Rect dst_rect_image = {0, 0, WIDTH*mult, 116*mult}; // position and size for the image
        SDL_Rect dst_rect_image2 = {36*mult, 26*mult, 63*mult, 13*mult}; // position and size for the image
        SDL_Rect dst_rect_play = {posplay*mult, 28*mult, 9*mult, 9*mult}; // position and size for the image
        SDL_Rect dst_rect_font = {111*mult, 27*mult, 154*mult, 6*mult}; // position and size for the image
        SDL_Rect dst_rect_font2 = {111*mult, 43*mult, 15*mult, 6*mult}; // position and size for the image
        SDL_Rect dst_rect_font3 = {156*mult, 43*mult, 10*mult, 6*mult}; // position and size for the image
        SDL_Rect dst_rect_posbar = {16*mult, 72*mult, 248*mult, 10*mult}; 
        SDL_Rect dst_rect_titlb = {0, 0, WIDTH*mult, 14*mult}; 
        SDL_Rect dst_rect_mon = {212*mult, 41*mult, 56*mult, 12*mult};
        SDL_Rect dst_rect_cb = {16*mult, 88*mult, 114*mult, 18*mult};
        SDL_Rect dst_rect_vol = {107*mult, 57*mult, 68*mult, 13*mult};

        SDL_Rect src_rect_clutterbar = {304, 0, 8, 43};
        SDL_Rect dst_rect_clutterbar = {10*mult, 22*mult, 8*mult, 43*mult};
        // maybe they'll be some more sophisticated handling for this, iunno
        SDL_Rect src_rect_sync;
        if ((sample_rate == 0 && bitrate == 0 && total_time == 0) || (bitrate == 0 && total_time == 0)){
            src_rect_sync = {39, 0, 3, 9};
        } else {
            src_rect_sync = {36, 0, 3, 9};
        }
        SDL_Rect dst_rect_sync = {24*mult, 28*mult, 3*mult, 9*mult};

        // Set blend mode for renderer to enable transparency
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        SDL_RenderCopy(renderer, image_texture, NULL, &dst_rect_image); // render the image
        SDL_RenderCopy(renderer, vis_texture, NULL, &dst_rect_vis);
        SDL_RenderCopy(renderer, num_texture, NULL, &dst_rect_image2);
        SDL_RenderCopy(renderer, playpaus_texture, &src_rect, &dst_rect_play);
        if (res == 1){
            SDL_RenderCopy(renderer, playpaus_texture, &src_rect_sync, &dst_rect_sync);
        }
        SDL_RenderCopy(renderer, text_texture, NULL, &dst_rect_font);
        SDL_RenderCopy(renderer, bitratenum_texture, NULL, &dst_rect_font2);
        SDL_RenderCopy(renderer, khznum_texture, NULL, &dst_rect_font3);
        SDL_RenderCopy(renderer, progress_texture, NULL, &dst_rect_posbar);
        SDL_RenderCopy(renderer, titleb_texture, NULL, &dst_rect_titlb);
        SDL_RenderCopy(renderer, titlebar_texture, &src_rect_clutterbar, &dst_rect_clutterbar);
        SDL_RenderCopy(renderer, mons_texture, NULL, &dst_rect_mon);
        SDL_RenderCopy(renderer, cbutt_texture, NULL, &dst_rect_cb);
        SDL_RenderCopy(renderer, vol_texture, NULL, &dst_rect_vol);

        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, master_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        status = mpd_run_status(conn);
        if (!status) {
            fprintf(stderr, "Error retrieving MPD status: %s\n", mpd_connection_get_error_message(conn));
            break;
        }
        time = mpd_status_get_elapsed_time(status);
        total_time = mpd_status_get_total_time(status);
        state = mpd_status_get_state(status);
        mpd_status_free(status);

        SDL_Delay(16);  // Control the frame rate
    }

    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);

    //fclose(ptr_file);
    SDL_DestroyTexture(image_texture);
    SDL_DestroyTexture(vis_texture);
    SDL_DestroyTexture(spec_texture);
    SDL_DestroyTexture(master_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}