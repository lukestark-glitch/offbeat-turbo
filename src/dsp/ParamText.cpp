#include "ParamText.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace offbeat {

namespace {
float clampParam(int p, float v)
{
    const ParamInfo& i = kParams[p];
    v = v < i.min ? i.min : (v > i.max ? i.max : v);
    if (i.flags & (kFlagInt | kFlagBool))
        v = std::round(v);
    return v;
}

// Namen ("Moll", "Bass"), An/Aus, true/false; −1 wenn nichts passt.
float wordValue(int p, const char* word)
{
    const ParamInfo& i = kParams[p];
    if (i.labels)
        for (int k = 0; k <= (int)i.max; ++k)
            if (!strcasecmp(word, i.labels[k]))
                return (float)k;
    if (i.flags & kFlagBool) {
        if (!strcasecmp(word, "an") || !strcasecmp(word, "on") || !strcasecmp(word, "true"))
            return 1.0f;
        if (!strcasecmp(word, "aus") || !strcasecmp(word, "off") || !strcasecmp(word, "false"))
            return 0.0f;
    }
    return -1.0f;
}

int findSymbol(const char* symbol)
{
    for (int p = 0; p < kParamCount; ++p)
        if (!std::strcmp(kParams[p].symbol, symbol))
            return p;
    return -1;
}
} // namespace

bool parseParamText(int p, const char* text, float& out)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s", text);
    for (char* c = buf; *c; ++c)
        if (*c == ',')
            *c = '.';
    const char* s = buf;
    while (std::isspace((unsigned char)*s))
        ++s;

    const float w = wordValue(p, s);
    if (w >= 0.0f) {
        out = w;
        return true;
    }
    char* end;
    double v = std::strtod(s, &end);
    if (end == s)
        return false;
    while (std::isspace((unsigned char)*end))
        ++end;
    const ParamInfo& i = kParams[p];
    if (*end == 'k' || *end == 'K')
        v *= 1000.0;
    else if (!i.unit[0] && !(i.flags & (kFlagInt | kFlagBool)) && i.max <= 1.0f)
        v *= 0.01; // 0–1-Regler werden in Prozent angezeigt und eingegeben
    out = clampParam(p, (float)v);
    return true;
}

int applyJson(const char* json, float values[kParamCount], bool changed[kParamCount], std::string& errors)
{
    int count = 0;
    const char* s = json;
    while ((s = std::strchr(s, '"'))) {
        const char* keyEnd = std::strchr(++s, '"');
        if (!keyEnd)
            break;
        const std::string key(s, keyEnd);
        s = keyEnd + 1;
        while (std::isspace((unsigned char)*s))
            ++s;
        if (*s != ':')
            continue; // kein Schlüssel, z. B. ein Text-Wert
        ++s;
        while (std::isspace((unsigned char)*s))
            ++s;

        std::string value;
        if (*s == '"') {
            const char* e = std::strchr(++s, '"');
            if (!e)
                break;
            value.assign(s, e);
            s = e + 1;
        } else {
            const char* e = s;
            while (*e && *e != ',' && *e != '}' && !std::isspace((unsigned char)*e))
                ++e;
            value.assign(s, e);
            s = e;
        }

        const int p = findSymbol(key.c_str());
        if (p < 0 || (kParams[p].flags & kFlagOutput)) {
            errors += (errors.empty() ? "" : ", ") + key + " unbekannt";
            continue;
        }
        float v = wordValue(p, value.c_str());
        if (v < 0.0f) {
            char* end;
            v = std::strtof(value.c_str(), &end);
            if (end == value.c_str()) {
                errors += (errors.empty() ? "" : ", ") + key + " ungültig";
                continue;
            }
        }
        values[p] = clampParam(p, v);
        changed[p] = true;
        ++count;
    }
    return count;
}

std::string toJson(const float values[kParamCount], const int* list, int n)
{
    std::string out = "{\n";
    char buf[96];
    for (int i = 0; i < n; ++i) {
        const ParamInfo& info = kParams[list[i]];
        const float v = values[list[i]];
        if (info.labels)
            std::snprintf(buf, sizeof(buf), "  \"%s\": \"%s\"", info.symbol, info.labels[(int)std::lround(v)]);
        else if (info.flags & kFlagBool)
            std::snprintf(buf, sizeof(buf), "  \"%s\": %s", info.symbol, v > 0.5f ? "true" : "false");
        else
            std::snprintf(buf, sizeof(buf), "  \"%s\": %g", info.symbol, std::round(v * 1000.0f) / 1000.0f);
        out += buf;
        out += i + 1 < n ? ",\n" : "\n";
    }
    return out + "}\n";
}

} // namespace offbeat
