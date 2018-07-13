#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#ifdef LV2_EXTENDED
#include <cairo/cairo.h>
#include "ardour/lv2_extensions.h"
#endif

#define SQUAR_URI "urn:ardour:squargen"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#define NUM(a) (sizeof(a) / sizeof(*a))

typedef enum {
	SQUARGEN_FREQ,
	SQUARGEN_AMP,
	SQUARGEN_OUT,
} PortIndex;

typedef struct {
	float* freq;
	float* amp;
	float srate;
	float* output;
	float phase;
#ifdef LV2_EXTENDED
	LV2_Inline_Display_Image_Surface surf;
	bool                             need_expose;
	cairo_surface_t*                 display;
	LV2_Inline_Display*              queue_draw;
	uint32_t                         w, h;
#endif
} SquarGen;

static float
db_to_coeff(float db)
{
	if(db <= -80)      { return 0;  }
	else if(db >=  20) { return 10; }

	return powf(10.f, .05f * db);
}

static float
lowpass_filter_param(SquarGen* squargen,
					 float     old_val,
					 float     new_val,
					 float     limit)
{
	float lpf = 2048 / squargen->srate;

	if( fabs(old_val - new_val) < limit ) {
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
instantiate(const LV2_Descriptor*     descriptor,
			double                    rate,
			const char*               bundle_path,
			const LV2_Feature* const* features)
{
	SquarGen* squargen = (SquarGen*)calloc(1, sizeof(SquarGen));

	squargen->srate = rate;

	for(int i=0; features[i]; ++i) {
#ifdef LV2_EXTENDED
		if(!strcmp(features[i]->URI, LV2_INLINEDISPLAY__queue_draw)) {
			squargen->queue_draw = (LV2_Inline_Display*) features[i]->data;
		}
#endif
	}
	return (LV2_Handle)squargen;
}

static void cleanup(LV2_Handle instance) {
	free(instance);
}

static void
connect_port(LV2_Handle instance,
			 uint32_t   port,
			 void*      data)
{
	SquarGen* squargen = (SquarGen*)instance;

	switch((PortIndex)port) {
		case SQUARGEN_FREQ:
			squargen->freq = (float*)data;
			break;
		case SQUARGEN_AMP:
			squargen->amp = (float*)data;
			break;
		case SQUARGEN_OUT:
			squargen->output = (float*)data;
		default:
			break;
	}
}

float old_freq = 0;
float old_amp = 0;

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	SquarGen* squargen = (SquarGen*)instance;

	float amp = lowpass_filter_param(squargen, old_amp, *(squargen->amp), 0.2);
	float phase = squargen->phase;
	float freq = lowpass_filter_param(squargen, old_freq, *(squargen->freq), 0.2);
	double srate = squargen->srate;
	float* output = squargen->output;

	float inc = freq/srate;

	int num_h = (int)(srate/(freq*2));
	if(num_h > 800) {num_h = 800;} // limit to 800 harmonics
	double coeffs[num_h + 1];
	coeffs[0] = 0;

	for(uint32_t i = 1; i < NUM(coeffs); i++) {
		coeffs[i] = sinf(i * 0.5 * M_PI) * 2 / (i * M_PI);
	}

	for(uint32_t i = 0; i < n_samples; i++) {
		double baseCos = cosf(phase * (2 * M_PI));
		double baseSin = sinf(phase * (2 * M_PI));
		double phasorCos = baseCos;
		double phasorSin = baseSin;
		double val = coeffs[0];

		for(int j = 1; j < NUM(coeffs); j++) {
			val += coeffs[j] * phasorCos;
			float t = phasorCos * baseCos - phasorSin * baseSin;
			phasorSin = phasorSin * baseCos + phasorCos * baseSin;
			phasorCos = t;
		}
		output[i] = db_to_coeff(amp) * val;

		phase += inc;
	}
	squargen->phase = fmodf(phase, 1.0);

	old_freq = freq;
	old_amp = amp;
	squargen->queue_draw->queue_draw(squargen->queue_draw->handle);
}

#ifdef LV2_EXTENDED

#include "dynamic_display.c"

static LV2_Inline_Display_Image_Surface *
render_inline(LV2_Handle instance, uint32_t w, uint32_t max_h)
{
	SquarGen* squargen = (SquarGen*)instance;

	squargen->h = 30;

	float amp = *(squargen->amp);
	float phase = 0;
	float freq = *(squargen->freq);
	double srate = squargen->srate;

	if (freq < 750) {freq=750;}
	if (freq > 6000) {freq=6000;}

	float inc = freq / srate;

	float h = squargen->h;

	squargen->display = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
	cairo_t* cr = cairo_create(squargen->display);

	cairo_rectangle (cr, 0, 0, w, squargen->h);
	cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
	cairo_fill(cr);
	cairo_set_line_width(cr, 1.5);
	cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 1.0);

	float l_x, l_y = 0;
	for(uint32_t x = 0; x < w; x++) {
		float y = db_to_coeff(amp) * sinf(2.0f * M_PI * phase);
		if (y > 0) {
			y = 0.950;
		} else {
			y = -0.900;
		}
		float yc = 0.5 * h + ((-0.5 * h) * y);
		cairo_move_to (cr, x, yc);
		cairo_line_to (cr, l_x, l_y);
		l_x = x;
		l_y = yc;
		phase += inc;
		cairo_stroke(cr);
	}
	phase = fmodf(phase, 1.0);

	squargen->surf.width = cairo_image_surface_get_width (squargen->display);
	squargen->surf.height = cairo_image_surface_get_height (squargen->display);
	squargen->surf.stride = cairo_image_surface_get_stride (squargen->display);
	squargen->surf.data = cairo_image_surface_get_data  (squargen->display);

	return &squargen->surf;
}
#endif

static const void*
extension_data(const char* uri) {
#ifdef LV2_EXTENDED
	static const LV2_Inline_Display_Interface display = { render_inline };
	if(!strcmp(uri, LV2_INLINEDISPLAY__interface)) {
		return &display;
	}
#endif
	return NULL;
}


static const LV2_Descriptor descriptor = {
	SQUAR_URI,
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
	switch(index) {
		case 0:
			return &descriptor;
		default:
			return NULL;
	}
}
