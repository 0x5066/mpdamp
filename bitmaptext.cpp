#include "bitmaptext.h"
#include <QChar>
#include <QRect>
#include <iostream>

#define CHARF_WIDTH 5
#define CHARF_HEIGHT 6
#define FONT_IMAGE_WIDTH 155

void draw_bitmaptext(QPainter *painter, QImage *fontImage, const QString &text, int x, int y) {
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

    std::string stdText = text.toStdString();

    for (const char* c = stdText.c_str(); *c != '\0'; ++c) {
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
/*             std::cout << "Character: " << upper_char 
               << ", Index: " << index 
               << ", Col: " << col 
               << ", Row: " << row 
               << std::endl;
 */
            switch (*c)
            {
            case '"': col = 26; row = 0; break;
            case '@': col = 27; row = 0; break;
            case ' ': col = 29; row = 0; break;
            case ':':
            case ';':
            case '|': col = 12; row = 1; break;
            case '(':
            case '{': col = 13; row = 1; break;
            case ')':
            case '}': col = 14; row = 1; break;
            case '-':
            case '~': col = 15; row = 1; break;
            case '`':
            case '\'': col = 16; row = 1; break;
            case '!': col = 17; row = 1; break;
            case '_': col = 18; row = 1; break;
            case '+': col = 19; row = 1; break;
            case '\\': col = 20; row = 1; break;
            case '/': col = 21; row = 1; break;
            case '[': col = 22; row = 1; break;
            case ']': col = 23; row = 1; break;
            case '^': col = 24; row = 1; break;
            case '&': col = 25; row = 1; break;
            case '%': col = 26; row = 1; break;
            case '.':
            case ',': col = 27; row = 1; break;
            case '=': col = 28; row = 1; break;
            case '$': col = 29; row = 1; break;
            case '#': col = 30; row = 1; break;
            case '?': col = 3; row = 2; break;
            case '*': col = 4; row = 2; break;
            }

        QRect srcRect(col * CHARF_WIDTH, row * CHARF_HEIGHT, CHARF_WIDTH, CHARF_HEIGHT);
        QRect dstRect(x, y, CHARF_WIDTH, CHARF_HEIGHT);

        painter->drawImage(dstRect, *fontImage, srcRect);
        x += CHARF_WIDTH;  // Increment x position for the next character
    }
}
