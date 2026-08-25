package io.github.dethrandir.ccharts;

/**
 * How a box plot is drawn.
 *
 * <p>Built with a fluent builder. The defaults use a green box/median line,
 * whiskers sharing the box color, no background fill, and no value axis:
 *
 * <pre>{@code
 * BoxplotOptions options = BoxplotOptions.builder()
 *         .riseColor(Color.BLUE)
 *         .areaColor(Color.BRIGHT_BLACK)
 *         .showPrices(true)
 *         .build();
 * }</pre>
 */
public final class BoxplotOptions {

    /** Options with every field at its default. */
    public static final BoxplotOptions DEFAULTS = builder().build();

    private final String riseColor;
    private final String areaColor;
    private final String backgroundColor;
    private final boolean showPrices;
    private final boolean plain;

    private BoxplotOptions(Builder builder) {
        this.riseColor = builder.riseColor;
        this.areaColor = builder.areaColor;
        this.backgroundColor = builder.backgroundColor;
        this.showPrices = builder.showPrices;
        this.plain = builder.plain;
    }

    /** A new builder. */
    public static Builder builder() {
        return new Builder();
    }

    /** ANSI escape for the box and median line, or {@code null} for green. */
    String riseColor() {
        return riseColor;
    }

    /** ANSI escape for the whiskers, or {@code null} to share the box color. */
    String areaColor() {
        return areaColor;
    }

    /** ANSI escape filling cells above/below a box, or {@code null} for the terminal background. */
    String backgroundColor() {
        return backgroundColor;
    }

    /** Print the global max/min value labels in a left margin. */
    public boolean showPrices() {
        return showPrices;
    }

    /** Render with no ANSI escapes at all, overriding every color. */
    public boolean plain() {
        return plain;
    }

    /** Builder for {@link BoxplotOptions}. */
    public static final class Builder {
        private String riseColor;
        private String areaColor;
        private String backgroundColor;
        private boolean showPrices;
        private boolean plain;

        private Builder() {
        }

        /** ANSI color for the box and median line. Default green. */
        public Builder riseColor(Color color) {
            return riseColorAnsi(color == null ? null : color.escape());
        }

        /** Box/median color as a raw escape sequence (256-color, truecolor, ...). */
        public Builder riseColorAnsi(String escape) {
            this.riseColor = escape;
            return this;
        }

        /** ANSI color for the whiskers. Default: same as {@link #riseColor}. */
        public Builder areaColor(Color color) {
            return areaColorAnsi(color == null ? null : color.escape());
        }

        /** Whisker color as a raw escape sequence. */
        public Builder areaColorAnsi(String escape) {
            this.areaColor = escape;
            return this;
        }

        /** ANSI color filling the empty cells above/below a box. */
        public Builder backgroundColor(Color color) {
            return backgroundColorAnsi(color == null ? null : color.escape());
        }

        /** Background color as a raw escape sequence. */
        public Builder backgroundColorAnsi(String escape) {
            this.backgroundColor = escape;
            return this;
        }

        /** Print the global max/min value labels in a left margin. Default false. */
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
        public BoxplotOptions build() {
            return new BoxplotOptions(this);
        }
    }
}
