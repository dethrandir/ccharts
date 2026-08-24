package io.github.dethrandir.ccharts;

/**
 * One slice of a pie chart: a legend label and a positive amount.
 *
 * @param label drawn in the legend when {@link PieOptions#showLegend()} is
 *     set; may be {@code null} to omit it
 * @param value a positive amount; the pie computes the percentage from the
 *     sum. A value {@code <= 0} (zero, negative, NaN, inf) makes the whole
 *     render return the empty string rather than an error.
 */
public record PieSlice(String label, double value) {
}