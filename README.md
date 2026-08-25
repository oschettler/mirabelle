# An SDA-based PIM suite

Let's code a [PalmOS-inspired PIM suite](https://en.wikipedia.org/wiki/Palm_OS#Built-in_applications) (todos, calendar, rolodex, notes) for [its API](https://www.lexaloffle.com/dl/docs/picotron_manual.txt), minus the handwriting recognition.

I want the GUI and fonts be heavily inspired by [classic Mac, version 1](http://lowendmac.com/2005/innovative-macintosh-system-1-0/), see also [System 1.0 Headquarters](https://web.archive.org/web/20140512112637/http://www3.nd.edu/~jvanderk/sysone/).

The GUI should be based on SDL 3, but be later ported to an ESP32S3. The target board is the Sunton ESP32-8048S070C, whose 800x480 panel is in fact *larger* than the Macintosh 128K screen; that resolution is used on the desktop too, so golden images and layout hold for both.

Data storage should be based on [gemtext](https://geminiprotocol.net/docs/gemtext-specification.gmi) files with front matter, and an SQlite database with full text search. Gemtext rather than Markdown, because it is the format SPARTAN:// serves: the note renderer and the browser end up being the same function, and the parser needs a single bit of state.

The system will later be extended with an information browser, using the SPARTAN:// protocol, an possibly additional apps.

The system should be extensible in Lua.

The while system should be based on a neat, small, generic API.

## Where things stand

Milestones M1 to M13 and M17 are built; M14 to M16 (recorded interaction
scripts, touch, the ESP32 port) are not. The program runs on SDL 3 and carries
seven applications:

| Application | Comes from |
|---|---|
| Tasks, Contacts, Notes, Events | one schema file each, no code |
| SPARTAN browser, Outline, Agenda | one Lua file each, no code |

Building needs a C11 compiler, CMake 3.18, SDL 3 and **Lua 5.4** — the schema
files are Lua tables, so without Lua there would be no applications at all
(decision D-16). SQLite is optional: without it every query is answered by
walking the files.

    make                # build
    make test           # 41 suites
    make asan           # the same under the sanitizers
    ./build/pda         # run it; the vault defaults to ~/PDA
    ./build/pda --apps  # list what was found, then exit

`--vault <dir>` or `PDA_VAULT` points somewhere else, `--lang en` switches the
catalogue, and `--shot <file.pbm>` writes one frame and exits.

See [DESIGN.md](DESIGN.md) for the system design and the numbered decisions
D-1 to D-17, [ROADMAP.md](ROADMAP.md) for the seventeen chapters, and
[STAND.md](STAND.md) for where to pick the work up. The handbook under
[handbuch/](handbuch/) doubles as a textbook.

## What it looks like inside

Nothing that a user can change is compiled in. Screen layout, key bindings,
every visible string, the sort order, the glyphs and the applications
themselves are files under [data/](data/); a fifth application is a fifth file.

A script gets the program's own widgets, not copies of them: the SPARTAN
browser scrolls with the same scrollbar and lays out gemtext with the same view
as the built-in applications (D-17). A rebuilt widget is a second truth, and it
drifts the moment the original changes.

Two decisions are worth knowing before reading the code. The screen is **one
bit per pixel** — patterns take the place of colour, and that is why an
overdue task gets a bar rather than a shade of red. And the **index is
derived**: deleting `index.db` costs nothing, because every query can also be
answered by walking the files, and a test insists on it.
