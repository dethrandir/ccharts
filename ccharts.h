// ccharts.h
#ifndef CCHARTS_H
#define CCHARTS_H
// ansi color codes
#define CC_COLOR_RESET "\x1b[0m"
#define CC_COLOR_BLACK "\x1b[30m"
#define CC_COLOR_RED "\x1b[31m"
#define CC_COLOR_GREEN "\x1b[32m"
#define CC_COLOR_YELLOW "\x1b[33m"
#define CC_COLOR_BLUE "\x1b[34m"
#define CC_COLOR_MAGENTA "\x1b[35m"
#define CC_COLOR_CYAN "\x1b[36m"
#define CC_COLOR_WHITE "\x1b[37m"
#define CC_COLOR_BRIGHT_BLACK "\x1b[90m"
#define CC_COLOR_BRIGHT_RED "\x1b[91m"
#define CC_COLOR_BRIGHT_GREEN "\x1b[92m"
#define CC_COLOR_BRIGHT_YELLOW "\x1b[93m"
#define CC_COLOR_BRIGHT_BLUE "\x1b[94m"
#define CC_COLOR_BRIGHT_MAGENTA "\x1b[95m"
#define CC_COLOR_BRIGHT_CYAN "\x1b[96m"
#define CC_COLOR_BRIGHT_WHITE "\x1b[97m"

// block element characters
#define CC_BLOCK_FULL "\u2588"
#define CC_BLOCK_7_8 "\u2589"
#define CC_BLOCK_3_4 "\u258A"
#define CC_BLOCK_5_8 "\u258B"
#define CC_BLOCK_1_2 "\u258C"
#define CC_BLOCK_3_8 "\u258D"
#define CC_BLOCK_1_4 "\u258E"
#define CC_BLOCK_1_8 "\u258F"
#define CC_BLOCK_UPPER_HALF "\u2580"
#define CC_BLOCK_LOWER_HALF "\u2584"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// timestamp ekle
typedef struct cc_chart_settings cc_chart_settings_t;
typedef struct cc_ohlc cc_ohlc_t;
int cc_str_to_ohlc(const char* data, int size, cc_ohlc_t** ohlc,
                   char val_seperator, char line_seperator);
char* cc_line_create(const cc_ohlc_t* data, int size, int width, int height);
char* cc_candle_create(const cc_ohlc_t* data, int size, int width, int height);
#ifdef __cplusplus
}
#endif
#endif
#ifdef CCHARTS_IMPLEMENTATION
#ifdef __cplusplus
extern "C" {
#endif
struct cc_chart_settings {
    char* rise_color;
    char* fall_color;
    char* bg_color;
};

struct cc_ohlc {
    double open;
    double high;
    double low;
    double close;
};

// inline trim
static inline char* cc_trim_whitespace(char* str) {
    if (str == NULL || *str == '\0') {
        return str;
    }

    // Trim leading whitespace
    while (*str != '\0' && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) {
        str++;
    }

    // If the string became empty after trimming leading whitespace
    if (*str == '\0') {
        return str;
    }

    // Trim trailing whitespace
    char* end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    *(end + 1) = '\0';

    return str;
}


// chari const yap sonra datayi
inline int cc_str_to_ohlc(const char* data, int size, cc_ohlc_t** ohlc, char val_seperator, char line_seperator)
{
    *ohlc = (cc_ohlc_t*)calloc(size, sizeof(cc_ohlc_t));
    if (*ohlc == NULL) {
        printf("Memory allocation failed.\n");
        return 1;
    }

    char tokens[size][256];

    int start = 0;
    int end = 0;
    int i = 0;
    int num = 0;
    while (data[i] != '\0') {
        if (data[i] != line_seperator) {
            i++;
            continue;
        }


        // line seperator
        end = i;
        int len = end - start;
        strncpy(tokens[num], data + start, len);
        tokens[num][len] = '\0';
        num++;
        start=end +1;
        i++;


    }

    if (start < i) {
        int len = i - start;
        strncpy(tokens[num], data + start, len);
        tokens[num][len] = '\0';
        num++;
    }

    int token_count = num;
    for (int i = 0; i<token_count;i++) {
        char* token = tokens[i];

        strcpy(token, cc_trim_whitespace(token));

        cc_ohlc_t _ohlc = {0};
        
        int start = 0;
        int end = 0;
        int j = 0;
        int num = 0;
        while (num <= 3) {
            if (token[j] != val_seperator && token[j] != '\0') {
                j++;
                continue;
            }

            // val seperator
            end = j;
            int len = end -start;
            char val_str[32];
            strncpy(val_str, token+start, len);
            val_str[len] = '\0';
            start=end +1;
            j++;

            double val = atof(val_str);
            if (num == 0) {
                _ohlc.open=val;
            } else if (num == 1) {
                _ohlc.high=val;
            } else if (num == 2) {
                _ohlc.low=val;
            } else if (num == 3) {
                _ohlc.close=val;
            }

            num++;
            
        }
        
        (*ohlc)[i] = _ohlc;
    }

    return 0;
}


static inline double find_min(double arr[], int n) {
	double min = arr[0];
	for (int i =1; i<n;i++) {
		if (arr[i] < min) {
			min = arr[i];
		}
	}
	return min;
}

static inline double find_max(double arr[], int n) {
	double  max = arr[0];
	for (int i =1; i<n; i++) {
		if (arr[i] > max) {
			max = arr[i];
		}
	}
	return max;
}

inline char* cc_line_create(const cc_ohlc_t* data, int size, int width, int height) {

    double closes[size];
    for (int i = 0; i < size; i++) {
        closes[i] = data[i].close;
    }

    double vals[width];
    // Her sütuna düşen ortalama değer:
    for (int w = 0; w < width; w++) {
        int start_idx = (w * size) / width;
        int end_idx = ((w + 1) * size) / width;
        if (end_idx <= start_idx) end_idx = start_idx + 1;

        double sum = 0.0;
        for (int k = start_idx; k < end_idx && k < size; k++) {
            sum += closes[k];
        }
        vals[w] = sum / (end_idx - start_idx);
    }

    double min = find_min(vals, width);
    double max = find_max(vals, width);
    double diff_range = max - min;

    char *color;
    double change = (size > 1) ? (closes[size-1] - closes[size-2]) : 0.0;
    if (change >= 0.0) {
        color = CC_COLOR_GREEN;
    } else {
        color = CC_COLOR_RED;
    }

    // Yatay çözünürlüğü 2 katına çıkar: her hücre = alt + üst yarım piksel
    int pixel_height = height * 2;

    // Her sütunun piksel cinsinden y konumu (0 = alt, pixel_height-1 = üst)
    int py[width];
    for (int i = 0; i < width; i++) {
        double t = (diff_range == 0.0) ? 0.5 : (vals[i] - min) / diff_range;
        py[i] = (int)lround(t * (pixel_height - 1));
        if (py[i] < 0) py[i] = 0;
        if (py[i] >= pixel_height) py[i] = pixel_height - 1;
    }

    // Her hücre ANSI kodu + UTF-8 blok + \0 alacak kadar yer tutsun (32 bayt yeterli)
    char columns[width][height][32];

    // mask[i][j]: bit0 = alt yarım piksel dolu, bit1 = üst yarım piksel dolu
    unsigned char mask[width][height];
    memset(mask, 0, sizeof(mask));

    // Çizgi: her sütunda, kendi noktası ile bir sonraki nokta arasındaki
    // pikselleri doldur (komşu sütunlar ortak pikseli paylaşır -> boşluk kalmaz)
    for (int i = 0; i < width; i++) {
        int lo = py[i];
        int hi = py[i];
        if (i + 1 < width) {
            if (py[i+1] < lo) lo = py[i+1];
            if (py[i+1] > hi) hi = py[i+1];
        }

        for (int p = lo; p <= hi; p++) {
            int cell = p / 2;
            int sub = p % 2;   // 0 = alt yarım, 1 = üst yarım
            mask[i][cell] |= (sub == 1) ? 2 : 1;
        }
    }

    // Maskeyi blok karakterlere dönüştür
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            const char *block;
            if (mask[i][j] == 3)      block = CC_BLOCK_FULL;        // her iki yarım da dolu
            else if (mask[i][j] == 2) block = CC_BLOCK_UPPER_HALF;  // sadece üst yarım
            else if (mask[i][j] == 1) block = CC_BLOCK_LOWER_HALF;  // sadece alt yarım
            else                      block = NULL;

            if (block != NULL) {
                snprintf(columns[i][j], sizeof(columns[i][j]), "%s%s%s", color, block, CC_COLOR_RESET);
            } else {
                snprintf(columns[i][j], sizeof(columns[i][j]), " ");
            }
        }
    }

    // columns to lines, then 1D text
    // Her satırda (width karakter + 1 adet '\n') + en sonda 1 adet '\0'
    size_t total_size = (size_t)width * height * 32 + height + 1;
    char* chart = (char*)calloc(total_size, sizeof(char));

    for (int y = height - 1; y >= 0; y--) { // Grafiği yukarıdan aşağıya tara
        for (int x = 0; x < width; x++) {
            strcat(chart, columns[x][y]);
        }
        strcat(chart, "\n");
    }

    return chart; // main içinde kullandıktan sonra free(chart) etmeyi unutma
}

inline char* cc_candle_create(const cc_ohlc_t* data, int size, int width, int height) {
	return "deneme amacli";
}

#ifdef __cplusplus
}
#endif
#endif
