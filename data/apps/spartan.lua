-- Ein SPARTAN://-Browser, ganz in Lua.
--
-- Er ist der Beweis, dass die Anwendungsschnittstelle für etwas taugt, das
-- beim Entwurf nicht auf dem Tisch lag: eine Adresszeile, ein Abruf über das
-- Netz, eine Seite aus Gemtext, Verweise zum Anklicken, ein Verlauf.
--
-- Was er aus C benutzt: net.fetch, net.resolve, gemtext.parse und die
-- Zeichenfunktionen. Was er selbst macht: alles andere - den Umbruch, die
-- Auswahl, die Tastatur, den Verlauf.
--
-- Bedienung:
--   Return         die gezeigte Adresse abrufen, oder den gewählten Verweis
--   tippen         eine neue Adresse eingeben (Esc bricht ab)
--   Ziffern        einen Verweis auswählen
--   Hoch/Runter    blättern
--   Backspace      im Verlauf zurück (beim Tippen: Zeichen löschen)

local START = "spartan://mozz.us/"

-- Breite des Rollbalkens. Im Thema steht dieselbe Zahl (scrollbar_w); Lua
-- kommt an das Thema nicht heran, also steht sie hier noch einmal. Das ist
-- eine bekannte Naht: wer das Thema ändert, ändert sie hier mit.
local BAR = 16

browser = {
  address  = START,
  editing  = false,   -- steht die Schreibmarke in der Adresszeile?
  lines    = {},      -- gefaltete Anzeigezeilen
  links    = {},      -- Adressen, nach Nummer
  selected = 0,
  top      = 1,
  status   = "",
  history  = {},

  visible  = 1,      -- wie viele Zeilen zuletzt ins Fenster passten
  bar      = nil,    -- Lage des Rollbalkens, beim Zeichnen gesetzt
  drag     = nil,    -- Abstand des Zeigers zur Schiebervorderkante
}

-- Klemmt den Anfang der Anzeige auf einen gültigen Bereich.
local function clamp_top()
  local last = math.max(1, #browser.lines - browser.visible + 1)
  if browser.top > last then browser.top = last end
  if browser.top < 1 then browser.top = 1 end
end

-- Lage und Länge des Schiebers in der Rinne.
--
-- Dieselbe Rechnung wie in ui/scroll.c: die Länge zeigt an, welcher Anteil des
-- Ganzen zu sehen ist, und eine Untergrenze hält ihn greifbar.
local function thumb(track)
  local total = #browser.lines
  if total <= browser.visible then return 0, track end

  local len = math.max(BAR, math.floor(track * browser.visible / total + 0.5))
  local span = track - len
  local maxtop = total - browser.visible

  local pos = (span > 0) and math.floor((browser.top - 1) * span / maxtop + 0.5) or 0
  return pos, len
end

-- Zeichnet den Balken und merkt sich seine Lage für die Bedienung.
local function draw_bar(x, y, h)
  browser.bar = { x = x, y = y, h = h, track = h - 2 * BAR + 2 }

  if #browser.lines <= browser.visible then
    -- Nichts zu rollen: nur der Umriss, wie in System 1.
    pattern("white") rectfill(x, y, BAR, h)
    pattern("black") rect(x, y, BAR, h)
    return
  end

  pattern("gray")  rectfill(x, y, BAR, h)
  pattern("black") rect(x, y, BAR, h)

  -- Die beiden Pfeilfelder.
  for i, dir in ipairs({ -1, 1 }) do
    local by = (i == 1) and y or (y + h - BAR)
    pattern("white") rectfill(by == y and x or x, by, BAR, BAR)
    pattern("black") rect(x, by, BAR, BAR)

    local cx, cy = x + BAR // 2, by + BAR // 2
    for k = 0, 3 do
      local w = 2 * k + 1
      local ly = (dir < 0) and (cy - 2 + k) or (cy + 2 - k)
      line(cx - k, ly, cx + k, ly)
    end
  end

  local pos, len = thumb(browser.bar.track)
  local ty = y + BAR - 1 + pos

  pattern("white") rectfill(x, ty, BAR, len)
  pattern("black") rect(x, ty, BAR, len)
end

-- Bedient den Balken. Liefert true, wenn der Klick ihm galt.
local function bar_click(mx, my)
  local b = browser.bar
  if not b or mx < b.x or mx >= b.x + BAR then return false end
  if my < b.y or my >= b.y + b.h then return false end

  if #browser.lines <= browser.visible then return true end

  if my < b.y + BAR then
    browser.top = browser.top - 1
  elseif my >= b.y + b.h - BAR then
    browser.top = browser.top + 1
  else
    local pos, len = thumb(b.track)
    local ty = b.y + BAR - 1 + pos

    if my < ty then
      browser.top = browser.top - browser.visible
    elseif my >= ty + len then
      browser.top = browser.top + browser.visible
    else
      browser.drag = my - ty
    end
  end

  clamp_top()
  return true
end

-- Zieht am Schieber.
local function bar_drag(my)
  local b = browser.bar
  if not b or not browser.drag then return false end

  local total = #browser.lines
  local _, len = thumb(b.track)
  local span = b.track - len
  if span <= 0 then return true end

  local pos = my - browser.drag - (b.y + BAR - 1)
  pos = math.max(0, math.min(span, pos))

  browser.top = 1 + math.floor(pos * (total - browser.visible) / span + 0.5)
  clamp_top()
  return true
end

-- Bricht einen Text auf eine Breite in Zeichen um.
--
-- In Zeichen und nicht in Pixeln: die Schrift ist eine Festbreitenschrift,
-- also ist beides dasselbe, und das Rechnen bleibt einfach. Wer je eine
-- Proportionalschrift einbaut, misst hier mit textwidth nach.
local function wrap(text, cols)
  local out = {}
  if text == "" then return { "" } end

  while #text > cols do
    local cut = cols
    -- Rückwärts bis zum letzten Leerzeichen; findet sich keins, wird hart
    -- getrennt - sonst liefe eine lange Adresse aus dem Fenster.
    while cut > 1 and text:sub(cut, cut) ~= " " do cut = cut - 1 end
    if cut <= 1 then cut = cols end

    out[#out + 1] = text:sub(1, cut)
    text = text:gsub("^%s+", "", 1)
    text = text:sub(cut + 1):gsub("^%s+", "", 1)
  end
  out[#out + 1] = text
  return out
end

-- Baut aus dem Gemtext die Anzeigezeilen und die Verweisliste.
function browser.render(body, cols)
  browser.lines = {}
  browser.links = {}

  for _, line in ipairs(gemtext.parse(body)) do
    local prefix = ""
    local text   = line.text

    if line.kind == "link" then
      browser.links[#browser.links + 1] = line.url
      prefix = "[" .. #browser.links .. "] "
      if text == "" then text = line.url end
    elseif line.kind == "item" then
      prefix = T("gemtext.bullet") .. " "
    elseif line.kind == "quote" then
      prefix = "| "
    end

    -- Vorformatiertes bricht nicht um: dort bedeutet die Zeilenlage etwas.
    local parts = (line.kind == "pre") and { text }
                                       or wrap(text, math.max(8, cols - #prefix))

    for i, part in ipairs(parts) do
      browser.lines[#browser.lines + 1] = {
        text = (i == 1 and prefix or string.rep(" ", #prefix)) .. part,
        kind = line.kind,
        link = (line.kind == "link") and #browser.links or nil,
      }
    end
  end

  browser.top = 1
end

function browser.go(url, remember)
  browser.status = T("spartan.loading")

  local page, why = net.fetch(url)
  if not page then
    browser.status = why or T("spartan.failed")
    return false
  end

  if page.status == 3 then
    -- Umleitungen werden gezeigt, nicht verfolgt. Wer ihnen von selbst folgt,
    -- verschweigt dem Nutzer, wo er gelandet ist - und zwei Server, die
    -- aufeinander zeigen, ergeben ein Aufhängen statt einer Meldung.
    browser.status = T("spartan.redirect") .. " " .. page.meta
    browser.render("=> " .. page.meta .. "\n", browser.cols or 60)
    return true
  end

  if page.status ~= 2 then
    browser.status = page.status .. " " .. page.meta
    browser.render(page.body or "", browser.cols or 60)
    return true
  end

  if remember ~= false and browser.address ~= url then
    browser.history[#browser.history + 1] = browser.address
  end

  browser.address  = page.url or url
  browser.status   = page.meta
  browser.selected = 0
  browser.render(page.body, browser.cols or 60)
  return true
end

function browser.back()
  local n = #browser.history
  if n == 0 then return false end

  local prev = browser.history[n]
  browser.history[n] = nil
  browser.go(prev, false)
  return true
end

function browser.open_link(number)
  local href = browser.links[number]
  if not href then return false end

  local target = net.resolve(browser.address, href)
  if not target then
    browser.status = T("spartan.badlink")
    return false
  end
  return browser.go(target)
end

app{
  name  = "spartan",
  title = "app.spartan",

  draw = function(w, h)
    cls()

    local lh   = textheight() + 2
    local text_w = w - BAR
    local cols = math.max(8, math.floor((text_w - 8) / textwidth("M")))

    -- Die Adresszeile. Beim Tippen steht ein Strich dahinter, damit man
    -- sieht, wo die Schreibmarke ist.
    rect(2, 2, w - 4, lh + 4)
    print(browser.address .. (browser.editing and "_" or ""), 6, 5)

    -- Solange nichts geholt wurde, steht hier, was zu tun ist. Ein leeres
    -- Fenster, in dem man raten muss, ist keine Bedienoberfläche.
    local status = browser.status
    if status == "" and #browser.lines == 0 then status = T("spartan.hint") end

    local y = lh + 10
    if status ~= "" then
      print(status, 6, y)
      y = y + lh
    end

    line(2, y, w - 2, y)
    y = y + 3

    local visible = math.max(1, math.floor((h - y) / lh))
    browser.visible = visible
    clamp_top()

    draw_bar(w - BAR, y, h - y)

    for i = browser.top, math.min(#browser.lines, browser.top + visible - 1) do
      local l = browser.lines[i]
      print(l.text, 6, y)

      -- Überschriften bekommen einen Balken darunter. Es gibt nur einen
      -- Schnitt, also muss die Hervorhebung aus der Geometrie kommen.
      if l.kind == "heading" then
        rectfill(6, y + textheight(), textwidth(l.text), 1)
      end

      if l.link and l.link == browser.selected then
        mode("xor")
        rectfill(4, y - 1, text_w - 8, lh - 1)
        mode("copy")
      end

      y = y + lh
    end

    -- Die Breite, mit der umbrochen wurde, merken: ändert sich das Fenster,
    -- wird beim nächsten Abruf anders gefaltet.
    browser.cols = cols
  end,

  event = function(e)
    if e.kind == "mouse_down" then
      return bar_click(e.x, e.y)
    elseif e.kind == "mouse_move" then
      return bar_drag(e.y)
    elseif e.kind == "mouse_up" then
      browser.drag = nil
      return false
    elseif e.kind == "wheel" then
      browser.top = browser.top - e.wheel
      clamp_top()
      return true
    end

    if e.kind == "text" then
      if browser.editing then
        browser.address = browser.address .. e.text
        return true
      end

      -- Ziffern wählen einen Verweis. Mehrstellige entstehen durch
      -- Weitertippen: aus 1 wird 12, solange es einen Verweis 12 gibt.
      local digit = tonumber(e.text)
      if digit then
        local wide = browser.selected * 10 + digit
        if browser.links[wide] then browser.selected = wide
        elseif browser.links[digit] then browser.selected = digit
        else browser.selected = 0 end
        return true
      end

      -- Jedes andere Zeichen beginnt eine neue Adresse.
      browser.editing = true
      browser.address = e.text
      return true
    end

    if e.kind ~= "key_down" then return false end

    if e.key == key.enter then
      if browser.selected > 0 and not browser.editing then
        browser.open_link(browser.selected)
      else
        -- Auch ohne Tippen: Return holt, was in der Zeile steht. So kommt man
        -- beim ersten Öffnen weiter, ohne die Adresse abzuschreiben.
        browser.editing = false
        browser.go(browser.address)
      end
      return true

    elseif e.key == key.backspace then
      if browser.editing then
        browser.address = browser.address:sub(1, -2)
      else
        browser.back()
      end
      return true

    elseif e.key == key.escape then
      browser.editing = false
      return true

    elseif e.key == key.down then
      browser.top = browser.top + 1
      clamp_top()
      return true

    elseif e.key == key.up then
      browser.top = browser.top - 1
      clamp_top()
      return true

    elseif e.key == key.pagedown then
      browser.top = browser.top + browser.visible
      clamp_top()
      return true

    elseif e.key == key.pageup then
      browser.top = browser.top - browser.visible
      clamp_top()
      return true
    end

    return false
  end,
}
