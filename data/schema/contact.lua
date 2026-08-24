-- Kontakte. Dieselbe Anwendung wie Aufgaben, andere Felder.
--
-- Es gibt keinen Programmcode für Kontakte. Der Browser liest diese Tabelle
-- und baut daraus Liste, Formular und Menü.

return {
  type   = "contact",
  folder = "Kontakte",
  label  = "app.contacts",

  sort    = "name",
  columns = { "name", "city", "phone" },
  form    = { "name", "city", "phone", "email", "body" },

  fields = {
    { name = "name",  kind = "text",    label = "field.name", required = true },
    { name = "city",  kind = "text",    label = "field.city" },
    { name = "phone", kind = "text",    label = "field.phone" },
    { name = "email", kind = "text",    label = "field.email" },
    { name = "body",  kind = "gemtext", label = "field.notes" },
  },
}
