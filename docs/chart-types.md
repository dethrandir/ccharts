# Chart Types Reference

`ccharts` provides 9 distinct chart types tailored for terminal, console, and plain text rendering. Every chart is rendered using specialized Unicode block characters for optimal sub-pixel density and aesthetic clarity.

---

## Summary of Chart Types

| Chart Type | Category | Input Shape | Sub-pixel Resolution | Primary Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Line** | Financial / Time Series | OHLC / Close prices + TS | 8 vertical sub-pixels / cell (`▁..█`) | Price trend curves, financial closes |
| **Candlestick** | Financial / OHLC | Open, High, Low, Close + TS | 2 vertical sub-pixels + thin wick (`│`) | Asset price action & volatility |
| **Pie / Donut** | Part-to-Whole | Slices `(label, value)` | Radial sub-pixel grid + Aspect Ratio | Budget allocation, asset distributions |
| **Histogram** | Statistical / Distribution | 1-D Scalar sample array | 8 vertical sub-pixels / cell (`▁..█`) | Frequency distributions, value spreads |
| **Sparkline** | Compact Trend | 1-D Scalar sample array | 8 vertical sub-pixels / cell (`▁..█`) | Inline metrics, compact micro-charts |
| **Bar** | Categorical Comparison | Ordered `(label, value)` pairs | 8 vertical sub-pixels / cell (`▁..█`) | Discrete category comparisons |
| **Stacked Bar** | Multi-Series Part-to-Whole | Series $\times$ Categories matrix | 8 vertical sub-pixels / cell (`▁..█`) | Multi-segment category breakdowns |
| **Heatmap** | 2-D Matrix Density | 2-D Numeric matrix (rows $\times$ cols)| 1 cell / matrix element + 10-stop ramp | Correlation matrices, 2-D heat distributions |
| **Box Plot** | Statistical Distribution | Grouped categorical samples | 8 vertical sub-pixels / cell (`▁..█`) | Five-number summary distribution comparison |

---

## 1. Line Chart (`line`)

### Description & Mechanics
Renders smooth price curves over a timeline.
- **Vertical Resolution**: Each character cell is split into 8 vertical levels using the Unicode lower block characters (`▁▂▃▄▅▆▇█`, `U+2581` to `U+2588`), providing 8× vertical precision.
- **Downsampling**: When output `width < data_len`, adjacent close prices are averaged over each column span (`span_mean`).
- **Coloring**: Individual segments can be colored dynamically based on rising/falling movement, or drawn in a unified single color (`single_color`).
- **Area Fill**: Optional fill under the line using `area_color`.

### Visual Output
```text
  328.00████████████                                                
                   █                                                
                   █           █████████████▁▁▁▁▁▁▁▁▁▁▁▁            
                   █████████████                       █            
                                                       █            
                                                       █            
                                                       █            
  301.00                                               █▁▁▁▁▁▁▁▁▁▁▁▁
2026-07-20                                                2026-07-24
```

### Options & Parameters

| Option | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `width` | `int` | `60` | Width of the chart plot area in cells |
| `height` | `int` | `8` | Height of the chart plot area in cells |
| `rise_color` | `string` | Green (`\x1b[32m`) | Color for rising segments |
| `fall_color` | `string` | Red (`\x1b[31m`) | Color for falling segments |
| `bg_color` | `string` | `None` | Background color for empty cells |
| `area_color` | `string` | `None` | Fill color for the region beneath the curve |
| `single_color` | `bool` | `false` | When true, renders entire line using one color based on net change |
| `show_prices` | `bool` | `false` | Prepends an 8-column price axis with max and min price labels |
| `show_times` | `bool` | `false` | Appends a timestamp footer formatted by interval (`YYYY-MM-DD` or `HH:MM`) |
| `plain` | `bool` | `false` | When true, strips all ANSI escapes for clean plain-text output |

---

## 2. Candlestick Chart (`candle`)

### Description & Mechanics
Renders Japanese candlestick charts representing Open, High, Low, and Close (OHLC).
- **Body & Wick Rendering**: Uses upper/lower half blocks (`▀`, `▄`, `█`) for candle bodies and vertical line glyphs (`│`, `U+2502`) for the upper and lower wicks (shadows).
- **Bitmask Grid**: Each cell tracks body and wick presence in 2-sub-pixel vertical resolution (bit 0 = lower half, bit 1 = upper half). Body rendering takes precedence over wicks.
- **Spacing & Aggregation**: When `width >= size`, candles have multi-cell widths separated by a 1-column gap. When `width < size`, virtual candles are aggregated via `cc_agg_ohlc` (first Open, highest High, lowest Low, last Close in window).

### Visual Output
```text
  330.25▄▄▄▄▄▄▄▄▄▄▄ ███████████                  │                  
             │      ███████████                  │                  
                    ███████████ ▄▄▄▄▄▄▄▄▄▄▄ ▄▄▄▄▄▄▄▄▄▄▄             
                    ▀▀▀▀▀▀▀▀▀▀▀ ███████████ ▀▀▀▀▀▀▀▀▀▀▀             
                                     │                              
                                                                    
                                                        ███████████ 
  300.75                                                ███████████ 
2026-07-20                                                2026-07-24
```

### Options & Parameters
Shares the same configuration options as the **Line Chart** (`rise_color`, `fall_color`, `bg_color`, `show_prices`, `show_times`, `plain`).

---

## 3. Pie & Donut Chart (`pie`)

### Description & Mechanics
Renders circular proportional charts from a series of `(label, value)` slices ($value > 0$).
- **Aspect Ratio Compensation**: Terminals have non-square character cells (typically ~2:1 height-to-width). `ccharts` applies an elliptical correction factor so rendered disks look circular rather than squished.
- **Center Sampling**: Each cell's center is mapped to polar coordinates $(r, \theta)$ to select the corresponding slice.
- **Donut Hole**: When `donut=true` or `inner_radius_ratio > 0`, cells inside the inner radius are hollowed out.

### Visual Output
```text
        ████████        
     ██████████████     
   ██████████████████   
  ██████        ██████  
  █████   Core   █████  
  █████          █████  
  ██████        ██████  
   ██████████████████   
     ██████████████     
        ████████        
Rent   40 (40%)
Food   25 (25%)
Tech   15 (15%)
Other  20 (20%)
```

### Options & Parameters

| Option | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `donut` | `bool` | `false` | Enables a hollow donut center (default 0.5 ratio) |
| `inner_radius_ratio` | `float` | `-1.0` (auto) | Custom inner hole ratio in range `[0.0, 1.0]` |
| `slice_gap` | `float` | `0.0` | Angular gap between slices in radians |
| `start_angle` | `float` | `π / 2` | Starting angle in radians (`π/2` = 12 o'clock) |
| `counter_clockwise` | `bool` | `false` | `false` for CCW sweep, `true` for CW (clockwise) |
| `legend_format` | `int` | `0` | Legend style: `0` (Label Value %), `1` (Label %), `2` (Value %), `3` (Label only) |
| `center_text` | `string` | `None` | Centered text drawn inside donut hole |
| `colors` | `string[]` | Default Palette | Per-slice ANSI color overrides |
| `show_legend` | `bool` | `true` | Appends slice legend rows beneath the disk |
| `show_pct` | `bool` | `false` | Appends percentage calculation to legend format 0 |

---

## 4. Histogram (`histogram` / `hist`)

### Description & Mechanics
Visualizes the frequency distribution of a continuous 1-D sequence of numerical observations.
- **Binning**: Divides values into equal-width intervals across `[min_value, max_value]`. Defaults to 20 bins for $\ge 40$ samples, or 10 bins otherwise.
- **Outlier Clamping**: Extreme values beyond `min_value` or `max_value` are clamped into the edge bins.
- **8-Level Sub-pixel Bars**: Each column bar is scaled to the peak frequency and rendered with 8-level lower blocks (`▁..█`).

### Visual Output
```text
       8                ████                    
                        ████                    
                        ████                    
                        ████████████            
                        ████████████            
                        ████████████            
        ████████████████████████████████        
       1████████████████████████████████████████
3.10                                        7.90
```

### Options & Parameters

| Option | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `bin_count` | `int` | `0` (auto) | Number of frequency bins |
| `min_value` | `float` | `NaN` (auto) | Left boundary of value window |
| `max_value` | `float` | `NaN` (auto) | Right boundary of value window |
| `rise_color` | `string` | Green | Bar fill color |
| `bg_color` | `string` | `None` | Background of empty cells |
| `show_bins` | `bool` | `false` | Appends value axis footer (`min_value` left, `max_value` right) |
| `show_prices` | `bool` | `false` | Prepends an 8-column frequency count axis (peak count to 1) |

---

## 5. Sparkline (`sparkline` / `spark`)

### Description & Mechanics
Ultra-compact, axis-less micro-chart designed for inline display in dashboards, status tables, and log lines.
- **Ultra-low Height**: Defaults to `height=1` (can be set to 2 or 3).
- **8 Sub-pixels per Cell**: Provides smooth micro-curves even in a single terminal line.
- **Padding Margins**: `min_above` and `min_below` reserve sub-pixel margins to prevent clipping.

### Visual Output
```text
 ▂▃▅▆▇██▇▆▅▄▃▂  ▂▄▆█
```
With `area_color` and `height=2`:
```text
    ▄▆██▇▅▃ 
████████████
```

### Options & Parameters

| Option | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `width` | `int` | `60` | Width in character columns |
| `height` | `int` | `1` | Height in character rows (typically 1 to 3) |
| `rise_color` | `string` | Green | Trend line color |
| `area_color` | `string` | `None` | Under-line fill color |
| `min_above` | `int` | `0` | Reserved sub-pixels at the top edge |
| `min_below` | `int` | `0` | Reserved sub-pixels at the bottom edge |

---

## 6. Bar Chart (`bar`)

### Description & Mechanics
Renders discrete categorical bars extending upward from a shared zero baseline.
- **Direct Scaling**: Each bar's height is proportional to its own value relative to the maximum bar in the dataset.
- **Sub-pixel Tops**: Bar tops use 8-level fractional block characters (`▁..█`).
- **Folding**: When `width < items_count`, columns deterministically aggregate categories using `MAX`.

### Visual Output
```text
  100.00            ████████████                        
                    ████████████                        
                    ████████████            ████████████
        ████████████████████████            ████████████
        ████████████████████████            ████████████
        ████████████████████████████████████████████████
        ████████████████████████████████████████████████
    0.00████████████████████████████████████████████████
        Q1          Q2          Q3          Q4          
```

### Options & Parameters

| Option | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `color` / `rise_color` | `string` | Green | Fill color of the bars |
| `bg_color` | `string` | `None` | Background color for empty cells above bars |
| `show_labels` | `bool` | `false` | Appends a footer row containing categorical labels |
| `show_prices` | `bool` | `false` | Prepends an 8-column value axis (max value top, 0 bottom) |

---

## 7. Stacked Bar Chart (`stacked_bar` / `stack`)

### Description & Mechanics
Displays multi-series categorical data where each vertical bar represents the sum of multiple segments.
- **Part-to-Whole Breakdown**: Shows both the total categorical volume and the individual series breakdown.
- **Series Palette**: Each series is assigned a distinct color from a deterministic 16-color palette or a custom list of ANSI colors.
- **8-Level Precision**: Segments transition smoothly across sub-pixel boundaries.

### Visual Output
```text
  150.00            ████████████                        
                    ████████████                        
                    ████████████            ████████████
        ████████████████████████            ████████████
        ████████████████████████            ████████████
        ████████████████████████████████████████████████
        ████████████████████████████████████████████████
    0.00████████████████████████████████████████████████
        North       South       East        West        
```

### Options & Parameters

| Option | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `colors` | `string[]` | Default Palette | ANSI escape string per series |
| `bg_color` | `string` | `None` | Background for empty cells above stacks |
| `category_labels` | `string[]` | `None` | Category labels printed in footer |
| `show_labels` | `bool` | `false` | Appends category labels footer |
| `show_prices` | `bool` | `false` | Prepends value axis (peak stack sum top, 0 bottom) |

---

## 8. Heatmap (`heatmap` / `heat`)

### Description & Mechanics
Renders a 2-D matrix of scalar values as a color-intensity grid.
- **10-Step Deterministic Colormap Ladder**: Normalizes matrix values to $[0.0, 1.0]$ between matrix min and max:
  - `0`: Low Color (Default: Bright Black / Gray)
  - `1`: Blue
  - `2`: Cyan
  - `3`: Bright Cyan
  - `4`: Green
  - `5`: Middle Color (Default: Yellow)
  - `6`: Bright Yellow
  - `7`: Red
  - `8`: Bright Red
  - `9`: High Color (Default: Bright White)
- **2-D Block-Average Resizing**: When matrix size exceeds grid dimensions, cells display the average value of their covered window.
- **Label Grid**: Optional row labels (left) and column labels (bottom).

### Visual Output
```text
Mon █ █ █ █ █ █ █
Tue █ █ █ █ █ █ █
Wed █ █ █ █ █ █ █
Thu █ █ █ █ █ █ █
Fri █ █ █ █ █ █ █
    00 04 08 12 16 20
```

### Options & Parameters

| Option | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `low_color` | `string` | Bright Black (`\x1b[90m`) | Color for minimum matrix value (index 0) |
| `high_color` | `string` | Bright White (`\x1b[97m`) | Color for maximum matrix value (index 9) |
| `mid_color` | `string` | `None` (Yellow) | Optional 3-stop center ramp color override (index 5) |
| `bg_color` | `string` | `None` | Padding background when matrix is smaller than grid |
| `row_labels` | `string[]` | `None` | Labels for matrix rows |
| `col_labels` | `string[]` | `None` | Labels for matrix columns |
| `show_labels` | `bool` | `false` | Enables row and column label framing |

---

## 9. Box Plot (`boxplot` / `box`)

### Description & Mechanics
Renders statistical distributions for one or more categories using five-number summaries: $[Min, Q_1, Median, Q_3, Max]$.
- **Nearest-Rank Quartiles**:
  $$\begin{aligned}
  Min &= s[0] \\
  Q_1 &= s[\lfloor (n-1) \times 0.25 \rfloor] \\
  Median &= s[\lfloor (n-1) \times 0.50 \rfloor] \\
  Q_3 &= s[\lfloor (n-1) \times 0.75 \rfloor] \\
  Max &= s[n-1]
  \end{aligned}$$
- **Glyphs**:
  - **Median line**: Solid full block `█` in `rise_color`.
  - **Interquartile Box ($Q_1..Q_3$)**: Solid blocks and fractional edges.
  - **Whiskers ($Min..Q_1$ and $Q_3..Max$)**: Thin vertical line `│` in `area_color` or `rise_color`.
- **Global Scaling**: All categories are scaled onto a unified vertical price span $[gmin, gmax]$.

### Visual Output
```text
  120.00                    │                   
                            │                   
                  │         ████████████        
                  ▃         ████████████        
                  █         ████████████        
        ▂         █         ████████████        
        █         █                             
   10.00█         │                             
        Group A   Group B   Group C             
```

### Options & Parameters

| Option | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `color` / `rise_color` | `string` | Green | Color of the $Q_1..Q_3$ box and median bar |
| `area_color` | `string` | `None` (shares `rise_color`) | Color of the $Min$ and $Max$ whiskers |
| `bg_color` | `string` | `None` | Background of empty cells |
| `show_prices` | `bool` | `false` | Prepends an 8-column value axis with global max and min |

---

## Code Examples

### Python
```python
from ccharts import Chart

# 1. Line & Candle
chart = Chart.from_arrays(opens, highs, lows, closes, ts=timestamps)
print(chart.line(width=60, height=8, show_prices=True))
print(chart.candle(width=60, height=8, show_prices=True))

# 2. Pie / Donut
print(Chart.pie(["A", "B", "C"], [30, 50, 20], donut=True, center_text="Total"))

# 3. Histogram
print(Chart.histogram([1.2, 2.3, 2.5, 3.1, 4.5, 5.0], bin_count=5, show_bins=True))

# 4. Sparkline
print(Chart.sparkline([10, 12, 15, 14, 18, 22, 19, 25], height=1))

# 5. Categorical Bar
print(Chart.bar(["Jan", "Feb", "Mar"], [100, 150, 120], show_labels=True))

# 6. Stacked Bar
series = [
    {"name": "Product A", "values": [40, 55, 70]},
    {"name": "Product B", "values": [30, 25, 45]},
]
print(Chart.stacked_bar(series, category_labels=["Q1", "Q2", "Q3"], show_labels=True))

# 7. Heatmap
matrix = [
    [10, 20, 30],
    [40, 50, 60],
    [70, 80, 90]
]
print(Chart.heatmap(matrix, show_labels=True, row_labels=["R1", "R2", "R3"]))

# 8. Box Plot
box_data = [
    {"name": "Treatment", "samples": [12, 14, 15, 18, 19, 24, 28]},
    {"name": "Control", "samples": [8, 9, 11, 12, 13, 15, 16]}
]
print(Chart.boxplot(box_data, show_prices=True))
```

### Rust
```rust
use ccharts::{Chart, Color, Settings, PieSlice, PieOptions, HistogramOptions};

// 1. Line & Candle
let chart = Chart::from_arrays(&opens, &highs, &lows, &closes, None)?;
println!("{}", chart.line(60, 8, &Settings::new().rise(Color::Blue))?);

// 2. Pie
let slices = [
    PieSlice::new(Some("A"), 40.0),
    PieSlice::new(Some("B"), 60.0),
];
println!("{}", Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true))?);

// 3. Histogram
let samples = [1.0, 2.0, 2.5, 3.0, 4.0, 5.0];
println!("{}", Chart::histogram(&samples, 40, 8, &HistogramOptions::new().show_bins(true))?);
```
