#!/usr/bin/make -f
# DPF-Build des Plugins. Aufruf über das Makefile im Projektordner.

NAME = offbeat-turbo

FILES_DSP = \
	TurboPlugin.cpp \
	../src/dsp/Turbo.cpp \
	../src/dsp/Params.cpp \
	../src/dsp/ParamText.cpp

# Params.cpp und ParamText.cpp nutzt auch die UI; sie steckt bei VST3 und LV2 (eine Datei) im selben Binary.
FILES_UI = TurboUI.cpp

DPF_PATH = ../dpf
DPF_BUILD_DIR = ../build/offbeat-turbo
DPF_TARGET_DIR = ../bin
include ../dpf/Makefile.plugins.mk

BUILD_CXX_FLAGS += -std=gnu++17 -I../src/dsp -I$(DPF_BUILD_DIR)

# Turbolader-Bild für die Oberfläche als C-Array einbetten
$(DPF_BUILD_DIR)/TurboImage.h: ../assets/turbo.png
	-@mkdir -p $(DPF_BUILD_DIR)
	cd ../assets && xxd -i turbo.png > $(abspath $@)

$(DPF_BUILD_DIR)/TurboUI.cpp.o: $(DPF_BUILD_DIR)/TurboImage.h

TARGETS += vst3
TARGETS += lv2
TARGETS += ladspa

all: $(TARGETS)
