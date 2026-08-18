#define CCHARTS_IMPLEMENTATION
#include "ccharts.h"

int main() {
    // THYAO 1m (fl market history THYAO 1m 1d --json)
    const char data[] = 
        "328.75,330.0,323.75,328.0\n"
        "330.0,330.25,317.5,317.5\n"
        "330.0,330.25,317.5,317.5\n"
        "317.25,321.0,314.5,321.0\n"
        "317.25,321.0,314.5,321.0\n"
        "318.25,319.5,310.25,312.75\n"
        "318.25,319.5,310.25,312.75\n"
        "312.25,315.5,311.25,312.0\n"
        "320.0,328.75,317.75,318.0\n"
        "319.5,325.25,316.25,319.0\n"
        "317.5,318.25,310.5,313.25\n"
        "317.5,318.25,310.5,313.25\n"
        "311.0,315.5,305.25,308.25\n"
        "320.5,322.25,316.5,317.0\n"
        "320.5,322.25,316.5,317.0\n"
        "317.0,325.5,314.5,323.75\n"
        "317.0,325.5,314.5,323.75\n"
        "318.5,318.5,311.5,314.0\n"
        "314.75,318.25,311.25,311.25\n"
        "312.0,312.25,303.25,306.25\n"
        "312.0,312.25,303.25,306.25\n"
        "307.0,307.75,302.25,303.0\n"
        "302.5,303.5,295.25,301.0\n"
        "301.0,310.5,300.5,308.75\n"
        "301.0,310.5,300.5,308.75\n"
        "308.75,310.5,306.75,308.0\n"
        "308.75,310.5,306.75,308.0\n"
        "308.5,309.5,305.25,305.25\n"
        "308.5,309.5,305.25,305.25\n"
        "306.0,307.25,302.75,303.0";

    int candle_count = 30;
    int chart_width = 60;
    int chart_height = 8;

    cc_ohlc_t* ohlc = NULL;
    cc_str_to_ohlc(data, candle_count, &ohlc, ',', '\n');

    if (ohlc != NULL) {
        cc_settings_t s = { .rise_color = CC_COLOR_BLUE, .fall_color = CC_COLOR_RED, .bg_color = CC_COLOR_BRIGHT_BLACK };
        char* output = cc_line_create(ohlc, candle_count, chart_width, chart_height, &s);
        
        if (output != NULL) {
            printf("%s\n", output);
            free(output); // cc_line_create içindeki malloc/calloc temizliği
        }
        
        free(ohlc);
    }
    
    return 0;
}

// #define CCHARTS_IMPLEMENTATION
// #include "ccharts.h"
// 
// int main() {
// 	// TODO : OHLC DATASINA OPSIYONEL TIMESTAMP EKLE (terminalde renderlemesi zor olur ama en azindan gunleri veya isaretleri gostermek ve araliklari hesaplamaya yardimci olmak icin faydali)
//     const char data[] = "321.50,322.8,312.6,314.25";
//     cc_ohlc_t* ohlc;
//     cc_str_to_ohlc(data, sizeof(data), &ohlc, ',','\n');
//     double close = ohlc[0].close;
//     printf("%.2f\n",close);
// 
// 	auto output = cc_line_create(ohlc, 1, 1, 1);
// 	printf(output);
//     
//     free(ohlc);
//     return 0;
// }
