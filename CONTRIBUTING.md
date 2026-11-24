# Contributing

Thanks for wanting to help. This is replacement firmware for a device that
feeds animals, so it moves carefully: behaviour is written down before it is
built, and every change needs to be something a reviewer can check against the
spec, the tests and, where it matters, the bench.

Please read this whole page before opening a pull request. It is short, and it
can save you a lot of wasted work.

This is a hobby project with one maintainer. Reviews can take a while; that
is not a sign your contribution is unwelcome.

## Start a design discussion before you write code

**New features, changes to how the feeder behaves and architecture changes
start as a discussion** in the
[Design category](https://github.com/cristianchelu/xiaomi-feeder-iv2001/discussions/categories/design)
of GitHub Discussions. Describe the problem first, then the change you have
in mind as the spec text you would add or edit, plus any alternatives you
considered. Wait for agreement before building on it. A design that is
turned down after 5,000 lines costs you far more than one turned down after
a paragraph.

**Fixes can go straight to a pull request** when the spec already describes
the correct behaviour and the code gets it wrong. So can typos, documentation
and tooling fixes. If a fix turns out to need a spec change nobody has
discussed, or grows past a few hundred lines, stop and start a design
discussion.

Bugs and hardware findings are issues; questions go in
[Q&A](https://github.com/cristianchelu/xiaomi-feeder-iv2001/discussions/categories/q-a).

### Hardware

This firmware targets one board: the XMWSQ02 / IV2001, as described in
[`spec/10-hardware/`](spec/10-hardware/). Other products are out of scope.
If your unit differs from the spec (a different flash chip, another board
revision, a part that does not match), open a hardware finding with what you
see before changing any code.

## Clean-room rule

The firmware is an independent implementation. Nothing in this repository
is copied from the stock firmware, and the code is written only from
[`spec/`](spec/), never from the stock firmware itself.

The `spec/` tree is the wall between the two. Knowledge about the hardware
and how the device should behave can cross it; the stock firmware's code
and its design cannot.

- **Spec side.** A spec change states facts and required behaviour in this
  project's own words: what a pin does, which bytes a chip expects, what the
  user sees when the bowl is missing. It does not say "the stock firmware
  does X", and it does not describe how the stock firmware is structured
  (its tasks, modules, state machines or storage layout).
- **Code side.** Code is written from the spec alone. If you, or an agent
  working for you, have looked at the stock firmware for an area, split the
  work in two:
  1. Write the spec for that area, detailed enough that someone could
     implement it from the spec and the hardware docs alone, and commit it.
  2. Implement it in a fresh agent session (or have someone else do it),
     whose only context is this repository: no dumps, research notes or
     chat history from the session that wrote the spec.

  Both halves can go in one pull request, with the spec commit first.

**Never put any of this in the repository**, in code, specs, comments,
tests or PR text:

- dumps, disassembly or decompiler output, RE project files or research
  notes;
- code, lookup tables, calibration tables or config blobs taken from the
  stock firmware;
- stock log strings, error codes, NVDM or config key names, flash addresses,
  symbol or function names;
- cloud protocol details: Mi Home / MIoT property and action IDs, cloud
  endpoints, app strings;
- reverse-engineering vocabulary.

Matching stock behaviour is a [non-goal](spec/00-overview.md#non-goals), so
"the stock firmware does it this way" is never a reason for a design on its
own. Argue from what the user needs or what the hardware requires.

Every fact in a spec carries a [provenance tag](spec/README.md#provenance-legend):

| Tag | Use it for |
|-----|------------|
| `[ds:PART §N]` | A datasheet, with the section |
| `[probe]` | Something you measured on hardware you own |
| `[probe-needed]` | A fact nobody has measured yet |
| `[bootlog]` | Output seen on the wire, such as UART logs |
| `[product]` | The retail product page, manual or FCC filing |
| `[design]` | Our own engineering choice, argued in the design discussion or PR |
| `[tune]` | A starting value to be refined on the bench |

`[probe]` means you actually measured it. Until someone has, tag it
`[probe-needed]`. Thresholds, retry counts and other values you cannot see
on a wire are design choices: tag them `[design]` or `[tune]` and say why
you picked them.

The compiled firmware contains no Xiaomi / Mi / MIoT strings except the
Home Assistant discovery `device.manufacturer` and `device.model` fields,
which identify the retail hardware. MQTT topics live under
`petfeeder/<device_id>/` only.

## How changes are made: spec first

The [`spec/`](spec/) tree is the source of truth, and the firmware is written
from it. [spec/README.md](spec/README.md) explains the four tiers (hardware,
user stories, processes, architecture). Every change follows the same order,
whether it is a feature or a bugfix:

1. **Spec**: edit the Tier 3 process file (and Tier 2 story, Tier 4
   architecture if they change) to describe the behaviour, including CLI
   strings, MQTT fields, error paths and timeouts. If a fact appears in more
   than one spec file, update every copy.
2. **Red**: write a host test that fails against the current code.
3. **Green**: make the smallest firmware change that passes it.
4. **Refactor**, then repeat for the next behaviour.

For a bugfix, the test must fail on the code before the fix. Ask yourself:
"would this test have caught the bug?"

[AGENTS.md](AGENTS.md) covers this in full detail, including the layering
rules (application code talks to ports, only adapters include SDK headers,
no `#ifdef UNIT_TEST`). It is written for coding agents, but it is the
complete rulebook for people too.

Specs are living reference documents: write them in the present tense and
describe the system as it is. Do not leave "previously…" or "changed from…"
notes; git history is the changelog.

## Development setup

```bash
./tools/bootstrap.sh          # fetch SDK, run host tests
make test-host                # firmware unit tests (host)
make test-web                 # web UI logic tests
source tools/build-env.sh && ./tools/build-firmware.sh   # ARM image
```

See the [README](README.md#quick-start) for prerequisites, flashing and OTA.

A change to `firmware/` is ready when **both** `make test-host` and
`build-firmware.sh` pass after your last edit. Host tests do not prove that
the ARM image links.

### SDK

The Airoha IoT SDK is fetched into gitignored `external/` and never
committed. Never edit files under `external/`. SDK changes go in
`firmware/patches/<name>.patch` and are listed in `firmware/patches/series`
(see [build-integration.md](spec/40-architecture/build-integration.md)).
Do not commit SDK sources, prebuilt libraries or firmware binaries.

### Web UI

The admin UI in [`tools/web/`](tools/web/) is a single HTML file that ships
gzipped in flash, so size matters. Use native CSS nesting and the existing
`html-minifier-next` build; see [tools/web/README.md](tools/web/README.md).

## Pull requests

- **One behaviour or feature per pull request.** Several small PRs get
  reviewed faster than one large one, and a problem in one part does not hold
  up the rest.
- Link the design discussion or issue it implements, if there is one.
- Put the spec change first: either its own commit, or the first commit of
  the PR.
- Say which spec files you touched and which tests cover the change.
- Confirm that `make test-host` and `build-firmware.sh` both pass (and
  `make test-web` for web UI changes).
- **Bench evidence**: say whether you ran the change on a real feeder. For
  anything touching the motor, weighing, power, OTA or boot, include a UART
  log or OTA log excerpt. "Not tested on hardware" is an honest and acceptable
  answer; claiming a bench test you did not run is not.
- Commit subjects start with the area they touch, then a short summary:
  `ota: keep the reported bank when resetting status`. Common areas are
  `ota`, `web`, `boot`, `sdk`, `tools`, `docs` and `spec`. The body
  explains *why* the change is made; the diff shows what.
- There is no formatter config: match the style of the code around you.
- Keep unrelated cleanups, reformatting and renames out of feature PRs.

### AI-assisted contributions

Using AI tools is fine. You are still the author: you must have read and
understood the whole diff, you own the design discussion, and
every bench result in the PR must come from your own hardware. Large
generated PRs that skip the design discussion are likely to be closed with a
request to start over from a discussion.

## Reporting bugs

Use the bug report form. It asks for the firmware commit, how you flashed
it, the power source and your logs, and whether the bug affects feeding.

## Safety

Read [SAFETY.md](SAFETY.md) before flashing. Changes to motor control, power
handling or scheduling should keep stall detection, timeouts and supervision
in place. While testing, keep a backup way to feed your animals.

## License

By contributing, you agree that your contribution is released under the
project's license, the [GNU Affero General Public License v3.0 only](LICENSE)
(`AGPL-3.0-only`).
Only contribute work you have the right to release under that license.
Third-party code needs a design discussion first and an entry in [NOTICE](NOTICE).
