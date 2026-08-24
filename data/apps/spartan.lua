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
--   tippen         Adresse eingeben
--   Return         abrufen, oder den ausgewählten Verweis öffnen
--   Ziffern        einen Verweis auswählen
--   Hoch/Runter    blättern
--   Backspace      im Verlauf zurück (in der Adresszeile: Zeichen löschen)

local START = "spartan://mozz.us/"

browser = {
  address  = START,
  editing  = false,   -- steht die Schreibmarke in der Adresszeile?
  lines    = {},      -- gefaltete Anzeigezeilen
  links    = {},      -- Adressen, nach Nummer
  selected = 0,
  top      = 1,
  status   = "",
  history  = {},
}

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
    browser.render("=> " .. page.meta .. "\n", 60)
    return true
  end

  if page.status ~= 2 then
    browser.status = page.status .. " " .. page.meta
    browser.render(page.body or "", 60)
    return true
  end

  if remember ~= false and browser.address ~= url then
    browser.history[#browser.history + 1] = browser.address
  end

  browser.address  = page.url or url
  browser.status   = page.meta
  browser.selected = 0
  browser.render(page.body, 60)
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
    local cols = math.max(8, math.floor((w - 8) / textwidth("M")))

    -- Die Adresszeile. Beim Tippen steht ein Strich dahinter, damit man
    -- sieht, wo die Schreibmarke ist.
    rect(2, 2, w - 4, lh + 4)
    print(browser.address .. (browser.editing and "_" or ""), 6, 5)

    local y = lh + 10
    if browser.status ~= "" then
      print(browser.status, 6, y)
      y = y + lh
    end

    line(2, y, w - 2, y)
    y = y + 3

    local visible = math.max(1, math.floor((h - y) / lh))
    if browser.top + visible - 1 > #browser.lines then
      browser.top = math.max(1, #browser.lines - visible + 1)
    end

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
        rectfill(4, y - 1, w - 8, lh - 1)
        mode("copy")
      end

      y = y + lh
    end

    -- Die Breite, mit der umbrochen wurde, merken: ändert sich das Fenster,
    -- wird beim nächsten Abruf anders gefaltet.
    browser.cols = cols
  end,

  event = function(e)
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
      if browser.editing then
        browser.editing = false
        browser.go(browser.address)
      elseif browser.selected > 0 then
        browser.open_link(browser.selected)
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
      browser.top = math.min(browser.top + 1, math.max(1, #browser.lines))
      return true

    elseif e.key == key.up then
      browser.top = math.max(browser.top - 1, 1)
      return true
    end

    return false
  end,
}
