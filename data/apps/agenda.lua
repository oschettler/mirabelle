-- Eine Anwendung, die es nur in Lua gibt.
--
-- Sie beweist, dass die Anbindung trägt: sie liest Datensätze, ändert sie,
-- schreibt sie zurück und zeichnet. Kein C-Code weiß von ihr.
--
-- Aufgerufen wird sie über zwei Funktionen, die das Programm kennt:
--
--   agenda.update()   rechnet, ändert Daten
--   agenda.draw(w, h) zeichnet in den Bildspeicher
--
-- Mehr Vereinbarung braucht es nicht. Ein Ereignisbus wäre die nächste Stufe;
-- solange eine Anwendung nur diese beiden Dinge tut, wäre er Maschinerie ohne
-- Anlass.

agenda = {}

local COLLECTION = "Aufgaben"

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

function agenda.update(today)
  agenda.today   = today
  agenda.tasks   = agenda.open_tasks()
  agenda.late    = agenda.overdue(today)
end

function agenda.draw(w, h)
  cls()
  pattern("black")
  rect(0, 0, w, h)

  local y = 4
  print(Tn("list.count", #agenda.tasks), 6, y)
  y = y + textheight() + 2

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
end
