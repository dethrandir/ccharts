package io.github.dethrandir.ccharts;

/**
 * How a stacked bar chart is drawn.
 *
 * <p>Built with a fluent builder. The defaults use the fixed per-series
 * palette, no background fill, and no category-label or value footer:
 *
 * <pre>{@code
 * StackOptions options = StackOptions.builder()
 *         .colors(Color.RED, Color.GREEN)
 *         .categoryLabels("Jan", "Feb", "Mar")
 *         .showLabels(true)
 *         .build();
 * }</pre>
 */
public final class StackOptions {

    /** Options with every field at its default. */
    public static final StackOptions DEFAULTS = builder().build();

    private final String[] colors;
    private final String backgroundColor;
    private final String[] categoryLabels;
    private final int series;
    private final int cats;
    private final boolean showLabels;
    private final boolean showPrices;
    private final boolean plain;

    private StackOptions(Builder builder) {
        this.colors = builder.colors;
        this.backgroundColor = builder.backgroundColor;
        this.categoryLabels = builder.categoryLabels;
        this.series = builder.series;
        this.cats = builder.cats;
        this.showLabels = builder.showLabels;
        this.showPrices = builder.showPrices;
        this.plain = builder.plain;
    }

    /** A new builder. */
    public static Builder builder() {
        return new Builder();
    }

    /**
     * Per-series ANSI escapes; series {@code i} uses
     * {@code colors[i % colors.length]}. A {@code null} entry means the
     * default palette color for that index; a {@code null} array selects the
     * fixed default palette.
     */
    String[] colors() {
        return colors;
    }

    /** ANSI escape filling empty cells, or {@code null} for the terminal background. */
    String backgroundColor() {
        return backgroundColor;
    }

    /** Category names for the label footer, one per category; may be {@code null}. */
    String[] categoryLabels() {
        return categoryLabels;
    }

    /** Number of series; 0 lets the chart derive it. */
    int series() {
        return series;
    }

    /** Number of categories; 0 lets the chart derive it. */
    int cats() {
        return cats;
    }

    /** Print each column's category label in a footer row below the chart. */
    public boolean showLabels() {
        return showLabels;
    }

    /** Print the tallest stack total and 0 (the baseline) in a left value-axis margin. */
    public boolean showPrices() {
        return showPrices;
    }

    /** Render with no ANSI escapes at all, overriding every color. */
    public boolean plain() {
        return plain;
    }

    /** Builder for {@link StackOptions}. */
    public static final class Builder {
        private String[] colors;
        private String backgroundColor;
        private String[] categoryLabels;
        private int series;
        private int cats;
        private boolean showLabels;
        private boolean showPrices;
        private boolean plain;

        private Builder() {
        }

        /** Per-series ANSI escape colors. */
        public Builder colors(Color... colors) {
            if (colors == null) {
                this.colors = null;
                return this;
            }
            String[] escape = new String[colors.length];
            for (int i = 0; i < colors.length; i++) {
                escape[i] = colors[i] == null ? null : colors[i].escape();
            }
            return colorsAnsi(escape);
        }

        /** Per-series colors as raw escape sequences (256-color, truecolor, ...). */
        public Builder colorsAnsi(String... colors) {
            this.colors = colors;
            return this;
        }

        /** ANSI escape color filling empty cells. */
        public Builder backgroundColor(Color color) {
            return backgroundColorAnsi(color == null ? null : color.escape());
        }

        /** Background color as a raw escape sequence. */
        public Builder backgroundColorAnsi(String escape) {
            this.backgroundColor = escape;
            return this;
        }

        /** Category names for the label footer, one per category. Default none. */
        public Builder categoryLabels(String... labels) {
            this.categoryLabels = labels;
            return this;
        }

        /** Number of series; 0 lets the chart derive it. Default 0. */
        public Builder series(int series) {
            this.series = series;
            return this;
        }

        /** Number of categories; 0 lets the chart derive it. Default 0. */
        public Builder cats(int cats) {
            this.cats = cats;
            return this;
        }

        /** Print each column's category label in a footer row below the chart. Default false. */
        public Builder showLabels(boolean yes) {
            this.showLabels = yes;
            return this;
        }

        /** Print the tallest stack total and 0 (the baseline) in a left margin. Default false. */
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
        public StackOptions build() {
            return new StackOptions(this);
        }
    }
}