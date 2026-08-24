-- Notizen. Am wenigsten Felder von allen - und trotzdem dieselbe Anwendung.

return {
  type   = "note",
  folder = "Notizen",
  label  = "app.notes",

  sort    = "title",
  columns = { "title" },
  form    = { "title", "body" },

  fields = {
    { name = "title", kind = "text",    label = "field.title", required = true },
    { name = "body",  kind = "gemtext", label = "field.notes" },
  },
}
