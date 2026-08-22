# ccharts (Java)

Terminal charts for financial OHLC data — line and candlestick charts as
ANSI-colored strings of Unicode block characters, ready to print.

Java bindings for the C library [ccharts](https://github.com/dethrandir/ccharts),
built on the Foreign Function & Memory API. **There is no JNI code at all** —
just plain Java over `java.lang.foreign`, which is why this requires **JDK 22
or newer**.

```xml
<dependency>
  <groupId>io.github.dethrandir</groupId>
  <artifactId>ccharts</artifactId>
  <version>0.2.0</version>
</dependency>
```

```java
import io.github.dethrandir.ccharts.*;

try (Chart chart = Chart.fromArrays(open, high, low, close, epochSeconds)) {
    System.out.print(chart.line(ChartOptions.builder()
            .size(60, 8)
            .rise(Color.BLUE)
            .showPrices(true)
            .showTimes(true)
            .build()));
    System.out.print(chart.candle(ChartOptions.builder().size(60, 8).build()));
}
```

- Data can also come from `Chart.fromJson` (fixed-schema JSON) or
  `Chart.fromCsv`.
- `Color` exposes the sixteen ANSI colors; `riseAnsi(...)` and friends take a
  raw escape sequence, so 256-color and truecolor work.
- A `Chart` is immutable once built and safe to render from several threads.
- `close()` releases the native memory; a `Cleaner` covers a forgotten call.

Errors are `CchartsException` with a `status()` of `INVALID_ARGUMENT`,
`PARSE`, `OUT_OF_MEMORY`, `NON_FINITE` or `DIMENSIONS`.

## Native access

On JDK 24 and later the runtime warns about restricted native access. Add:

```sh
java --enable-native-access=ALL-UNNAMED -jar your-app.jar
```

The jar sets `Enable-Native-Access: ALL-UNNAMED` in its manifest, which covers
the case where ccharts is the executable jar.

## The native library

The published jar carries `native/{os}-{arch}/`, so nothing has to be
installed. Inside the repository the library comes from the CMake build:

```sh
cmake -S . -B build && cmake --build build
cd bindings/java && mvn test
```

`Native` probes, in order: the copy packaged in the jar, `CCHARTS_NATIVE_DIR`,
then `build/` directories above the working directory.

## License

MIT — see the [repository](https://github.com/dethrandir/ccharts).
