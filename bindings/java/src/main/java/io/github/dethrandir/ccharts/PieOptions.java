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
    private final double sliceGap;
    private final double innerRadiusRatio;
    private final int legendFormat;
    private final double startAngle;
    private final boolean counterClockwise;
    private final String centerText;

    private PieOptions(Builder builder) {
        this.width = builder.width;
        this.height = builder.height;
        this.donut = builder.donut;
        this.colors = builder.colors;
        this.showLegend = builder.showLegend;
        this.showPct = builder.showPct;
        this.sliceGap = builder.sliceGap;
        this.innerRadiusRatio = builder.innerRadiusRatio;
        this.legendFormat = builder.legendFormat;
        this.startAngle = builder.startAngle;
        this.counterClockwise = builder.counterClockwise;
        this.centerText = builder.centerText;
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

    /** Angular gap between slices, in radians. */
    public double sliceGap() {
        return sliceGap;
    }

    /** Donut thickness, or a negative value when unspecified. */
    public double innerRadiusRatio() {
        return innerRadiusRatio;
    }

    /** Legend entry format enum. */
    public int legendFormat() {
        return legendFormat;
    }

    /** Start angle in radians, or a negative value when unspecified. */
    public double startAngle() {
        return startAngle;
    }

    /** Sweep clockwise instead of counter-clockwise. */
    public boolean counterClockwise() {
        return counterClockwise;
    }

    /** Text drawn in the hollow center of a donut, or {@code null}. */
    String centerText() {
        return centerText;
    }

    /** Builder for {@link PieOptions}. */
    public static final class Builder {
        private int width = 24;
        private int height = 10;
        private boolean donut;
        private String[] colors;
        private boolean showLegend = true;
        private boolean showPct;

        private double sliceGap;

        /** -1 = unspecified (library default, donut 0.5 / disk 0). */
        private double innerRadiusRatio = -1;

        private int legendFormat;

        /** -1 = unspecified (library default, 12 o'clock). */
        private double startAngle = -1;

        private boolean counterClockwise;
        private String centerText;

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

        /** Angular gap between slices, in radians. Default {@code 0} (adjacent). */
        public Builder sliceGap(double radians) {
            this.sliceGap = radians;
            return this;
        }

        /**
         * Donut thickness in {@code [0, 1]}: {@code 0} a filled disk, {@code 1}
         * a hairline ring. A negative value leaves it to {@link #donut(boolean)}
         * ({@code 0.5} for a donut, {@code 0} for a disk). Above 1 is clamped.
         */
        public Builder innerRadiusRatio(double ratio) {
            this.innerRadiusRatio = ratio;
            return this;
        }

        /**
         * Legend entry format: {@code 0} value, {@code 1} label + pct,
         * {@code 2} value + pct, {@code 3} label. Unknown values fall back to
         * {@code 0}.
         */
        public Builder legendFormat(int format) {
            this.legendFormat = format;
            return this;
        }

        /** Angle (radians) at which slice 0 begins; negative = library default. */
        public Builder startAngle(double radians) {
            this.startAngle = radians;
            return this;
        }

        /** Sweep the slices clockwise instead of the default counter-clockwise. */
        public Builder counterClockwise(boolean yes) {
            this.counterClockwise = yes;
            return this;
        }

        /** Text drawn in the hollow center of a donut (only when there is a hollow). */
        public Builder centerText(String text) {
            this.centerText = text;
            return this;
        }

        /** Builds the options. */
        public PieOptions build() {
            return new PieOptions(this);
        }
    }
}