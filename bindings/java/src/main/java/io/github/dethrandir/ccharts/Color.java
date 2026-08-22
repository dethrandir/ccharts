package io.github.dethrandir.ccharts;

/**
 * The sixteen ANSI colors and the reset sequence.
 *
 * <p>The escape strings are read from the native library rather than being
 * duplicated here, so every ccharts binding uses the same values. Anywhere a
 * color is accepted, any escape string works — so 256-color and truecolor do
 * too.
 */
public enum Color {
    /** Black. */
    BLACK(0),
    /** Red. */
    RED(1),
    /** Green. */
    GREEN(2),
    /** Yellow. */
    YELLOW(3),
    /** Blue. */
    BLUE(4),
    /** Magenta. */
    MAGENTA(5),
    /** Cyan. */
    CYAN(6),
    /** White. */
    WHITE(7),
    /** Bright black (gray). */
    BRIGHT_BLACK(8),
    /** Bright red. */
    BRIGHT_RED(9),
    /** Bright green. */
    BRIGHT_GREEN(10),
    /** Bright yellow. */
    BRIGHT_YELLOW(11),
    /** Bright blue. */
    BRIGHT_BLUE(12),
    /** Bright magenta. */
    BRIGHT_MAGENTA(13),
    /** Bright cyan. */
    BRIGHT_CYAN(14),
    /** Bright white. */
    BRIGHT_WHITE(15),
    /** Reset. */
    RESET(16);

    private final int index;

    Color(int index) {
        this.index = index;
    }

    /** The ANSI escape sequence for this color. */
    public String escape() {
        return Native.colorAt(index);
    }

    @Override
    public String toString() {
        return escape();
    }
}
