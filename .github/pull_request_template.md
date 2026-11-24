<!--
Thanks for the PR. Please read CONTRIBUTING.md first: features, behaviour
changes and architecture changes need an agreed design discussion before code.
-->

## What and why

<!-- One behaviour or feature. What changes for the user, and why. -->

Design discussion: <!-- link, if this implements one -->

Closes #

## Spec

<!-- Spec files this PR adds or changes. The spec change comes first: its own commit, or the first commit. -->

- (none for docs or tooling changes)

## Tests

<!-- Which tests cover it. For a bugfix: which test failed before the fix? -->

-

## Checks

- [ ] `make test-host` passes
- [ ] `source tools/build-env.sh && ./tools/build-firmware.sh` passes after the last change
- [ ] `make test-web` passes (web UI changes only)

## Bench

- [ ] Tested on a real feeder (UART flash or OTA)
- [ ] Not tested on hardware

<!--
For motor, weighing, power, OTA or boot changes, paste a UART or OTA log excerpt:

```text
```
-->

## Clean room

- [ ] The code was written from the spec, and nothing is copied from the stock firmware (code, strings, tables, addresses or key names)
- [ ] I have read and understood the whole diff, including any AI-generated parts
