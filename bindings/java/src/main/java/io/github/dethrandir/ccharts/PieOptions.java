package io.github.dethrandir.ccharts;

/**
 * How a pie chart is drawn.
 *
 * <p>Built with a fluent builder; the defaults give a filled disk, no
 * override colors, a legend without percentages:
 *
 * <pre>{@code
 * PieOptions options = PieOptions.builder()
 *         .size(24, 10)
 *         .donut(true)
 *         .build();
 * }</pre>
 */
public final class PieOptions {

    /** Options with every field at its default. */
    public static final PieOptions DEFAULTS = builder().build();

    private final int width;
    private final int height;
    private final boolean donut;
    private final String[] colors;
    private final boolean showLegend;
    private final boolean showPct;

    private PieOptions(Builder builder) {
        this.width = builder.width;
        this.height = builder.height;
        this.donut = builder.donut;
        this.colors = builder.colors;
        this.showLegend = builder.showLegend;
        this.showPct = builder.showPct;
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

    /** Hollow out the center (a donut) instead of a filled disk. */
    public boolean donut() {
        return donut;
    }

    /** Per-slice ANSI escapes, or {@code null} for the fixed default palette. */
    String[] colors() {
        return colors;
    }

    /** Print one {@code label  value (pct%)} line per slice below the disk. */
    public boolean showLegend() {
        return showLegend;
    }

    /** Append {@code (NN%)} to each legend entry. */
    public boolean showPct() {
        return showPct;
    }

    /** Builder for {@link PieOptions}. */
    public static final class Builder {
        private int width = 24;
        private int height = 10;
        private boolean donut;
        private String[] colors;
        private boolean showLegend = true;
        private boolean showPct;

        private Builder() {
        }

        /** Chart size in cells. Defaults to 24 by 10. */
        public Builder size(int width, int height) {
            this.width = width;
            this.height = height;
            return this;
        }

        /** Hollow out the center (a donut) instead of a filled disk. */
        public Builder donut(boolean yes) {
            this.donut = yes;
            return this;
        }

        /**
         * Per-slice override colors, one ANSI escape per slice; slice
         * {@code i} uses {@code colors[i % colors.length]}. A {@code null}
         * entry means the default palette color for that index.
         */
        public Builder colors(Color... colors) {
            if (colors == null) {
                this.colors = null;
            } else {
                String[] escapes = new String[colors.length];
                for (int i = 0; i < colors.length; i++) {
                    escapes[i] = colors[i] == null ? null : colors[i].escape();
                }
                this.colors = escapes;
            }
            return this;
        }

        /** Per-slice override colors as raw escape sequences. */
        public Builder colorsAnsi(String... colors) {
            this.colors = colors;
            return this;
        }

        /** Print one {@code label  value (pct%)} line per slice below the disk. */
        public Builder showLegend(boolean yes) {
            this.showLegend = yes;
            return this;
        }

        /** Append {@code (NN%)} to each legend entry. */
        public Builder showPct(boolean yes) {
            this.showPct = yes;
            return this;
        }

        /** Builds the options. */
        public PieOptions build() {
            return new PieOptions(this);
        }
    }
}