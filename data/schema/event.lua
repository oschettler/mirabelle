-- Termine. Dieselbe Anwendung wie Aufgaben und Kontakte - mit einer Ausnahme:
-- die Übersicht ist ein Monatsraster und keine Liste.
--
-- Das ist die Stelle, an der die Generik aufhört. Ein Monatsraster in
-- Konfiguration zu fassen wäre komplizierter als der Sonderfall; deshalb gibt
-- es eine Ansicht dafür, und das Schema sagt nur, welche es will und welches
-- Feld den Tag trägt.

return {
  type   = "event",
  folder = "Termine",
  label  = "app.events",

  view       = "month",
  view_field = "date",

  sort    = "date",
  columns = { "date", "title" },
  form    = { "title", "date", "time", "body" },

  fields = {
    { name = "title", kind = "text",    label = "field.title", required = true },
    { name = "date",  kind = "date",    label = "field.date",  required = true },
    { name = "time",  kind = "text",    label = "field.time" },
    { name = "body",  kind = "gemtext", label = "field.notes" },
  },
}
