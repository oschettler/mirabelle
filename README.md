# An SDA-based PIM suite

Let's code a [PalmOS-inspired PIM suite](https://en.wikipedia.org/wiki/Palm_OS#Built-in_applications) (todos, calendar, rolodex, notes) for [its API](https://www.lexaloffle.com/dl/docs/picotron_manual.txt), minus the handwriting recognition.

I want the GUI and fonts be heavily inspired by [classic Mac, version 1](http://lowendmac.com/2005/innovative-macintosh-system-1-0/), see also [System 1.0 Headquarters](https://web.archive.org/web/20140512112637/http://www3.nd.edu/~jvanderk/sysone/).

The GUI should be based on SDL 3, but be later ported to an ESP32S3 with small screen.

Data storage should be based on Markdown files, possibly with front matter, and an SQlite database with full text search.

The system will later be extended with an information browser, using the SPARTAN:// protocol, an possibly additional apps.

The system should be extensible in Lua.

The while system should be based on a neat, small, generic API.