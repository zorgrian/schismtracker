/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * URL: http://rigelseven.com/schism/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "headers.h"

#include "mixer.h"
#include "util.h"

#ifdef USE_ALSA
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

static char *alsa_card_id = "default";

/* --------------------------------------------------------------------- */
#ifdef USE_DLTRICK_ALSA
#include <alsa/mixer.h>

/* see midi-alsa for details about how this works */
#include <dlfcn.h>

extern void *_dltrick_handle;

/* don't try this at home... */
#define _void_dltrick(a,b,c) static void (*_dltrick_ ## a)b = 0; \
void a b { if (!_dltrick_##a) _dltrick_##a = dlsym(_dltrick_handle, #a); \
if (!_dltrick_##a) abort(); _dltrick_ ## a c; }

#define _any_dltrick(r,a,b,c) static r (*_dltrick_ ## a)b = 0; \
r a b { if (!_dltrick_##a) _dltrick_##a = dlsym(_dltrick_handle, #a); \
if (!_dltrick_##a) abort(); return _dltrick_ ## a c; }

_any_dltrick(int,snd_mixer_selem_set_playback_volume,
(snd_mixer_elem_t*e,snd_mixer_selem_channel_id_t ch,long v),(e,ch,v))

_any_dltrick(int,snd_mixer_selem_is_playback_mono,(snd_mixer_elem_t*e),(e))
_any_dltrick(int,snd_mixer_selem_has_playback_channel,(snd_mixer_elem_t*e,snd_mixer_selem_channel_id_t c),(e,c))

_any_dltrick(int,snd_ctl_card_info,(snd_ctl_t*c,snd_ctl_card_info_t*i),(c,i))
_any_dltrick(size_t,snd_ctl_card_info_sizeof,(void),())

_any_dltrick(int,snd_mixer_close,(snd_mixer_t*mm),(mm))
_any_dltrick(int,snd_mixer_selem_get_playback_volume,
(snd_mixer_elem_t*e,snd_mixer_selem_channel_id_t ch,long*v),(e,ch,v))
#if (SND_LIB_MAJOR == 1 && SND_LIB_MINOR == 0 && SND_LIB_SUBMINOR < 10) || (SND_LIB_MAJOR < 1)
_void_dltrick(void,snd_mixer_selem_get_playback_volume_range,
(snd_mixer_elem_t*e,long*m,long*v),(e,m,v))
#else
_any_dltrick(int,snd_mixer_selem_get_playback_volume_range,
(snd_mixer_elem_t*e,long*m,long*v),(e,m,v))
#endif
_any_dltrick(int,snd_ctl_open,(snd_ctl_t**c,const char *name,int mode),(c,name,mode))
_any_dltrick(int,snd_ctl_close,(snd_ctl_t*ctl),(ctl))
_any_dltrick(int,snd_mixer_open,(snd_mixer_t**m,int mode),(m,mode))
_any_dltrick(int,snd_mixer_attach,(snd_mixer_t*m,const char *name),(m,name))
_any_dltrick(int,snd_mixer_selem_register,(snd_mixer_t*m,
	struct snd_mixer_selem_regopt*opt, snd_mixer_class_t **cp),(m,opt,cp))
_any_dltrick(int,snd_mixer_selem_is_active,(snd_mixer_elem_t*e),(e))
_any_dltrick(int,snd_mixer_selem_has_playback_volume,(snd_mixer_elem_t*e),(e))
_any_dltrick(int,snd_mixer_selem_has_capture_switch,(snd_mixer_elem_t*e),(e))
_any_dltrick(int,snd_mixer_selem_has_capture_switch_joined,(snd_mixer_elem_t*e),(e))
_any_dltrick(int,snd_mixer_selem_has_capture_switch_exclusive,(snd_mixer_elem_t*e),(e))

_any_dltrick(int,snd_mixer_load,(snd_mixer_t*m),(m))
_any_dltrick(snd_mixer_elem_t*,snd_mixer_first_elem,(snd_mixer_t*m),(m))
_any_dltrick(snd_mixer_elem_t*,snd_mixer_elem_next,(snd_mixer_elem_t*m),(m))
_any_dltrick(snd_mixer_elem_type_t,snd_mixer_elem_get_type,(const snd_mixer_elem_t *obj),(obj))
#endif


static void _alsa_writeout(snd_mixer_elem_t *em,
			snd_mixer_selem_channel_id_t d,
				int use, int lim)
{
	if (use > lim) use = lim;
	(void)snd_mixer_selem_set_playback_volume(em, d, (long)use);
}
static void _alsa_write(snd_mixer_elem_t *em, int *l, int *r, long min, long range)
{
	long al, ar;
	long mr, md;

	al = ((*l) * 255 / range) + min;
	ar = ((*r) * 255 / range) + min;

	md = ((al) + (ar)) / 2;
	mr = min+range;

	if (snd_mixer_selem_is_playback_mono(em)) {
		_alsa_writeout(em, SND_MIXER_SCHN_MONO, md, mr);
	} else {
		_alsa_writeout(em, SND_MIXER_SCHN_FRONT_LEFT, al, mr);
		_alsa_writeout(em, SND_MIXER_SCHN_FRONT_RIGHT, ar, mr);
		_alsa_writeout(em, SND_MIXER_SCHN_FRONT_CENTER, md, mr);
		_alsa_writeout(em, SND_MIXER_SCHN_REAR_LEFT, al, mr);
		_alsa_writeout(em, SND_MIXER_SCHN_REAR_RIGHT, ar, mr);
		_alsa_writeout(em, SND_MIXER_SCHN_WOOFER, md, mr);
	}
}
static void _alsa_readin(snd_mixer_elem_t *em, snd_mixer_selem_channel_id_t d,
				int *aa, long min, long range)
{
	long v;
	if (snd_mixer_selem_has_playback_channel(em, d)) {
		snd_mixer_selem_get_playback_volume(em, d, &v);
		v -= min;
		v = (v * range) / 255;
		if (!*aa) {
			(*aa) += v;
		} else {
			(*aa) += v;
			(*aa) /= 2;
		}
	}
	
}
static void _alsa_read(snd_mixer_elem_t *em, int *l, int *r, long min, long range)
{
	if (snd_mixer_selem_is_playback_mono(em)) {
		_alsa_readin(em, SND_MIXER_SCHN_MONO, l, min, range);
		_alsa_readin(em, SND_MIXER_SCHN_MONO, r, min, range);
	} else {
		_alsa_readin(em, SND_MIXER_SCHN_FRONT_LEFT, l, min, range);
		_alsa_readin(em, SND_MIXER_SCHN_FRONT_RIGHT, r, min, range);
		_alsa_readin(em, SND_MIXER_SCHN_REAR_LEFT, l, min, range);
		_alsa_readin(em, SND_MIXER_SCHN_REAR_RIGHT, r, min, range);
		_alsa_readin(em, SND_MIXER_SCHN_FRONT_CENTER, l, min, range);
		_alsa_readin(em, SND_MIXER_SCHN_FRONT_CENTER, r, min, range);
		_alsa_readin(em, SND_MIXER_SCHN_WOOFER, l, min, range);
		_alsa_readin(em, SND_MIXER_SCHN_WOOFER, r, min, range);
	}
}
static void _alsa_doit(void (*busy)(snd_mixer_elem_t *em,
			int *, int *, long, long), int *l, int *r)
{
	long ml, mr;
	snd_mixer_elem_t *em;
	snd_mixer_t *mix;
	snd_ctl_t *ctl_handle;
	snd_ctl_card_info_t *hw_info;
	int err;

	snd_ctl_card_info_alloca(&hw_info);
	if (snd_ctl_open(&ctl_handle, alsa_card_id, 0) < 0) return;
	if (snd_ctl_card_info(ctl_handle, hw_info) < 0) {
		snd_ctl_close(ctl_handle);
		return;
	}
	snd_ctl_close(ctl_handle);

	if (snd_mixer_open(&mix, 0) == 0) {
		if (snd_mixer_attach(mix, alsa_card_id) < 0) {
			snd_mixer_close(mix);
			return;
		}
		if (snd_mixer_selem_register(mix, NULL, NULL) < 0) {
			snd_mixer_close(mix);
			return;
		}
		if (snd_mixer_load(mix) < 0) {
			snd_mixer_close(mix);
			return;
		}
		for (em = snd_mixer_first_elem(mix); em;
						em = snd_mixer_elem_next(em)) {
			if (snd_mixer_elem_get_type(em) !=
						SND_MIXER_ELEM_SIMPLE)
				continue;
			if (!snd_mixer_selem_is_active(em)) continue;
			if (!snd_mixer_selem_has_playback_volume(em))
				continue;
			if (!snd_mixer_selem_has_playback_volume(em))
				continue;
			if (snd_mixer_selem_has_capture_switch_exclusive(em))
				continue;
			if (snd_mixer_selem_has_capture_switch_joined(em))
				continue;

			ml = mr = 0;
			snd_mixer_selem_get_playback_volume_range(em, &ml, &mr);
			if (ml == mr) continue;

			busy(em, l, r, ml, mr - ml);
		}
		snd_mixer_close(mix);
	}
	
}

int alsa_mixer_get_max_volume(void)
{
	int a,b;
	return 0xFF;
}

void alsa_mixer_read_volume(int *left, int *right)
{
	*left = *right = 0;
	_alsa_doit(_alsa_read, left, right);
}

void alsa_mixer_write_volume(int left, int right)
{
	_alsa_doit(_alsa_write, &left, &right);
}
#endif
