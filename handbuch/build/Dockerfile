# SPDX-License-Identifier: GPL-3.0-or-later
# Dockerfile für das mirabelle-Handbuch
# Enthält alle Tools zum Erstellen von EPUB, PDF und HTML aus AsciiDoc

FROM asciidoctor/docker-asciidoctor:latest

LABEL maintainer="Olav Schettler <olav@schettler.net>"
LABEL description="Build environment für das Handbuch der Visual Novel 'Das Netzwerk'"

WORKDIR /documents

CMD ["asciidoctor", "--version"]
