/* GStreamer
 *
 * Copyright (C) 2001-2002 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *               2006 Edgard Lima <edgard.lima@indt.org.br>
 *
 * gstv4l2object.c: base class for V4L2 elements
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This library is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Library General Public License for more details.
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

/* FIXME 0.11: suppress warnings for deprecated API such as GValueArray
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_GUDEV
#include <gudev/gudev.h>
#endif

#include "v4l2_calls.h"
#include "gstv4l2tuner.h"
#include "gstv4l2colorbalance.h"

#include "gst/gst-i18n-plugin.h"

#include <gst/video/video.h>

GST_DEBUG_CATEGORY_EXTERN (v4l2_debug);
GST_DEBUG_CATEGORY_EXTERN (GST_CAT_PERFORMANCE);
#define GST_CAT_DEFAULT v4l2_debug

#define DEFAULT_PROP_DEVICE_NAME        NULL
#define DEFAULT_PROP_DEVICE_FD          -1
#define DEFAULT_PROP_FLAGS              0
#define DEFAULT_PROP_TV_NORM            0
#define DEFAULT_PROP_CHANNEL            NULL
#define DEFAULT_PROP_FREQUENCY          0
#define DEFAULT_PROP_IO_MODE            GST_V4L2_IO_AUTO

#define ENCODED_BUFFER_SIZE             (1 * 1024 * 1024)

enum
{
  PROP_0,
  V4L2_STD_OBJECT_PROPS,
};

static GSList *gst_v4l2_object_get_format_list (GstV4l2Object * v4l2object);


#define GST_TYPE_V4L2_DEVICE_FLAGS (gst_v4l2_device_get_type ())
static GType
gst_v4l2_device_get_type (void)
{
  static GType v4l2_device_type = 0;

  if (v4l2_device_type == 0) {
    static const GFlagsValue values[] = {
      {V4L2_CAP_VIDEO_CAPTURE, "Device supports video capture", "capture"},
      {V4L2_CAP_VIDEO_OUTPUT, "Device supports video playback", "output"},
      {V4L2_CAP_VIDEO_OVERLAY, "Device supports video overlay", "overlay"},

      {V4L2_CAP_VBI_CAPTURE, "Device supports the VBI capture", "vbi-capture"},
      {V4L2_CAP_VBI_OUTPUT, "Device supports the VBI output", "vbi-output"},

      {V4L2_CAP_TUNER, "Device has a tuner or modulator", "tuner"},
      {V4L2_CAP_AUDIO, "Device has audio inputs or outputs", "audio"},

      {0, NULL, NULL}
    };

    v4l2_device_type =
        g_flags_register_static ("GstV4l2DeviceTypeFlags", values);
  }

  return v4l2_device_type;
}

#define GST_TYPE_V4L2_TV_NORM (gst_v4l2_tv_norm_get_type ())
static GType
gst_v4l2_tv_norm_get_type (void)
{
  static GType v4l2_tv_norm = 0;

  if (!v4l2_tv_norm) {
    static const GEnumValue tv_norms[] = {
      {0, "none", "none"},

      {V4L2_STD_NTSC, "NTSC", "NTSC"},
      {V4L2_STD_NTSC_M, "NTSC-M", "NTSC-M"},
      {V4L2_STD_NTSC_M_JP, "NTSC-M-JP", "NTSC-M-JP"},
      {V4L2_STD_NTSC_M_KR, "NTSC-M-KR", "NTSC-M-KR"},
      {V4L2_STD_NTSC_443, "NTSC-443", "NTSC-443"},

      {V4L2_STD_PAL, "PAL", "PAL"},
      {V4L2_STD_PAL_BG, "PAL-BG", "PAL-BG"},
      {V4L2_STD_PAL_B, "PAL-B", "PAL-B"},
      {V4L2_STD_PAL_B1, "PAL-B1", "PAL-B1"},
      {V4L2_STD_PAL_G, "PAL-G", "PAL-G"},
      {V4L2_STD_PAL_H, "PAL-H", "PAL-H"},
      {V4L2_STD_PAL_I, "PAL-I", "PAL-I"},
      {V4L2_STD_PAL_DK, "PAL-DK", "PAL-DK"},
      {V4L2_STD_PAL_D, "PAL-D", "PAL-D"},
      {V4L2_STD_PAL_D1, "PAL-D1", "PAL-D1"},
      {V4L2_STD_PAL_K, "PAL-K", "PAL-K"},
      {V4L2_STD_PAL_M, "PAL-M", "PAL-M"},
      {V4L2_STD_PAL_N, "PAL-N", "PAL-N"},
      {V4L2_STD_PAL_Nc, "PAL-Nc", "PAL-Nc"},
      {V4L2_STD_PAL_60, "PAL-60", "PAL-60"},

      {V4L2_STD_SECAM, "SECAM", "SECAM"},
      {V4L2_STD_SECAM_B, "SECAM-B", "SECAM-B"},
      {V4L2_STD_SECAM_G, "SECAM-G", "SECAM-G"},
      {V4L2_STD_SECAM_H, "SECAM-H", "SECAM-H"},
      {V4L2_STD_SECAM_DK, "SECAM-DK", "SECAM-DK"},
      {V4L2_STD_SECAM_D, "SECAM-D", "SECAM-D"},
      {V4L2_STD_SECAM_K, "SECAM-K", "SECAM-K"},
      {V4L2_STD_SECAM_K1, "SECAM-K1", "SECAM-K1"},
      {V4L2_STD_SECAM_L, "SECAM-L", "SECAM-L"},
      {V4L2_STD_SECAM_LC, "SECAM-Lc", "SECAM-Lc"},

      {0, NULL, NULL}
    };

    v4l2_tv_norm = g_enum_register_static ("V4L2_TV_norms", tv_norms);
  }

  return v4l2_tv_norm;
}

GType
gst_v4l2_io_mode_get_type (void)
{
  static GType v4l2_io_mode = 0;

  if (!v4l2_io_mode) {
    static const GEnumValue io_modes[] = {
      {GST_V4L2_IO_AUTO, "GST_V4L2_IO_AUTO", "auto"},
      {GST_V4L2_IO_RW, "GST_V4L2_IO_RW", "rw"},
      {GST_V4L2_IO_MMAP, "GST_V4L2_IO_MMAP", "mmap"},
      {GST_V4L2_IO_USERPTR, "GST_V4L2_IO_USERPTR", "userptr"},
      {GST_V4L2_IO_DMABUF, "GST_V4L2_IO_DMABUF", "dmabuf"},

      {0, NULL, NULL}
    };
    v4l2_io_mode = g_enum_register_static ("GstV4l2IOMode", io_modes);
  }
  return v4l2_io_mode;
}

void
gst_v4l2_object_install_properties_helper (GObjectClass * gobject_class,
    const char *default_device)
{
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device", "Device location",
          default_device, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Name of the device", DEFAULT_PROP_DEVICE_NAME,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE_FD,
      g_param_spec_int ("device-fd", "File descriptor",
          "File descriptor of the device", -1, G_MAXINT, DEFAULT_PROP_DEVICE_FD,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FLAGS,
      g_param_spec_flags ("flags", "Flags", "Device type flags",
          GST_TYPE_V4L2_DEVICE_FLAGS, DEFAULT_PROP_FLAGS,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstV4l2Src:brightness:
   *
   * Picture brightness, or more precisely, the black level
   */
  g_object_class_install_property (gobject_class, PROP_BRIGHTNESS,
      g_param_spec_int ("brightness", "Brightness",
          "Picture brightness, or more precisely, the black level", G_MININT,
          G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));
  /**
   * GstV4l2Src:contrast:
   *
   * Picture contrast or luma gain
   */
  g_object_class_install_property (gobject_class, PROP_CONTRAST,
      g_param_spec_int ("contrast", "Contrast",
          "Picture contrast or luma gain", G_MININT,
          G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));
  /**
   * GstV4l2Src:saturation:
   *
   * Picture color saturation or chroma gain
   */
  g_object_class_install_property (gobject_class, PROP_SATURATION,
      g_param_spec_int ("saturation", "Saturation",
          "Picture color saturation or chroma gain", G_MININT,
          G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));
  /**
   * GstV4l2Src:hue:
   *
   * Hue or color balance
   */
  g_object_class_install_property (gobject_class, PROP_HUE,
      g_param_spec_int ("hue", "Hue",
          "Hue or color balance", G_MININT,
          G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  /**
   * GstV4l2Src:norm:
   *
   * TV norm
   */
  g_object_class_install_property (gobject_class, PROP_TV_NORM,
      g_param_spec_enum ("norm", "TV norm",
          "video standard",
          GST_TYPE_V4L2_TV_NORM, DEFAULT_PROP_TV_NORM,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstV4l2Src:io-mode:
   *
   * IO Mode
   */
  g_object_class_install_property (gobject_class, PROP_IO_MODE,
      g_param_spec_enum ("io-mode", "IO mode",
          "I/O mode",
          GST_TYPE_V4L2_IO_MODE, DEFAULT_PROP_IO_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstV4l2Src:extra-controls:
   *
   * Additional v4l2 controls for the device. The controls are identified
   * by the control name (lowercase with '_' for any non-alphanumeric
   * characters).
   *
   * Since: 1.2
   */
  g_object_class_install_property (gobject_class, PROP_EXTRA_CONTROLS,
      g_param_spec_boxed ("extra-controls", "Extra Controls",
          "Extra v4l2 controls (CIDs) for the device",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstV4l2Src:pixel-aspect-ratio:
   *
   * The pixel aspect ratio of the device. This overwrites the pixel aspect
   * ratio queried from the device.
   *
   * Since: 1.2
   */
  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
      g_param_spec_string ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "Overwrite the pixel aspect ratio of the device", "1/1",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstV4l2Src:force-aspect-ratio:
   *
   * When enabled, the pixel aspect ratio queried from the device or set
   * with the pixel-aspect-ratio property will be enforced.
   *
   * Since: 1.2
   */
  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio", "Force aspect ratio",
          "When enabled, the pixel aspect ratio will be enforced", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

GstV4l2Object *
gst_v4l2_object_new (GstElement * element,
    enum v4l2_buf_type type,
    const char *default_device,
    GstV4l2GetInOutFunction get_in_out_func,
    GstV4l2SetInOutFunction set_in_out_func,
    GstV4l2UpdateFpsFunction update_fps_func)
{
  GstV4l2Object *v4l2object;

  /*
   * some default values
   */
  v4l2object = g_new0 (GstV4l2Object, 1);

  v4l2object->type = type;
  v4l2object->formats = NULL;

  v4l2object->element = element;
  v4l2object->get_in_out_func = get_in_out_func;
  v4l2object->set_in_out_func = set_in_out_func;
  v4l2object->update_fps_func = update_fps_func;

  v4l2object->video_fd = -1;
  v4l2object->poll = gst_poll_new (TRUE);
  v4l2object->active = FALSE;
  v4l2object->videodev = g_strdup (default_device);

  v4l2object->norms = NULL;
  v4l2object->channels = NULL;
  v4l2object->colors = NULL;

  v4l2object->xwindow_id = 0;

  v4l2object->keep_aspect = TRUE;

  v4l2object->n_v4l2_planes = 0;

  /*
   * this boolean only applies in v4l2-MPLANE mode.
   * TRUE: means it prefers to use several v4l2 (non contiguous)
   * planes. For example if the device supports NV12 and NV12M
   * both in MPLANE mode, then it will prefer NV12M
   * FALSE: means it prefers to use one v4l2 plane (which contains
   * all gst planes as if it was working in non-v4l2-MPLANE mode.
   * For example if the device supports NV12 and NV12M
   * both in MPLANE mode, then it will prefer NV12
   *
   * this boolean is also used to manage the case where the
   * device only supports the mode MPLANE and at the same time it
   * does not support both NV12 and NV12M. So in this case we first
   * try to use the prefered config, and at least try the other case
   * if it fails. For example in MPLANE mode if it has NV12 and not
   * NV21M then even if you set prefered_non_contiguous to TRUE it will
   * try NV21 as well.
   */
  v4l2object->prefered_non_contiguous = TRUE;

  v4l2object->no_initial_format = FALSE;

  return v4l2object;
}

static gboolean gst_v4l2_object_clear_format_list (GstV4l2Object * v4l2object);


void
gst_v4l2_object_destroy (GstV4l2Object * v4l2object)
{
  g_return_if_fail (v4l2object != NULL);

  if (v4l2object->videodev)
    g_free (v4l2object->videodev);

  if (v4l2object->poll)
    gst_poll_free (v4l2object->poll);

  if (v4l2object->channel)
    g_free (v4l2object->channel);

  if (v4l2object->formats) {
    gst_v4l2_object_clear_format_list (v4l2object);
  }

  if (v4l2object->probed_caps) {
    gst_caps_unref (v4l2object->probed_caps);
  }

  g_free (v4l2object);
}


static gboolean
gst_v4l2_object_clear_format_list (GstV4l2Object * v4l2object)
{
  g_slist_foreach (v4l2object->formats, (GFunc) g_free, NULL);
  g_slist_free (v4l2object->formats);
  v4l2object->formats = NULL;

  return TRUE;
}

static gint
gst_v4l2_object_prop_to_cid (guint prop_id)
{
  gint cid = -1;

  switch (prop_id) {
    case PROP_BRIGHTNESS:
      cid = V4L2_CID_BRIGHTNESS;
      break;
    case PROP_CONTRAST:
      cid = V4L2_CID_CONTRAST;
      break;
    case PROP_SATURATION:
      cid = V4L2_CID_SATURATION;
      break;
    case PROP_HUE:
      cid = V4L2_CID_HUE;
      break;
    default:
      GST_WARNING ("unmapped property id: %d", prop_id);
  }
  return cid;
}


gboolean
gst_v4l2_object_set_property_helper (GstV4l2Object * v4l2object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_DEVICE:
      g_free (v4l2object->videodev);
      v4l2object->videodev = g_value_dup_string (value);
      break;
    case PROP_BRIGHTNESS:
    case PROP_CONTRAST:
    case PROP_SATURATION:
    case PROP_HUE:
    {
      gint cid = gst_v4l2_object_prop_to_cid (prop_id);

      if (cid != -1) {
        if (GST_V4L2_IS_OPEN (v4l2object)) {
          gst_v4l2_set_attribute (v4l2object, cid, g_value_get_int (value));
        }
      }
      return TRUE;
    }
      break;
    case PROP_TV_NORM:
      v4l2object->tv_norm = g_value_get_enum (value);
      break;
#if 0
    case PROP_CHANNEL:
      if (GST_V4L2_IS_OPEN (v4l2object)) {
        GstTuner *tuner = GST_TUNER (v4l2object->element);
        GstTunerChannel *channel = gst_tuner_find_channel_by_name (tuner,
            (gchar *) g_value_get_string (value));

        if (channel) {
          /* like gst_tuner_set_channel (tuner, channel)
             without g_object_notify */
          gst_v4l2_tuner_set_channel (v4l2object, channel);
        }
      } else {
        g_free (v4l2object->channel);
        v4l2object->channel = g_value_dup_string (value);
      }
      break;
    case PROP_FREQUENCY:
      if (GST_V4L2_IS_OPEN (v4l2object)) {
        GstTuner *tuner = GST_TUNER (v4l2object->element);
        GstTunerChannel *channel = gst_tuner_get_channel (tuner);

        if (channel &&
            GST_TUNER_CHANNEL_HAS_FLAG (channel, GST_TUNER_CHANNEL_FREQUENCY)) {
          /* like
             gst_tuner_set_frequency (tuner, channel, g_value_get_ulong (value))
             without g_object_notify */
          gst_v4l2_tuner_set_frequency (v4l2object, channel,
              g_value_get_ulong (value));
        }
      } else {
        v4l2object->frequency = g_value_get_ulong (value);
      }
      break;
#endif
    case PROP_IO_MODE:
      v4l2object->req_mode = g_value_get_enum (value);
      break;
    case PROP_EXTRA_CONTROLS:{
      const GstStructure *s = gst_value_get_structure (value);

      if (v4l2object->extra_controls)
        gst_structure_free (v4l2object->extra_controls);

      v4l2object->extra_controls = s ? gst_structure_copy (s) : NULL;
      if (GST_V4L2_IS_OPEN (v4l2object))
        gst_v4l2_set_controls (v4l2object, v4l2object->extra_controls);
      break;
    }
    case PROP_PIXEL_ASPECT_RATIO:
      g_free (v4l2object->par);
      v4l2object->par = g_new0 (GValue, 1);
      g_value_init (v4l2object->par, GST_TYPE_FRACTION);
      if (!g_value_transform (value, v4l2object->par)) {
        g_warning ("Could not transform string to aspect ratio");
        gst_value_set_fraction (v4l2object->par, 1, 1);
      }
      GST_DEBUG_OBJECT (v4l2object->element, "set PAR to %d/%d",
          gst_value_get_fraction_numerator (v4l2object->par),
          gst_value_get_fraction_denominator (v4l2object->par));
      break;
    case PROP_FORCE_ASPECT_RATIO:
      v4l2object->keep_aspect = g_value_get_boolean (value);
      break;
    default:
      return FALSE;
      break;
  }
  return TRUE;
}


gboolean
gst_v4l2_object_get_property_helper (GstV4l2Object * v4l2object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, v4l2object->videodev);
      break;
    case PROP_DEVICE_NAME:
    {
      const guchar *new = NULL;

      if (GST_V4L2_IS_OPEN (v4l2object)) {
        new = v4l2object->vcap.card;
      } else if (gst_v4l2_open (v4l2object)) {
        new = v4l2object->vcap.card;
        gst_v4l2_close (v4l2object);
      }
      g_value_set_string (value, (gchar *) new);
      break;
    }
    case PROP_DEVICE_FD:
    {
      if (GST_V4L2_IS_OPEN (v4l2object))
        g_value_set_int (value, v4l2object->video_fd);
      else
        g_value_set_int (value, DEFAULT_PROP_DEVICE_FD);
      break;
    }
    case PROP_FLAGS:
    {
      guint flags = 0;

      if (GST_V4L2_IS_OPEN (v4l2object)) {
        flags |= v4l2object->vcap.capabilities &
            (V4L2_CAP_VIDEO_CAPTURE |
            V4L2_CAP_VIDEO_OUTPUT |
            V4L2_CAP_VIDEO_OVERLAY |
            V4L2_CAP_VBI_CAPTURE |
            V4L2_CAP_VBI_OUTPUT | V4L2_CAP_TUNER | V4L2_CAP_AUDIO);

        if (v4l2object->vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
          flags |= V4L2_CAP_VIDEO_CAPTURE;

        if (v4l2object->vcap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
          flags |= V4L2_CAP_VIDEO_OUTPUT;
      }
      g_value_set_flags (value, flags);
      break;
    }
    case PROP_BRIGHTNESS:
    case PROP_CONTRAST:
    case PROP_SATURATION:
    case PROP_HUE:
    {
      gint cid = gst_v4l2_object_prop_to_cid (prop_id);

      if (cid != -1) {
        if (GST_V4L2_IS_OPEN (v4l2object)) {
          gint v;
          if (gst_v4l2_get_attribute (v4l2object, cid, &v)) {
            g_value_set_int (value, v);
          }
        }
      }
      return TRUE;
    }
      break;
    case PROP_TV_NORM:
      g_value_set_enum (value, v4l2object->tv_norm);
      break;
    case PROP_IO_MODE:
      g_value_set_enum (value, v4l2object->req_mode);
      break;
    case PROP_EXTRA_CONTROLS:
      gst_value_set_structure (value, v4l2object->extra_controls);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      if (v4l2object->par)
        g_value_transform (v4l2object->par, value);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, v4l2object->keep_aspect);
      break;
    default:
      return FALSE;
      break;
  }
  return TRUE;
}

static void
gst_v4l2_set_defaults (GstV4l2Object * v4l2object)
{
  GstTunerNorm *norm = NULL;
  GstTunerChannel *channel = NULL;
  GstTuner *tuner;

  if (!GST_IS_TUNER (v4l2object->element))
    return;

  tuner = GST_TUNER (v4l2object->element);

  if (v4l2object->tv_norm)
    norm = gst_v4l2_tuner_get_norm_by_std_id (v4l2object, v4l2object->tv_norm);
  GST_DEBUG_OBJECT (v4l2object->element, "tv_norm=0x%" G_GINT64_MODIFIER "x, "
      "norm=%p", (guint64) v4l2object->tv_norm, norm);
  if (norm) {
    gst_tuner_set_norm (tuner, norm);
  } else {
    norm =
        GST_TUNER_NORM (gst_tuner_get_norm (GST_TUNER (v4l2object->element)));
    if (norm) {
      v4l2object->tv_norm =
          gst_v4l2_tuner_get_std_id_by_norm (v4l2object, norm);
      gst_tuner_norm_changed (tuner, norm);
    }
  }

  if (v4l2object->channel)
    channel = gst_tuner_find_channel_by_name (tuner, v4l2object->channel);
  if (channel) {
    gst_tuner_set_channel (tuner, channel);
  } else {
    channel =
        GST_TUNER_CHANNEL (gst_tuner_get_channel (GST_TUNER
            (v4l2object->element)));
    if (channel) {
      g_free (v4l2object->channel);
      v4l2object->channel = g_strdup (channel->label);
      gst_tuner_channel_changed (tuner, channel);
    }
  }

  if (channel
      && GST_TUNER_CHANNEL_HAS_FLAG (channel, GST_TUNER_CHANNEL_FREQUENCY)) {
    if (v4l2object->frequency != 0) {
      gst_tuner_set_frequency (tuner, channel, v4l2object->frequency);
    } else {
      v4l2object->frequency = gst_tuner_get_frequency (tuner, channel);
      if (v4l2object->frequency == 0) {
        /* guess */
        gst_tuner_set_frequency (tuner, channel, 1000);
      } else {
      }
    }
  }
}

gboolean
gst_v4l2_object_open (GstV4l2Object * v4l2object)
{
  if (gst_v4l2_open (v4l2object))
    gst_v4l2_set_defaults (v4l2object);
  else
    return FALSE;

  return TRUE;
}

gboolean
gst_v4l2_object_open_shared (GstV4l2Object * v4l2object, GstV4l2Object * other)
{
  gboolean ret;

  ret = gst_v4l2_dup (v4l2object, other);

  return ret;
}

gboolean
gst_v4l2_object_close (GstV4l2Object * v4l2object)
{
  if (!gst_v4l2_close (v4l2object))
    return FALSE;

  gst_caps_replace (&v4l2object->probed_caps, NULL);

  if (v4l2object->formats) {
    gst_v4l2_object_clear_format_list (v4l2object);
  }

  return TRUE;
}


/*
 * common format / caps utilities:
 */
typedef enum
{
  GST_V4L2_RAW = 1 << 0,
  GST_V4L2_CODEC = 1 << 1,
  GST_V4L2_TRANSPORT = 1 << 2,
  GST_V4L2_NO_PARSE = 1 << 3,
  GST_V4L2_ALL = 0xffff
} GstV4L2FormatFlags;

typedef struct
{
  guint32 format;
  gboolean dimensions;
  GstV4L2FormatFlags flags;
} GstV4L2FormatDesc;

static const GstV4L2FormatDesc gst_v4l2_formats[] = {
  /* from Linux 2.6.15 videodev2.h */
  {V4L2_PIX_FMT_RGB332, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_RGB555, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_RGB565, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_RGB555X, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_RGB565X, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_BGR24, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_RGB24, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_BGR32, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_RGB32, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_GREY, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_YVU410, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_YVU420, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_YUYV, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_UYVY, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_YUV422P, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_YUV411P, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_Y41P, TRUE, GST_V4L2_RAW},

  /* two planes -- one Y, one Cr + Cb interleaved  */
  {V4L2_PIX_FMT_NV12, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_NV12M, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_NV12MT, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_NV21, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_NV21M, TRUE, GST_V4L2_RAW},

  /*  The following formats are not defined in the V4L2 specification */
  {V4L2_PIX_FMT_YUV410, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_YUV420, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_YYUV, TRUE, GST_V4L2_RAW},
  {V4L2_PIX_FMT_HI240, TRUE, GST_V4L2_RAW},

  /* see http://www.siliconimaging.com/RGB%20Bayer.htm */
  {V4L2_PIX_FMT_SBGGR8, TRUE, GST_V4L2_CODEC},

  /* compressed formats */
  {V4L2_PIX_FMT_MJPEG, FALSE, GST_V4L2_CODEC},
  {V4L2_PIX_FMT_JPEG, FALSE, GST_V4L2_CODEC},
  {V4L2_PIX_FMT_PJPG, FALSE, GST_V4L2_CODEC},
  {V4L2_PIX_FMT_DV, FALSE, GST_V4L2_TRANSPORT},
  {V4L2_PIX_FMT_MPEG, FALSE, GST_V4L2_TRANSPORT},
  {V4L2_PIX_FMT_MPEG1, FALSE, GST_V4L2_CODEC},
  {V4L2_PIX_FMT_MPEG2, FALSE, GST_V4L2_CODEC},
  {V4L2_PIX_FMT_MPEG4, FALSE, GST_V4L2_CODEC},

  {V4L2_PIX_FMT_H263, FALSE, GST_V4L2_CODEC},
  {V4L2_PIX_FMT_H264, FALSE, GST_V4L2_CODEC},
  /* VP8 not parseable */
  {V4L2_PIX_FMT_VP8, FALSE, GST_V4L2_CODEC | GST_V4L2_NO_PARSE},

  /*  Vendor-specific formats   */
  {V4L2_PIX_FMT_WNVA, TRUE, GST_V4L2_CODEC},
  {V4L2_PIX_FMT_SN9C10X, TRUE, GST_V4L2_CODEC},
  {V4L2_PIX_FMT_PWC1, TRUE, GST_V4L2_CODEC},
  {V4L2_PIX_FMT_PWC2, TRUE, GST_V4L2_CODEC},
  {V4L2_PIX_FMT_YVYU, TRUE, GST_V4L2_RAW},
};

#define GST_V4L2_FORMAT_COUNT (G_N_ELEMENTS (gst_v4l2_formats))


static struct v4l2_fmtdesc *
gst_v4l2_object_get_format_from_fourcc (GstV4l2Object * v4l2object,
    guint32 fourcc)
{
  struct v4l2_fmtdesc *fmt;
  GSList *walk;

  if (fourcc == 0)
    return NULL;

  walk = gst_v4l2_object_get_format_list (v4l2object);
  while (walk) {
    fmt = (struct v4l2_fmtdesc *) walk->data;
    if (fmt->pixelformat == fourcc)
      return fmt;
    /* special case for jpeg */
    if (fmt->pixelformat == V4L2_PIX_FMT_MJPEG ||
        fmt->pixelformat == V4L2_PIX_FMT_JPEG ||
        fmt->pixelformat == V4L2_PIX_FMT_PJPG) {
      if (fourcc == V4L2_PIX_FMT_JPEG || fourcc == V4L2_PIX_FMT_MJPEG ||
          fourcc == V4L2_PIX_FMT_PJPG) {
        return fmt;
      }
    }
    walk = g_slist_next (walk);
  }

  return NULL;
}



/* complete made up ranking, the values themselves are meaningless */
/* These ranks MUST be X such that X<<15 fits on a signed int - see
   the comment at the end of gst_v4l2_object_format_get_rank. */
#define YUV_BASE_RANK     1000
#define JPEG_BASE_RANK     500
#define DV_BASE_RANK       200
#define RGB_BASE_RANK      100
#define YUV_ODD_BASE_RANK   50
#define RGB_ODD_BASE_RANK   25
#define BAYER_BASE_RANK     15
#define S910_BASE_RANK      10
#define GREY_BASE_RANK       5
#define PWC_BASE_RANK        1

static gint
gst_v4l2_object_format_get_rank (const struct v4l2_fmtdesc *fmt)
{
  guint32 fourcc = fmt->pixelformat;
  gboolean emulated = ((fmt->flags & V4L2_FMT_FLAG_EMULATED) != 0);
  gint rank = 0;

  switch (fourcc) {
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_PJPG:
      rank = JPEG_BASE_RANK;
      break;
    case V4L2_PIX_FMT_JPEG:
      rank = JPEG_BASE_RANK + 1;
      break;
    case V4L2_PIX_FMT_MPEG:    /* MPEG          */
      rank = JPEG_BASE_RANK + 2;
      break;

    case V4L2_PIX_FMT_RGB332:
    case V4L2_PIX_FMT_RGB555:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB565X:
      rank = RGB_ODD_BASE_RANK;
      break;

    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
      rank = RGB_BASE_RANK - 1;
      break;

    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_BGR32:
      rank = RGB_BASE_RANK;
      break;

    case V4L2_PIX_FMT_GREY:    /*  8  Greyscale     */
      rank = GREY_BASE_RANK;
      break;

    case V4L2_PIX_FMT_NV12:    /* 12  Y/CbCr 4:2:0  */
    case V4L2_PIX_FMT_NV12M:   /* Same as NV12      */
    case V4L2_PIX_FMT_NV12MT:  /* NV12 64x32 tile   */
    case V4L2_PIX_FMT_NV21:    /* 12  Y/CrCb 4:2:0  */
    case V4L2_PIX_FMT_NV21M:   /* Same as NV21      */
    case V4L2_PIX_FMT_YYUV:    /* 16  YUV 4:2:2     */
    case V4L2_PIX_FMT_HI240:   /*  8  8-bit color   */
      rank = YUV_ODD_BASE_RANK;
      break;

    case V4L2_PIX_FMT_YVU410:  /* YVU9,  9 bits per pixel */
      rank = YUV_BASE_RANK + 3;
      break;
    case V4L2_PIX_FMT_YUV410:  /* YUV9,  9 bits per pixel */
      rank = YUV_BASE_RANK + 2;
      break;
    case V4L2_PIX_FMT_YUV420:  /* I420, 12 bits per pixel */
      rank = YUV_BASE_RANK + 7;
      break;
    case V4L2_PIX_FMT_YUYV:    /* YUY2, 16 bits per pixel */
      rank = YUV_BASE_RANK + 10;
      break;
    case V4L2_PIX_FMT_YVU420:  /* YV12, 12 bits per pixel */
      rank = YUV_BASE_RANK + 6;
      break;
    case V4L2_PIX_FMT_UYVY:    /* UYVY, 16 bits per pixel */
      rank = YUV_BASE_RANK + 9;
      break;
    case V4L2_PIX_FMT_Y41P:    /* Y41P, 12 bits per pixel */
      rank = YUV_BASE_RANK + 5;
      break;
    case V4L2_PIX_FMT_YUV411P: /* Y41B, 12 bits per pixel */
      rank = YUV_BASE_RANK + 4;
      break;
    case V4L2_PIX_FMT_YUV422P: /* Y42B, 16 bits per pixel */
      rank = YUV_BASE_RANK + 8;
      break;

    case V4L2_PIX_FMT_DV:
      rank = DV_BASE_RANK;
      break;

    case V4L2_PIX_FMT_WNVA:    /* Winnov hw compres */
      rank = 0;
      break;

    case V4L2_PIX_FMT_SBGGR8:
      rank = BAYER_BASE_RANK;
      break;

    case V4L2_PIX_FMT_SN9C10X:
      rank = S910_BASE_RANK;
      break;

    case V4L2_PIX_FMT_PWC1:
      rank = PWC_BASE_RANK;
      break;
    case V4L2_PIX_FMT_PWC2:
      rank = PWC_BASE_RANK;
      break;

    default:
      rank = 0;
      break;
  }

  /* All ranks are below 1<<15 so a shift by 15
   * will a) make all non-emulated formats larger
   * than emulated and b) will not overflow
   */
  if (!emulated)
    rank <<= 15;

  return rank;
}



static gint
format_cmp_func (gconstpointer a, gconstpointer b)
{
  const struct v4l2_fmtdesc *fa = a;
  const struct v4l2_fmtdesc *fb = b;

  if (fa->pixelformat == fb->pixelformat)
    return 0;

  return gst_v4l2_object_format_get_rank (fb) -
      gst_v4l2_object_format_get_rank (fa);
}

/******************************************************
 * gst_v4l2_object_fill_format_list():
 *   create list of supported capture formats
 * return value: TRUE on success, FALSE on error
 ******************************************************/
static gboolean
gst_v4l2_object_fill_format_list (GstV4l2Object * v4l2object,
    enum v4l2_buf_type type)
{
  gint n;
  struct v4l2_fmtdesc *format;

  GST_DEBUG_OBJECT (v4l2object->element, "getting src format enumerations");

  /* format enumeration */
  for (n = 0;; n++) {
    format = g_new0 (struct v4l2_fmtdesc, 1);

    format->index = n;
    format->type = type;

    if (v4l2_ioctl (v4l2object->video_fd, VIDIOC_ENUM_FMT, format) < 0) {
      if (errno == EINVAL) {
        g_free (format);
        break;                  /* end of enumeration */
      } else {
        goto failed;
      }
    }

    GST_LOG_OBJECT (v4l2object->element, "index:       %u", format->index);
    GST_LOG_OBJECT (v4l2object->element, "type:        %d", format->type);
    GST_LOG_OBJECT (v4l2object->element, "flags:       %08x", format->flags);
    GST_LOG_OBJECT (v4l2object->element, "description: '%s'",
        format->description);
    GST_LOG_OBJECT (v4l2object->element, "pixelformat: %" GST_FOURCC_FORMAT,
        GST_FOURCC_ARGS (format->pixelformat));

    /* sort formats according to our preference;  we do this, because caps
     * are probed in the order the formats are in the list, and the order of
     * formats in the final probed caps matters for things like fixation */
    v4l2object->formats = g_slist_insert_sorted (v4l2object->formats, format,
        (GCompareFunc) format_cmp_func);
  }

#ifndef GST_DISABLE_GST_DEBUG
  {
    GSList *l;

    GST_INFO_OBJECT (v4l2object->element, "got %d format(s):", n);
    for (l = v4l2object->formats; l != NULL; l = l->next) {
      format = l->data;

      GST_INFO_OBJECT (v4l2object->element,
          "  %" GST_FOURCC_FORMAT "%s", GST_FOURCC_ARGS (format->pixelformat),
          ((format->flags & V4L2_FMT_FLAG_EMULATED)) ? " (emulated)" : "");
    }
  }
#endif

  return TRUE;

  /* ERRORS */
failed:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, SETTINGS,
        (_("Failed to enumerate possible video formats device '%s' can work with"), v4l2object->videodev), ("Failed to get number %d in pixelformat enumeration for %s. (%d - %s)", n, v4l2object->videodev, errno, g_strerror (errno)));
    g_free (format);
    return FALSE;
  }
}

/*
  * Get the list of supported capture formats, a list of
  * <code>struct v4l2_fmtdesc</code>.
  */
static GSList *
gst_v4l2_object_get_format_list (GstV4l2Object * v4l2object)
{
  if (!v4l2object->formats) {

    /* check usual way */
    gst_v4l2_object_fill_format_list (v4l2object, v4l2object->type);

    /* if our driver supports multi-planar
     * and if formats are still empty then we can workaround driver bug
     * by also looking up formats as if our device was not supporting
     * multiplanar */
    if (!v4l2object->formats) {
      switch (v4l2object->type) {
        case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
          gst_v4l2_object_fill_format_list (v4l2object,
              V4L2_BUF_TYPE_VIDEO_CAPTURE);
          break;

        case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
          gst_v4l2_object_fill_format_list (v4l2object,
              V4L2_BUF_TYPE_VIDEO_OUTPUT);
          break;

        default:
          break;
      }
    }
  }
  return v4l2object->formats;
}

static GstVideoFormat
gst_v4l2_object_v4l2fourcc_to_video_format (guint32 fourcc)
{
  GstVideoFormat format;

  switch (fourcc) {
    case V4L2_PIX_FMT_GREY:    /*  8  Greyscale     */
      format = GST_VIDEO_FORMAT_GRAY8;
      break;
    case V4L2_PIX_FMT_RGB555:
      format = GST_VIDEO_FORMAT_RGB15;
      break;
    case V4L2_PIX_FMT_RGB565:
      format = GST_VIDEO_FORMAT_RGB16;
      break;
    case V4L2_PIX_FMT_RGB24:
      format = GST_VIDEO_FORMAT_RGB;
      break;
    case V4L2_PIX_FMT_BGR24:
      format = GST_VIDEO_FORMAT_BGR;
      break;
    case V4L2_PIX_FMT_RGB32:
      format = GST_VIDEO_FORMAT_xRGB;
      break;
    case V4L2_PIX_FMT_BGR32:
      format = GST_VIDEO_FORMAT_BGRx;
      break;
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      format = GST_VIDEO_FORMAT_NV12;
      break;
    case V4L2_PIX_FMT_NV12MT:
      format = GST_VIDEO_FORMAT_NV12_64Z32;
      break;
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV21M:
      format = GST_VIDEO_FORMAT_NV21;
      break;
    case V4L2_PIX_FMT_YVU410:
      format = GST_VIDEO_FORMAT_YVU9;
      break;
    case V4L2_PIX_FMT_YUV410:
      format = GST_VIDEO_FORMAT_YUV9;
      break;
    case V4L2_PIX_FMT_YUV420:
      format = GST_VIDEO_FORMAT_I420;
      break;
    case V4L2_PIX_FMT_YUYV:
      format = GST_VIDEO_FORMAT_YUY2;
      break;
    case V4L2_PIX_FMT_YVU420:
      format = GST_VIDEO_FORMAT_YV12;
      break;
    case V4L2_PIX_FMT_UYVY:
      format = GST_VIDEO_FORMAT_UYVY;
      break;
#if 0
    case V4L2_PIX_FMT_Y41P:
      format = GST_VIDEO_FORMAT_Y41P;
      break;
#endif
    case V4L2_PIX_FMT_YUV411P:
      format = GST_VIDEO_FORMAT_Y41B;
      break;
    case V4L2_PIX_FMT_YUV422P:
      format = GST_VIDEO_FORMAT_Y42B;
      break;
    case V4L2_PIX_FMT_YVYU:
      format = GST_VIDEO_FORMAT_YVYU;
      break;
    default:
      format = GST_VIDEO_FORMAT_UNKNOWN;
      g_assert_not_reached ();
      break;
  }

  return format;
}

GstStructure *
gst_v4l2_object_v4l2fourcc_to_structure (guint32 fourcc)
{
  GstStructure *structure = NULL;

  switch (fourcc) {
    case V4L2_PIX_FMT_MJPEG:   /* Motion-JPEG */
    case V4L2_PIX_FMT_PJPG:    /* Progressive-JPEG */
    case V4L2_PIX_FMT_JPEG:    /* JFIF JPEG */
      structure = gst_structure_new_empty ("image/jpeg");
      break;
    case V4L2_PIX_FMT_YYUV:    /* 16  YUV 4:2:2     */
    case V4L2_PIX_FMT_HI240:   /*  8  8-bit color   */
      /* FIXME: get correct fourccs here */
      break;
    case V4L2_PIX_FMT_MPEG1:
      structure = gst_structure_new ("video/mpeg",
          "mpegversion", G_TYPE_INT, 2, NULL);
      break;
    case V4L2_PIX_FMT_MPEG2:
      structure = gst_structure_new ("video/mpeg",
          "mpegversion", G_TYPE_INT, 2, NULL);
      break;
    case V4L2_PIX_FMT_MPEG4:
      structure = gst_structure_new ("video/mpeg",
          "mpegversion", G_TYPE_INT, 4, "systemstream",
          G_TYPE_BOOLEAN, FALSE, NULL);
      break;
    case V4L2_PIX_FMT_H263:
      structure = gst_structure_new ("video/x-h263",
          "variant", G_TYPE_STRING, "itu", NULL);
      break;
    case V4L2_PIX_FMT_H264:    /* H.264 */
      structure = gst_structure_new ("video/x-h264",
          "stream-format", G_TYPE_STRING, "byte-stream", "alignment",
          G_TYPE_STRING, "au", NULL);
      break;
    case V4L2_PIX_FMT_VP8:
      structure = gst_structure_new_empty ("video/x-vp8");
      break;
    case V4L2_PIX_FMT_RGB332:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB565X:
      /* FIXME: get correct fourccs here */
      break;
    case V4L2_PIX_FMT_GREY:    /*  8  Greyscale     */
    case V4L2_PIX_FMT_RGB555:
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_BGR24:
    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_NV12:    /* 12  Y/CbCr 4:2:0  */
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV12MT:
    case V4L2_PIX_FMT_NV21:    /* 12  Y/CrCb 4:2:0  */
    case V4L2_PIX_FMT_NV21M:
    case V4L2_PIX_FMT_YVU410:
    case V4L2_PIX_FMT_YUV410:
    case V4L2_PIX_FMT_YUV420:  /* I420/IYUV */
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_UYVY:
#if 0
    case V4L2_PIX_FMT_Y41P:
#endif
    case V4L2_PIX_FMT_YUV422P:
    case V4L2_PIX_FMT_YVYU:
    case V4L2_PIX_FMT_YUV411P:{
      GstVideoFormat format;
      format = gst_v4l2_object_v4l2fourcc_to_video_format (fourcc);
      if (format != GST_VIDEO_FORMAT_UNKNOWN)
        structure = gst_structure_new ("video/x-raw",
            "format", G_TYPE_STRING, gst_video_format_to_string (format), NULL);
      break;
    }
    case V4L2_PIX_FMT_DV:
      structure =
          gst_structure_new ("video/x-dv", "systemstream", G_TYPE_BOOLEAN, TRUE,
          NULL);
      break;
    case V4L2_PIX_FMT_MPEG:    /* MPEG          */
      structure = gst_structure_new ("video/mpegts",
          "systemstream", G_TYPE_BOOLEAN, TRUE, NULL);
      break;
    case V4L2_PIX_FMT_WNVA:    /* Winnov hw compres */
      break;
    case V4L2_PIX_FMT_SBGGR8:
      structure = gst_structure_new_empty ("video/x-bayer");
      break;
    case V4L2_PIX_FMT_SN9C10X:
      structure = gst_structure_new_empty ("video/x-sonix");
      break;
    case V4L2_PIX_FMT_PWC1:
      structure = gst_structure_new_empty ("video/x-pwc1");
      break;
    case V4L2_PIX_FMT_PWC2:
      structure = gst_structure_new_empty ("video/x-pwc2");
      break;
    default:
      GST_DEBUG ("Unknown fourcc 0x%08x %" GST_FOURCC_FORMAT,
          fourcc, GST_FOURCC_ARGS (fourcc));
      break;
  }

  return structure;
}


static GstCaps *
gst_v4l2_object_get_caps_helper (GstV4L2FormatFlags flags)
{
  GstStructure *structure;
  GstCaps *caps;
  guint i;

  caps = gst_caps_new_empty ();
  for (i = 0; i < GST_V4L2_FORMAT_COUNT; i++) {

    if ((gst_v4l2_formats[i].flags & flags) == 0)
      continue;

    structure =
        gst_v4l2_object_v4l2fourcc_to_structure (gst_v4l2_formats[i].format);
    if (structure) {
      if (gst_v4l2_formats[i].dimensions) {
        gst_structure_set (structure,
            "width", GST_TYPE_INT_RANGE, 1, GST_V4L2_MAX_SIZE,
            "height", GST_TYPE_INT_RANGE, 1, GST_V4L2_MAX_SIZE,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, 100, 1, NULL);
      }
      gst_caps_append_structure (caps, structure);
    }
  }

  return gst_caps_simplify (caps);
}

GstCaps *
gst_v4l2_object_get_all_caps (void)
{
  static GstCaps *caps = NULL;

  if (caps == NULL)
    caps = gst_v4l2_object_get_caps_helper (GST_V4L2_ALL);

  return gst_caps_ref (caps);
}

GstCaps *
gst_v4l2_object_get_raw_caps (void)
{
  static GstCaps *caps = NULL;

  if (caps == NULL)
    caps = gst_v4l2_object_get_caps_helper (GST_V4L2_RAW);

  return gst_caps_ref (caps);
}

GstCaps *
gst_v4l2_object_get_codec_caps (void)
{
  static GstCaps *caps = NULL;

  if (caps == NULL)
    caps = gst_v4l2_object_get_caps_helper (GST_V4L2_CODEC);

  return gst_caps_ref (caps);
}

/* gst_v4l2_object_choose_fourcc:
 * @obj a #GstV4l2Object
 * @fourcc_splane The format type in single plane representation
 * @fourcc_mplane The format type in multi-plane representation
 * @fourcc Set to the first format to try
 * @fourcc_alt The alternative format to use, or zero if mplane is not
 * supported. Note that if alternate is used, the prefered_non_contiguous
 * setting need to be inversed.
 *
 * Certain format can be stored into multi-planar buffer type with two
 * representation. As an example, NV12, which has two planes, can be stored
 * into 1 plane of multi-planar buffer sturcture, or two. This function will
 * choose the right format to use base on the object settings.
 */
static void
gst_v4l2_object_choose_fourcc (GstV4l2Object * obj, guint32 fourcc_splane,
    guint32 fourcc_mplane, guint32 * fourcc, guint32 * fourcc_alt)
{
  if (V4L2_TYPE_IS_MULTIPLANAR (obj->type)) {
    if (obj->prefered_non_contiguous) {
      *fourcc = fourcc_mplane;
      *fourcc_alt = fourcc_splane;
    } else {
      *fourcc = fourcc_splane;
      *fourcc_alt = fourcc_mplane;
    }
  } else {
    *fourcc = fourcc_splane;
    *fourcc_alt = 0;
  }
}

/* collect data for the given caps
 * @caps: given input caps
 * @format: location for the v4l format
 * @w/@h: location for width and height
 * @fps_n/@fps_d: location for framerate
 * @size: location for expected size of the frame or 0 if unknown
 */
static gboolean
gst_v4l2_object_get_caps_info (GstV4l2Object * v4l2object, GstCaps * caps,
    struct v4l2_fmtdesc **format, GstVideoInfo * info)
{
  GstStructure *structure;
  guint32 fourcc, fourcc_alt = 0;
  const gchar *mimetype;
  struct v4l2_fmtdesc *fmt;

  /* default unknown values */
  fourcc = 0;

  structure = gst_caps_get_structure (caps, 0);

  mimetype = gst_structure_get_name (structure);

  if (!gst_video_info_from_caps (info, caps))
    goto invalid_format;

  if (g_str_equal (mimetype, "video/x-raw")) {
    switch (GST_VIDEO_INFO_FORMAT (info)) {
      case GST_VIDEO_FORMAT_I420:
        fourcc = V4L2_PIX_FMT_YUV420;
        break;
      case GST_VIDEO_FORMAT_YUY2:
        fourcc = V4L2_PIX_FMT_YUYV;
        break;
#if 0
      case GST_VIDEO_FORMAT_Y41P:
        fourcc = V4L2_PIX_FMT_Y41P;
        break;
#endif
      case GST_VIDEO_FORMAT_UYVY:
        fourcc = V4L2_PIX_FMT_UYVY;
        break;
      case GST_VIDEO_FORMAT_YV12:
        fourcc = V4L2_PIX_FMT_YVU420;
        break;
      case GST_VIDEO_FORMAT_Y41B:
        fourcc = V4L2_PIX_FMT_YUV411P;
        break;
      case GST_VIDEO_FORMAT_Y42B:
        fourcc = V4L2_PIX_FMT_YUV422P;
        break;
      case GST_VIDEO_FORMAT_NV12:
        gst_v4l2_object_choose_fourcc (v4l2object, V4L2_PIX_FMT_NV12,
            V4L2_PIX_FMT_NV12M, &fourcc, &fourcc_alt);
        break;
      case GST_VIDEO_FORMAT_NV12_64Z32:
        fourcc = V4L2_PIX_FMT_NV12MT;
        break;
      case GST_VIDEO_FORMAT_NV21:
        gst_v4l2_object_choose_fourcc (v4l2object, V4L2_PIX_FMT_NV21,
            V4L2_PIX_FMT_NV21M, &fourcc, &fourcc_alt);
        break;
      case GST_VIDEO_FORMAT_YVYU:
        fourcc = V4L2_PIX_FMT_YVYU;
        break;
      case GST_VIDEO_FORMAT_RGB15:
        fourcc = V4L2_PIX_FMT_RGB555;
        break;
      case GST_VIDEO_FORMAT_RGB16:
        fourcc = V4L2_PIX_FMT_RGB565;
        break;
      case GST_VIDEO_FORMAT_RGB:
        fourcc = V4L2_PIX_FMT_RGB24;
        break;
      case GST_VIDEO_FORMAT_BGR:
        fourcc = V4L2_PIX_FMT_BGR24;
        break;
      case GST_VIDEO_FORMAT_xRGB:
      case GST_VIDEO_FORMAT_ARGB:
        fourcc = V4L2_PIX_FMT_RGB32;
        break;
      case GST_VIDEO_FORMAT_BGRx:
      case GST_VIDEO_FORMAT_BGRA:
        fourcc = V4L2_PIX_FMT_BGR32;
        break;
      case GST_VIDEO_FORMAT_GRAY8:
        fourcc = V4L2_PIX_FMT_GREY;
      default:
        break;
    }
  } else {
    if (g_str_equal (mimetype, "video/mpegts")) {
      fourcc = V4L2_PIX_FMT_MPEG;
    } else if (g_str_equal (mimetype, "video/x-dv")) {
      fourcc = V4L2_PIX_FMT_DV;
    } else if (g_str_equal (mimetype, "image/jpeg")) {
      fourcc = V4L2_PIX_FMT_JPEG;
    } else if (g_str_equal (mimetype, "video/mpeg")) {
      gint version;
      if (gst_structure_get_int (structure, "mpegversion", &version)) {
        switch (version) {
          case 1:
            fourcc = V4L2_PIX_FMT_MPEG1;
            break;
          case 2:
            fourcc = V4L2_PIX_FMT_MPEG2;
            break;
          case 4:
            fourcc = V4L2_PIX_FMT_MPEG4;
            break;
          default:
            break;
        }
      }
    } else if (g_str_equal (mimetype, "video/x-h263")) {
      fourcc = V4L2_PIX_FMT_H263;
    } else if (g_str_equal (mimetype, "video/x-h264")) {
      fourcc = V4L2_PIX_FMT_H264;
    } else if (g_str_equal (mimetype, "video/x-vp8")) {
      fourcc = V4L2_PIX_FMT_VP8;
    } else if (g_str_equal (mimetype, "video/x-bayer")) {
      fourcc = V4L2_PIX_FMT_SBGGR8;
    } else if (g_str_equal (mimetype, "video/x-sonix")) {
      fourcc = V4L2_PIX_FMT_SN9C10X;
    } else if (g_str_equal (mimetype, "video/x-pwc1")) {
      fourcc = V4L2_PIX_FMT_PWC1;
    } else if (g_str_equal (mimetype, "video/x-pwc2")) {
      fourcc = V4L2_PIX_FMT_PWC2;
    }
  }

  if (fourcc == 0)
    goto unhandled_format;

  fmt = gst_v4l2_object_get_format_from_fourcc (v4l2object, fourcc);

  if (fmt == NULL && fourcc_alt != 0) {
    GST_DEBUG_OBJECT (v4l2object, "No support for %" GST_FOURCC_FORMAT
        " trying %" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (fourcc),
        GST_FOURCC_ARGS (fourcc_alt));
    v4l2object->prefered_non_contiguous = !v4l2object->prefered_non_contiguous;
    fmt = gst_v4l2_object_get_format_from_fourcc (v4l2object, fourcc_alt);
  }

  if (fmt == NULL)
    goto unsupported_format;

  *format = fmt;

  return TRUE;

  /* ERRORS */
invalid_format:
  {
    GST_DEBUG_OBJECT (v4l2object, "invalid format");
    return FALSE;
  }
unhandled_format:
  {
    GST_DEBUG_OBJECT (v4l2object, "unhandled format");
    return FALSE;
  }
unsupported_format:
  {
    GST_DEBUG_OBJECT (v4l2object, "unsupported format");
    return FALSE;
  }
}

static gboolean
gst_v4l2_object_get_nearest_size (GstV4l2Object * v4l2object,
    guint32 pixelformat, gint * width, gint * height, gboolean * interlaced);

static void
gst_v4l2_object_add_aspect_ratio (GstV4l2Object * v4l2object, GstStructure * s)
{
  struct v4l2_cropcap cropcap;
  int num = 1, den = 1;

  if (!v4l2object->keep_aspect)
    return;

  if (v4l2object->par) {
    num = gst_value_get_fraction_numerator (v4l2object->par);
    den = gst_value_get_fraction_denominator (v4l2object->par);
    goto done;
  }

  memset (&cropcap, 0, sizeof (cropcap));

  cropcap.type = v4l2object->type;
  if (v4l2_ioctl (v4l2object->video_fd, VIDIOC_CROPCAP, &cropcap) < 0)
    goto cropcap_failed;

  num = cropcap.pixelaspect.numerator;
  den = cropcap.pixelaspect.denominator;

done:
  gst_structure_set (s, "pixel-aspect-ratio", GST_TYPE_FRACTION, num, den,
      NULL);
  return;

cropcap_failed:
  if (errno != ENOTTY)
    GST_WARNING_OBJECT (v4l2object->element,
        "Failed to probe pixel aspect ratio with VIDIOC_CROPCAP: %s",
        g_strerror (errno));
  goto done;
}


/* The frame interval enumeration code first appeared in Linux 2.6.19. */
static GstStructure *
gst_v4l2_object_probe_caps_for_format_and_size (GstV4l2Object * v4l2object,
    guint32 pixelformat,
    guint32 width, guint32 height, const GstStructure * template)
{
  gint fd = v4l2object->video_fd;
  struct v4l2_frmivalenum ival;
  guint32 num, denom;
  GstStructure *s;
  GValue rates = { 0, };
  gboolean interlaced;
  gint int_width = width;
  gint int_height = height;

  if (v4l2object->never_interlaced) {
    interlaced = FALSE;
  } else {
    /* Interlaced detection using VIDIOC_TRY/S_FMT */
    if (!gst_v4l2_object_get_nearest_size (v4l2object, pixelformat,
            &int_width, &int_height, &interlaced))
      return NULL;
  }

  memset (&ival, 0, sizeof (struct v4l2_frmivalenum));
  ival.index = 0;
  ival.pixel_format = pixelformat;
  ival.width = width;
  ival.height = height;

  GST_LOG_OBJECT (v4l2object->element,
      "get frame interval for %ux%u, %" GST_FOURCC_FORMAT, width, height,
      GST_FOURCC_ARGS (pixelformat));

  /* keep in mind that v4l2 gives us frame intervals (durations); we invert the
   * fraction to get framerate */
  if (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) < 0)
    goto enum_frameintervals_failed;

  if (ival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
    GValue rate = { 0, };

    g_value_init (&rates, GST_TYPE_LIST);
    g_value_init (&rate, GST_TYPE_FRACTION);

    do {
      num = ival.discrete.numerator;
      denom = ival.discrete.denominator;

      if (num > G_MAXINT || denom > G_MAXINT) {
        /* let us hope we don't get here... */
        num >>= 1;
        denom >>= 1;
      }

      GST_LOG_OBJECT (v4l2object->element, "adding discrete framerate: %d/%d",
          denom, num);

      /* swap to get the framerate */
      gst_value_set_fraction (&rate, denom, num);
      gst_value_list_append_value (&rates, &rate);

      ival.index++;
    } while (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) >= 0);
  } else if (ival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
    GValue min = { 0, };
    GValue step = { 0, };
    GValue max = { 0, };
    gboolean added = FALSE;
    guint32 minnum, mindenom;
    guint32 maxnum, maxdenom;

    g_value_init (&rates, GST_TYPE_LIST);

    g_value_init (&min, GST_TYPE_FRACTION);
    g_value_init (&step, GST_TYPE_FRACTION);
    g_value_init (&max, GST_TYPE_FRACTION);

    /* get the min */
    minnum = ival.stepwise.min.numerator;
    mindenom = ival.stepwise.min.denominator;
    if (minnum > G_MAXINT || mindenom > G_MAXINT) {
      minnum >>= 1;
      mindenom >>= 1;
    }
    GST_LOG_OBJECT (v4l2object->element, "stepwise min frame interval: %d/%d",
        minnum, mindenom);
    gst_value_set_fraction (&min, minnum, mindenom);

    /* get the max */
    maxnum = ival.stepwise.max.numerator;
    maxdenom = ival.stepwise.max.denominator;
    if (maxnum > G_MAXINT || maxdenom > G_MAXINT) {
      maxnum >>= 1;
      maxdenom >>= 1;
    }

    GST_LOG_OBJECT (v4l2object->element, "stepwise max frame interval: %d/%d",
        maxnum, maxdenom);
    gst_value_set_fraction (&max, maxnum, maxdenom);

    /* get the step */
    num = ival.stepwise.step.numerator;
    denom = ival.stepwise.step.denominator;
    if (num > G_MAXINT || denom > G_MAXINT) {
      num >>= 1;
      denom >>= 1;
    }

    if (num == 0 || denom == 0) {
      /* in this case we have a wrong fraction or no step, set the step to max
       * so that we only add the min value in the loop below */
      num = maxnum;
      denom = maxdenom;
    }

    /* since we only have gst_value_fraction_subtract and not add, negate the
     * numerator */
    GST_LOG_OBJECT (v4l2object->element, "stepwise step frame interval: %d/%d",
        num, denom);
    gst_value_set_fraction (&step, -num, denom);

    while (gst_value_compare (&min, &max) != GST_VALUE_GREATER_THAN) {
      GValue rate = { 0, };

      num = gst_value_get_fraction_numerator (&min);
      denom = gst_value_get_fraction_denominator (&min);
      GST_LOG_OBJECT (v4l2object->element, "adding stepwise framerate: %d/%d",
          denom, num);

      /* invert to get the framerate */
      g_value_init (&rate, GST_TYPE_FRACTION);
      gst_value_set_fraction (&rate, denom, num);
      gst_value_list_append_value (&rates, &rate);
      added = TRUE;

      /* we're actually adding because step was negated above. This is because
       * there is no _add function... */
      if (!gst_value_fraction_subtract (&min, &min, &step)) {
        GST_WARNING_OBJECT (v4l2object->element, "could not step fraction!");
        break;
      }
    }
    if (!added) {
      /* no range was added, leave the default range from the template */
      GST_WARNING_OBJECT (v4l2object->element,
          "no range added, leaving default");
      g_value_unset (&rates);
    }
  } else if (ival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
    guint32 maxnum, maxdenom;

    g_value_init (&rates, GST_TYPE_FRACTION_RANGE);

    num = ival.stepwise.min.numerator;
    denom = ival.stepwise.min.denominator;
    if (num > G_MAXINT || denom > G_MAXINT) {
      num >>= 1;
      denom >>= 1;
    }

    maxnum = ival.stepwise.max.numerator;
    maxdenom = ival.stepwise.max.denominator;
    if (maxnum > G_MAXINT || maxdenom > G_MAXINT) {
      maxnum >>= 1;
      maxdenom >>= 1;
    }

    GST_LOG_OBJECT (v4l2object->element,
        "continuous frame interval %d/%d to %d/%d", maxdenom, maxnum, denom,
        num);

    gst_value_set_fraction_range_full (&rates, maxdenom, maxnum, denom, num);
  } else {
    goto unknown_type;
  }

return_data:
  s = gst_structure_copy (template);
  gst_structure_set (s, "width", G_TYPE_INT, (gint) width,
      "height", G_TYPE_INT, (gint) height, NULL);
  gst_v4l2_object_add_aspect_ratio (v4l2object, s);
  if (g_str_equal (gst_structure_get_name (s), "video/x-raw"))
    gst_structure_set (s, "interlace-mode", G_TYPE_STRING,
        (interlaced ? "mixed" : "progressive"), NULL);

  if (G_IS_VALUE (&rates)) {
    /* only change the framerate on the template when we have a valid probed new
     * value */
    gst_structure_set_value (s, "framerate", &rates);
    g_value_unset (&rates);
  } else if (v4l2object->type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      v4l2object->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
    gst_structure_set (s, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, 100, 1,
        NULL);
  }
  return s;

  /* ERRORS */
enum_frameintervals_failed:
  {
    GST_DEBUG_OBJECT (v4l2object->element,
        "Unable to enumerate intervals for %" GST_FOURCC_FORMAT "@%ux%u",
        GST_FOURCC_ARGS (pixelformat), width, height);
    goto return_data;
  }
unknown_type:
  {
    /* I don't see how this is actually an error, we ignore the format then */
    GST_WARNING_OBJECT (v4l2object->element,
        "Unknown frame interval type at %" GST_FOURCC_FORMAT "@%ux%u: %u",
        GST_FOURCC_ARGS (pixelformat), width, height, ival.type);
    return NULL;
  }
}

static gint
sort_by_frame_size (GstStructure * s1, GstStructure * s2)
{
  int w1, h1, w2, h2;

  gst_structure_get_int (s1, "width", &w1);
  gst_structure_get_int (s1, "height", &h1);
  gst_structure_get_int (s2, "width", &w2);
  gst_structure_get_int (s2, "height", &h2);

  /* I think it's safe to assume that this won't overflow for a while */
  return ((w2 * h2) - (w1 * h1));
}

static void
gst_v4l2_object_update_and_append (GstV4l2Object * v4l2object,
    guint32 format, GstCaps * caps, GstStructure * s)
{
  /* Encoded stream on output buffer need to be parsed */
  if (v4l2object->type == V4L2_BUF_TYPE_VIDEO_OUTPUT ||
      v4l2object->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    gint i = 0;

    for (; i < GST_V4L2_FORMAT_COUNT; i++) {
      if (format == gst_v4l2_formats[i].format &&
          gst_v4l2_formats[i].flags & GST_V4L2_CODEC &&
          !(gst_v4l2_formats[i].flags & GST_V4L2_NO_PARSE)) {
        gst_structure_set (s, "parsed", G_TYPE_BOOLEAN, TRUE, NULL);
        break;
      }
    }
  }

  gst_caps_append_structure (caps, s);
}

static GstCaps *
gst_v4l2_object_probe_caps_for_format (GstV4l2Object * v4l2object,
    guint32 pixelformat, const GstStructure * template)
{
  GstCaps *ret = gst_caps_new_empty ();
  GstStructure *tmp;
  gint fd = v4l2object->video_fd;
  struct v4l2_frmsizeenum size;
  GList *results = NULL;
  guint32 w, h;

  if (pixelformat == GST_MAKE_FOURCC ('M', 'P', 'E', 'G')) {
    gst_caps_append_structure (ret, gst_structure_copy (template));
    return ret;
  }

  memset (&size, 0, sizeof (struct v4l2_frmsizeenum));
  size.index = 0;
  size.pixel_format = pixelformat;

  GST_DEBUG_OBJECT (v4l2object->element,
      "Enumerating frame sizes for %" GST_FOURCC_FORMAT,
      GST_FOURCC_ARGS (pixelformat));

  if (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMESIZES, &size) < 0)
    goto enum_framesizes_failed;

  if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
    do {
      GST_LOG_OBJECT (v4l2object->element, "got discrete frame size %dx%d",
          size.discrete.width, size.discrete.height);

      w = MIN (size.discrete.width, G_MAXINT);
      h = MIN (size.discrete.height, G_MAXINT);

      if (w && h) {
        tmp =
            gst_v4l2_object_probe_caps_for_format_and_size (v4l2object,
            pixelformat, w, h, template);

        if (tmp)
          results = g_list_prepend (results, tmp);
      }

      size.index++;
    } while (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMESIZES, &size) >= 0);
    GST_DEBUG_OBJECT (v4l2object->element,
        "done iterating discrete frame sizes");
  } else if (size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
    GST_DEBUG_OBJECT (v4l2object->element, "we have stepwise frame sizes:");
    GST_DEBUG_OBJECT (v4l2object->element, "min width:   %d",
        size.stepwise.min_width);
    GST_DEBUG_OBJECT (v4l2object->element, "min height:  %d",
        size.stepwise.min_height);
    GST_DEBUG_OBJECT (v4l2object->element, "max width:   %d",
        size.stepwise.max_width);
    GST_DEBUG_OBJECT (v4l2object->element, "min height:  %d",
        size.stepwise.max_height);
    GST_DEBUG_OBJECT (v4l2object->element, "step width:  %d",
        size.stepwise.step_width);
    GST_DEBUG_OBJECT (v4l2object->element, "step height: %d",
        size.stepwise.step_height);

    for (w = size.stepwise.min_width, h = size.stepwise.min_height;
        w <= size.stepwise.max_width && h <= size.stepwise.max_height;
        w += size.stepwise.step_width, h += size.stepwise.step_height) {
      if (w == 0 || h == 0)
        continue;

      tmp =
          gst_v4l2_object_probe_caps_for_format_and_size (v4l2object,
          pixelformat, w, h, template);

      if (tmp)
        results = g_list_prepend (results, tmp);
    }
    GST_DEBUG_OBJECT (v4l2object->element,
        "done iterating stepwise frame sizes");
  } else if (size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
    guint32 maxw, maxh;

    GST_DEBUG_OBJECT (v4l2object->element, "we have continuous frame sizes:");
    GST_DEBUG_OBJECT (v4l2object->element, "min width:   %d",
        size.stepwise.min_width);
    GST_DEBUG_OBJECT (v4l2object->element, "min height:  %d",
        size.stepwise.min_height);
    GST_DEBUG_OBJECT (v4l2object->element, "max width:   %d",
        size.stepwise.max_width);
    GST_DEBUG_OBJECT (v4l2object->element, "min height:  %d",
        size.stepwise.max_height);

    w = MAX (size.stepwise.min_width, 1);
    h = MAX (size.stepwise.min_height, 1);
    maxw = MIN (size.stepwise.max_width, G_MAXINT);
    maxh = MIN (size.stepwise.max_height, G_MAXINT);

    tmp =
        gst_v4l2_object_probe_caps_for_format_and_size (v4l2object, pixelformat,
        w, h, template);
    if (tmp) {
      gst_structure_set (tmp, "width", GST_TYPE_INT_RANGE, (gint) w,
          (gint) maxw, "height", GST_TYPE_INT_RANGE, (gint) h, (gint) maxh,
          NULL);

      /* no point using the results list here, since there's only one struct */
      gst_v4l2_object_update_and_append (v4l2object, pixelformat, ret, tmp);
    }
  } else {
    goto unknown_type;
  }

  /* we use an intermediary list to store and then sort the results of the
   * probing because we can't make any assumptions about the order in which
   * the driver will give us the sizes, but we want the final caps to contain
   * the results starting with the highest resolution and having the lowest
   * resolution last, since order in caps matters for things like fixation. */
  results = g_list_sort (results, (GCompareFunc) sort_by_frame_size);
  while (results != NULL) {
    gst_v4l2_object_update_and_append (v4l2object, pixelformat, ret,
        results->data);
    results = g_list_delete_link (results, results);
  }

  if (gst_caps_is_empty (ret))
    goto enum_framesizes_no_results;

  return ret;

  /* ERRORS */
enum_framesizes_failed:
  {
    /* I don't see how this is actually an error */
    GST_DEBUG_OBJECT (v4l2object->element,
        "Failed to enumerate frame sizes for pixelformat %" GST_FOURCC_FORMAT
        " (%s)", GST_FOURCC_ARGS (pixelformat), g_strerror (errno));
    goto default_frame_sizes;
  }
enum_framesizes_no_results:
  {
    /* it's possible that VIDIOC_ENUM_FRAMESIZES is defined but the driver in
     * question doesn't actually support it yet */
    GST_DEBUG_OBJECT (v4l2object->element,
        "No results for pixelformat %" GST_FOURCC_FORMAT
        " enumerating frame sizes, trying fallback",
        GST_FOURCC_ARGS (pixelformat));
    goto default_frame_sizes;
  }
unknown_type:
  {
    GST_WARNING_OBJECT (v4l2object->element,
        "Unknown frame sizeenum type for pixelformat %" GST_FOURCC_FORMAT
        ": %u", GST_FOURCC_ARGS (pixelformat), size.type);
    goto default_frame_sizes;
  }

default_frame_sizes:
  {
    gint min_w, max_w, min_h, max_h, fix_num = 0, fix_denom = 0;
    gboolean interlaced;

    /* This code is for Linux < 2.6.19 */
    min_w = min_h = 1;
    max_w = max_h = GST_V4L2_MAX_SIZE;
    if (!gst_v4l2_object_get_nearest_size (v4l2object, pixelformat, &min_w,
            &min_h, &interlaced)) {
      GST_WARNING_OBJECT (v4l2object->element,
          "Could not probe minimum capture size for pixelformat %"
          GST_FOURCC_FORMAT, GST_FOURCC_ARGS (pixelformat));
    }
    if (!gst_v4l2_object_get_nearest_size (v4l2object, pixelformat, &max_w,
            &max_h, &interlaced)) {
      GST_WARNING_OBJECT (v4l2object->element,
          "Could not probe maximum capture size for pixelformat %"
          GST_FOURCC_FORMAT, GST_FOURCC_ARGS (pixelformat));
    }

    /* Since we can't get framerate directly, try to use the current norm */
    if (v4l2object->tv_norm && v4l2object->norms) {
      GList *norms;
      GstTunerNorm *norm = NULL;
      GstTunerNorm *current =
          gst_v4l2_tuner_get_norm_by_std_id (v4l2object, v4l2object->tv_norm);

      for (norms = v4l2object->norms; norms != NULL; norms = norms->next) {
        norm = (GstTunerNorm *) norms->data;
        if (!strcmp (norm->label, current->label))
          break;
      }
      /* If it's possible, set framerate to that (discrete) value */
      if (norm) {
        fix_num = gst_value_get_fraction_numerator (&norm->framerate);
        fix_denom = gst_value_get_fraction_denominator (&norm->framerate);
      }
    }

    tmp = gst_structure_copy (template);
    if (fix_num) {
      gst_structure_set (tmp, "framerate", GST_TYPE_FRACTION, fix_num,
          fix_denom, NULL);
    } else if (v4l2object->type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
        v4l2object->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
      /* if norm can't be used, copy the template framerate */
      gst_structure_set (tmp, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1,
          100, 1, NULL);
    }

    if (min_w == max_w)
      gst_structure_set (tmp, "width", G_TYPE_INT, max_w, NULL);
    else
      gst_structure_set (tmp, "width", GST_TYPE_INT_RANGE, min_w, max_w, NULL);

    if (min_h == max_h)
      gst_structure_set (tmp, "height", G_TYPE_INT, max_h, NULL);
    else
      gst_structure_set (tmp, "height", GST_TYPE_INT_RANGE, min_h, max_h, NULL);

    if (g_str_equal (gst_structure_get_name (tmp), "video/x-raw"))
      gst_structure_set (tmp, "interlace-mode", G_TYPE_STRING,
          (interlaced ? "mixed" : "progressive"), NULL);
    gst_v4l2_object_add_aspect_ratio (v4l2object, tmp);

    gst_v4l2_object_update_and_append (v4l2object, pixelformat, ret, tmp);
    return ret;
  }
}

static gboolean
gst_v4l2_object_get_nearest_size (GstV4l2Object * v4l2object,
    guint32 pixelformat, gint * width, gint * height, gboolean * interlaced)
{
  struct v4l2_format fmt, prevfmt;
  int fd;
  int r;
  int prevfmt_valid = FALSE;
  gboolean ret = FALSE;

  g_return_val_if_fail (width != NULL, FALSE);
  g_return_val_if_fail (height != NULL, FALSE);

  GST_LOG_OBJECT (v4l2object->element,
      "getting nearest size to %dx%d with format %" GST_FOURCC_FORMAT,
      *width, *height, GST_FOURCC_ARGS (pixelformat));

  fd = v4l2object->video_fd;

  memset (&fmt, 0, sizeof (struct v4l2_format));
  memset (&prevfmt, 0, sizeof (struct v4l2_format));

  /* Some drivers are buggy and will modify the currently set format
     when processing VIDIOC_TRY_FMT, so we remember what is set at the
     minute, and will reset it when done. */
  if (!v4l2object->no_initial_format) {
    prevfmt.type = v4l2object->type;
    prevfmt_valid = (v4l2_ioctl (fd, VIDIOC_G_FMT, &prevfmt) >= 0);
  }

  /* get size delimiters */
  memset (&fmt, 0, sizeof (fmt));
  fmt.type = v4l2object->type;
  fmt.fmt.pix.width = *width;
  fmt.fmt.pix.height = *height;
  fmt.fmt.pix.pixelformat = pixelformat;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;

  r = v4l2_ioctl (fd, VIDIOC_TRY_FMT, &fmt);
  if (r < 0 && errno == EINVAL) {
    /* try again with interlaced video */
    fmt.fmt.pix.width = *width;
    fmt.fmt.pix.height = *height;
    fmt.fmt.pix.pixelformat = pixelformat;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    r = v4l2_ioctl (fd, VIDIOC_TRY_FMT, &fmt);
  }

  if (r < 0) {
    /* The driver might not implement TRY_FMT, in which case we will try
       S_FMT to probe */
    if (errno != ENOTTY)
      goto error;

    /* Only try S_FMT if we're not actively capturing yet, which we shouldn't
       be, because we're still probing */
    if (GST_V4L2_IS_ACTIVE (v4l2object))
      goto error;

    GST_LOG_OBJECT (v4l2object->element,
        "Failed to probe size limit with VIDIOC_TRY_FMT, trying VIDIOC_S_FMT");

    fmt.fmt.pix.width = *width;
    fmt.fmt.pix.height = *height;

    r = v4l2_ioctl (fd, VIDIOC_S_FMT, &fmt);
    if (r < 0 && errno == EINVAL) {
      /* try again with progressive video */
      fmt.fmt.pix.width = *width;
      fmt.fmt.pix.height = *height;
      fmt.fmt.pix.pixelformat = pixelformat;
      fmt.fmt.pix.field = V4L2_FIELD_NONE;
      r = v4l2_ioctl (fd, VIDIOC_S_FMT, &fmt);
    }

    if (r < 0)
      goto error;
  }

  GST_LOG_OBJECT (v4l2object->element,
      "got nearest size %dx%d", fmt.fmt.pix.width, fmt.fmt.pix.height);

  *width = fmt.fmt.pix.width;
  *height = fmt.fmt.pix.height;

  switch (fmt.fmt.pix.field) {
    case V4L2_FIELD_ANY:
    case V4L2_FIELD_NONE:
      *interlaced = FALSE;
      break;
    case V4L2_FIELD_INTERLACED:
    case V4L2_FIELD_INTERLACED_TB:
    case V4L2_FIELD_INTERLACED_BT:
      *interlaced = TRUE;
      break;
    default:
      GST_WARNING_OBJECT (v4l2object->element,
          "Unsupported field type for %" GST_FOURCC_FORMAT "@%ux%u",
          GST_FOURCC_ARGS (pixelformat), *width, *height);
      goto error;
  }

  ret = TRUE;

error:
  if (!ret) {
    GST_WARNING_OBJECT (v4l2object->element,
        "Unable to try format: %s", g_strerror (errno));
  }
  if (prevfmt_valid)
    if (v4l2_ioctl (fd, VIDIOC_S_FMT, &prevfmt) < 0) {
      GST_WARNING_OBJECT (v4l2object->element,
          "Unable to restore format after trying format: %s",
          g_strerror (errno));
    }

  return ret;
}

static gboolean
gst_v4l2_object_setup_pool (GstV4l2Object * v4l2object, GstCaps * caps)
{
  GstV4l2IOMode mode;

  GST_DEBUG_OBJECT (v4l2object->element, "initializing the capture system");

  GST_V4L2_CHECK_OPEN (v4l2object);
  GST_V4L2_CHECK_NOT_ACTIVE (v4l2object);

  /* find transport */
  mode = v4l2object->req_mode;

  if (v4l2object->vcap.capabilities & V4L2_CAP_READWRITE) {
    if (v4l2object->req_mode == GST_V4L2_IO_AUTO)
      mode = GST_V4L2_IO_RW;
  } else if (v4l2object->req_mode == GST_V4L2_IO_RW)
    goto method_not_supported;

  if (v4l2object->vcap.capabilities & V4L2_CAP_STREAMING) {
    if (v4l2object->req_mode == GST_V4L2_IO_AUTO)
      mode = GST_V4L2_IO_MMAP;
  } else if (v4l2object->req_mode == GST_V4L2_IO_MMAP)
    goto method_not_supported;

  /* if still no transport selected, error out */
  if (mode == GST_V4L2_IO_AUTO)
    goto no_supported_capture_method;

  GST_INFO_OBJECT (v4l2object->element, "accessing buffers via mode %d", mode);
  v4l2object->mode = mode;

  /* Map the buffers */
  GST_LOG_OBJECT (v4l2object->element, "initiating buffer pool");

  if (!(v4l2object->pool = gst_v4l2_buffer_pool_new (v4l2object, caps)))
    goto buffer_pool_new_failed;

  GST_V4L2_SET_ACTIVE (v4l2object);

  return TRUE;

  /* ERRORS */
buffer_pool_new_failed:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, READ,
        (_("Could not map buffers from device '%s'"),
            v4l2object->videodev),
        ("Failed to create buffer pool: %s", g_strerror (errno)));
    return FALSE;
  }
method_not_supported:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, READ,
        (_("The driver of device '%s' does not support the IO method %d"),
            v4l2object->videodev, mode), (NULL));
    return FALSE;
  }
no_supported_capture_method:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, READ,
        (_("The driver of device '%s' does not support any known IO "
                "method."), v4l2object->videodev), (NULL));
    return FALSE;
  }
}

static void
gst_v4l2_object_save_format (GstV4l2Object * v4l2object,
    struct v4l2_fmtdesc *fmtdesc, struct v4l2_format *format,
    GstVideoInfo * info)
{
  const GstVideoFormatInfo *finfo = info->finfo;
  gint i;

  if (V4L2_TYPE_IS_MULTIPLANAR (v4l2object->type)) {
    /* figure out the frame layout */
    v4l2object->n_v4l2_planes = MAX (1, format->fmt.pix_mp.num_planes);
    v4l2object->sizeimage = 0;
    for (i = 0; i < format->fmt.pix_mp.num_planes; i++) {
      v4l2object->bytesperline[i] =
          format->fmt.pix_mp.plane_fmt[i].bytesperline;
      v4l2object->sizeimage += format->fmt.pix_mp.plane_fmt[i].sizeimage;
    }
  } else {
    /* only one plane in non-MPLANE mode */
    v4l2object->n_v4l2_planes = 1;

    /* figure out the frame layout */
    for (i = 0; i < finfo->n_planes; i++) {
      guint stride = format->fmt.pix.bytesperline;

      switch (finfo->format) {
        case GST_VIDEO_FORMAT_NV12:
        case GST_VIDEO_FORMAT_NV21:
        case GST_VIDEO_FORMAT_NV16:
        case GST_VIDEO_FORMAT_NV24:
          v4l2object->bytesperline[i] = (i == 0 ? 1 : 2) *
              GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (finfo, i, stride);
          break;
        default:
          v4l2object->bytesperline[i] =
              GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (finfo, i, stride);
          break;
      }

      GST_DEBUG_OBJECT (v4l2object->element,
          "Extrapolated stride for plane %d from %d to %d", i, stride,
          v4l2object->bytesperline[i]);
    }

    v4l2object->sizeimage = format->fmt.pix.sizeimage;
  }

  GST_DEBUG_OBJECT (v4l2object->element, "Got sizeimage %u",
      v4l2object->sizeimage);

  v4l2object->info = *info;
  v4l2object->fmtdesc = fmtdesc;

  /* if we have a framerate pre-calculate duration */
  if (info->fps_n > 0 && info->fps_d > 0) {
    v4l2object->duration = gst_util_uint64_scale_int (GST_SECOND, info->fps_d,
        info->fps_n);
  } else {
    v4l2object->duration = GST_CLOCK_TIME_NONE;
  }
}

gboolean
gst_v4l2_object_set_format (GstV4l2Object * v4l2object, GstCaps * caps)
{
  gint fd = v4l2object->video_fd;
  struct v4l2_format format;
  struct v4l2_streamparm streamparm;
  enum v4l2_field field;
  guint32 pixelformat;
  struct v4l2_fmtdesc *fmtdesc;
  GstVideoInfo info;
  gint width, height, fps_n, fps_d;
  gint i = 0;

  GST_V4L2_CHECK_OPEN (v4l2object);
  GST_V4L2_CHECK_NOT_ACTIVE (v4l2object);

  if (!gst_v4l2_object_get_caps_info (v4l2object, caps, &fmtdesc, &info))
    goto invalid_caps;

  pixelformat = fmtdesc->pixelformat;
  width = GST_VIDEO_INFO_WIDTH (&info);
  height = GST_VIDEO_INFO_HEIGHT (&info);
  fps_n = GST_VIDEO_INFO_FPS_N (&info);
  fps_d = GST_VIDEO_INFO_FPS_D (&info);

  /* get bytesperline for each plane */
  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&info); i++)
    v4l2object->bytesperline[i] = GST_VIDEO_INFO_PLANE_STRIDE (&info, i);

  if (GST_VIDEO_INFO_IS_INTERLACED (&info)) {
    GST_DEBUG_OBJECT (v4l2object->element, "interlaced video");
    /* ideally we would differentiate between types of interlaced video
     * but there is not sufficient information in the caps..
     */
    field = V4L2_FIELD_INTERLACED;
  } else {
    GST_DEBUG_OBJECT (v4l2object->element, "progressive video");
    field = V4L2_FIELD_NONE;
  }

  GST_DEBUG_OBJECT (v4l2object->element, "Desired format %dx%d, format "
      "%" GST_FOURCC_FORMAT " stride: %d", width, height,
      GST_FOURCC_ARGS (pixelformat), v4l2object->bytesperline[0]);

  memset (&format, 0x00, sizeof (struct v4l2_format));
  format.type = v4l2object->type;

  if (!v4l2object->no_initial_format) {
    if (v4l2_ioctl (fd, VIDIOC_G_FMT, &format) < 0)
      goto get_fmt_failed;
  }

  if (V4L2_TYPE_IS_MULTIPLANAR (v4l2object->type)) {
    GST_DEBUG_OBJECT (v4l2object->element, "Got format to %dx%d, format "
        "%" GST_FOURCC_FORMAT " colorspace %d, nb planes %d",
        format.fmt.pix_mp.width, format.fmt.pix_mp.height,
        GST_FOURCC_ARGS (format.fmt.pix.pixelformat),
        format.fmt.pix_mp.colorspace, format.fmt.pix_mp.num_planes);

    /* If no size in caps, use configured size */
    if (width == 0 && height == 0) {
      width = format.fmt.pix_mp.width;
      height = format.fmt.pix_mp.height;
    }

    if (format.type != v4l2object->type ||
        format.fmt.pix_mp.width != width ||
        format.fmt.pix_mp.height != height ||
        format.fmt.pix_mp.pixelformat != pixelformat ||
        format.fmt.pix_mp.field != field) {
      /* even in v4l2 multiplanar mode we can work in contiguous mode
       * if the device supports it */
      gint n_v4l_planes = GST_VIDEO_INFO_N_PLANES (&info);

      /* if encoded format (GST_VIDEO_INFO_N_PLANES return 0)
       * or if contiguous is prefered */
      if (!n_v4l_planes || !v4l2object->prefered_non_contiguous)
        n_v4l_planes = 1;

      /* something different, set the format */
      GST_DEBUG_OBJECT (v4l2object->element, "Setting format to %dx%d, format "
          "%" GST_FOURCC_FORMAT, width, height, GST_FOURCC_ARGS (pixelformat));

      format.type = v4l2object->type;
      format.fmt.pix_mp.pixelformat = pixelformat;
      format.fmt.pix_mp.width = width;
      format.fmt.pix_mp.height = height;
      format.fmt.pix_mp.field = field;
      format.fmt.pix_mp.num_planes = n_v4l_planes;
      /* try to ask our prefered stride but it's not a failure
       * if not accepted */
      for (i = 0; i < format.fmt.pix_mp.num_planes; i++)
        format.fmt.pix_mp.plane_fmt[i].bytesperline =
            v4l2object->bytesperline[i];

      if (GST_VIDEO_INFO_FORMAT (&info) == GST_VIDEO_FORMAT_ENCODED) {
        format.fmt.pix_mp.plane_fmt[0].sizeimage = ENCODED_BUFFER_SIZE;
      }

      if (v4l2_ioctl (fd, VIDIOC_S_FMT, &format) < 0)
        goto set_fmt_failed;

      GST_DEBUG_OBJECT (v4l2object->element, "Got format to %dx%d, format "
          "%" GST_FOURCC_FORMAT ", nb planes %d", format.fmt.pix.width,
          format.fmt.pix_mp.height,
          GST_FOURCC_ARGS (format.fmt.pix.pixelformat),
          format.fmt.pix_mp.num_planes);

#ifndef GST_DISABLE_GST_DEBUG
      for (i = 0; i < format.fmt.pix_mp.num_planes; i++)
        GST_DEBUG_OBJECT (v4l2object->element, "  stride %d",
            format.fmt.pix_mp.plane_fmt[i].bytesperline);
#endif

      if (format.fmt.pix_mp.pixelformat != pixelformat)
        goto invalid_pixelformat;

      /* we set the dimensions just in case but don't validate them afterwards
       * For some codecs the dimensions are *not* in the bitstream, IIRC VC1
       * in ASF mode for example. */
      if (info.finfo->format != GST_VIDEO_FORMAT_ENCODED) {
        if (format.fmt.pix_mp.width != width
            || format.fmt.pix_mp.height != height)
          goto invalid_dimensions;
      }

      if (format.fmt.pix_mp.num_planes != n_v4l_planes)
        goto invalid_planes;
    }

    /* figure out the frame layout */
    v4l2object->n_v4l2_planes = format.fmt.pix_mp.num_planes;
    v4l2object->sizeimage = 0;
    for (i = 0; i < format.fmt.pix_mp.num_planes; i++) {
      v4l2object->bytesperline[i] = format.fmt.pix_mp.plane_fmt[i].bytesperline;
      v4l2object->sizeimage += format.fmt.pix_mp.plane_fmt[i].sizeimage;
    }
  } else {
    GST_DEBUG_OBJECT (v4l2object->element, "Got format to %dx%d, format "
        "%" GST_FOURCC_FORMAT " bytesperline %d, colorspace %d",
        format.fmt.pix.width, format.fmt.pix.height,
        GST_FOURCC_ARGS (format.fmt.pix.pixelformat),
        format.fmt.pix.bytesperline, format.fmt.pix.colorspace);

    /* If no size in caps, use configured size */
    if (width == 0 && height == 0) {
      width = format.fmt.pix_mp.width;
      height = format.fmt.pix_mp.height;
    }

    if (format.type != v4l2object->type ||
        format.fmt.pix.width != width ||
        format.fmt.pix.height != height ||
        format.fmt.pix.pixelformat != pixelformat ||
        format.fmt.pix.field != field) {
      /* something different, set the format */
      GST_DEBUG_OBJECT (v4l2object->element, "Setting format to %dx%d, format "
          "%" GST_FOURCC_FORMAT " bytesperline %d", width, height,
          GST_FOURCC_ARGS (pixelformat), v4l2object->bytesperline[0]);

      format.type = v4l2object->type;
      format.fmt.pix.width = width;
      format.fmt.pix.height = height;
      format.fmt.pix.pixelformat = pixelformat;
      format.fmt.pix.field = field;
      /* try to ask our prefered stride */
      format.fmt.pix.bytesperline = v4l2object->bytesperline[0];

      if (GST_VIDEO_INFO_FORMAT (&info) == GST_VIDEO_FORMAT_ENCODED) {
        format.fmt.pix.sizeimage = ENCODED_BUFFER_SIZE;
      }

      if (v4l2_ioctl (fd, VIDIOC_S_FMT, &format) < 0)
        goto set_fmt_failed;

      GST_DEBUG_OBJECT (v4l2object->element, "Got format to %dx%d, format "
          "%" GST_FOURCC_FORMAT " stride %d", format.fmt.pix.width,
          format.fmt.pix.height, GST_FOURCC_ARGS (format.fmt.pix.pixelformat),
          format.fmt.pix.bytesperline);

      /* we set the dimensions just in case but don't validate them afterwards
       * For some codecs the dimensions are *not* in the bitstream, IIRC VC1
       * in ASF mode for example. */
      if (info.finfo->format != GST_VIDEO_FORMAT_ENCODED) {
        if (format.fmt.pix.width != width || format.fmt.pix.height != height)
          goto invalid_dimensions;
      }

      if (format.fmt.pix.pixelformat != pixelformat)
        goto invalid_pixelformat;
    }
  }

  /* Is there a reason we require the caller to always specify a framerate? */
  GST_DEBUG_OBJECT (v4l2object->element, "Desired framerate: %u/%u", fps_n,
      fps_d);

  memset (&streamparm, 0x00, sizeof (struct v4l2_streamparm));
  streamparm.type = v4l2object->type;

  if (v4l2_ioctl (fd, VIDIOC_G_PARM, &streamparm) < 0)
    goto get_parm_failed;

  GST_VIDEO_INFO_FPS_N (&info) =
      streamparm.parm.capture.timeperframe.denominator;
  GST_VIDEO_INFO_FPS_D (&info) = streamparm.parm.capture.timeperframe.numerator;

  if (v4l2object->type == V4L2_BUF_TYPE_VIDEO_CAPTURE
      || v4l2object->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
    GST_DEBUG_OBJECT (v4l2object->element, "Got framerate: %u/%u",
        streamparm.parm.capture.timeperframe.denominator,
        streamparm.parm.capture.timeperframe.numerator);

    /* We used to skip frame rate setup if the camera was already setup
     * with the requested frame rate. This breaks some cameras though,
     * causing them to not output data (several models of Thinkpad cameras
     * have this problem at least).
     * So, don't skip. */
    GST_LOG_OBJECT (v4l2object->element, "Setting framerate to %u/%u", fps_n,
        fps_d);
    /* We want to change the frame rate, so check whether we can. Some cheap USB
     * cameras don't have the capability */
    if ((streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) == 0) {
      GST_DEBUG_OBJECT (v4l2object->element,
          "Not setting framerate (not supported)");
      goto done;
    }

    /* Note: V4L2 wants the frame interval, we have the frame rate */
    streamparm.parm.capture.timeperframe.numerator = fps_d;
    streamparm.parm.capture.timeperframe.denominator = fps_n;

    /* some cheap USB cam's won't accept any change */
    if (v4l2_ioctl (fd, VIDIOC_S_PARM, &streamparm) < 0)
      goto set_parm_failed;

    /* get new values */
    fps_d = streamparm.parm.capture.timeperframe.numerator;
    fps_n = streamparm.parm.capture.timeperframe.denominator;

    GST_INFO_OBJECT (v4l2object->element, "Set framerate to %u/%u", fps_n,
        fps_d);

    GST_VIDEO_INFO_FPS_N (&info) = fps_n;
    GST_VIDEO_INFO_FPS_D (&info) = fps_d;
  }

done:
  gst_v4l2_object_save_format (v4l2object, fmtdesc, &format, &info);

  /* now configure ther pools */
  if (!gst_v4l2_object_setup_pool (v4l2object, caps))
    goto pool_failed;

  return TRUE;

  /* ERRORS */
invalid_caps:
  {
    GST_DEBUG_OBJECT (v4l2object->element, "can't parse caps %" GST_PTR_FORMAT,
        caps);
    return FALSE;
  }
get_fmt_failed:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, SETTINGS,
        (_("Device '%s' does not support video capture"),
            v4l2object->videodev),
        ("Call to G_FMT failed: (%s)", g_strerror (errno)));
    return FALSE;
  }
set_fmt_failed:
  {
    if (errno == EBUSY) {
      GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, BUSY,
          (_("Device '%s' is busy"), v4l2object->videodev),
          ("Call to S_FMT failed for %" GST_FOURCC_FORMAT " @ %dx%d: %s",
              GST_FOURCC_ARGS (pixelformat), width, height,
              g_strerror (errno)));
    } else {
      GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, SETTINGS,
          (_("Device '%s' cannot capture at %dx%d"),
              v4l2object->videodev, width, height),
          ("Call to S_FMT failed for %" GST_FOURCC_FORMAT " @ %dx%d: %s",
              GST_FOURCC_ARGS (pixelformat), width, height,
              g_strerror (errno)));
    }
    return FALSE;
  }
invalid_dimensions:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, SETTINGS,
        (_("Device '%s' cannot capture at %dx%d"),
            v4l2object->videodev, width, height),
        ("Tried to capture at %dx%d, but device returned size %dx%d",
            width, height, format.fmt.pix.width, format.fmt.pix.height));
    return FALSE;
  }
invalid_pixelformat:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, SETTINGS,
        (_("Device '%s' cannot capture in the specified format"),
            v4l2object->videodev),
        ("Tried to capture in %" GST_FOURCC_FORMAT
            ", but device returned format" " %" GST_FOURCC_FORMAT,
            GST_FOURCC_ARGS (pixelformat),
            GST_FOURCC_ARGS (format.fmt.pix.pixelformat)));
    return FALSE;
  }
invalid_planes:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, SETTINGS,
        (_("Device '%s' does support non-contiguous planes"),
            v4l2object->videodev),
        ("Device wants %d planes", format.fmt.pix_mp.num_planes));
    return FALSE;
  }
get_parm_failed:
  {
    /* it's possible that this call is not supported */
    if (errno != EINVAL && errno != ENOTTY) {
      GST_ELEMENT_WARNING (v4l2object->element, RESOURCE, SETTINGS,
          (_("Could not get parameters on device '%s'"),
              v4l2object->videodev), GST_ERROR_SYSTEM);
    }
    goto done;
  }
set_parm_failed:
  {
    GST_ELEMENT_WARNING (v4l2object->element, RESOURCE, SETTINGS,
        (_("Video device did not accept new frame rate setting.")),
        GST_ERROR_SYSTEM);
    goto done;
  }
pool_failed:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, SETTINGS,
        (_("Video device could not create buffer pool.")), GST_ERROR_SYSTEM);
    return FALSE;
  }
}

/**
 * gst_v4l2_object_setup_format:
 * @v4l2object the object
 * @info a GstVideoInfo to be filled
 * @align a GstVideoAlignment to be filled
 *
 * Setup the format base on the currently configured format. This is useful in
 * decoder or encoder elements where the output format is dictated by the
 * input.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 */
gboolean
gst_v4l2_object_setup_format (GstV4l2Object * v4l2object,
    GstVideoInfo * info, GstVideoAlignment * align)
{
  struct v4l2_fmtdesc *fmtdesc;
  struct v4l2_format fmt;
  struct v4l2_crop crop;
  GstVideoFormat format;
  guint width, height;

  gst_video_info_init (info);
  gst_video_alignment_reset (align);

  memset (&fmt, 0x00, sizeof (struct v4l2_format));
  fmt.type = v4l2object->type;
  if (v4l2_ioctl (v4l2object->video_fd, VIDIOC_G_FMT, &fmt) < 0)
    goto get_fmt_failed;

  fmtdesc = gst_v4l2_object_get_format_from_fourcc (v4l2object,
      fmt.fmt.pix.pixelformat);
  if (fmtdesc == NULL)
    goto unsupported_format;

  /* No need to care about mplane, the four first params are the same */
  format = gst_v4l2_object_v4l2fourcc_to_video_format (fmt.fmt.pix.pixelformat);

  /* FIXME do more work in the whole function if
   * format is GST_VIDEO_FORMAT_ENCODED
   * Also gst_v4l2_object_v4l2fourcc_to_video_format should be improved
   * because for now it never returns GST_VIDEO_FORMAT_ENCODED
   */

  /* fails if we do no translate the fmt.pix.pixelformat to GstVideoFormat */
  if (format == GST_VIDEO_FORMAT_UNKNOWN)
    goto unsupported_format;

  if (fmt.fmt.pix.width == 0 || fmt.fmt.pix.height == 0)
    goto invalid_dimensions;

  width = fmt.fmt.pix.width;
  height = fmt.fmt.pix.height;

  memset (&crop, 0, sizeof (struct v4l2_crop));
  crop.type = v4l2object->type;
  if (v4l2_ioctl (v4l2object->video_fd, VIDIOC_G_CROP, &crop) >= 0) {
    align->padding_left = crop.c.left;
    align->padding_top = crop.c.top;
    align->padding_right = width - crop.c.width - crop.c.left;
    align->padding_bottom = height - crop.c.height - crop.c.top;
    width = crop.c.width;
    height = crop.c.height;
  }

  gst_video_info_set_format (info, format, width, height);

  switch (fmt.fmt.pix.field) {
    case V4L2_FIELD_ANY:
    case V4L2_FIELD_NONE:
      info->interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
      break;
    case V4L2_FIELD_INTERLACED:
    case V4L2_FIELD_INTERLACED_TB:
    case V4L2_FIELD_INTERLACED_BT:
      info->interlace_mode = GST_VIDEO_INTERLACE_MODE_INTERLEAVED;
      break;
    default:
      goto unsupported_field;
  }

  gst_v4l2_object_save_format (v4l2object, fmtdesc, &fmt, info);

  /* Shall we setup the pool ? */

  return TRUE;

get_fmt_failed:
  {
    GST_ELEMENT_WARNING (v4l2object->element, RESOURCE, SETTINGS,
        (_("Video device did not provide output format.")), GST_ERROR_SYSTEM);
    return FALSE;
  }
invalid_dimensions:
  {
    GST_ELEMENT_WARNING (v4l2object->element, RESOURCE, SETTINGS,
        (_("Video device returned invalid dimensions.")),
        ("Expected non 0 dimensions, got %dx%d", fmt.fmt.pix.width,
            fmt.fmt.pix.height));
    return FALSE;
  }
unsupported_field:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, SETTINGS,
        (_("Video devices uses an unsupported interlacing method.")),
        ("V4L2 field type %d not supported", fmt.fmt.pix.field));
    return FALSE;
  }
unsupported_format:
  {
    GST_ELEMENT_ERROR (v4l2object->element, RESOURCE, SETTINGS,
        (_("Video devices uses an unsupported pixel format.")),
        ("V4L2 format %" GST_FOURCC_FORMAT " not supported",
            GST_FOURCC_ARGS (fmt.fmt.pix.pixelformat)));
    return FALSE;
  }
}

gboolean
gst_v4l2_object_caps_equal (GstV4l2Object * v4l2object, GstCaps * caps)
{
  GstStructure *s;
  GstCaps *oldcaps;

  if (!v4l2object->pool)
    return FALSE;

  s = gst_buffer_pool_get_config (GST_BUFFER_POOL_CAST (v4l2object->pool));
  gst_buffer_pool_config_get_params (s, &oldcaps, NULL, NULL, NULL);

  return oldcaps && gst_caps_is_equal (caps, oldcaps);
}

gboolean
gst_v4l2_object_unlock (GstV4l2Object * v4l2object)
{
  GST_LOG_OBJECT (v4l2object->element, "flush poll");
  gst_poll_set_flushing (v4l2object->poll, TRUE);

  return TRUE;
}

gboolean
gst_v4l2_object_unlock_stop (GstV4l2Object * v4l2object)
{
  GST_LOG_OBJECT (v4l2object->element, "flush stop poll");
  gst_poll_set_flushing (v4l2object->poll, FALSE);

  return TRUE;
}

gboolean
gst_v4l2_object_stop (GstV4l2Object * v4l2object)
{
  GST_DEBUG_OBJECT (v4l2object->element, "stopping");

  if (!GST_V4L2_IS_OPEN (v4l2object))
    goto done;
  if (!GST_V4L2_IS_ACTIVE (v4l2object))
    goto done;

  if (v4l2object->pool) {
    GST_DEBUG_OBJECT (v4l2object->element, "deactivating pool");
    gst_buffer_pool_set_active (GST_BUFFER_POOL_CAST (v4l2object->pool), FALSE);
    gst_object_unref (v4l2object->pool);
    v4l2object->pool = NULL;
  }

  GST_V4L2_SET_INACTIVE (v4l2object);

done:
  return TRUE;
}

gboolean
gst_v4l2_object_copy (GstV4l2Object * v4l2object, GstBuffer * dest,
    GstBuffer * src)
{
  const GstVideoFormatInfo *finfo = v4l2object->info.finfo;

  if (finfo && (finfo->format != GST_VIDEO_FORMAT_UNKNOWN &&
          finfo->format != GST_VIDEO_FORMAT_ENCODED)) {
    GstVideoFrame src_frame, dest_frame;

    GST_DEBUG_OBJECT (v4l2object->element, "copy video frame");

    /* we have raw video, use videoframe copy to get strides right */
    if (!gst_video_frame_map (&src_frame, &v4l2object->info, src, GST_MAP_READ))
      goto invalid_buffer;

    if (!gst_video_frame_map (&dest_frame, &v4l2object->info, dest,
            GST_MAP_WRITE)) {
      gst_video_frame_unmap (&src_frame);
      goto invalid_buffer;
    }

    gst_video_frame_copy (&dest_frame, &src_frame);

    gst_video_frame_unmap (&src_frame);
    gst_video_frame_unmap (&dest_frame);
  } else {
    GstMapInfo map;

    GST_DEBUG_OBJECT (v4l2object->element, "copy raw bytes");
    gst_buffer_map (src, &map, GST_MAP_READ);
    gst_buffer_fill (dest, 0, map.data, gst_buffer_get_size (src));
    gst_buffer_unmap (src, &map);
    gst_buffer_resize (dest, 0, gst_buffer_get_size (src));
  }
  GST_CAT_LOG_OBJECT (GST_CAT_PERFORMANCE, v4l2object->element,
      "slow copy into buffer %p", dest);

  return TRUE;

  /* ERRORS */
invalid_buffer:
  {
    /* No Window available to put our image into */
    GST_WARNING_OBJECT (v4l2object->element, "could not map image");
    return FALSE;
  }
}

GstCaps *
gst_v4l2_object_get_caps (GstV4l2Object * v4l2object, GstCaps * filter)
{
  GstCaps *ret;
  GSList *walk;
  GSList *formats;

  if (v4l2object->probed_caps == NULL) {
    formats = gst_v4l2_object_get_format_list (v4l2object);

    ret = gst_caps_new_empty ();

    for (walk = formats; walk; walk = walk->next) {
      struct v4l2_fmtdesc *format;
      GstStructure *template;

      format = (struct v4l2_fmtdesc *) walk->data;

      template = gst_v4l2_object_v4l2fourcc_to_structure (format->pixelformat);

      if (template) {
        GstCaps *tmp;

        tmp = gst_v4l2_object_probe_caps_for_format (v4l2object,
            format->pixelformat, template);
        if (tmp)
          gst_caps_append (ret, tmp);

        gst_structure_free (template);
      } else {
        GST_DEBUG_OBJECT (v4l2object->element, "unknown format %u",
            format->pixelformat);
      }
    }
    v4l2object->probed_caps = ret;
  }

  if (filter) {
    ret = gst_caps_intersect_full (filter, v4l2object->probed_caps,
        GST_CAPS_INTERSECT_FIRST);
  } else {
    ret = gst_caps_ref (v4l2object->probed_caps);
  }

  GST_INFO_OBJECT (v4l2object->element, "probed caps: %" GST_PTR_FORMAT, ret);
  LOG_CAPS (v4l2object->element, ret);

  return ret;
}

gboolean
gst_v4l2_object_decide_allocation (GstV4l2Object * obj, GstQuery * query)
{
  GstBufferPool *pool;
  guint size, min, max;
  gboolean update;
  struct v4l2_control ctl = { 0, };

  GST_DEBUG_OBJECT (obj->element, "decide allocation");

  g_return_val_if_fail (obj->type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      obj->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, FALSE);

  if (obj->pool == NULL) {
    GstCaps *caps;
    gst_query_parse_allocation (query, &caps, NULL);

    if (!gst_v4l2_object_setup_pool (obj, caps))
      goto pool_failed;
  }

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    update = TRUE;
  } else {
    pool = NULL;
    min = max = 0;
    size = 0;
    update = FALSE;
  }

  GST_DEBUG_OBJECT (obj->element, "allocation: size:%u min:%u max:%u pool:%"
      GST_PTR_FORMAT, size, min, max, pool);

  if (min != 0) {
    /* if there is a min-buffers suggestion, use it. We add 1 because we need 1
     * buffer extra to capture while the other two buffers are downstream */
    min += 1;
  } else {
    min = 2;
  }

  /* Certain driver may expose a minimum through controls */
  ctl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
  if (v4l2_ioctl (obj->video_fd, VIDIOC_G_CTRL, &ctl) >= 0) {
    GST_DEBUG_OBJECT (obj->element, "driver require a minimum of %d buffers",
        ctl.value);
    obj->min_buffers_for_capture = ctl.value;
    min += ctl.value;
  }

  /* Request a bigger max, if one was suggested but it's too small */
  if (max != 0 && max < min)
    max = min;

  /* select a pool */
  switch (obj->mode) {
    case GST_V4L2_IO_RW:
      if (pool == NULL) {
        /* no downstream pool, use our own then */
        GST_DEBUG_OBJECT (obj->element,
            "read/write mode: no downstream pool, using our own");
        pool = GST_BUFFER_POOL_CAST (obj->pool);
        size = obj->sizeimage;
      } else {
        /* in READ/WRITE mode, prefer a downstream pool because our own pool
         * doesn't help much, we have to write to it as well */
        GST_DEBUG_OBJECT (obj->element,
            "read/write mode: using downstream pool");
        /* use the bigest size, when we use our own pool we can't really do any
         * other size than what the hardware gives us but for downstream pools
         * we can try */
        size = MAX (size, obj->sizeimage);
      }
      break;
    case GST_V4L2_IO_MMAP:
    case GST_V4L2_IO_USERPTR:
    case GST_V4L2_IO_DMABUF:
      /* in streaming mode, prefer our own pool */
      if (pool)
        gst_object_unref (pool);
      pool = GST_BUFFER_POOL_CAST (obj->pool);
      size = obj->sizeimage;
      max = 0;
      GST_DEBUG_OBJECT (obj->element,
          "streaming mode: using our own pool %" GST_PTR_FORMAT, pool);
      break;
    case GST_V4L2_IO_AUTO:
    default:
      GST_WARNING_OBJECT (obj->element, "unhandled mode");
      break;
  }

  if (pool) {
    GstStructure *config;
    GstCaps *caps;

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &caps, NULL, NULL, NULL);
    gst_buffer_pool_config_set_params (config, caps, size, min, max);

    /* if downstream supports video metadata, add this to the pool config */
    if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
      GST_DEBUG_OBJECT (pool, "activate Video Meta");
      gst_buffer_pool_config_add_option (config,
          GST_BUFFER_POOL_OPTION_VIDEO_META);
    }

    gst_buffer_pool_set_config (pool, config);
  }

  if (update)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  return TRUE;

pool_failed:
  {
    GST_ELEMENT_ERROR (obj->element, RESOURCE, SETTINGS,
        (_("Video device could not create buffer pool.")), GST_ERROR_SYSTEM);
    return FALSE;
  }
}
