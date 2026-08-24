-- Eine Anwendung, die es nur in Lua gibt.
--
-- Sie liest Datensätze, ändert sie, schreibt sie zurück und zeichnet. Kein
-- C-Code weiß von ihr; die Schale findet sie, weil sie in data/apps liegt und
-- sich mit app{} anmeldet.

local COLLECTION = "Aufgaben"

agenda = {
  today = "2026-01-01",
  tasks = {},
  late  = 0,
}

-- Alle offenen Aufgaben, nach Fälligkeit.
function agenda.open_tasks()
  return store.find(COLLECTION, { done = "no", sort = "due" })
end

-- Hakt eine Aufgabe ab. Der Datensatz kommt als Tabelle, wird geändert und
-- zurückgeschrieben - store.put erkennt ihn an seiner Kennung wieder.
function agenda.finish(id)
  local rec = store.get(COLLECTION, id)
  if not rec then return false end

  rec.done = "yes"
  store.put(COLLECTION, rec)

  -- Wer sonst noch Aufgaben zeigt, soll es erfahren. Ob jemand zuhört, muss
  -- der Sender nicht wissen.
  send("tasks.changed", id)
  return true
end

-- Wie viele Aufgaben vor diesem Tag fällig sind. Daten stehen als JJJJ-MM-TT
-- im Datensatz, also entscheidet ein gewöhnlicher Textvergleich.
function agenda.overdue(today)
  local n = 0
  for _, task in ipairs(agenda.open_tasks()) do
    if task.due and task.due < today then n = n + 1 end
  end
  return n
end

function agenda.refresh(today)
  agenda.today = today or agenda.today
  agenda.tasks = agenda.open_tasks()
  agenda.late  = agenda.overdue(agenda.today)
end

app{
  name  = "agenda",
  title = "app.agenda",

  update = function()
    -- Bei jedem Bild neu zu lesen wäre verschwenderisch, aber richtig: der
    -- Vault ist die Wahrheit, und ein anderes Fenster kann ihn geändert
    -- haben. Für eine Handvoll Dateien ist das nicht zu merken.
    agenda.refresh()
  end,

  draw = function(w, h)
    cls()

    local y = 4
    print(Tn("list.count", #agenda.tasks), 6, y)
    y = y + textheight() + 3

    line(4, y, w - 4, y)
    y = y + 3

    for _, task in ipairs(agenda.tasks) do
      if y + textheight() > h - 4 then break end

      -- Überfällige bekommen einen Balken davor statt einer Farbe: bei einem
      -- Bit je Pixel ist das der Unterschied, den man sehen kann.
      if task.due and task.due < agenda.today then
        rectfill(4, y + 2, 3, textheight() - 4)
      end

      print(task.title, 12, y)
      y = y + textheight() + 1
    end
  end,

  event = function(e)
    -- Return hakt die oberste Aufgabe ab. Mehr Bedienung braucht eine
    -- Übersicht nicht; wer eine Aufgabe ändern will, öffnet die Aufgaben.
    if e.kind == "key_down" and e.key == 10 and agenda.tasks[1] then
      agenda.finish(agenda.tasks[1].id)
      agenda.refresh()
      return true
    end
    return false
  end,
}
