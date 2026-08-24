-- Aufgaben, in Lua.
--
-- Dieselbe Anwendung wie data/schema/task.schema, in der anderen Schreibweise.
-- Beide ergeben dieselbe Struktur; ein Test lädt sie nebeneinander und
-- vergleicht sie Feld für Feld. Das ist D-15: der Vertrag ist die Struktur,
-- nicht die Sprache.
--
-- Wozu dann zwei Fassungen? Weil die Textdatei ohne Lua auskommt und auf dem
-- Gerät gebraucht wird, und weil Lua Rechnungen erlaubt - `values` ließe sich
-- hier aus einer Schleife bauen, wo die Textdatei sie ausschreiben muss.

return {
  type   = "task",
  folder = "Aufgaben",
  label  = "app.tasks",

  sort    = "due",
  columns = { "done", "title", "due" },
  form    = { "title", "due", "priority", "category", "done", "body" },

  fields = {
    { name = "title",    kind = "text",    label = "field.title", required = true },
    { name = "due",      kind = "date",    label = "field.due" },
    { name = "priority", kind = "choice",  label = "field.priority",
      values = { 1, 2, 3, 4, 5 } },
    { name = "category", kind = "choice",  label = "field.category",
      values = { "privat", "arbeit", "einkauf" } },
    { name = "done",     kind = "bool",    label = "field.done" },
    { name = "body",     kind = "gemtext", label = "field.notes" },
  },
}
