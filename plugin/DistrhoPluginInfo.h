#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

// URI, IDs und Bundle-Name (plugin/Makefile: NAME) ab dem ersten Release nie mehr ändern:
// gespeicherte LMMS-/Carla-Projekte finden das Plugin darüber.
#define DISTRHO_PLUGIN_BRAND   "Offbeat"
#define DISTRHO_PLUGIN_NAME    "Offbeat Turbo"
#define DISTRHO_PLUGIN_URI     "https://github.com/lukestark-glitch/offbeat-turbo"
#define DISTRHO_PLUGIN_CLAP_ID "com.github.lukestark-glitch.offbeat-turbo"

#define DISTRHO_PLUGIN_BRAND_ID  Ofbt
#define DISTRHO_PLUGIN_UNIQUE_ID OfbU

#define DISTRHO_PLUGIN_HAS_UI        1
#define DISTRHO_PLUGIN_IS_RT_SAFE    1
#define DISTRHO_PLUGIN_IS_SYNTH      0
#define DISTRHO_PLUGIN_NUM_INPUTS    2
#define DISTRHO_PLUGIN_NUM_OUTPUTS   2
#define DISTRHO_PLUGIN_WANT_PROGRAMS 1
#define DISTRHO_PLUGIN_WANT_LATENCY  1

// VST2 kennt keine Anzeige-Werte; DPF zeigt dort sonst nur Parameter vor dem ersten Anzeige-Wert,
// und "Auto" (hinten angehängt) würde fehlen. So zeigt VST2 alle Parameter (Anzeigen als Regler).
#define DPF_VST_SHOW_PARAMETER_OUTPUTS 1

#define DISTRHO_UI_USE_NANOVG        1
#define DISTRHO_UI_USER_RESIZABLE    0
#define DISTRHO_UI_DEFAULT_WIDTH     860
#define DISTRHO_UI_DEFAULT_HEIGHT    560

#define DISTRHO_PLUGIN_LV2_CATEGORY    "lv2:LimiterPlugin"
#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Dynamics|Stereo"

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
