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
#define CC_LINE_VERTICAL "\u2502"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// timestamp ekle
typedef struct cc_ohlc cc_ohlc_t;
typedef struct cc_settings {
    const char* rise_color;
    const char* fall_color;
    const char* bg_color;
} cc_settings_t;
cc_settings_t cc_settings_resolve(const cc_settings_t* settings);
int cc_str_to_ohlc(const char* data, int size, cc_ohlc_t** ohlc,
                   char val_seperator, char line_seperator);
char* cc_line_create(const cc_ohlc_t* data, int size, int width, int height,
                     const cc_settings_t* settings);
char* cc_candle_create(const cc_ohlc_t* data, int size, int width, int height,
                       const cc_settings_t* settings);
#ifdef __cplusplus
}
#endif
#endif
#ifdef CCHARTS_IMPLEMENTATION
#ifdef __cplusplus
extern "C" {
#endif
struct cc_ohlc {
    double open;
    double high;
    double low;
    double close;
};

inline cc_settings_t cc_settings_resolve(const cc_settings_t* settings) {
    cc_settings_t resolved = {
        .rise_color = (settings && settings->rise_color) ? settings->rise_color : CC_COLOR_GREEN,
        .fall_color = (settings && settings->fall_color) ? settings->fall_color : CC_COLOR_RED,
        .bg_color   = (settings && settings->bg_color)   ? settings->bg_color   : NULL,
    };
    return resolved;
}

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

static inline int cc_pixel(double val, double min, double max, double range, int pixel_height) {
    double t = (range == 0.0) ? 0.5 : (val - min) / range;
    int p = (int)lround(t * (pixel_height - 1));
    if (p < 0) p = 0;
    if (p >= pixel_height) p = pixel_height - 1;
    return p;
}

static inline void cc_render_cell(char out[32], unsigned char bodybits, unsigned char wickbits, const char* fg, const char* bg) {
    const char *block;
    if (bodybits == 3)          block = CC_BLOCK_FULL;        // her iki yarım da dolu
    else if (bodybits == 2)     block = CC_BLOCK_UPPER_HALF;  // sadece üst yarım
    else if (bodybits == 1)     block = CC_BLOCK_LOWER_HALF;  // sadece alt yarım
    else if (wickbits != 0)     block = CC_LINE_VERTICAL;     // ince fitil
    else                        block = NULL;

    if (block != NULL) {
        if (bg != NULL) {
            snprintf(out, 32, "%s%s%s%s", bg, fg, block, CC_COLOR_RESET);
        } else {
            snprintf(out, 32, "%s%s%s", fg, block, CC_COLOR_RESET);
        }
    } else {
        if (bg != NULL) {
            snprintf(out, 32, "%s %s", bg, CC_COLOR_RESET);
        } else {
            snprintf(out, 32, " ");
        }
    }
}

static cc_ohlc_t cc_agg_ohlc(const cc_ohlc_t* data, int start, int end) {
    cc_ohlc_t v = { .open = data[start].open, .high = data[start].high,
                    .low = data[start].low, .close = data[end - 1].close };
    for (int k = start + 1; k < end; k++) {
        if (data[k].high > v.high) v.high = data[k].high;
        if (data[k].low < v.low) v.low = data[k].low;
    }
    return v;
}

inline char* cc_line_create(const cc_ohlc_t* data, int size, int width, int height,
                            const cc_settings_t* settings) {

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

    cc_settings_t s = cc_settings_resolve(settings);

    const char *color;
    double change = (size > 1) ? (closes[size-1] - closes[size-2]) : 0.0;
    if (change >= 0.0) {
        color = s.rise_color;
    } else {
        color = s.fall_color;
    }

    // Yatay çözünürlüğü 2 katına çıkar: her hücre = alt + üst yarım piksel
    int pixel_height = height * 2;

    // Her sütunun piksel cinsinden y konumu (0 = alt, pixel_height-1 = üst)
    int py[width];
    for (int i = 0; i < width; i++) {
        py[i] = cc_pixel(vals[i], min, max, diff_range, pixel_height);
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
            cc_render_cell(columns[i][j], mask[i][j], 0, color, s.bg_color);
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

inline char* cc_candle_create(const cc_ohlc_t* data, int size, int width, int height,
                              const cc_settings_t* settings) {
    if (size <= 0 || width <= 0 || height <= 0) {
        return (char*)calloc(1, sizeof(char));
    }

    cc_settings_t s = cc_settings_resolve(settings);
    int pixel_height = height * 2;

    // Fitiller taşmasın diye aralık tüm high/low üzerinden
    double min = data[0].low;
    double max = data[0].high;
    for (int i = 1; i < size; i++) {
        if (data[i].low < min)  min = data[i].low;
        if (data[i].high > max) max = data[i].high;
    }
    double range = max - min;

    // body_mask[i][j]: bit0 = alt yarım piksel dolu, bit1 = üst yarım piksel dolu (gövde)
    // wick_mask[i][j]:  hücreden fitil geçiyor mu (ince çizgi)
    unsigned char body_mask[width][height];
    unsigned char wick_mask[width][height];
    memset(body_mask, 0, sizeof(body_mask));
    memset(wick_mask, 0, sizeof(wick_mask));

    const char* col_color[width];

    if (width >= size) {
        // Kalın gövde: her mum kendi kolon aralığına yayılır, cw>=2 ise son kolon boşluk
        for (int i = 0; i < size; i++) {
            int cs = (i * width) / size;
            int ce = ((i + 1) * width) / size;
            if (ce <= cs) ce = cs + 1;
            int gap = (ce - cs >= 2) ? (ce - 1) : ce;
            int cmid = cs + (gap - cs - 1) / 2;

            const char* color = (data[i].close >= data[i].open) ? s.rise_color : s.fall_color;

            int po = cc_pixel(data[i].open, min, max, range, pixel_height);
            int pc = cc_pixel(data[i].close, min, max, range, pixel_height);
            int ph = cc_pixel(data[i].high, min, max, range, pixel_height);
            int pl = cc_pixel(data[i].low, min, max, range, pixel_height);

            int blo = (po < pc) ? po : pc;
            int bhi = (po > pc) ? po : pc;

            for (int c = cs; c < ce; c++) {
                col_color[c] = color;
                if (c < gap) {
                    for (int p = blo; p <= bhi; p++) {
                        body_mask[c][p / 2] |= (p % 2) ? 2 : 1;
                    }
                }
            }

            // Fitil: ortadaki kolonda high..low
            for (int p = pl; p <= ph; p++) {
                wick_mask[cmid][p / 2] |= (p % 2) ? 2 : 1;
            }
        }
    } else {
        // Kompresyon: her kolon birden fazla mumu sanal muma indirger
        for (int w = 0; w < width; w++) {
            int ss = (w * size) / width;
            int se = ((w + 1) * size) / width;
            if (se <= ss) se = ss + 1;

            cc_ohlc_t v = cc_agg_ohlc(data, ss, se);
            col_color[w] = (v.close >= v.open) ? s.rise_color : s.fall_color;

            int po = cc_pixel(v.open, min, max, range, pixel_height);
            int pc = cc_pixel(v.close, min, max, range, pixel_height);
            int ph = cc_pixel(v.high, min, max, range, pixel_height);
            int pl = cc_pixel(v.low, min, max, range, pixel_height);

            int blo = (po < pc) ? po : pc;
            int bhi = (po > pc) ? po : pc;

            for (int p = blo; p <= bhi; p++) {
                body_mask[w][p / 2] |= (p % 2) ? 2 : 1;
            }
            for (int p = pl; p <= ph; p++) {
                wick_mask[w][p / 2] |= (p % 2) ? 2 : 1;
            }
        }
    }

    // Maskeyi blok karakterlere dönüştür
    char columns[width][height][32];
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            cc_render_cell(columns[i][j], body_mask[i][j], wick_mask[i][j], col_color[i], s.bg_color);
        }
    }

    // columns to lines, then 1D text
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

#ifdef __cplusplus
}
#endif
#endif
