// DPF-Hülle von Offbeat Turbo: Parameter, Programme, Latenz und Anzeige-Werte (Ausgabe-Parameter).
#include "DistrhoPlugin.hpp"
#include "OffbeatParam.hpp"
#include "Turbo.hpp"

START_NAMESPACE_DISTRHO

using namespace offbeat;

class TurboPlugin : public Plugin {
public:
    TurboPlugin()
        : Plugin(kParamCount, kPresetCount, 0)
    {
        turbo.setSampleRate((float)getSampleRate());
        setLatency(turbo.latency());
    }

protected:
    const char* getLabel() const override { return "OffbeatTurbo"; }
    const char* getDescription() const override { return "Master-Lautheit: Glue-Kompressor, Soft-Clipper, True-Peak-Limiter"; }
    const char* getMaker() const override { return "Offbeat"; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    void initParameter(uint32_t index, Parameter& parameter) override { fillParameter(kParams[index], parameter); }

    void initProgramName(uint32_t index, String& programName) override { programName = kPresets[index].name; }

    void loadProgram(uint32_t index) override
    {
        float values[kParamCount];
        applyPreset((int)index, values);
        for (int i = 0; i < kParamCount; ++i)
            turbo.setParam(i, values[i]); // Ausgabe-Parameter ignoriert setParam
    }

    float getParameterValue(uint32_t index) const override { return turbo.getParam((int)index); }
    void setParameterValue(uint32_t index, float value) override { turbo.setParam((int)index, value); }

    void sampleRateChanged(double newSampleRate) override
    {
        turbo.setSampleRate((float)newSampleRate);
        setLatency(turbo.latency());
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        turbo.setTransport(getTimePosition().playing);
        turbo.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    Turbo turbo;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TurboPlugin)
};

Plugin* createPlugin() { return new TurboPlugin(); }

END_NAMESPACE_DISTRHO
