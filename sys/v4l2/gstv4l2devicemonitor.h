/* GStreamer
 * Copyright (C) 2012 Olivier Crete <olivier.crete@collabora.com>
 *
 * gstv4l2devicemonitor.h: V4l2 device probing and monitoring
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __GST_V4L2_DEVICE_MONITOR_H__
#define __GST_V4L2_DEVICE_MONITOR_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#ifdef HAVE_GUDEV
#include <gudev/gudev.h>
#endif

G_BEGIN_DECLS

typedef struct _GstV4l2DeviceMonitor GstV4l2DeviceMonitor;
typedef struct _GstV4l2DeviceMonitorPrivate GstV4l2DeviceMonitorPrivate;
typedef struct _GstV4l2DeviceMonitorClass GstV4l2DeviceMonitorClass;

#define GST_TYPE_V4L2_DEVICE_MONITOR                 (gst_v4l2_device_monitor_get_type())
#define GST_IS_V4L2_DEVICE_MONITOR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_V4L2_DEVICE_MONITOR))
#define GST_IS_V4L2_DEVICE_MONITOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_V4L2_DEVICE_MONITOR))
#define GST_V4L2_DEVICE_MONITOR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_V4L2_DEVICE_MONITOR, GstV4l2DeviceMonitorClass))
#define GST_V4L2_DEVICE_MONITOR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_V4L2_DEVICE_MONITOR, GstV4l2DeviceMonitor))
#define GST_V4L2_DEVICE_MONITOR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE_MONITOR, GstV4l2DeviceMonitorClass))
#define GST_V4L2_DEVICE_MONITOR_CAST(obj)            ((GstV4l2DeviceMonitor *)(obj))

struct _GstV4l2DeviceMonitor {
  GstDeviceMonitor         parent;

#ifdef HAVE_GUDEV
  GMainContext *context;
  GMainLoop *loop;
  GThread *thread;
  gboolean started;
  GCond started_cond;
#endif
};

typedef enum {
  GST_V4L2_DEVICE_TYPE_INVALID = 0,
  GST_V4L2_DEVICE_TYPE_SOURCE,
  GST_V4L2_DEVICE_TYPE_SINK
} GstV4l2DeviceType;

struct _GstV4l2DeviceMonitorClass {
  GstDeviceMonitorClass    parent_class;
};

GType        gst_v4l2_device_monitor_get_type (void);


typedef struct _GstV4l2Device GstV4l2Device;
typedef struct _GstV4l2DevicePrivate GstV4l2DevicePrivate;
typedef struct _GstV4l2DeviceClass GstV4l2DeviceClass;

#define GST_TYPE_V4L2_DEVICE                 (gst_v4l2_device_get_type())
#define GST_IS_V4L2_DEVICE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_V4L2_DEVICE))
#define GST_IS_V4L2_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_V4L2_DEVICE))
#define GST_V4L2_DEVICE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_V4L2_DEVICE, GstV4l2DeviceClass))
#define GST_V4L2_DEVICE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_V4L2_DEVICE, GstV4l2Device))
#define GST_V4L2_DEVICE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE, GstV4l2DeviceClass))
#define GST_V4L2_DEVICE_CAST(obj)            ((GstV4l2Device *)(obj))

struct _GstV4l2Device {
  GstDevice         parent;

  gchar            *device_path;
  gchar            *syspath;
  const gchar      *element;
};

struct _GstV4l2DeviceClass {
  GstDeviceClass    parent_class;
};

GType        gst_v4l2_device_get_type (void);

#endif /* __GST_V4L2_DEVICE_MONITOR_H__ */
