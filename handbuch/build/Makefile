# SPDX-License-Identifier: GPL-3.0-or-later
# Makefile für das mirabelle-Handbuch
# Erstellt EPUB, PDF und HTML aus AsciiDoc-Quellen mittels Docker

# Variablen
# SRC_DIR und OUT_DIR sind relativ zum Handbuch-Wurzelverzeichnis
# (/documents im Container). HOST_* sind dieselben Pfade, aber von build/
# aus gesehen, für Kommandos, die direkt auf dem Rechner laufen (mkdir, rm, find).
PROJECT_NAME = mirabelle-handbuch
BUILD_DIR = build
SRC_DIR = src
OUT_DIR = output
MASTER_DOC = $(SRC_DIR)/00-master.adoc
HOST_OUT_DIR = ../output
HOST_SRC_DIR = ../src

# Docker
# Gemountet wird das Handbuch-Wurzelverzeichnis (eine Ebene über build/),
# damit src/ (mit den Diagrammen unter src/diagramme/) und build/ gemeinsam
# im Container sichtbar sind.
DOCKER_IMAGE = asciidoctor/docker-asciidoctor:latest
DOCKER_RUN = docker run --rm -v "$(CURDIR)/..":/documents -w /documents $(DOCKER_IMAGE)

# Standard-Target
.PHONY: all
all: epub pdf
	@echo "Alle Formate erstellt"

# Verzeichnisse erstellen
.PHONY: dirs
dirs:
	@mkdir -p $(HOST_OUT_DIR)
	@echo "Verzeichnisse erstellt"

# Docker-Image vorbereiten (pull wenn nötig)
.PHONY: docker-pull
docker-pull:
	@echo "Prüfe Docker-Image..."
	@docker pull $(DOCKER_IMAGE)
	@echo "Docker-Image bereit"

# EPUB erstellen
.PHONY: epub
epub: dirs
	@echo "Erstelle EPUB..."
	@$(DOCKER_RUN) asciidoctor-epub3 \
		-D $(OUT_DIR) \
		-o $(PROJECT_NAME).epub \
		-a ebook-format=epub3 \
		$(MASTER_DOC)
	@echo "EPUB erstellt: $(OUT_DIR)/$(PROJECT_NAME).epub"

# PDF erstellen
.PHONY: pdf
pdf: dirs
	@echo "Erstelle PDF..."
	@$(DOCKER_RUN) asciidoctor-pdf \
		-D $(OUT_DIR) \
		-o $(PROJECT_NAME).pdf \
		-a pdf-theme=$(BUILD_DIR)/theme.yml \
		-a allow-uri-read \
		$(MASTER_DOC)
	@echo "PDF erstellt: $(OUT_DIR)/$(PROJECT_NAME).pdf"

# HTML erstellen
.PHONY: html
# data-uri bettet die Bilder in die HTML-Datei ein. Ohne das verweist
# asciidoctor relativ auf das Quellverzeichnis, und die fertige Datei in
# output/ zeigt keine Bilder mehr, sobald man sie irgendwohin kopiert.
html: dirs
	@echo "Erstelle HTML..."
	@$(DOCKER_RUN) asciidoctor \
		-D $(OUT_DIR) \
		-o $(PROJECT_NAME).html \
		-a toc=left \
		-a toclevels=2 \
		-a icons=font \
		-a data-uri \
		$(MASTER_DOC)
	@echo "HTML erstellt: $(OUT_DIR)/$(PROJECT_NAME).html"

# Diagramme generieren
.PHONY: diagrams
diagrams:
	@echo "Generiere Diagramme..."
	@chmod +x ./generate-diagrams.sh
	@./generate-diagrams.sh

# Statistiken
.PHONY: stats
stats:
	@echo "Handbuch-Statistiken:"
	@echo ""
	@echo "Kapitel:"
	@find $(HOST_SRC_DIR) -name "*.adoc" -type f | wc -l | xargs echo "  Dateien:"
	@echo ""
	@echo "Wörter:"
	@find $(HOST_SRC_DIR) -name "*.adoc" -type f -exec cat {} \; | wc -w | xargs echo "  Gesamt:"

# Aufräumen
.PHONY: clean
clean:
	@echo "Räume auf..."
	@rm -rf $(HOST_OUT_DIR)
	@echo "Output-Verzeichnis gelöscht"

# Hilfe
.PHONY: help
help:
	@echo "Verfügbare Targets:"
	@echo ""
	@echo "  all          - Erstellt EPUB und PDF"
	@echo "  epub         - Erstellt EPUB"
	@echo "  pdf          - Erstellt PDF"
	@echo "  html         - Erstellt HTML"
	@echo "  diagrams     - Generiert SVG-Diagramme aus nomnoml"
	@echo "  docker-pull  - Lädt/aktualisiert Docker-Image"
	@echo "  stats        - Zeigt Statistiken"
	@echo "  clean        - Löscht Output-Verzeichnis"
	@echo "  help         - Zeigt diese Hilfe"
	@echo ""
	@echo "Beispiele:"
	@echo "  make all        # EPUB und PDF erstellen"
	@echo "  make html       # HTML erstellen (am einfachsten zu lesen)"
	@echo "  make diagrams   # Diagramme generieren"
	@echo ""
	@echo "Voraussetzungen:"
	@echo "  - Docker muss installiert und gestartet sein"
	@echo "  - Das Image $(DOCKER_IMAGE) wird automatisch geladen"
	@echo "  - nomnoml muss für Diagramme installiert sein (npm install -g nomnoml)"

# Phony-Targets
.PHONY: all epub pdf html diagrams dirs docker-pull stats clean help
