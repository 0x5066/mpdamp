#include "mpdamp.h"

#define M_PI 3.14159265358979323846

#define WIDTH 275
#define HEIGHT 116

const int BSZ = 1152;

int VisMode = 0;
int res = 0;
int ampstatus, bitrate, sample_rate, volume;
bool repeat, shuffle, time_remaining;
int blinking;
bool hasfocus;

const int mult = 2;

float total_time = 0;

bool isDragging = false;
int mouseX, mouseY;
unsigned int current_song_id = 0;

unsigned long total_queue_time = 0;

const int DOCK_RADIUS = 40 * 2;

time_t theTime = time(NULL);
struct tm *aTime = localtime(&theTime);

int day = aTime->tm_mday;
int month = aTime->tm_mon + 1; // Month is 0 - 11, add 1 to get a jan-dec 1-12 concept
int year = aTime->tm_year + 1900; // Year is # years since 1900

SDL_Texture *masterTexture = NULL;

FILE *ptr_file;
char shutdown = 0;

TTF_Font *font = NULL;

std::vector<std::string> songs;
std::string song_title;

void handle_sigint(int signal) {
    shutdown = 1;
}

typedef struct {
    Uint8 r, g, b, a;
} Color;

int last_y = 0;
int top = 0, bottom = 0;

// Define a structure to pass additional parameters to the event filter
struct EventFilterData {
    SDL_Window *window;
    SDL_Window *window2;
    int offsetX;
    int offsetY;
};

// Event filter function
int EventFilter(void *userdata, SDL_Event *event) {
    EventFilterData *data = static_cast<EventFilterData *>(userdata);

    if (event->type == SDL_WINDOWEVENT) {
        if (event->window.windowID == SDL_GetWindowID(data->window)) {
            if (event->window.event == SDL_WINDOWEVENT_MOVED) {
                int winX, winY;
                SDL_GetWindowPosition(data->window, &winX, &winY);

                // Calculate the new position of window2 based on window's position and the offsets
                int newWin2X = winX + data->offsetX;
                int newWin2Y = winY + data->offsetY;

                // Get the current position of window2
                int win2X, win2Y;
                SDL_GetWindowPosition(data->window2, &win2X, &win2Y);

                // Check if window2 is docked or within the docking radius
                bool isDocked = (std::abs(win2X - newWin2X) <= DOCK_RADIUS && std::abs(win2Y - newWin2Y) <= DOCK_RADIUS);

                // Update the position of window2 if it is docked or near the main window
                if (isDocked) {
                    SDL_SetWindowPosition(data->window2, newWin2X, newWin2Y);
                }
            }
        }
    }

    // Return 1 to allow the event to be processed normally
    return 1;
}

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
    if (month == 6){
        SDL_Rect rect1 = {0, 0, 75, 3};
        SDL_SetRenderDrawColor(renderer, 91, 206, 250, day * 2);
        SDL_RenderFillRect(renderer, &rect1);
        SDL_Rect rect2 = {0, 3, 75, 4};
        SDL_SetRenderDrawColor(renderer, 245, 169, 184, day * 2);
        SDL_RenderFillRect(renderer, &rect2);
        SDL_Rect rect3 = {0, 7, 75, 3};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, day * 2);
        SDL_RenderFillRect(renderer, &rect3);
        SDL_Rect rect4 = {0, 10, 75, 4};
        SDL_SetRenderDrawColor(renderer, 245, 169, 184, day * 2);
        SDL_RenderFillRect(renderer, &rect4);
        SDL_Rect rect5 = {0, 14, 75, 3};
        SDL_SetRenderDrawColor(renderer, 91, 206, 250, day * 2);
        SDL_RenderFillRect(renderer, &rect5);
    }
}

void clearTexture(SDL_Renderer *renderer, SDL_Texture *texture) {
    SDL_SetRenderTarget(renderer, texture);
    clearRenderer(renderer);
    SDL_SetRenderTarget(renderer, NULL);
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
            int y = (((buffer[buffer_index / 2] + 32768 + 1024) * 2) * 16 / 65536);
            y = (-y) + 23;

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
                    drawRect(renderer, x, dy, 1, scope_color);
                }            
            }

        if ((x == i + 3 && x < 72) && sa_thick) {
            // skip rendering
            } else {
                if (intValue2 < 15) {
                    Color peaksColor = colors[23];
                    drawRect(renderer, x, intValue2, 1, peaksColor);
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
    const char *numbers = nullptr;
    SDL_Rect dst_rect;
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
            int char_index = -1;
            SDL_Rect src_rect;
            if (*c >= '0' && *c <= '9') {
                char_index = *c - '0';
                src_rect = { char_index * CHAR_WIDTH, 0, CHAR_WIDTH, CHAR_HEIGHT };
            } else if (*c == '-') {
                src_rect = { 27, 6, 9, 1 };  // Specific coordinates for dash
                char_index = 10; // Use a specific index for internal logic if needed
            } else if (*c == ':') {
                char_index = 11;
                src_rect = { char_index * CHAR_WIDTH, 0, CHAR_WIDTH, CHAR_HEIGHT };
            } else if (*c == ' ') {
                char_index = 12;  // Assuming 12th position is for space
                src_rect = { char_index * CHAR_WIDTH, 0, CHAR_WIDTH, CHAR_HEIGHT };
            }

            if (char_index != -1) {
                if (*c == '-'){
                    dst_rect = { x, 6, 9, 1 };
                } else {
                    dst_rect = { x, y, CHAR_WIDTH, CHAR_HEIGHT };
                }
                SDL_RenderCopy(renderer, font_texture, &src_rect, &dst_rect);
                if (*c == ':') {
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
        51, // backslash
        52, // slash
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
            /* std::wcout << L"Character: " << upper_char 
               << L", Index: " << index 
               << L", Col: " << col 
               << L", Row: " << row 
               << std::endl; */

        SDL_Rect src_rect = { col * CHARF_WIDTH, row * CHARF_HEIGHT, CHARF_WIDTH, CHARF_HEIGHT };
        SDL_Rect dst_rect = { x, y, CHARF_WIDTH, CHARF_HEIGHT };
        SDL_RenderCopy(renderer, font_texture, &src_rect, &dst_rect);
        x += CHARF_WIDTH;
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

    // Define source rectangles for mono and stereo
    SDL_Rect mono = { 29, 12, 29, 12 };  // Default value
    SDL_Rect stereo = { 0, 12, 29, 12 };  // Default value

    if (res == 1 || res == 3) {
        if (channels == 2) {
            stereo = { 0, 0, 29, 12 };
            mono = { 29, 12, 29, 12 };
        } else if (channels == 1) {
            mono = { 29, 0, 29, 12 };
            stereo = { 0, 12, 29, 12 };
        }
    } else if (res == 0) {
        stereo = { 0, 12, 29, 12 };
        mono = { 29, 12, 29, 12 };
    }

    // Destination rectangles
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
    elapsed_time = elapsed_time / 1000;
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
    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
        fprintf(stderr, "MPD connection error: %s\n", mpd_connection_get_error_message(conn));
        return;
    }

    if (strcmp(command, "previous") == 0) {
        mpd_run_previous(conn);
    } else if (strcmp(command, "play") == 0) {
        if (res == 3) {
            mpd_run_pause(conn, false);
        } else {
            mpd_run_stop(conn);
            mpd_run_play(conn);
        }
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

    if (strcmp(command, "repeat") == 0) {
        if (!repeat) {
            mpd_run_repeat(conn, true);
            repeat = true;
        } else {
            mpd_run_repeat(conn, false);
            repeat = false;
        }
    } else if (strcmp(command, "shuffle") == 0) {
        shuffle = !shuffle; // Toggle shuffle state
        mpd_run_random(conn, shuffle);
    }
    mpd_response_finish(conn);
}

void cbuttons(SDL_Event &e, SDL_Renderer *renderer, SDL_Texture *cbutt_texture, SDL_Texture *cbuttons_texture, struct mpd_connection *conn) {
    // Set render target to cbutt_texture
    SDL_SetRenderTarget(renderer, cbutt_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color
    SDL_RenderClear(renderer);

    int i1, i2;
    if (ampstatus == 0){
        i1 = 1;
        i2 = 4;
    } else {
        i1 = 0;
        i2 = 5;
    }

    // Button source rectangles
    SDL_Rect buttons[i2];
    for (int i = i1; i < i2; ++i) {
        buttons[i] = { 23 * i, 0, 23, 18 };
    }

    // Button destination rectangles
    SDL_Rect dst_buttons[i2];
    for (int i = i1; i < i2; ++i) {
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
            for (int i = i1; i < i2; ++i) {
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
            for (int i = i1; i < i2; ++i) {
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
    for (int i = i1; i < i2; ++i) {
        SDL_RenderCopy(renderer, cbuttons_texture, &buttons[i], &dst_buttons[i]);
    }

    // Reset render target
    SDL_SetRenderTarget(renderer, NULL);
}

void shufrep(SDL_Event &e, SDL_Renderer *renderer, SDL_Texture *shuf_texture, SDL_Texture *shufrep_texture, struct mpd_connection *conn) {
    // Set render target to shuf_texture
    SDL_SetRenderTarget(renderer, shuf_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);  // Clear with transparent color
    SDL_RenderClear(renderer);

    int shufstate = shuffle ? 30 : 0;
    int repstate = repeat ? 30 : 0;

    // Button source rectangles
    SDL_Rect shuffleRect = { 28, shufstate, 46, 15 };
    SDL_Rect repeatRect = { 0, repstate, 28, 15 };

    // Button destination rectangles
    SDL_Rect dst_shuffle = { 0, 0, 46, 15 };
    SDL_Rect dst_repeat = { 46, 0, 28, 15 };  // Adjusted the position to the right of shuffle button

    // Offset for button area on the screen
    const int buttonAreaX = 164 * mult;
    const int buttonAreaY = 89 * mult;

    // Handle events
    if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
        int x = e.button.x - buttonAreaX;
        int y = e.button.y - buttonAreaY;

        if (e.type == SDL_MOUSEBUTTONDOWN) {
            // Check if shuffle button was clicked
            if (x >= dst_shuffle.x * mult && x < (dst_shuffle.x + dst_shuffle.w) * mult &&
                y >= dst_shuffle.y * mult && y < (dst_shuffle.y + dst_shuffle.h) * mult) {
                shuffleRect.y = 15 + shufstate;  // Show the clicked state
            }

            // Check if repeat button was clicked
            if (x >= dst_repeat.x * mult && x < (dst_repeat.x + dst_repeat.w) * mult &&
                y >= dst_repeat.y * mult && y < (dst_repeat.y + dst_repeat.h) * mult) {
                repeatRect.y = 15 + repstate;  // Show the clicked state
            }
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            // Check if shuffle button was released
            if (x >= dst_shuffle.x * mult && x < (dst_shuffle.x + dst_shuffle.w) * mult &&
                y >= dst_shuffle.y * mult && y < (dst_shuffle.y + dst_shuffle.h) * mult) {
                shuffleRect.y = 0 + shufstate;  // Reset to the unclicked state

                // Send the shuffle command to MPD and update the state
                send_mpd_command(conn, "shuffle");
                shufstate = shuffle ? 30 : 0;  // Update the state
                shuffleRect.y = shufstate;     // Update the y-coordinate for rendering
            }

            // Check if repeat button was released
            if (x >= dst_repeat.x * mult && x < (dst_repeat.x + dst_repeat.w) * mult &&
                y >= dst_repeat.y * mult && y < (dst_repeat.y + dst_repeat.h) * mult) {
                repeatRect.y = 0 + repstate;  // Reset to the unclicked state

                // Send the repeat command to MPD and update the state
                send_mpd_command(conn, "repeat");
                repstate = repeat ? 30 : 0;   // Update the state
                repeatRect.y = repstate;      // Update the y-coordinate for rendering
            }
        }
    }

    // Render shuffle and repeat buttons
    SDL_RenderCopy(renderer, shufrep_texture, &shuffleRect, &dst_shuffle);
    SDL_RenderCopy(renderer, shufrep_texture, &repeatRect, &dst_repeat);

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

void format_time(float time, char *buffer, int width) {
    time = time / 1000;
    unsigned int display_time = (total_time == 0 || !time_remaining) ? time : total_time - time;

    unsigned int minutes = display_time / 60;
    unsigned int seconds = display_time % 60;

    if (time_remaining && total_time != 0) {
        sprintf(buffer, "-%02u:%02u", minutes, seconds);
    } else {
        sprintf(buffer, "%02u:%02u", minutes, seconds);
    }

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

const char *format_time2(unsigned int time, char *buffer) {
    unsigned int minutes = time / 60;
    unsigned int seconds = time % 60;
    sprintf(buffer, "%u:%02u", minutes, seconds);

    return buffer;
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

// Function to calculate the Blackman-Harris window coefficient
double blackman_harris(int n, int N) {
    const double a0 = 0.35875;
    const double a1 = 0.48829;
    const double a2 = 0.14128;
    const double a3 = 0.01168;
    return a0 - a1 * cos((2.0 * M_PI * n) / (N - 1))
               + a2 * cos((4.0 * M_PI * n) / (N - 1))
               - a3 * cos((6.0 * M_PI * n) / (N - 1));
}

bool HasWindowFocus(SDL_Window* window)
{
    uint32_t flags = SDL_GetWindowFlags(window);
    // We *don't* want to check mouse focus:
    // SDL_WINDOW_INPUT_FOCUS - input is going to the window
    // SDL_WINDOW_MOUSE_FOCUS - mouse is hovered over the window, regardless of window focus
    return (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
}

SDL_HitTestResult WindowDrag(SDL_Window *Window, const SDL_Point *Area, void *Data)
{
    int Width, Height;
    SDL_GetWindowSize(Window, &Width, &Height);

    // Draggable area
    int draggableWidth = Width * mult;
    int draggableHeight = 14 * mult;

    if (Area->y < draggableHeight) {
        return SDL_HITTEST_DRAGGABLE;
    }

    return SDL_HITTEST_NORMAL; // Return normal if not in draggable area
}

SDL_HitTestResult HitTestCallback(SDL_Window *Window, const SDL_Point *Area, void *Data)
{
    int Width, Height;
    SDL_GetWindowSize(Window, &Width, &Height);

    // Draggable area
    int draggableWidth = Width;
    int draggableHeight = 14;

    // Specify the rectangle where resizing is allowed
    int resizeRectX = PL_ResizeX; // X-coordinate of the top-left corner of the resizable rectangle
    int resizeRectY = PL_ResizeY; // Y-coordinate of the top-left corner of the resizable rectangle
    int resizeRectW = 19;         // Width of the resizable rectangle
    int resizeRectH = 19;         // Height of the resizable rectangle
    int MOUSE_GRAB_PADDING = 19;

    // Check if the mouse is within the draggable area
    if (Area->y < draggableHeight) {
        return SDL_HITTEST_DRAGGABLE;
    }

    // Check if the mouse is within the resizable rectangle
    if (Area->x >= resizeRectX && Area->x < resizeRectX + resizeRectW &&
        Area->y >= resizeRectY && Area->y < resizeRectY + resizeRectH)
    {
        // Determine if the mouse is in the bottom-right corner
        if (Area->x >= resizeRectX + resizeRectW - MOUSE_GRAB_PADDING &&
            Area->y >= resizeRectY + resizeRectH - MOUSE_GRAB_PADDING)
        {
            return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        }
    }

    return SDL_HITTEST_NORMAL; // Return normal if not in the resizable area
}

std::string getFilenameFromURI(const std::string& uri) {
    // Find the last slash in the URI
    size_t lastSlashPos = uri.find_last_of('/');
    if (lastSlashPos != std::string::npos) {
        // Extract the filename from the URI
        return uri.substr(lastSlashPos + 1);
    }
    // If no slash is found, return the original URI (unlikely case)
    return uri;
}

void DockWindows(SDL_Window *window, SDL_Window *window2, int offsetX, int offsetY) {
    int winX, winY, winW, winH;
    SDL_GetWindowPosition(window, &winX, &winY);
    SDL_GetWindowSize(window, &winW, &winH);

    int win2X, win2Y, win2W, win2H;
    SDL_GetWindowPosition(window2, &win2X, &win2Y);
    SDL_GetWindowSize(window2, &win2W, &win2H);

    // Calculate the new position of window2 based on window's position and the offsets
    int newWin2X = winX + offsetX;
    int newWin2Y = winY + offsetY;

    // Check if window2 is docked or within 10 pixels of window
    bool isDocked = (abs(win2X - newWin2X) <= DOCK_RADIUS && abs(win2Y - newWin2Y) <= DOCK_RADIUS);

    // Update the position of window2 if it is docked or near the main window
    if (isDocked) {
        SDL_SetWindowPosition(window2, newWin2X, newWin2Y);
    }
}

const char* qlength(struct mpd_connection *conn, char *duration_str) {
    const struct mpd_song *song;
    struct mpd_entity *entity;
    float total_duration = 0.0f;

    // Send playlist command and get the list of songs
    if (!mpd_send_list_queue_meta(conn)) {
        std::cerr << "Failed to send list queue command: " << mpd_connection_get_error_message(conn) << std::endl;
        mpd_connection_free(conn);
        return NULL;
    }

    // Read the songs from the queue
    while ((entity = mpd_recv_entity(conn)) != NULL) {
        if (mpd_entity_get_type(entity) == MPD_ENTITY_TYPE_SONG) {
            song = mpd_entity_get_song(entity);
            total_duration += mpd_song_get_duration_ms(song) / 1000.0f;
        }
        mpd_entity_free(entity);
    }

    // Check for errors
    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
        std::cerr << "Error while reading the queue: " << mpd_connection_get_error_message(conn) << std::endl;
        mpd_connection_free(conn);
        return NULL;
    }

    // Finish the command
    if (!mpd_response_finish(conn)) {
        std::cerr << "Failed to finish response: " << mpd_connection_get_error_message(conn) << std::endl;
        mpd_connection_free(conn);
        return NULL;
    }

    // Convert total duration to HH:MM:SS or MM:SS format
    int hours = static_cast<int>(total_duration) / 3600;
    int minutes = (static_cast<int>(total_duration) % 3600) / 60;
    int seconds = static_cast<int>(total_duration) % 60;

    if (hours > 0) {
        sprintf(duration_str, "%02d:%02d:%02d", hours, minutes, seconds);
    } else if (minutes < 10) {
        sprintf(duration_str, "%d:%02d", minutes, seconds);
    } else {
        sprintf(duration_str, "%02d:%02d", minutes, seconds);
    }

    // Return the formatted string
    return duration_str;
}

void set_skip_taskbar_hint(Display* display, Window window) {
    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
    Atom skipTaskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);

    XChangeProperty(display, window, wmState, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&skipTaskbar), 1);

    XFlush(display);
}

int main(int argc, char *argv[]) {

    SDL_Window *window = SDL_CreateWindow("MPDamp",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WIDTH*mult, HEIGHT*mult, SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Window *window2 = SDL_CreateWindow("MPDamp Playlist",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED+HEIGHT,
                                          WIDTH, HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS);

    SDL_Renderer *renderer2 = SDL_CreateRenderer(window2, -1, SDL_RENDERER_ACCELERATED);

    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        std::cerr << "Failed to initialize SDL_ttf: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

        // Load font
    font = TTF_OpenFont("/usr/share/fonts/TTF/tahoma.ttf", 11);
    if (!font) {
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Get the native window handles
    SDL_SysWMinfo info1, info2;
    SDL_VERSION(&info1.version);
    SDL_VERSION(&info2.version);

    Display* display = nullptr;
    Window mainWindow = 0;
    Window childWindow = 0;

    if (SDL_GetWindowWMInfo(window, &info1) && SDL_GetWindowWMInfo(window2, &info2)) {
        display = info1.info.x11.display;
        mainWindow = info1.info.x11.window;
        childWindow = info2.info.x11.window;

        // Set the child window's parent
        XSetTransientForHint(display, childWindow, mainWindow);

        // Set the _NET_WM_STATE_SKIP_TASKBAR hint
        set_skip_taskbar_hint(display, childWindow);
    } else {
        std::cerr << "Failed to get window information: " << SDL_GetError() << std::endl;
    }

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
    SDL_SetTextureBlendMode(cbutt_texture, SDL_BLENDMODE_BLEND);
    SDL_Texture *vol_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 68, 13);
    SDL_Texture *shuf_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 73, 15);

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

    SDL_Surface *shufrep_surface = IMG_Load("shufrep.bmp");
    SDL_Texture *shufrep_texture = SDL_CreateTextureFromSurface(renderer, shufrep_surface);
    SDL_FreeSurface(shufrep_surface);

    SDL_Surface *pledit_surface = IMG_Load("pledit.bmp");
    SDL_Texture *pledit_texture = SDL_CreateTextureFromSurface(renderer2, pledit_surface);
    SDL_FreeSurface(pledit_surface);

    load_textbmp(renderer2);

    const char *mmonth;
    if (month == 6){
        mmonth = "TRANSamp.png";
    } else {
        mmonth = "MPDamp.png";
    }
    SDL_Surface *icon_surface = IMG_Load(mmonth);
    //SDL_FreeSurface(icon_surface);

    SDL_SetWindowHitTest(window, WindowDrag, 0);
    SDL_SetWindowHitTest(window2, HitTestCallback, 0);

    SDL_SetWindowIcon(window, icon_surface);

    // you dont wanna see video memory in that box, do ya?
    clearTexture(renderer, vis_texture);
    SDL_Rect dst_rect_vis = {24*mult, 43*mult, 75*mult, 16*mult};
    SDL_RenderCopy(renderer, vis_texture, NULL, &dst_rect_vis);

    SDL_SetWindowMinimumSize(window2, WIDTH, HEIGHT);

    float time = 0;
    int seek_x = 0;
    int channels;
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
    ptr_file = fdopen(fd, "rb");
    if (fd == -1) {
        fprintf(stderr, "Error opening FIFO file: %s\n", strerror(errno));
        return 1;
    }

    signal(SIGINT, handle_sigint);

    char duration_str[9];

    fftw_complex *in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * BSZ);
    fftw_complex *out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * BSZ);
    fftw_plan p = fftw_plan_dft_1d(BSZ, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    SDL_Rect src_rect = {0, 0, 0, 0}; // Source rect for image

    int offsetX = 0;
    int offsetY = HEIGHT * mult;

    // Set the event filter data
    EventFilterData filterData = {window, window2, offsetX, offsetY}; // Example offset, change as needed

    // Set the event filter
    SDL_SetEventFilter(EventFilter, &filterData);


    short buf[BSZ];
    double fft_result[BSZ];
    int shutdown = 0;
    int BSZ2 = 1024;
    enum mpd_state state = mpd_status_get_state(status);
    ssize_t read_count;
    while (!shutdown) {
        if (res == 1 || res == 3) {
            read_count = read(fd, buf, sizeof(short) * BSZ);
        } else {
            memset(buf, 0, sizeof(buf));
            read_count = BSZ; // Simulate full buffer of zeros
        }

        // Apply Blackman-Harris window to the input data
        if (read_count > 0) {
            for (int i = 0; i < BSZ; i++) {
                double window = blackman_harris(i, BSZ);
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
            if (event.window.windowID == SDL_GetWindowID(window)) {
                if (event.type == SDL_QUIT) {
                    shutdown = 1;
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    if (event.button.button == SDL_BUTTON_LEFT) {  // Check for left mouse button
                        int mouseX = event.button.x;
                        int mouseY = event.button.y;

                        SDL_Rect clickableRect = {24 * mult, 43 * mult, 76 * mult, 16 * mult};
                        SDL_Rect num_click = {36*mult, 26*mult, 63*mult, 13*mult};

                        if (mouseX >= clickableRect.x && mouseX <= clickableRect.x + clickableRect.w &&
                            mouseY >= clickableRect.y && mouseY <= clickableRect.y + clickableRect.h) {
                            VisMode = (VisMode + 1) % 3;  // Increment VisMode and wrap around using modulo
                        }

                        if (mouseX >= num_click.x && mouseX <= num_click.x + num_click.w &&
                            mouseY >= num_click.y && mouseY <= num_click.y + num_click.h) {
                            time_remaining = !time_remaining;
                        }
                    }
                }
            }
            handleMouseEvent(event, seek_x, time, total_time, conn);
            cbuttons(event, renderer, cbutt_texture, cbuttons_texture, conn);
            shufrep(event, renderer, shuf_texture, shufrep_texture, conn);
        }

        // Reapply the skip taskbar hint and dock the windows continuously
        if (display && childWindow) {
            set_skip_taskbar_hint(display, childWindow);
            //DockWindows(window, window2, offsetX, offsetY); // Example offset, change as needed
        }

        struct mpd_status *status2 = mpd_run_status(conn);
        if (status2 == NULL) {
            fprintf(stderr, "Error getting MPD status: %s\n", mpd_connection_get_error_message(conn));
            mpd_connection_free(conn);
            return 1;
        }
        struct mpd_song *current_song = mpd_run_current_song(conn);
        std::string bitrate_info;
        std::string sample_rate_info;

        hasfocus = HasWindowFocus(window);

        if (current_song != NULL) {
            const char *title = mpd_song_get_tag(current_song, MPD_TAG_TITLE, 0);
            const char *artist = mpd_song_get_tag(current_song, MPD_TAG_ARTIST, 0);
            const char *album = mpd_song_get_tag(current_song, MPD_TAG_ALBUM, 0);
            const char *track = mpd_song_get_tag(current_song, MPD_TAG_UNKNOWN, 0);
            std::string filename = getFilenameFromURI(mpd_song_get_uri(current_song));
            
            // Fetching bitrate from status
            bitrate = mpd_status_get_kbit_rate(status2);

            // Fetching sample rate from audio format
            const struct mpd_audio_format *audio_format = mpd_status_get_audio_format(status2);
            sample_rate = (audio_format != NULL) ? audio_format->sample_rate : 0;
            channels = (audio_format != NULL) ? audio_format->channels : 0;

            if (title && artist/*  && album */) {
                song_title = std::to_string(current_song_id) + ". " + std::string(artist) + " - " + std::string(title) + " (" + std::string(format_time2(total_time, time_str2)) + ")"/* + " (" + std::string(album) + ")"*/;
            } else if (title) {
                song_title = std::to_string(current_song_id) + ". " + std::string(title) + " (" + std::string(format_time2(total_time, time_str2)) + ")";
            } else {
                song_title = std::to_string(current_song_id) + ". " + std::string(filename) + " (" + std::string(format_time2(total_time, time_str2)) + ")";
            }

            if (bitrate > 0) {
                if (bitrate < 10){
                    bitrate_info = "  " + std::to_string(bitrate);      
                } else if (bitrate < 100){
                    bitrate_info = " " + std::to_string(bitrate);      
                } else if (bitrate > 1000){
                    std::string bitrate_str = std::to_string(bitrate);
                    bitrate_info = bitrate_str.substr(0, 2) + "H";
                } else {
                    bitrate_info = std::to_string(bitrate);                    
                }
            } else {
                if (res == 1 || res == 3){
                    bitrate_info = "  0";
                }
            }

            if (sample_rate > 0) {
                if (sample_rate < 10){
                    sample_rate_info = " " + std::to_string(sample_rate / 1000.0);
                } else if (sample_rate / 1000 > 100) {
                    std::string sample_rate_str = std::to_string(sample_rate / 1000.0);
                    sample_rate_info = sample_rate_str.substr(1, 3);
                } else {
                    sample_rate_info = std::to_string(sample_rate / 1000.0);
                }

            } else {
                if (res == 1 || res == 3){
                    sample_rate_info = " 0";
                }
            }

            mpd_song_free(current_song);
        } /* else {
            song_title = "No current song";
        } */

        //printf(bitrate_info.c_str());
        current_song_id = getCurrentSongID(conn);

        /* std::cout << sample_rate / 1000
        << bitrate << std::endl; */

        total_duration_str = qlength(conn, duration_str);

        //std::cout << total_duration_str << std::endl;

        std::wstringstream wide_stream;
        wide_stream << std::wstring(song_title.begin(), song_title.end()); // Convert std::string
        std::wstringstream wide_bit;
        wide_bit << std::wstring(bitrate_info.begin(), bitrate_info.end()); // Convert std::string
        std::wstringstream wide_khz;
        wide_khz << std::wstring(sample_rate_info.begin(), sample_rate_info.end()); // Convert std::string

        ampstatus = print_status(state);
        // this is supposed to prevent reading the volume on playback because before it starts, it's 0
        // and that's wrong, but when an internet stream starts up, volume is 0, but the check isnt really working...
        if (res == 1 || res == 3 || (res == 1 || res == 3) && (sample_rate != 0 && bitrate != 0 && total_time != 0) || (res == 1 || res == 3) && (bitrate != 0 && total_time != 0)){
            volume = mpd_status_get_volume(status2);
        }
        repeat = mpd_status_get_repeat(status2);
        shuffle = mpd_status_get_random(status2);

        //std::cout << volume << std::endl;

        // Set source rect based on ampstatus
        switch (res) {
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

        blinking++;
        if (blinking == 120) {
            blinking = 0;
        }

        format_time(time, time_str, sizeof(time_str));

        if (res == 1){
            render_vis(renderer, vis_texture, buf, fft_result, BSZ, osc_colors);
        }
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

        SDL_Rect dst_rect_image = {0, 0, WIDTH*mult, 116*mult}; // position and size for the image
        SDL_Rect dst_rect_image2 = {36*mult, 26*mult, 63*mult, 13*mult}; // position and size for the image
        SDL_Rect dst_rect_play = {26*mult, 28*mult, 9*mult, 9*mult}; // position and size for the image
        SDL_Rect dst_rect_font = {111*mult, 27*mult, 154*mult, 6*mult}; // position and size for the image
        SDL_Rect dst_rect_font2 = {111*mult, 43*mult, 15*mult, 6*mult}; // position and size for the image
        SDL_Rect dst_rect_font3 = {156*mult, 43*mult, 10*mult, 6*mult}; // position and size for the image
        SDL_Rect dst_rect_posbar = {16*mult, 72*mult, 248*mult, 10*mult}; 
        SDL_Rect dst_rect_titlb = {0, 0, WIDTH*mult, 14*mult}; 
        SDL_Rect dst_rect_mon = {212*mult, 41*mult, 56*mult, 12*mult};
        SDL_Rect dst_rect_cb = {16*mult, 88*mult, 114*mult, 18*mult};
        SDL_Rect dst_rect_vol = {107*mult, 57*mult, 68*mult, 13*mult};
        SDL_Rect dst_rect_shuf = {164*mult, 89*mult, 73*mult, 15*mult};

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
        if (res == 1 || res == 3){
            SDL_RenderCopy(renderer, vis_texture, NULL, &dst_rect_vis);
        }
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
        SDL_RenderCopy(renderer, shuf_texture, NULL, &dst_rect_shuf);

        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, master_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        draw_playlisteditor(window2, renderer2);
        // Retrieve the list of songs
        retrieve_songs(conn, songs);

        status = mpd_run_status(conn);
        if (!status) {
            fprintf(stderr, "Error retrieving MPD status: %s\n", mpd_connection_get_error_message(conn));
            break;
        }
        time = mpd_status_get_elapsed_ms(status2);
        if (res == 1 || res == 3){
            total_time = mpd_status_get_total_time(status2);            
        }
        state = mpd_status_get_state(status);


/*         struct mpd_stats *stats = mpd_run_stats(conn);
        total_queue_time = mpd_stats_get_db_play_time(stats);
        printf("DB Play Time: %s\n", DHMS(total_queue_time));
        std::cout << total_queue_time << std::endl;
        mpd_stats_free(stats); */
        mpd_status_free(status);
        mpd_status_free(status2);

        SDL_Delay(16);
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