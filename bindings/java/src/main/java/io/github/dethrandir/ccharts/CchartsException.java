package io.github.dethrandir.ccharts;

/** An error reported by the ccharts native library. */
public final class CchartsException extends RuntimeException {

    private static final long serialVersionUID = 1L;

    /** Status codes mirroring {@code ccharts_status} in the C ABI. */
    public enum Status {
        /** No error. */
        OK(0),
        /** Empty or mismatched input. */
        INVALID_ARGUMENT(1),
        /** The JSON or CSV document could not be parsed. */
        PARSE(2),
        /** An allocation failed. */
        OUT_OF_MEMORY(3),
        /** A price was NaN or infinite. */
        NON_FINITE(4),
        /** Width or height was not positive, or exceeded the limits. */
        DIMENSIONS(5),
        /** A status code this binding does not know about. */
        UNKNOWN(-1);

        private final int code;

        Status(int code) {
            this.code = code;
        }

        /** The numeric status code. */
        public int code() {
            return code;
        }

        static Status fromCode(int code) {
            for (Status status : values()) {
                if (status.code == code) {
                    return status;
                }
            }
            return UNKNOWN;
        }
    }

    private final Status status;

    CchartsException(Status status, String message) {
        super(message);
        this.status = status;
    }

    CchartsException(int code) {
        this(Status.fromCode(code), Native.errorMessage(code));
    }

    /** The status behind this error. */
    public Status status() {
        return status;
    }

    static void throwIfError(int code) {
        if (code != 0) {
            throw new CchartsException(code);
        }
    }
}
