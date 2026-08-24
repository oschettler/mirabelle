"""Mutationstest fuer src/net/spartan.c. Direkt uebersetzt, mit Sanitizer."""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/net/spartan.c")
DEPS = ["tests/unit/test_spartan.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("fremdes Schema erlaubt",       "        if (scheme_end && (!first_slash || scheme_end < first_slash))\n            return false;", "        (void)scheme_end; (void)first_slash;"),
 ("Schema auch im Pfad verboten",  "        if (scheme_end && (!first_slash || scheme_end < first_slash))", "        if (scheme_end)"),
 ("Schema wird nicht uebersprungen", "        p += sizeof SCHEME - 1;", "        (void)SCHEME;"),
 ("leerer Rechnername erlaubt",   "    if (host_end == p) return false;", "    (void)0;"),
 ("Leerzeichen im Rechnernamen",  "    if (strchr(out->host, ' ')) return false;", "    (void)0;"),
 ("Port nicht geprueft",          "            if (*q < '0' || *q > '9') return false;", "            if (false) return false;"),
 ("Port ohne Obergrenze",         "            if (port > 65535) return false;", "            (void)0;"),
 ("Port null erlaubt",            "        if (port == 0) return false;", "        (void)0;"),
 ("Standardport nicht gesetzt",   "    out->port = SPARTAN_PORT;", "    out->port = 0;"),
 ("Wurzelpfad fehlt",             "        out->path[0] = '/';\n        out->path[1] = '\\0';\n        return true;", "        return true;"),
 ("Port immer ausgeschrieben",    "    if (u->port == SPARTAN_PORT)", "    if (false)"),
 ("Verzeichnis nicht gekuerzt",   "    char *slash = strrchr(path, '/');\n    if (slash) slash[1] = '\\0';", "    char *slash = strrchr(path, '/');\n    if (false) slash[1] = '\\0';"),
 ("Punkt nicht entfernt",         "        if (len == 1 && seg[0] == '.') {", "        if (false) {"),
 ("Doppelpunkt nicht entfernt",   "        } else if (len == 2 && seg[0] == '.' && seg[1] == '.') {", "        } else if (false) {"),
 ("ueber die Wurzel hinaus",      "            if (w > out + 1) {", "            if (true) {"),
 ("Schrägstrich am Ende verloren","    if (w > out + 1 && !trailing) w--;", "    if (w > out + 1) w--;"),
 ("absoluter Verweis relativ",    "    if (strstr(href, \"://\")) return spartan_parse_url(href, out);", "    (void)href;"),
 ("Wurzelverweis relativ",        "    if (href[0] == '/') {", "    if (false) {"),
 ("leerer Verweis erlaubt",       "    if (!href || !*href) return false;", "    if (!href) return false;"),
 ("Anfrage ohne CRLF",            "    int n = snprintf(out, out_size, \"%s %s %zu\\r\\n\", u->host, u->path, body_len);", "    int n = snprintf(out, out_size, \"%s %s %zu\\n\", u->host, u->path, body_len);"),
 ("Anfrage ohne Laenge",          "\"%s %s %zu\\r\\n\", u->host, u->path, body_len)", "\"%s %s %d\\r\\n\", u->host, u->path, 0)"),
 ("Anfrage laeuft ueber",         "    int n = snprintf(out, out_size, \"%s %s %zu\\r\\n\", u->host, u->path, body_len);\n    return n > 0 && (size_t)n < out_size;", "    int n = snprintf(out, out_size, \"%s %s %zu\\r\\n\", u->host, u->path, body_len);\n    return n > 0;"),
 ("Status nicht geprueft",        "    if (line_len < 1 || data[0] < '2' || data[0] > '5') {", "    if (false) {"),
 ("CR bleibt in der Angabe",      "    if (line_len > 0 && data[line_len - 1] == '\\r') line_len--;", "    (void)0;"),
 ("Leerzeichen bleibt in der Angabe", "    if (meta_at < line_len && data[meta_at] == ' ') meta_at++;", "    (void)0;"),
 ("Rumpf beginnt zu frueh",       "    out->body     = nl + 1;\n    out->body_len = len - (size_t)(nl + 1 - data);", "    out->body     = nl;\n    out->body_len = len - (size_t)(nl - data);"),
 ("fehlende Statuszeile erlaubt", "    if (!nl) {\n        snprintf(err, err_size, \"die Statuszeile hört nicht auf\");\n        return false;\n    }", "    if (!nl) nl = data + len - 1;"),
 ("Transport nicht geprueft",     "    if (!t || !t->open || !t->send_all || !t->recv_some) {", "    if (!t) {"),
 ("Verbindung bleibt offen",      "    if (t->close) t->close(t->user);\n    buf[n] = '\\0';", "    buf[n] = '\\0';"),
 ("Puffer laeuft ueber",          "        if (n + 1 >= buf_size) {", "        if (false) {"),
 ("Leseschleife bricht nicht ab", "        if (got == 0) break;", "        if (got == 0) continue;"),
 ("Lesefehler wird verschluckt",  "        if (got < 0) {", "        if (false) {"),
 ("leere Antwort ist eine Seite", "    if (n == 0) {\n        snprintf(err, err_size, \"die Gegenseite hat nichts geschickt\");\n        return false;\n    }", "    (void)0;"),
 ("Anfrage wird nicht geschickt", "    if (!t->send_all(t->user, request, strlen(request))) {", "    if (false) {"),
 ("Rechner wird nicht uebergeben","    if (!t->open(t->user, u->host, u->port, err, err_size)) return false;", "    if (!t->open(t->user, \"\", u->port, err, err_size)) return false;"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "spartan.c"); exe = os.path.join(d, "t")
        open(src, "w").write(text)
        b = subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Wpedantic",
                            "-fsanitize=address,undefined","-g","-Isrc","-Itests",
                            src] + DEPS + ["-o", exe], capture_output=True, text=True)
        if b.returncode != 0: return None, b.stderr
        r = subprocess.run([exe], capture_output=True, timeout=30)
        return r.returncode, r.stdout.decode("utf-8", "replace")

rc, out = run(orig)
assert rc == 0, "Der unveraenderte Stand muss gruen sein:\n" + str(out)

survivors = []
for name, a, b in MUTS:
    assert a in orig, "Muster nicht gefunden: " + name
    try:
        rc, out = run(orig.replace(a, b, 1))
    except subprocess.TimeoutExpired:
        print("erkannt     " + name + " (haengt)"); continue
    if rc is None:
        print("BAUT NICHT  " + name); survivors.append(name)
    elif rc == 0:
        print("UEBERLEBT   " + name); survivors.append(name)
    else:
        print("erkannt     " + name)

print()
print("Ueberlebende:", survivors if survivors else "keine")
