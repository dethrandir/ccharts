package io.github.dethrandir.ccharts;

/**
 * How a chart is drawn.
 *
 * <p>Built with a fluent builder; every unset field takes the library default
 * (green rising, red falling, no background, no area fill, no labels):
 *
 * <pre>{@code
 * ChartOptions options = ChartOptions.builder()
 *         .size(60, 8)
 *         .rise(Color.BLUE)
 *         .showPrices(true)
 *         .build();
 * }</pre>
 */
public final class ChartOptions {

    /** Options with every field at its default. */
    public static final ChartOptions DEFAULTS = builder().build();

    private final int width;
    private final int height;
    private final String riseColor;
    private final String fallColor;
    private final String backgroundColor;
    private final String areaColor;
    private final boolean singleColor;
    private final boolean showPrices;
    private final boolean showTimes;

    private ChartOptions(Builder builder) {
        this.width = builder.width;
        this.height = builder.height;
        this.riseColor = builder.riseColor;
        this.fallColor = builder.fallColor;
        this.backgroundColor = builder.backgroundColor;
        this.areaColor = builder.areaColor;
        this.singleColor = builder.singleColor;
        this.showPrices = builder.showPrices;
        this.showTimes = builder.showTimes;
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

    String riseColor() {
        return riseColor;
    }

    String fallColor() {
        return fallColor;
    }

    String backgroundColor() {
        return backgroundColor;
    }

    String areaColor() {
        return areaColor;
    }

    boolean singleColor() {
        return singleColor;
    }

    boolean showPrices() {
        return showPrices;
    }

    boolean showTimes() {
        return showTimes;
    }

    /** Builder for {@link ChartOptions}. */
    public static final class Builder {
        private int width = 60;
        private int height = 8;
        private String riseColor;
        private String fallColor;
        private String backgroundColor;
        private String areaColor;
        private boolean singleColor;
        private boolean showPrices;
        private boolean showTimes;

        private Builder() {
        }

        /** Chart size in cells. Defaults to 60 by 8. */
        public Builder size(int width, int height) {
            this.width = width;
            this.height = height;
            return this;
        }

        /** Color for rising values and candles. */
        public Builder rise(Color color) {
            return riseAnsi(color == null ? null : color.escape());
        }

        /** Color for falling values and candles. */
        public Builder fall(Color color) {
            return fallAnsi(color == null ? null : color.escape());
        }

        /** Background color of empty cells. */
        public Builder background(Color color) {
            return backgroundAnsi(color == null ? null : color.escape());
        }

        /** Fill color below a line chart. */
        public Builder area(Color color) {
            return areaAnsi(color == null ? null : color.escape());
        }

        /** Rising color as a raw escape sequence (256-color, truecolor, ...). */
        public Builder riseAnsi(String escape) {
            this.riseColor = escape;
            return this;
        }

        /** Falling color as a raw escape sequence. */
        public Builder fallAnsi(String escape) {
            this.fallColor = escape;
            return this;
        }

        /** Background color as a raw escape sequence. */
        public Builder backgroundAnsi(String escape) {
            this.backgroundColor = escape;
            return this;
        }

        /** Area fill as a raw escape sequence. */
        public Builder areaAnsi(String escape) {
            this.areaColor = escape;
            return this;
        }

        /**
         * Draw the whole chart in one color chosen from the overall change,
         * instead of coloring each segment by its own direction.
         */
        public Builder singleColor(boolean yes) {
            this.singleColor = yes;
            return this;
        }

        /** Print max/min price labels in a left margin. */
        public Builder showPrices(boolean yes) {
            this.showPrices = yes;
            return this;
        }

        /** Print the first and last timestamp under the chart. */
        public Builder showTimes(boolean yes) {
            this.showTimes = yes;
            return this;
        }

        /** Builds the options. */
        public ChartOptions build() {
            return new ChartOptions(this);
        }
    }
}
