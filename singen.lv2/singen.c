#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#ifdef LV2_EXTENDED
#include <cairo/cairo.h>
#include "ardour/lv2_extensions.h"
#endif

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
	float srate;
	float* output;
	float phase;
#ifdef LV2_EXTENDED
	LV2_Inline_Display_Image_Surface surf;
	bool                     need_expose;
	cairo_surface_t*         display;
	LV2_Inline_Display*      queue_draw;
	uint32_t                 w, h;
#endif
} SinGen;

static float
db_to_coeff (float db)
{
	if (db <= -80) { return 0; }
	else if (db >=  20) { return 10; }
	return powf (10.f, .05f * db);
}

static float 
lowpass_filter_param(SinGen* singen, 
					 float old_val, 
					 float new_val, 
					 float limit)
{
	float lpf = 2048 / singen->srate;
	if ( fabs(old_val - new_val) < limit ) {
		return new_val;
	} else {
		return old_val + lpf * (new_val - old_val);
	}
}

//just need these so the compiler doesn't complain
static void 
activate(LV2_Handle instance) {

}

static void 
deactivate(LV2_Handle instance) {
	
}

static LV2_Handle
instantiate (const LV2_Descriptor*     descriptor,
			 double                    rate,
			 const char*               bundle_path,
			 const LV2_Feature* const* features)
{
	SinGen* singen = (SinGen*)calloc(1, sizeof(SinGen));
	singen->srate = rate;
	
	for (int i=0; features[i]; ++i) {
#ifdef LV2_EXTENDED
		if (!strcmp(features[i]->URI, LV2_INLINEDISPLAY__queue_draw)) {
			singen->queue_draw = (LV2_Inline_Display*) features[i]->data;
		}
#endif
	}
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

float old_freq = 0;
float old_amp = 0;
static void
run(LV2_Handle instance, uint32_t n_samples)
{
	SinGen* singen = (SinGen*)instance;

	float amp = lowpass_filter_param(singen, old_amp, *(singen->amp), 0.02);
	float phase = singen->phase;
	float freq = lowpass_filter_param(singen, old_freq, *(singen->freq), 0.02);
	double srate = singen->srate;

	float* output = singen->output;

	float inc = freq / srate;

	for (uint32_t i = 0; i < n_samples; ++i)
	{
		output[i] = db_to_coeff(amp) * sinf(2.0f * M_PI * phase);
		phase += inc;
	}
	singen->phase = fmodf(phase, 1.0);
	old_freq = freq;
	old_amp = amp;
	singen->queue_draw->queue_draw (singen->queue_draw->handle);
}

#ifdef LV2_EXTENDED

#include "dynamic_display.c"

static LV2_Inline_Display_Image_Surface *
render_inline (LV2_Handle instance, uint32_t w, uint32_t max_h)
{
	SinGen* singen = (SinGen*)instance;

	singen->h = 30;

	float amp = *singen->amp;
	float phase = singen->phase;
	float freq = lowpass_filter_param(singen, old_freq, *(singen->freq), 0.02);
	double srate = singen->srate;

	float inc = freq / srate;

	float h = singen->h;


	cairo_t* cr = cairo_create(singen->display);
	singen->display = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, max_h);

	cairo_rectangle (cr, 0, 0, w, singen->h);
	cairo_set_source_rgba (cr, .8, .8, .8, 1.0);
	cairo_fill(cr);
	cairo_set_line_width(cr, 1.5);
	cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 1.0);

	float l_x, l_y = 0;
	for (uint32_t x = 0; x < w; x++) {
		float y = db_to_coeff(amp) * sinf(2.0f * M_PI * phase);
		float yc = 0.5 * h + ((-0.5 * h) * y);
		cairo_move_to (cr, x, yc + 3);
		cairo_line_to (cr, l_x, l_y + 3);
		l_x = x;
		l_y = yc;
		cairo_stroke(cr);
		cairo_close_path (cr);
		phase += inc;
	}
	singen->phase = fmodf(phase, 1.0);

	singen->surf.width = cairo_image_surface_get_width (singen->display);
	singen->surf.height = cairo_image_surface_get_height (singen->display);
	singen->surf.stride = cairo_image_surface_get_stride (singen->display);
	singen->surf.data = cairo_image_surface_get_data  (singen->display);

	return &singen->surf;
}
#endif

static const void*
extension_data(const char* uri)
{
#ifdef LV2_EXTENDED
	static const LV2_Inline_Display_Interface display  = { render_inline };
	if (!strcmp(uri, LV2_INLINEDISPLAY__interface)) {
		return &display;
	}
#endif
	return NULL;
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
		case 0:  
			return &descriptor;
		default: 
			return NULL;
	}
}