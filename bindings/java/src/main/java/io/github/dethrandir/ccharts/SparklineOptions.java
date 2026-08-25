package io.github.dethrandir.ccharts;

/**
 * How a sparkline is drawn.
 *
 * <p>Built with a fluent builder. The defaults give a one-row-high line that
 * follows the data, with no margins and the default rising color:
 *
 * <pre>{@code
 * SparklineOptions options = SparklineOptions.builder()
 *         .size(40, 2)
 *         .rise(Color.BLUE)
 *         .area(Color.BRIGHT_BLACK)
 *         .build();
 * }</pre>
 */
public final class SparklineOptions {

    /** Options with every field at its default. */
    public static final SparklineOptions DEFAULTS = builder().build();

    private final int width;
    private final int height;
    private final String riseColor;
    private final String areaColor;
    private final int minAbove;
    private final int minBelow;
    private final boolean plain;

    private SparklineOptions(Builder builder) {
        this.width = builder.width;
        this.height = builder.height;
        this.riseColor = builder.riseColor;
        this.areaColor = builder.areaColor;
        this.minAbove = builder.minAbove;
        this.minBelow = builder.minBelow;
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

    /** ANSI escape for the rising line, or {@code null} for the default green. */
    String riseColor() {
        return riseColor;
    }

    /** ANSI escape filling the area below the line, or {@code null} for none. */
    String areaColor() {
        return areaColor;
    }

    /** Sub-pixels reserved above the line so it does not clip. */
    public int minAbove() {
        return minAbove;
    }

    /** Sub-pixels reserved below the line so it does not clip. */
    public int minBelow() {
        return minBelow;
    }

    /** Render with no ANSI escapes at all, overriding every color. */
    public boolean plain() {
        return plain;
    }

    /** Builder for {@link SparklineOptions}. */
    public static final class Builder {
        private int width = 24;
        private int height = 1;
        private String riseColor;
        private String areaColor;
        private int minAbove;
        private int minBelow;
        private boolean plain;

        private Builder() {
        }

        /** Chart size in cells. Defaults to 24 by 1. */
        public Builder size(int width, int height) {
            this.width = width;
            this.height = height;
            return this;
        }

        /** ANSI escape color for the rising line. */
        public Builder rise(Color color) {
            return riseAnsi(color == null ? null : color.escape());
        }

        /** ANSI escape color filling the area below the line. */
        public Builder area(Color color) {
            return areaAnsi(color == null ? null : color.escape());
        }

        /** Rising-line color as a raw escape sequence (256-color, truecolor, ...). */
        public Builder riseAnsi(String escape) {
            this.riseColor = escape;
            return this;
        }

        /** Area-fill color as a raw escape sequence. */
        public Builder areaAnsi(String escape) {
            this.areaColor = escape;
            return this;
        }

        /** Sub-pixels reserved above the line so it does not clip. Default 0. */
        public Builder minAbove(int subPixels) {
            this.minAbove = subPixels;
            return this;
        }

        /** Sub-pixels reserved below the line so it does not clip. Default 0. */
        public Builder minBelow(int subPixels) {
            this.minBelow = subPixels;
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
        public SparklineOptions build() {
            return new SparklineOptions(this);
        }
    }
}