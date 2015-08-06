/* $Id: vumeter.cc 53 2007-09-06 21:50:05Z lennart $ */

/***
  This file is part of pavumeter.
 
  pavumeter is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 2 of the License,
  or (at your option) any later version.
 
  pavumeter is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with pavumeter; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include <stdio.h>
#include <math.h>

#include <glibmm.h>
GMainLoop *loop;

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>

#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#define LOGARITHMIC 1

//definitions
void showData(const float *d, unsigned samples);
void printData(float leftValue, float rightValue);

size_t sampleCounter = 0;
float levels[2];

static pa_context *context = NULL;
static pa_stream *stream = NULL;
static char* device_name = NULL;
static char* device_description = NULL;
static enum {
    PLAYBACK,
    RECORD
} mode = PLAYBACK;


void show_error(const char *txt, bool show_pa_error = true) {
    char buf[256];

    if (show_pa_error)
        snprintf(buf, sizeof(buf), "%s: %s", txt, pa_strerror(pa_context_errno(context)));

    printf("%s\n", buf);

    exit(1);
}

static void stream_update_timing_info_callback(pa_stream *s, int success, void *) {
    pa_usec_t t;
    int negative = 0;
    
    if (!success || pa_stream_get_latency(s, &t, &negative) < 0) {
        show_error("Failed to get latency information");
        return;
    }

/*    if (!mainWindow)
        return;

    mainWindow->updateLatency(negative ? 0 : t);
*/
}

static gboolean latency_func(gpointer) {
    pa_operation *o;
    
    if (!stream)
        return false;

    if (!(o = pa_stream_update_timing_info(stream, stream_update_timing_info_callback, NULL)))
        g_message("pa_stream_update_timing_info() failed: %s", pa_strerror(pa_context_errno(context)));
    else
        pa_operation_unref(o);
    
    return true;
}

static void stream_read_callback(pa_stream *s, size_t l, void *) {
    const void *p;

    if (pa_stream_peek(s, &p, &l) < 0) {
        g_message("pa_stream_peek() failed: %s", pa_strerror(pa_context_errno(context)));
        return;
    }

    showData((const float*) p, l/sizeof(float));

    pa_stream_drop(s);
}

void showData(const float *d, unsigned samples) {
    unsigned nchan = 2;

    while (samples >= nchan) {
        if(sampleCounter == 0) {
          sampleCounter = 1500;
          //printData(levels[0] / sampleCounter, levels[1] / sampleCounter);
          printData(levels[0], levels[1]);
          levels[0] = 0;
          levels[1] = 0;
        }

        for (unsigned c = 0; c < nchan; c++) {
            float v = fabs(d[c]);
            if (v > levels[c]) {
                levels[c] = v;
            }
            //levels[c] += v;
        }

      d += nchan;
      samples -= nchan;

      sampleCounter--;
    }
}

void printData(float leftValue, float rightValue) {
  printf("%f %f\n", log10(leftValue*9+1), log10(rightValue*9+1));
  fflush(stdout);
} 

static void stream_state_callback(pa_stream *s, void *) {
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;

        case PA_STREAM_READY:
//            mainWindow = new MainWindow(*pa_stream_get_channel_map(s), device_name, device_description);

            g_timeout_add(100, latency_func, NULL);
            pa_operation_unref(pa_stream_update_timing_info(stream, stream_update_timing_info_callback, NULL));
            break;
            
        case PA_STREAM_FAILED:
            show_error("Connection failed");
            break;
            
        case PA_STREAM_TERMINATED:
            exit(2);
    }
}

static void create_stream(const char *name, const char *description, const pa_sample_spec &ss, const pa_channel_map &cmap) {
    char t[256];
    pa_sample_spec nss;

    g_free(device_name);
    device_name = g_strdup(name);
    g_free(device_description);
    device_description = g_strdup(description);
    
    nss.format = PA_SAMPLE_FLOAT32;
    nss.rate = ss.rate;
    nss.channels = ss.channels;
    
    g_message("Using sample format: %s", pa_sample_spec_snprint(t, sizeof(t), &nss));
    g_message("Using channel map: %s", pa_channel_map_snprint(t, sizeof(t), &cmap));

    stream = pa_stream_new(context, "PulseAudio Volume Meter", &nss, &cmap);
    pa_stream_set_state_callback(stream, stream_state_callback, NULL);
    pa_stream_set_read_callback(stream, stream_read_callback, NULL);
    pa_stream_connect_record(stream, name, NULL, (enum pa_stream_flags) 0);
}

static void context_get_source_info_callback(pa_context *, const pa_source_info *si, int is_last, void *) {
    if (is_last < 0) {
        show_error("Failed to get source information");
        return;
    }

    if (!si)
        return;

    create_stream(si->name, si->description, si->sample_spec, si->channel_map);
}

static void context_get_sink_info_callback(pa_context *, const pa_sink_info *si, int is_last, void *) {
    if (is_last < 0) {
        show_error("Failed to get sink information");
        return;
    }

    if (!si)
        return;

    create_stream(si->monitor_source_name, si->description, si->sample_spec, si->channel_map);
}

static void context_get_server_info_callback(pa_context *c, const pa_server_info*si, void *) {
    if (!si) {
        show_error("Failed to get server information");
        return;
    }

    if (mode == PLAYBACK) {

        if (!si->default_sink_name) {
            show_error("No default sink set.", false);
            return;
        }

        pa_operation_unref(pa_context_get_sink_info_by_name(c, si->default_sink_name, context_get_sink_info_callback, NULL));
        
    } else if (mode == RECORD) {

        if (!si->default_source_name) {
            show_error("No default source set.", false);
            return;
        }

        pa_operation_unref(pa_context_get_source_info_by_name(c, si->default_source_name, context_get_source_info_callback, NULL));
    }
}

static void context_state_callback(pa_context *c, void *) {
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            g_assert(!stream);

            if (device_name && mode == RECORD)
                pa_operation_unref(pa_context_get_source_info_by_name(c, device_name, context_get_source_info_callback, NULL));
            else if (device_name && mode == PLAYBACK)
                pa_operation_unref(pa_context_get_sink_info_by_name(c, device_name, context_get_sink_info_callback, NULL));
            else
                pa_operation_unref(pa_context_get_server_info(c, context_get_server_info_callback, NULL));
            
            break;
            
        case PA_CONTEXT_FAILED:
            show_error("Connection failed");
            break;
            
        case PA_CONTEXT_TERMINATED:
            exit(3);
    }
}

int main(int argc, char *argv[]) {
    pa_glib_mainloop *m;
    
    signal(SIGPIPE, SIG_IGN);

    try {
        mode = PLAYBACK;
            
        g_message("Starting in %s mode.", mode == RECORD ? "record" : "playback");
            
        /* Rather ugly and incomplete */
        if (argc > 1) 
            device_name = g_strdup(argv[1]) ;
        else {
            char *e;
            if ((e = getenv(mode == RECORD ? "PULSE_SOURCE" : "PULSE_SINK")))
                device_name = g_strdup(e);
        }
            
        if (device_name)
            g_message("Using device '%s'", device_name);
            
        m = pa_glib_mainloop_new(g_main_context_default());
        g_assert(m);
            
        context = pa_context_new(pa_glib_mainloop_get_api(m), "PulseAudio Volume Meter");
        g_assert(context);
            
        pa_context_set_state_callback(context, context_state_callback, NULL);
        pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);
            
        loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (loop);
            
        if (stream)
            pa_stream_unref(stream);
        if (context)
            pa_context_unref(context);
        
        if(device_name)
            g_free(device_name);
            
        pa_glib_mainloop_free(m);

    } catch (Glib::OptionError) {
        g_message("Bad parameter");
    }
    
    return 0;
}
