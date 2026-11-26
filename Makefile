# Offbeat Turbo – Build für Plugin (VST3, VST2, AU, CLAP, LV2, LADSPA) und Offline-Werkzeug turbo-render.
#   make             Plugin + turbo-render (macOS: Universal-Binary für Intel und Apple Silicon)
#   make install     macOS: nach ~/Library/Audio/Plug-Ins kopieren und ad-hoc signieren; LADSPA auch in den LMMS-Ordner
#   make check       Abnahme mit turbo-render --check-all
#   make standalone  Standalone-App (z. B. für Bilder der Oberfläche, siehe docs/CODE.md)
# Windows (MSYS2/MinGW) und Linux: `make` baut nach bin/; Installation siehe README.

NAME = offbeat-turbo
CXX ?= c++
DSP_SRC = src/dsp/Turbo.cpp src/dsp/Params.cpp src/dsp/ParamText.cpp src/dsp/Preview.cpp
DSP_HDR = $(wildcard src/dsp/*.hpp) tools/Measure.hpp
PLUGINS_DIR = $(HOME)/Library/Audio/Plug-Ins
# LMMS-LADSPA-Ordner aus ~/.lmmsrc.xml (Attribut laddir), sonst der LMMS-Standard
LMMS_LADSPA_DIR := $(shell sed -n 's/.*laddir="\([^"]*\)".*/\1/p' $(HOME)/.lmmsrc.xml 2>/dev/null | head -1)
ifeq ($(LMMS_LADSPA_DIR),)
LMMS_LADSPA_DIR = $(HOME)/Documents/lmms/plugins/ladspa/
endif

# UNIVERSAL=false baut auf dem Mac nur für die eigene Architektur (z. B. schneller auf einem Intel-Mac).
UNIVERSAL ?= true
ifeq ($(shell uname -s)$(UNIVERSAL),Darwintrue)
# Universal-Binary: läuft auf Intel- und Apple-Silicon-Macs. NOOPT: ohne x86-only-Flags (-mfpmath=sse).
ARCH_FLAGS = -arch x86_64 -arch arm64 -mmacosx-version-min=10.15
PLUGIN_ENV = CFLAGS="$(ARCH_FLAGS)" CXXFLAGS="$(ARCH_FLAGS)" LDFLAGS="$(ARCH_FLAGS)" NOOPT=true
endif

all: plugin render

plugin:
	$(PLUGIN_ENV) $(MAKE) -C plugin
	$(MAKE) -C dpf/utils/lv2-ttl-generator
	dpf/utils/generate-ttl.sh $(CURDIR)/bin

standalone:
	$(PLUGIN_ENV) $(MAKE) -C plugin jack

render: bin/turbo-render

# Demo-Loop der Vorschau als C-Array (für Renderer; das Plugin erzeugt sich seine Kopie in plugin/Makefile)
build/DemoLoop.h: assets/demo-loop.raw
	@mkdir -p build
	cd assets && xxd -i demo-loop.raw > ../build/DemoLoop.h

bin/turbo-render: tools/turbo-render.cpp $(DSP_SRC) $(DSP_HDR) build/DemoLoop.h
	@mkdir -p bin
	$(CXX) -std=gnu++17 -O2 -Wall -Wextra -Isrc/dsp -Itools -Ibuild tools/turbo-render.cpp $(DSP_SRC) -o $@

check: render
	bin/turbo-render --check-all
	bin/turbo-render --preview-check

install: plugin
	rm -rf "$(PLUGINS_DIR)/VST3/$(NAME).vst3" "$(PLUGINS_DIR)/LV2/$(NAME).lv2" "$(PLUGINS_DIR)/VST/$(NAME).vst" \
	       "$(PLUGINS_DIR)/Components/$(NAME).component" "$(PLUGINS_DIR)/CLAP/$(NAME).clap"
	mkdir -p "$(PLUGINS_DIR)/VST3" "$(PLUGINS_DIR)/LV2" "$(PLUGINS_DIR)/VST" "$(PLUGINS_DIR)/Components" "$(PLUGINS_DIR)/CLAP"
	cp -R bin/$(NAME).vst3 "$(PLUGINS_DIR)/VST3/"
	cp -R bin/$(NAME).lv2 "$(PLUGINS_DIR)/LV2/"
	cp -R bin/$(NAME).vst "$(PLUGINS_DIR)/VST/"
	cp -R bin/$(NAME).component "$(PLUGINS_DIR)/Components/"
	cp -R bin/$(NAME).clap "$(PLUGINS_DIR)/CLAP/"
	codesign --force --deep -s - "$(PLUGINS_DIR)/VST3/$(NAME).vst3"
	codesign --force -s - "$(PLUGINS_DIR)/LV2/$(NAME).lv2/$(NAME).dylib"  # LV2-Ordner ist kein Bundle
	codesign --force --deep -s - "$(PLUGINS_DIR)/VST/$(NAME).vst"
	codesign --force --deep -s - "$(PLUGINS_DIR)/Components/$(NAME).component"
	codesign --force --deep -s - "$(PLUGINS_DIR)/CLAP/$(NAME).clap"
	mkdir -p "$(PLUGINS_DIR)/LADSPA" "$(LMMS_LADSPA_DIR)"
	cp bin/$(NAME)-ladspa.dylib "$(PLUGINS_DIR)/LADSPA/"
	rm -f "$(LMMS_LADSPA_DIR)/$(NAME)-ladspa.dylib"  # alte Kopie mit falscher Endung
	cp bin/$(NAME)-ladspa.dylib "$(LMMS_LADSPA_DIR)/$(NAME)-ladspa.so"  # LMMS 1.2 lädt nur *.so
	codesign --force -s - "$(PLUGINS_DIR)/LADSPA/$(NAME)-ladspa.dylib"
	codesign --force -s - "$(LMMS_LADSPA_DIR)/$(NAME)-ladspa.so"
	killall -9 AudioComponentRegistrar 2>/dev/null || true  # AU-Liste neu einlesen

clean:
	rm -rf bin build

.PHONY: all plugin standalone render check install clean
