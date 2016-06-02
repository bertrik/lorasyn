#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

// this should be in math.h already?
#define M_PI 3.14159265358979323846

typedef void (out_fn_t)(void *context, double i, double q);

typedef struct {
    // input parameters
    double BW;
    int SF;
    int Fs;
    // runtime state
    double phase;
    // output parameters
    out_fn_t *out_fn;
    void *context;
} syn_t;

/**
 * Initializes the lora synthesiser runtime variables.
 *
 * @param s the synthesiser structure
 */
static void lora_init(syn_t *s)
{
    s->phase = 0;
}

/**
 * Generates one lora symbol.
 *
 * @param s the synthesiser structure
 * @param symbol the lora symbol value
 * @param inverse whether this is an inverse symbol
 */
static void lora_syn(syn_t *s, int symbol, bool inverse)
{
    // initial cyclic shift is equal to the symbol value
    double shift = symbol;

    int num_samples = s->Fs * (1 << s->SF) / s->BW;
    for (int i = 0; i < num_samples; i++) {
        // output the complex signal
        double out_i = cos(s->phase);
        double out_q = sin(s->phase);
        s->out_fn(s->context, out_i, out_q);

        // calculate frequency from cyclic shift
        double f = s->BW * shift / (1 << s->SF);
        if (inverse) {
            f = s->BW - f;
        }
        // apply frequency offset away from DC
        f += (s->Fs / 4) - (s->BW / 2);

        // increment the phase according to frequency
        s->phase += 2.0 * M_PI * f / s->Fs;
        if (s->phase > M_PI) {
            // keep phase small to mitigate error accumulation
            s->phase -= 2.0 * M_PI;
        }

        // update the cyclic shift
        shift += s->BW / s->Fs;
        if (shift >= (1 << s->SF)) {
            shift -= (1 << s->SF);
        }
    }
}

// output complex signal as raw signed 8-bit (even = i, odd = q)
static void output(void *context, double i, double q)
{
    FILE *f = (FILE *)context;
    char b;
    b = (i * 100);
    fwrite(&b, 1, 1, f);
    b = (q * 100);
    fwrite(&b, 1, 1, f);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    FILE *f = fopen("out.raw", "wb");

    syn_t syn;
    lora_init(&syn);
    syn.BW = 125000;
    syn.SF = 8;
    syn.Fs = 1000000;
    syn.out_fn = output;
    syn.context = f;

    // pre-amble
    for (int i = 0; i < 8; i++) {
        lora_syn(&syn, 0, false);
    }

    // sync pattern
    lora_syn(&syn, 32, false);
    lora_syn(&syn, 32, false);

    // reverse chirps
    lora_syn(&syn, 0, true);
    lora_syn(&syn, 0, true);

    // pseudo-random data
    for (int i = 0; i < 16; i++) {
        int b = rand() & ((1 << syn.SF) - 1);
        lora_syn(&syn, b, false);
    }

    fclose(f);

    return 0;
}
