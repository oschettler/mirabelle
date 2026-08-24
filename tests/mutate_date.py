"""Mutationstest fuer src/core/date.c. Direkt uebersetzt, mit Sanitizer."""
import subprocess, pathlib, tempfile, os

SRC  = pathlib.Path("src/core/date.c")
DEPS = ["tests/unit/test_date.c"]
orig = SRC.read_text(encoding="utf-8")

MUTS = [
 ("Schaltjahr ohne 400er-Regel",  "    return year % 400 == 0;", "    return false;"),
 ("Schaltjahr ohne 100er-Regel",  "    if (year % 100 != 0) return true;", "    return true;"),
 ("Schaltjahr ohne 4er-Regel",    "    if (year % 4 != 0)   return false;", "    (void)0;"),
 ("Februar immer 28",             "    if (month == 2 && date_is_leap_year(year)) return 29;", "    (void)0;"),
 ("Monat 13 erlaubt",             "    if (month < 1 || month > 12) return 0;", "    if (false) return 0;"),
 ("Tag 0 erlaubt",                "    if (d.day < 1) return false;", "    (void)0;"),
 ("Monatslaenge nicht geprueft",  "    return d.day <= date_days_in_month(d.year, d.month);", "    return d.day <= 31;"),
 ("Wochentag verschoben",         "    long z = to_days(d) + 2;", "    long z = to_days(d) + 3;"),
 ("Wochentag ohne Korrektur",     "    return (int)(w < 0 ? w + 7 : w);", "    return (int)w;"),
 ("Maerz-Verschiebung fehlt",     "    int  y   = d.year - (d.month <= 2 ? 1 : 0);", "    int  y   = d.year;"),
 ("Monatsformel daneben",         "    long doy = (153 * (d.month + (d.month > 2 ? -3 : 9)) + 2) / 5 + d.day - 1;", "    long doy = (153 * (d.month + (d.month > 2 ? -3 : 9)) + 2) / 5 + d.day;"),
 ("Rueckrechnung ohne Korrektur", "    out.year  = (int)(y + (m <= 2 ? 1 : 0));", "    out.year  = (int)y;"),
 ("Monate: Tag nicht gekuerzt",   "    out.day  = d.day < last ? d.day : last;", "    out.day  = d.day;"),
 ("Monate: negativ nicht gedreht","    if (out.month < 1) {\n        out.month += 12;\n        out.year  -= 1;\n    }", "    (void)0;"),
 ("Monate: Nullbasis vergessen",  "    long m0 = (long)d.year * 12 + (d.month - 1) + months;", "    long m0 = (long)d.year * 12 + d.month + months;"),
 ("Vergleich ohne Jahr",          "    if (a.year  != b.year)  return a.year  < b.year  ? -1 : 1;", "    (void)0;"),
 ("Vergleich ohne Tag",           "    if (a.day   != b.day)   return a.day   < b.day   ? -1 : 1;", "    (void)0;"),
 ("ISO ohne Auffuellen",          "    snprintf(out, out_size, \"%04d-%02d-%02d\", d.year, d.month, d.day);", "    snprintf(out, out_size, \"%d-%d-%d\", d.year, d.month, d.day);"),
 ("ISO: Laenge nicht geprueft",   "    if (iso[10]) return false;", "    (void)0;"),
 ("ISO: Ziffern nicht geprueft",  "        if (digit && (iso[i] < '0' || iso[i] > '9')) return false;", "        (void)digit;"),
 ("ISO: Trennzeichen egal",       "        if (!digit && iso[i] != '-') return false;", "        (void)0;"),
 ("ISO: Gueltigkeit nicht geprueft", "    if (!date_valid(d)) return false;", "    (void)0;"),
 ("ISO: Ziel wird immer gesetzt", "    if (!date_valid(d)) return false;\n    *out = d;", "    *out = d;\n    if (!date_valid(d)) return false;"),
]

def run(text):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "date.c"); exe = os.path.join(d, "t")
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
