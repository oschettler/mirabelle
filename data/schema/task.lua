-- Aufgaben.
--
-- Diese Datei ist die Anwendung. Es gibt keinen Programmcode für Aufgaben -
-- der Browser liest diese Tabelle und baut daraus Liste, Formular und Menü.
--
-- Der Vertrag ist nicht Lua, sondern die Struktur `schema` in app/schema.h
-- (D-15). Lua ist die Schreibweise, in der sie hier steht, und der Grund für
-- diese Wahl steht dort: eine Tabelle in einer Sprache, die es ohnehin gibt,
-- statt eines eigenen kleinen Formats mit eigenem Parser und eigenen
-- Fehlermeldungen. Nebenbei erlaubt sie Rechnungen - `values` ließe sich aus
-- einer Schleife bauen.

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
