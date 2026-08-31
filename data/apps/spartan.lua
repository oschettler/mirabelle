-- SPDX-License-Identifier: GPL-3.0-or-later
-- Ein SPARTAN://-Browser, ganz in Lua.
--
-- Er ist der Beweis, dass die Anwendungsschnittstelle für etwas taugt, das
-- beim Entwurf nicht auf dem Tisch lag: eine Adresszeile, ein Abruf über das
-- Netz, eine Seite aus Gemtext, Verweise zum Anklicken, ein Verlauf.
--
-- Was er aus C benutzt: net.fetch, net.resolve, die Gemtext-Anzeige, den
-- Rollbalken und die Zeichenfunktionen. Was er selbst macht: die Adresszeile,
-- den Verlauf und die Frage, was Return gerade bedeutet.
--
-- Das ist Absicht. Umbrechen, Verweise durchzählen, sie mit Ziffern
-- auswählen, blättern - das kann kein Skript besser als das Programm, es kann
-- es nur anders. Anders heißt hier: eine zweite Wahrheit, die auseinandergeht,
-- sobald sich eine der beiden ändert.
--
-- Bedienung:
--   Return         die gezeigte Adresse abrufen, oder den gewählten Verweis
--   tippen         eine neue Adresse eingeben (Esc bricht ab)
--   Ziffern        einen Verweis auswählen
--   Hoch/Runter    blättern
--   Backspace      im Verlauf zurück (beim Tippen: Zeichen löschen)

local START = "spartan://mozz.us/"

-- Die Breite des Rollbalkens kommt aus dem Thema, nicht aus dieser Datei.
local BAR = theme.scrollbar_w

-- Zeilenhöhe der Adresszeile - für Zeichnen UND Ereignisbehandlung dieselbe
-- Zahl, damit ein Klick genau dort trifft, wo die Zeile gezeichnet wird.
local ADDR_H = textheight() + 2

-- Anzeige und Rollbalken sind die echten Widgets aus dem Programm.
--
-- Der Balken wird AN die Anzeige gehängt: beide teilen sich dann ein
-- Bildlaufmodell. Es gibt also keine zweite Zahl, die jemand abgleichen
-- müsste, und keine Stelle, an der die Anzeige woanders steht als der Balken
-- es zeigt.
local view = gemview()
local bar  = scrollbar(view)

browser = {
  address  = START,
  editing  = false,   -- steht die Schreibmarke in der Adresszeile?
  selected = false,   -- ist die ganze Adresse markiert (nach einem Klick)?
  status   = "",
  history  = {},
  heading  = nil,     -- die erste Überschrift der Seite, für den Fenstertitel

  -- Nach außen sichtbar, damit ein Test dorthin klicken kann, wo ein Nutzer
  -- klickt, und von der Konsole aus sehen kann, was angezeigt wird.
  view      = view,
  scrollbar = bar,
}

-- Die erste Überschrift einer Seite. Sie ist das, was ein Browser als Titel
-- zeigt; hat die Seite keine, bleibt die Adresse übrig.
local function first_heading(body)
  for _, line in ipairs(gemtext.parse(body)) do
    if line.kind == "heading" and line.text ~= "" then return line.text end
  end
  return nil
end

function browser.go(url, remember)
  browser.status = T("spartan.loading")

  local page, why = net.fetch(url)
  if not page then
    browser.status = why or T("spartan.failed")
    return false
  end

  browser.heading = nil

  if page.status == 3 then
    -- Umleitungen werden gezeigt, nicht verfolgt. Wer ihnen von selbst folgt,
    -- verschweigt dem Nutzer, wo er gelandet ist - und zwei Server, die
    -- aufeinander zeigen, ergeben ein Aufhängen statt einer Meldung.
    browser.status = T("spartan.redirect") .. " " .. page.meta
    view:set_text("=> " .. page.meta .. "\n")
    return true
  end

  if page.status ~= 2 then
    browser.status = page.status .. " " .. page.meta
    view:set_text(page.body or "")
    return true
  end

  if remember ~= false and browser.address ~= url then
    browser.history[#browser.history + 1] = browser.address
  end

  browser.address = page.url or url
  browser.status  = page.meta
  browser.heading = first_heading(page.body)
  view:set_text(page.body)
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
  local href = view:link_url(number)
  if not href then return false end

  local target = net.resolve(browser.address, href)
  if not target then
    browser.status = T("spartan.badlink")
    return false
  end
  return browser.go(target)
end

-- Die Anzeige meldet mit was_opened(), dass ein Verweis geöffnet werden soll -
-- durch Doppelklick oder Return. Nach jedem Ereignis, das sie bekommen hat,
-- ist das die Frage, die sich das Skript stellen muss.
local function follow_if_opened()
  if view:was_opened() then browser.open_link(view:selected()) end
end

app{
  name  = "spartan",
  title = "app.spartan",

  -- Was im Fenstertitel steht: die Überschrift der Seite, sonst die Adresse.
  -- Den Namen der Anwendung setzt die Shell selbst dahinter.
  window_title = function()
    return browser.heading or browser.address
  end,

  draw = function(w, h)
    cls()

    -- Die Adresszeile. Beim Tippen blinkt dahinter eine Schreibmarke - ein
    -- senkrechter Strich im selben Takt wie in jedem Textfeld des Programms,
    -- gezeichnet im Modus xor, damit derselbe Aufruf sie setzt und löscht.
    rect(2, 2, w - 4, ADDR_H + 4)
    print(browser.address, 6, 5)

    if browser.editing then
      if browser.selected then
        -- Die ganze Adresse ist markiert, wie nach einem Klick ins Feld: ein
        -- umgekehrtes Rechteck statt einer blinkenden Schreibmarke - genau wie
        -- ein markiertes Textfeld sonst im Programm aussieht.
        mode("xor")
        rectfill(6, 4, textwidth(browser.address), ADDR_H - 2)
        mode("copy")
      elseif caret() then
        local cx = 6 + textwidth(browser.address)
        mode("xor")
        line(cx, 5, cx, 5 + textheight() - 1)
        mode("copy")
      end
    end

    -- Die Zeile unten: der Inhaltstyp der geholten Seite, eine Meldung, oder
    -- - solange nichts geholt wurde - was zu tun ist. Ein leeres Fenster, in
    -- dem man raten muss, ist keine Bedienoberfläche.
    --
    -- Sie steht unten und nicht oben, weil dort die Antwort hingehört: oben
    -- die Frage, in der Mitte die Seite, unten was daraus geworden ist.
    local status = browser.status
    if status == "" and view:link_count() == 0 then status = T("spartan.hint") end

    -- Das Größenfeld wird über den Inhalt gezeichnet und sitzt in der unteren
    -- rechten Ecke. Anzeige, Balken und Meldung hören davor auf - sonst läge
    -- der untere Pfeil des Balkens darunter, und ein Klick darauf täte nichts.
    local foot = h - ADDR_H - 2
    local y    = ADDR_H + 10

    if status ~= "" then
      clip(4, foot, w - 8 - theme.grow_box, ADDR_H)
      print(status, 6, foot + 1)
      clip()
    end

    -- Anzeige und Balken stehen nebeneinander im selben Rahmen wie die
    -- Adresszeile darüber. Einen Trennstrich braucht es nicht: die Anzeige
    -- zeichnet ihren eigenen Rand, und zwei Striche übereinander sähen aus wie
    -- ein Fehler.
    --
    -- Erst die Anzeige, dann der Balken: die Anzeige bricht beim Zeichnen um
    -- und stellt dabei das gemeinsame Modell auf ihre neue Zeilenzahl ein.
    view:place(2, y, w - 4 - BAR, foot - y - 2)
    view:draw()

    bar:place(w - 2 - BAR, y, BAR, foot - y - 2)
    bar:draw()
  end,

  event = function(e)
    -- Das Rad rollt den Text, wo auch immer der Zeiger steht: das Fenster
    -- zeigt nur eine Sache. Anzeige und Balken nehmen es jeweils nur über
    -- sich an, weil sie sonst neben anderen Widgets sitzen.
    if e.kind == "wheel" then
      bar:scroll_by(-e.wheel)
      return true
    end

    if e.kind == "mouse_down" or e.kind == "mouse_move"
       or e.kind == "mouse_up" then
      -- Ein Klick in die Adresszeile holt den Fokus dorthin, mit der ganzen
      -- Adresse markiert - wie ein Textfeld, das man anklickt, statt es
      -- blind zu übertippen.
      if e.kind == "mouse_down" and e.y >= 2 and e.y < 2 + ADDR_H then
        browser.editing  = true
        browser.selected = true
        caret_wake()
        return true
      end

      if bar:event(e) then return true end

      local used = view:event(e)
      follow_if_opened()
      return used
    end

    if e.kind == "text" then
      if browser.editing then
        if browser.selected then
          -- Getippt wird über die Markierung hinweg: sie verschwindet, wie
          -- bei jedem markierten Textfeld im Programm.
          browser.address  = e.text
          browser.selected = false
        else
          browser.address = browser.address .. e.text
        end
        return true
      end

      -- Ziffern wählen einen Verweis; das macht die Anzeige. Was sie nicht
      -- nimmt, ist der Anfang einer neuen Adresse.
      if view:event(e) then return true end

      browser.editing  = true
      browser.selected = false
      browser.address  = e.text
      return true
    end

    if e.kind ~= "key_down" then return false end

    -- Return, Backspace und Esc gehören der Adresszeile, solange dort etwas
    -- geschieht. Deshalb entscheidet das Skript über sie, bevor die Anzeige
    -- sie zu sehen bekommt.
    if e.key == key.enter then
      if view:selected() > 0 and not browser.editing then
        browser.open_link(view:selected())
      else
        -- Auch ohne Tippen: Return holt, was in der Zeile steht. So kommt man
        -- beim ersten Öffnen weiter, ohne die Adresse abzuschreiben.
        browser.editing  = false
        browser.selected = false
        browser.go(browser.address)
      end
      return true

    elseif e.key == key.backspace then
      if browser.editing then
        if browser.selected then
          browser.address  = ""
          browser.selected = false
        else
          browser.address = browser.address:sub(1, -2)
        end
      else
        browser.back()
      end
      return true

    elseif e.key == key.escape then
      browser.editing  = false
      browser.selected = false
      return true
    end

    -- Alles Übrige - Hoch, Runter, Bild auf und ab, Pos1, Ende - ist
    -- Blättern, und das kann die Anzeige selbst.
    local used = view:event(e)
    follow_if_opened()
    return used
  end,
}
