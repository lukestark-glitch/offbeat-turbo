# Offbeat Turbo – Build für Plugin (VST3 + LV2 + LADSPA, x86_64) und Offline-Werkzeug.
#   make               Plugin + turbo-render
#   make install       VST3/LV2/LADSPA nach ~/Library/Audio/Plug-Ins, LADSPA zusätzlich in den
#                      LMMS-LADSPA-Ordner (laddir aus ~/.lmmsrc.xml), ad-hoc signieren
#   make check         Abnahme aller Presets und Grenzfälle mit turbo-render

CXX ?= c++
DSP_SRC = src/dsp/Turbo.cpp src/dsp/Params.cpp src/dsp/ParamText.cpp
DSP_HDR = $(wildcard src/dsp/*.hpp) tools/Measure.hpp
VST3_DIR   = $(HOME)/Library/Audio/Plug-Ins/VST3
LV2_DIR    = $(HOME)/Library/Audio/Plug-Ins/LV2
LADSPA_DIR = $(HOME)/Library/Audio/Plug-Ins/LADSPA
LMMS_LADSPA_DIR = $(shell sed -n 's/.*laddir="\([^"]*\)".*/\1/p' $(HOME)/.lmmsrc.xml 2>/dev/null | head -1)
ifeq ($(LMMS_LADSPA_DIR),)
LMMS_LADSPA_DIR = $(HOME)/Documents/lmms/plugins/ladspa/
endif

all: plugin render

plugin:
	$(MAKE) -C plugin
	$(MAKE) -C dpf/utils/lv2-ttl-generator
	dpf/utils/generate-ttl.sh $(CURDIR)/bin

render: bin/turbo-render

bin/turbo-render: tools/turbo-render.cpp $(DSP_SRC) $(DSP_HDR)
	@mkdir -p bin
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Isrc/dsp -Itools tools/turbo-render.cpp $(DSP_SRC) -o $@

check: render
	bin/turbo-render --check-all

install: plugin
	rm -rf "$(VST3_DIR)/offbeat-turbo.vst3" "$(LV2_DIR)/offbeat-turbo.lv2"
	mkdir -p "$(VST3_DIR)" "$(LV2_DIR)" "$(LADSPA_DIR)" "$(LMMS_LADSPA_DIR)"
	cp -R bin/offbeat-turbo.vst3 "$(VST3_DIR)/"
	cp -R bin/offbeat-turbo.lv2 "$(LV2_DIR)/"
	cp bin/offbeat-turbo-ladspa.dylib "$(LADSPA_DIR)/"
	rm -f "$(LMMS_LADSPA_DIR)/offbeat-turbo-ladspa.dylib"  # alte Kopie mit falscher Endung
	cp bin/offbeat-turbo-ladspa.dylib "$(LMMS_LADSPA_DIR)/offbeat-turbo-ladspa.so"  # LMMS lädt nur *.so
	codesign --force --deep -s - "$(VST3_DIR)/offbeat-turbo.vst3"
	codesign --force -s - "$(LV2_DIR)/offbeat-turbo.lv2/offbeat-turbo.dylib"  # LV2-Ordner ist kein Bundle
	codesign --force -s - "$(LADSPA_DIR)/offbeat-turbo-ladspa.dylib"
	codesign --force -s - "$(LMMS_LADSPA_DIR)/offbeat-turbo-ladspa.so"

clean:
	rm -rf bin build

.PHONY: all plugin render check install clean
