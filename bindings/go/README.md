# ccharts (Go)

Turns financial OHLC data into a string — line and candlestick charts drawn
with Unicode block characters. Nothing is printed for you, so the chart goes
wherever text goes: a terminal, a log line, a chat message, an HTML `<pre>`,
a file.

Go bindings for the C library [ccharts](https://github.com/dethrandir/ccharts).
The C sources live in this package and are built by cgo, so there is nothing to
install — but **cgo is required**: `CGO_ENABLED=0` builds will not work.

```sh
go get github.com/dethrandir/ccharts/bindings/go/ccharts
```

```go
package main

import (
	"fmt"
	"log"

	"github.com/dethrandir/ccharts/bindings/go/ccharts"
)

func main() {
	chart, err := ccharts.FromArrays(opens, highs, lows, closes, epochSeconds)
	if err != nil {
		log.Fatal(err)
	}
	defer chart.Close()

	out, err := chart.Candle(60, 8, &ccharts.Options{
		RiseColor:  ccharts.ColorBlue,
		ShowPrices: true,
		ShowTimes:  true,
	})
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println(out)
}
```

- Data can also come from `FromJSON` (fixed-schema JSON) or `FromCSV`.
- Colors are the `Color*` values or any escape sequence, so 256-color and
  truecolor work: `Options{RiseColor: "\x1b[38;5;208m"}`.
- `Options{Plain: true}` renders with no ANSI escapes at all, for output that
  is not going to a terminal.
- A `Chart` is immutable once built and safe for concurrent rendering.
- `Close` releases the C memory; a finalizer covers the case where it is
  forgotten, but relying on it holds the memory longer than necessary.

## Versioning

This module lives in a subdirectory of the ccharts repository, so its tags
carry the directory prefix:

```sh
go get github.com/dethrandir/ccharts/bindings/go/ccharts@bindings/go/v0.2.0
```
