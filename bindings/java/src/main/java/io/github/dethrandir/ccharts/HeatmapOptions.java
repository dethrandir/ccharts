package io.github.dethrandir.ccharts;

/**
 * How a heatmap is drawn.
 *
 * <p>Built with a fluent builder. The defaults use the fixed deterministic
 * colormap ladder, no background fill, and no row/column labels:
 *
 * <pre>{@code
 * HeatmapOptions options = HeatmapOptions.builder()
 *         .lowColor(Color.BLUE)
 *         .highColor(Color.RED)
 *         .rowLabels("R1", "R2", "R3")
 *         .showLabels(true)
 *         .build();
 * }</pre>
 */
public final class HeatmapOptions {

    /** Options with every field at its default. */
    public static final HeatmapOptions DEFAULTS = builder().build();

    private final String lowColor;
    private final String highColor;
    private final String midColor;
    private final String backgroundColor;
    private final String[] rowLabels;
    private final String[] colLabels;
    private final boolean showLabels;
    private final boolean plain;

    private HeatmapOptions(Builder builder) {
        this.lowColor = builder.lowColor;
        this.highColor = builder.highColor;
        this.midColor = builder.midColor;
        this.backgroundColor = builder.backgroundColor;
        this.rowLabels = builder.rowLabels;
        this.colLabels = builder.colLabels;
        this.showLabels = builder.showLabels;
        this.plain = builder.plain;
    }

    /** A new builder. */
    public static Builder builder() {
        return new Builder();
    }

    /** ANSI escape for the matrix minimum, or {@code null} for the ladder's low end. */
    String lowColor() {
        return lowColor;
    }

    /** ANSI escape for the matrix maximum, or {@code null} for the ladder's high end. */
    String highColor() {
        return highColor;
    }

    /** ANSI escape for the ladder's middle entry, or {@code null} for a 2-stop ramp. */
    String midColor() {
        return midColor;
    }

    /** ANSI escape filling cells the matrix does not cover, or {@code null} for the terminal background. */
    String backgroundColor() {
        return backgroundColor;
    }

    /** Row labels printed around the grid when {@link #showLabels()} is set, one per row; may be {@code null}. */
    String[] rowLabels() {
        return rowLabels;
    }

    /** Column labels printed around the grid when {@link #showLabels()} is set, one per column; may be {@code null}. */
    String[] colLabels() {
        return colLabels;
    }

    /** Print the row/column labels around the grid. */
    public boolean showLabels() {
        return showLabels;
    }

    /** Render with no ANSI escapes at all, overriding every color. */
    public boolean plain() {
        return plain;
    }

    /** Builder for {@link HeatmapOptions}. */
    public static final class Builder {
        private String lowColor;
        private String highColor;
        private String midColor;
        private String backgroundColor;
        private String[] rowLabels;
        private String[] colLabels;
        private boolean showLabels;
        private boolean plain;

        private Builder() {
        }

        /** ANSI color for the matrix minimum. */
        public Builder lowColor(Color color) {
            return lowColorAnsi(color == null ? null : color.escape());
        }

        /** Low color as a raw escape sequence (256-color, truecolor, ...). */
        public Builder lowColorAnsi(String escape) {
            this.lowColor = escape;
            return this;
        }

        /** ANSI color for the matrix maximum. */
        public Builder highColor(Color color) {
            return highColorAnsi(color == null ? null : color.escape());
        }

        /** High color as a raw escape sequence. */
        public Builder highColorAnsi(String escape) {
            this.highColor = escape;
            return this;
        }

        /** ANSI color for the ladder's middle entry (a 3-stop ramp). */
        public Builder midColor(Color color) {
            return midColorAnsi(color == null ? null : color.escape());
        }

        /** Mid color as a raw escape sequence. */
        public Builder midColorAnsi(String escape) {
            this.midColor = escape;
            return this;
        }

        /** ANSI color filling the cells the matrix does not cover. */
        public Builder backgroundColor(Color color) {
            return backgroundColorAnsi(color == null ? null : color.escape());
        }

        /** Background color as a raw escape sequence. */
        public Builder backgroundColorAnsi(String escape) {
            this.backgroundColor = escape;
            return this;
        }

        /** Row labels for the grid, one per matrix row. Default none. */
        public Builder rowLabels(String... labels) {
            this.rowLabels = labels;
            return this;
        }

        /** Column labels for the grid, one per matrix column. Default none. */
        public Builder colLabels(String... labels) {
            this.colLabels = labels;
            return this;
        }

        /** Print the row/column labels around the grid. Default false. */
        public Builder showLabels(boolean yes) {
            this.showLabels = yes;
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
        public HeatmapOptions build() {
            return new HeatmapOptions(this);
        }
    }
}