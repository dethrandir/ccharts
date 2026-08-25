package io.github.dethrandir.ccharts;

/**
 * How a histogram is drawn.
 *
 * <p>Built with a fluent builder. The defaults give green bars, an auto
 * bin count chosen from the sample count, an auto value window (the data
 * min/max), and no labels:
 *
 * <pre>{@code
 * HistogramOptions options = HistogramOptions.builder()
 *         .size(40, 6)
 *         .color(Color.BLUE)
 *         .binCount(10)
 *         .build();
 * }</pre>
 */
public final class HistogramOptions {

    /** Options with every field at its default. */
    public static final HistogramOptions DEFAULTS = builder().build();

    private final int width;
    private final int height;
    private final String color;
    private final String backgroundColor;
    private final int binCount;
    private final double minValue;
    private final double maxValue;
    private final boolean showBins;
    private final boolean showPrices;
    private final boolean plain;

    private HistogramOptions(Builder builder) {
        this.width = builder.width;
        this.height = builder.height;
        this.color = builder.color;
        this.backgroundColor = builder.backgroundColor;
        this.binCount = builder.binCount;
        this.minValue = builder.minValue == null ? Double.NaN : builder.minValue;
        this.maxValue = builder.maxValue == null ? Double.NaN : builder.maxValue;
        this.showBins = builder.showBins;
        this.showPrices = builder.showPrices;
        this.plain = builder.plain;
    }

    /** A new builder. */
    public static Builder builder() {
        return new Builder();
    }

    /** Chart width in cells. */
    public int width() {
        return width;
    }

    /** Chart height in cells. */
    public int height() {
        return height;
    }

    /** ANSI escape for the bars, or {@code null} for the default green. */
    String color() {
        return color;
    }

    /** ANSI escape filling empty cells, or {@code null} for the terminal background. */
    String backgroundColor() {
        return backgroundColor;
    }

    /**
     * Number of equal-width bins; {@code 0} selects a value from the sample
     * count, bounded by the chart width.
     */
    public int binCount() {
        return binCount;
    }

    /** Lower edge of the value window; NaN means use the data minimum. */
    public double minValue() {
        return minValue;
    }

    /** Upper edge of the value window; NaN means use the data maximum. */
    public double maxValue() {
        return maxValue;
    }

    /** Print a value-axis footer row (window min left, max right). */
    public boolean showBins() {
        return showBins;
    }

    /** Print the max-count / min-count labels in a left margin. */
    public boolean showPrices() {
        return showPrices;
    }

    /** Render with no ANSI escapes at all, overriding every color. */
    public boolean plain() {
        return plain;
    }

    /** Builder for {@link HistogramOptions}. */
    public static final class Builder {
        private int width = 60;
        private int height = 8;
        private String color;
        private String backgroundColor;
        private int binCount;
        private Double minValue;
        private Double maxValue;
        private boolean showBins;
        private boolean showPrices;
        private boolean plain;

        private Builder() {
        }

        /** Chart size in cells. Defaults to 60 by 8. */
        public Builder size(int width, int height) {
            this.width = width;
            this.height = height;
            return this;
        }

        /** ANSI escape color for the bars. */
        public Builder color(Color color) {
            return colorAnsi(color == null ? null : color.escape());
        }

        /** ANSI escape color filling empty cells. */
        public Builder backgroundColor(Color color) {
            return backgroundColorAnsi(color == null ? null : color.escape());
        }

        /** Bar color as a raw escape sequence (256-color, truecolor, ...). */
        public Builder colorAnsi(String escape) {
            this.color = escape;
            return this;
        }

        /** Background color as a raw escape sequence. */
        public Builder backgroundColorAnsi(String escape) {
            this.backgroundColor = escape;
            return this;
        }

        /**
         * Number of equal-width bins; {@code 0} (the default) selects a value
         * from the sample count, bounded by the chart width.
         */
        public Builder binCount(int count) {
            this.binCount = count;
            return this;
        }

        /**
         * Lower edge of the value window; {@code null} (the default, NaN) uses
         * the data minimum. When set, must be strictly less than
         * {@link #maxValue(Double)}.
         */
        public Builder minValue(Double value) {
            this.minValue = value;
            return this;
        }

        /**
         * Upper edge of the value window; {@code null} (the default, NaN) uses
         * the data maximum. When set, must be strictly greater than
         * {@link #minValue(Double)}.
         */
        public Builder maxValue(Double value) {
            this.maxValue = value;
            return this;
        }

        /** Print a value-axis footer row (window min left, max right). */
        public Builder showBins(boolean yes) {
            this.showBins = yes;
            return this;
        }

        /** Print the max-count / min-count labels in a left margin. */
        public Builder showPrices(boolean yes) {
            this.showPrices = yes;
            return this;
        }

        /**
         * Render with no ANSI escapes at all, overriding every color. Use it
         * when the chart is going somewhere that does not interpret escapes —
         * a log file, an HTML block, a commit message.
         */
        public Builder plain(boolean yes) {
            this.plain = yes;
            return this;
        }

        /** Builds the options. */
        public HistogramOptions build() {
            return new HistogramOptions(this);
        }
    }
}