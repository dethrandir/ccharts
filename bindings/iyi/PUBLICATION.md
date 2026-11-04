# iyi package publication

## Current distribution

ccharts is an iyi Git package: there is no tarball upload or registry token.
The root `iyi.mod` declares `github.com/dethrandir/ccharts/v3`, and the root
`ccharts.iyi` exports its public API. iyi requires `/v3` for the repository's
shared 3.x release series and fetches the repository without that suffix.

The first binding commit was pushed to `main`. A clean external consumer
successfully fetched it from GitHub (without a local replacement or mirror):

```sh
iyi get github.com/dethrandir/ccharts/v3@main
```

The compiler pinned commit `ce1386ab283f` as
`v3.0.1-0.20261104174031-ce1386ab283f`, wrote `iyi.sum`, and compiled a consumer
that called the native ABI and produced a chart. Newer `main` commits will have
their own pseudo-versions. The native archive must still be built separately;
see [setup instructions](README.md).

## Why it is not in iyipacks yet

[iyipacks](https://github.com/domify/iyipacks-website) is a generated discovery
catalog. Its importer searches root `iyi.mod` files and the `iyi` topic on
GitHub, as well as explicitly configured repository candidates. Discovery is
followed by validation of the manifest identity.

At inspected catalog commit `6748c92748a859475fdd9d16b86199212624e2bb`:

1. [`gettable?`](https://github.com/domify/iyipacks-website/blob/6748c92748a859475fdd9d16b86199212624e2bb/src/modules.iyi#L47-L51)
   only accepts an exact `github.com/OWNER/REPO` identity. It rejects a valid
   semantic-major suffix such as ccharts' `/v3`.
2. [The importer](https://github.com/domify/iyipacks-website/blob/6748c92748a859475fdd9d16b86199212624e2bb/src/update.iyi#L87-L101)
   also hardcodes an unsuffixed module identity and install command. Accepting
   the manifest alone would therefore still produce an incorrect install line.
3. A bare install command selects a release tag. The existing `v3.0.0` tag
   predates the binding and has no `iyi.mod`. It must not be moved.

Changing our package to an unsuffixed identity would make its 3.x releases
invalid to iyi. The catalog should accept canonical `/vN` suffixes for N >= 2,
preserve the actual manifest identity, and select the corresponding tag series.
A local patch with these changes was prepared and checked with the catalog's
`make check`, `make test` and `make test-smoke`. It has not been submitted or
merged upstream.

## Steps to make the catalog installation work

1. Finish ccharts CI and keep the working `@main` Git install available.
2. Upstream the catalog's semantic-major compatibility fix.
3. Publish a new ccharts **3.x** release containing the root iyi manifest and
   binding. Follow the root [release guide](../../RELEASING.md): a root version
   tag triggers the repository's other package publications too. This is a
   coordinated ccharts release, not an isolated iyipacks upload.
4. Add/preserve the `iyi` GitHub topic for discovery.
5. Have the catalog maintainer refresh and deploy, then verify the package page
   and its generated install command with a fresh consumer.

The catalog's [README](https://github.com/domify/iyipacks-website/blob/6748c92748a859475fdd9d16b86199212624e2bb/README.md)
documents `make refresh` (import plus static-site build) followed by
`meridian deploy`. No refresh schedule or GitHub Actions cron is defined in the
inspected repository; an external scheduler may exist, so a next-import time
cannot be promised. Generated `data/modules.toml` should not be edited by hand.
No catalog upload API, form or package credential is required.
