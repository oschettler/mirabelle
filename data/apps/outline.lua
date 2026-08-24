-- Ein Outliner über den Notizen.
--
-- Er zeigt jede Notiz als Zeile und klappt sie auf: darunter erscheinen ihre
-- Überschriften und Aufzählungspunkte, eingerückt. Damit lässt sich ein
-- Stapel Notizen überfliegen, ohne jede einzeln zu öffnen.
--
-- Die Gliederung kommt aus dem Gemtext selbst - „#" ist eine Überschrift,
-- „*" ein Punkt. Es gibt kein zweites Format daneben, und eine Notiz, die im
-- Outliner Struktur hat, hat sie auch im Notizfenster.
--
-- Diese Datei ist als Vorlage gedacht. Wer eine eigene Anwendung schreibt,
-- kann sie kopieren und die drei Funktionen unten austauschen.

local COLLECTION = "Notizen"
local INDENT     = 14

outline = {
  rows     = {},    -- { text=, level=, id=, kind= }
  open     = {},    -- welche Notizen aufgeklappt sind, nach Kennung
  selected = 1,
  top      = 1,
}

-- Zerlegt einen Gemtext-Körper in Gliederungszeilen.
--
-- Absichtlich nur die zwei Formen, die eine Gliederung ausmachen. Alles
-- andere wäre eine zweite Anzeige von Fließtext, und dafür gibt es das
-- Notizfenster.
local function structure_of(body)
  local rows = {}
  for raw in (body or ""):gmatch("[^\n]*") do
    local hashes, heading = raw:match("^(#+)%s*(.*)$")
    local item            = raw:match("^%*%s+(.*)$")

    if hashes and heading ~= "" then
      rows[#rows + 1] = { text = heading, level = math.min(#hashes, 3), kind = "heading" }
    elseif item and item ~= "" then
      rows[#rows + 1] = { text = item, level = 2, kind = "item" }
    end
  end
  return rows
end

function outline.rebuild()
  outline.rows = {}

  for _, note in ipairs(store.find(COLLECTION, { sort = "title" })) do
    local row = {
      text  = note.title or note.id,
      level = 0,
      id    = note.id,
      kind  = "note",
    }
    outline.rows[#outline.rows + 1] = row

    if outline.open[note.id] then
      for _, sub in ipairs(structure_of(note.body)) do
        sub.id = note.id
        outline.rows[#outline.rows + 1] = sub
      end
    end
  end

  if outline.selected > #outline.rows then outline.selected = #outline.rows end
  if outline.selected < 1 then outline.selected = 1 end
end

function outline.toggle()
  local row = outline.rows[outline.selected]
  if not row then return end

  outline.open[row.id] = not outline.open[row.id]
  outline.rebuild()
end

-- Hält die Auswahl im Bild. Dieselbe Rechnung wie überall: liegt sie darüber,
-- rückt der Anfang auf sie; liegt sie darunter, rückt er nach.
local function reveal(visible)
  if outline.selected < outline.top then
    outline.top = outline.selected
  elseif outline.selected >= outline.top + visible then
    outline.top = outline.selected - visible + 1
  end
  if outline.top < 1 then outline.top = 1 end
end

app{
  name  = "outline",
  title = "app.outline",

  update = function()
    outline.rebuild()
  end,

  draw = function(w, h)
    cls()

    local lh      = textheight() + 2
    local visible = math.max(1, math.floor((h - 8) / lh))
    reveal(visible)

    if #outline.rows == 0 then
      print(T("outline.empty"), 8, 6)
      return
    end

    local y = 4
    for i = outline.top, math.min(#outline.rows, outline.top + visible - 1) do
      local row = outline.rows[i]
      local x   = 6 + row.level * INDENT

      if row.kind == "note" then
        -- Ein Dreieck sagt, ob die Notiz aufgeklappt ist. Aufgeklappt zeigt
        -- es nach unten, zugeklappt nach rechts - so herum kennt man es.
        -- Aufgeklappt zeigt es nach unten, zugeklappt nach rechts. Gezeichnet
        -- aus Linien statt aus einem Zeichen: die Schrift hat keine Dreiecke,
        -- und ein „v" wäre ein Buchstabe und kein Zeiger.
        if outline.open[row.id] then
          for i = 0, 3 do line(x + i, y + 3 + i, x + 6 - i, y + 3 + i) end
        else
          for i = 0, 3 do line(x + i, y + 2 + i, x + i, y + 8 - i) end
        end
        x = x + 10
      elseif row.kind == "item" then
        print(T("gemtext.bullet"), x, y)
        x = x + 10
      end

      print(row.text, x, y)

      if i == outline.selected then
        mode("xor")
        rectfill(2, y, w - 4, lh - 1)
        mode("copy")
      end

      y = y + lh
    end
  end,

  event = function(e)
    if e.kind ~= "key_down" then return false end

    -- Die Tasten haben Namen. Ihre Zahlen zu raten geht schief: Return ist
    -- 13 und nicht 10, und wer sich vertut, merkt es erst, wenn die Taste
    -- nichts tut.
    if e.key == key.down then
      outline.selected = math.min(outline.selected + 1, #outline.rows)
      return true
    elseif e.key == key.up then
      outline.selected = math.max(outline.selected - 1, 1)
      return true
    elseif e.key == key.enter or e.key == key.space
        or e.key == key.right or e.key == key.left then
      outline.toggle()
      return true
    end
    return false
  end,
}
