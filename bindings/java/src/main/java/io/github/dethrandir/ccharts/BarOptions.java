package io.github.dethrandir.ccharts;

/**
 * How a bar chart is drawn.
 *
 * <p>Built with a fluent builder. The defaults give green bars, no
 * background fill, and no label or value footer:
 *
 * <pre>{@code
 * BarOptions options = BarOptions.builder()
 *         .rise(Color.BLUE)
 *         .backgroundColor(Color.BRIGHT_BLACK)
 *         .showLabels(true)
 *         .showPrices(true)
 *         .build();
 * }</pre>
 */
public final class BarOptions {

    /** Options with every field at its default. */
    public static final BarOptions DEFAULTS = builder().build();

    private final String riseColor;
    private final String backgroundColor;
    private final boolean showLabels;
    private final boolean showPrices;
    private final boolean plain;

    private BarOptions(Builder builder) {
        this.riseColor = builder.riseColor;
        this.backgroundColor = builder.backgroundColor;
        this.showLabels = builder.showLabels;
        this.showPrices = builder.showPrices;
        this.plain = builder.plain;
    }

    /** A new builder. */
    public static Builder builder() {
        return new Builder();
    }

    /** ANSI escape for the bars, or {@code null} for the default green. */
    String riseColor() {
        return riseColor;
    }

    /** ANSI escape filling empty cells, or {@code null} for the terminal background. */
    String backgroundColor() {
        return backgroundColor;
    }

    /** Print each column's label in a footer row below the chart. */
    public boolean showLabels() {
        return showLabels;
    }

    /** Print the max bar value and 0 (the baseline) in a left value-axis margin. */
    public boolean showPrices() {
        return showPrices;
    }

    /** Render with no ANSI escapes at all, overriding every color. */
    public boolean plain() {
        return plain;
    }

    /** Builder for {@link BarOptions}. */
    public static final class Builder {
        private String riseColor;
        private String backgroundColor;
        private boolean showLabels;
        private boolean showPrices;
        private boolean plain;

        private Builder() {
        }

        /** ANSI escape color for the bars. */
        public Builder rise(Color color) {
            return riseAnsi(color == null ? null : color.escape());
        }

        /** ANSI escape color filling empty cells. */
        public Builder backgroundColor(Color color) {
            return backgroundColorAnsi(color == null ? null : color.escape());
        }

        /** Bar color as a raw escape sequence (256-color, truecolor, ...). */
        public Builder riseAnsi(String escape) {
            this.riseColor = escape;
            return this;
        }

        /** Background color as a raw escape sequence. */
        public Builder backgroundColorAnsi(String escape) {
            this.backgroundColor = escape;
            return this;
        }

        /** Print each column's label in a footer row below the chart. Default false. */
        public Builder showLabels(boolean yes) {
            this.showLabels = yes;
            return this;
        }

        /** Print the max bar value and 0 (the baseline) in a left margin. Default false. */
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
        public BarOptions build() {
            return new BarOptions(this);
        }
    }
}