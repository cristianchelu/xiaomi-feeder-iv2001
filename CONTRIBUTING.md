# Contributing

Thanks for wanting to help. This is replacement firmware for a device that
feeds animals, so it moves carefully: behaviour is written down before it is
built, and every change needs to be something a reviewer can check against the
spec, the tests and, where it matters, the bench.

Please read this whole page before opening a pull request. It is short, and it
can save you a lot of wasted work.

## Open an issue before you write code

**Start with an issue, not a pull request, for anything that changes how the
system is designed.** That includes:

- anything under [`spec/40-architecture/`](spec/40-architecture/) (task model,
  ports, flash layout, partitions, bootloader, build system, SDK patches);
- a new subsystem, module, task or port;
- user-visible interfaces: MQTT topics and payload fields, Home Assistant
  entities, the web UI's HTTP API, UART CLI commands, NVDM keys;
- security and authentication (web UI, MQTT, OTA);
- power, battery, sleep, motor or dispense behaviour, and anything else in
  [SAFETY.md](SAFETY.md);
- new third-party code or dependencies;
- user-facing design choices (display states, button gestures, languages).

Describe the problem first, then the change you have in mind, as the spec text
you would add or edit, plus any alternatives you considered. Wait for agreement
before building on it. A design that is turned down after 5,000 lines costs you
far more than one turned down after a paragraph.

**Small, self-contained fixes can go straight to a pull request**: typos,
documentation, a clear bug with a failing test, a tooling fix. As a rule of
thumb, if your change goes past a few hundred lines, or you notice you need to
change a spec in a way nobody has discussed, stop and open an issue.

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
     implement it from the spec and the hardware docs alone, and get it
     reviewed as a spec PR.
  2. Implement it in a fresh agent session (or have someone else do it),
     whose only context is this repository: no dumps, research notes or
     chat history from the session that studied the firmware.

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
a datasheet (`[ds:PART §N]`), your own measurement on hardware you own
(`[probe]`), output seen on the wire (`[bootlog]`), the retail product page,
manual or FCC filing (`[product]`), or our own choice (`[design]`).
Prefer a datasheet or your own measurement to anything learned from the
stock firmware: if a number can be measured on the bench, measure it.

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
- Link the issue it implements, if there is one.
- Put the spec change first: either its own commit, or the first commit of
  the PR.
- Say which spec files you touched and which tests cover the change.
- Confirm that `make test-host` and `build-firmware.sh` both pass (and
  `make test-web` for web UI changes).
- **Bench evidence**: say whether you ran the change on a real feeder. For
  anything touching the motor, weighing, power, OTA or boot, include a UART
  log or OTA log excerpt. "Not tested on hardware" is an honest and acceptable
  answer; claiming a bench test you did not run is not.
- Commit messages explain *why* the change is made; the diff shows what.
- Keep unrelated cleanups, reformatting and renames out of feature PRs.

### AI-assisted contributions

Using AI tools is fine. You are still the author: you must have read and
understood the whole diff, you own the design discussion in the issue, and
every bench result in the PR must come from your own hardware. Large
generated PRs that skip the issue step are likely to be closed with a request
to start over from an issue.

## Reporting bugs

Please include:

- the firmware commit (or release) and how you flashed it (UART or OTA);
- what you did, what you expected, and what happened;
- a UART log (`./tools/uart-console.sh`) and/or the relevant MQTT messages
  under `petfeeder/<device_id>/`, if you have them;
- whether the feeder was on mains or battery.

If a bug can make the feeder over-dispense, stop dispensing or jam the motor,
say so in the title.

## Security issues

Do not open a public issue for a security problem (for example, a way to
dispense food, change settings or push an OTA image from the LAN without
authorisation). Report it privately through GitHub's
[security advisory form](../../security/advisories/new) instead.

## Safety

Read [SAFETY.md](SAFETY.md) before flashing. Changes to motor control, power
handling or scheduling should keep stall detection, timeouts and supervision
in place. While testing, keep a backup way to feed your animals.

## License

By contributing, you agree that your contribution is released under the
project's license, the [GNU Affero General Public License v3.0](LICENSE).
Only contribute work you have the right to release under that license.
Third-party code needs an issue first and an entry in [NOTICE](NOTICE).
