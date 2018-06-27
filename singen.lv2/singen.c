#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#define SIN_URI "urn:ardour:singen"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

typedef enum {
    SINGEN_FREQ,
    SINGEN_AMP,
    SINGEN_OUT,
} PortIndex;

typedef struct {
	float* freq;
	float* amp;
	float  srate;
    float* output;
    float phase;
} SinGen;

static void activate(LV2_Handle instance) {}
static void deactivate(LV2_Handle instance) {}
static const void* extension_data(const char *uri) { return NULL; }

static LV2_Handle
instantiate (const LV2_Descriptor*     descriptor,
             double                    rate,
             const char*               bundle_path,
             const LV2_Feature* const* features)
{
	SinGen* singen = (SinGen*)calloc(1, sizeof(SinGen));
	singen->srate = rate;
	return (LV2_Handle)singen;
}

static void cleanup(LV2_Handle instance) { free(instance); }

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
	SinGen* singen = (SinGen*)instance;

	switch ((PortIndex)port) {
		case SINGEN_FREQ:
			singen->freq = (float*)data;
			break;
		case SINGEN_AMP:
			singen->amp = (float*)data;
			break;
		case SINGEN_OUT:
			singen->output = (float*)data;
		default:
			break;
	}
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	SinGen* singen = (SinGen*)instance;

	float amp = *(singen->amp);
	float phase = singen->phase;
	float freq = *(singen->freq);
	double srate = singen->srate;

	float* output = singen->output;

	float inc = freq / srate;

	for (uint32_t i = 0; i < n_samples; ++i)
	{
		output[i] = amp * sinf(2.0f * M_PI * phase);
		phase += inc;
	}
	singen->phase = fmodf(phase, 1.0);
}

static const LV2_Descriptor descriptor = {
	SIN_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:  return &descriptor;
	default: return NULL;
	}
}