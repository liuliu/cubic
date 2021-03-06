/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>
#include "freenect_internal.h"

int fnusb_num_devices(fnusb_ctx *ctx)
{
	libusb_device **devs; 
	//pointer to pointer of device, used to retrieve a list of devices	
	ssize_t cnt = libusb_get_device_list (ctx->ctx, &devs); 
	//get the list of devices	
	if (cnt < 0)
		return (-1);
	int nr = 0, i = 0;
	struct libusb_device_descriptor desc;
	for (i = 0; i < cnt; ++i)
	{
		int r = libusb_get_device_descriptor (devs[i], &desc);
		if (r < 0)
			continue;
		if (desc.idVendor == VID_MICROSOFT && desc.idProduct == PID_NUI_CAMERA)
			nr++;
	}
	libusb_free_device_list (devs, 1);
	// free the list, unref the devices in it
	return nr;
}

int fnusb_list_device_attributes(fnusb_ctx *ctx, struct freenect_device_attributes** attribute_list)
{
	*attribute_list = NULL; // initialize some return value in case the user is careless.
	libusb_device **devs;
	//pointer to pointer of device, used to retrieve a list of devices
	ssize_t count = libusb_get_device_list (ctx->ctx, &devs);
	if (count < 0)
		return -1;

	struct freenect_device_attributes** camera_prev_next = attribute_list;

	// Pass over the list.  For each camera seen, if we already have a camera
	// for the newest_camera device, allocate a new one and append it to the list,
	// incrementing num_devs.  Likewise for each audio device.
	struct libusb_device_descriptor desc;
	int num_cams = 0;
	int i;
	for (i = 0; i < count; i++)
	{
		int r = libusb_get_device_descriptor (devs[i], &desc);
		if (r < 0)
			continue;
		if (desc.idVendor == VID_MICROSOFT && desc.idProduct == PID_NUI_CAMERA)
		{
			// Verify that a serial number exists to query.  If not, don't touch the device.
			if (desc.iSerialNumber == 0)
				continue;

			// Open device.
			int res;
			libusb_device_handle *this_device;
			res = libusb_open(devs[i], &this_device);
			unsigned char string_desc[256]; // String descriptors are at most 256 bytes.
			if (res != 0)
				continue;

			// Read string descriptor referring to serial number.
			res = libusb_get_string_descriptor_ascii(this_device, desc.iSerialNumber, string_desc, 256);
			libusb_close(this_device);
			if (res < 0)
				continue;

			// Add item to linked list.
			struct freenect_device_attributes* new_dev_attrs = (struct freenect_device_attributes*)malloc(sizeof(struct freenect_device_attributes));
			memset(new_dev_attrs, 0, sizeof(*new_dev_attrs));

			*camera_prev_next = new_dev_attrs;
			// Copy string with serial number
			new_dev_attrs->camera_serial = strdup((char*)string_desc);
			camera_prev_next = &(new_dev_attrs->next);
			// Increment number of cameras found
			num_cams++;
		}
	}

	libusb_free_device_list(devs, 1);
	return num_cams;
}

int fnusb_init(fnusb_ctx *ctx, freenect_usb_context *usb_ctx)
{
	int res;
	if (!usb_ctx)
	{
		res = libusb_init(&ctx->ctx);
		if (res >= 0)
		{
			ctx->should_free_ctx = 1;
			return 0;
		} else {
			ctx->should_free_ctx = 0;
			ctx->ctx = NULL;
			return res;
		}
	} else {
    // explicit cast required: in WIN32, freenect_usb_context* maps to void*
		ctx->ctx = (libusb_context*)usb_ctx;
		ctx->should_free_ctx = 0;
		return 0;
	}
}

int fnusb_shutdown(fnusb_ctx *ctx)
{
	//int res;
	if (ctx->should_free_ctx)
	{
		libusb_exit(ctx->ctx);
		ctx->ctx = NULL;
	}
	return 0;
}

int fnusb_process_events(fnusb_ctx *ctx)
{
	return libusb_handle_events(ctx->ctx);
}

int fnusb_process_events_timeout(fnusb_ctx *ctx, struct timeval* timeout)
{
	return libusb_handle_events_timeout(ctx->ctx, timeout);
}

int fnusb_open_subdevices(freenect_device *dev, int index)
{
	freenect_context *ctx = dev->parent;

	dev->usb_cam.parent = dev;
	dev->usb_cam.dev = NULL;
	dev->usb_motor.parent = dev;
	dev->usb_motor.dev = NULL;

	libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
	ssize_t cnt = libusb_get_device_list(dev->parent->usb.ctx, &devs); //get the list of devices
	if (cnt < 0)
		return -1;

	int i = 0, nr_ms_dev = 0, nr_cam = 0, nr_mot = cnt, mot_idx = cnt;
	int res;
	struct libusb_device_descriptor desc;
	dev->hwrev = HWREV_K4W_0;

	for (i = 0; i < cnt; i++)
	{
		int r = libusb_get_device_descriptor(devs[i], &desc);
		if (r < 0)
			continue;

		if (desc.idVendor != VID_MICROSOFT)
			continue;

		// new Kinect doesn't have separate motor interface anymore, this gives us clue which version of Kinect we are using
		if (desc.idProduct == PID_NUI_MOTOR)
			mot_idx = i, nr_mot = nr_ms_dev;

		// Search for the camera
		if ((ctx->enabled_subdevices & FREENECT_DEVICE_CAMERA) && !dev->usb_cam.dev && desc.idProduct == PID_NUI_CAMERA)
		{
			// If the index given by the user matches our camera index
			if (nr_cam == index)
			{
				// basically, if the device immediately follows motor is the camera, then it is a Xbox 360 Kinect
				if (nr_ms_dev == nr_mot + 1)
					dev->hwrev = HWREV_XBOX360_0;
				res = libusb_open(devs[i], &dev->usb_cam.dev);
				if (res < 0 || !dev->usb_cam.dev)
				{
					FN_ERROR("Could not open camera: %d\n", res);
					dev->usb_cam.dev = NULL;
					break;
				}
				// Detach an existing kernel driver for the device
				res = libusb_kernel_driver_active(dev->usb_cam.dev, 0);
				if (res == 1)
				{
					res = libusb_detach_kernel_driver(dev->usb_cam.dev, 0);
					if (res < 0)
					{
						FN_ERROR("Could not detach kernel driver for camera: %d\n", res);
						libusb_close(dev->usb_cam.dev);
						dev->usb_cam.dev = NULL;
						break;
					}
				}
				res = libusb_claim_interface(dev->usb_cam.dev, 0);
				if (res < 0)
				{
					FN_ERROR("Could not claim interface on camera: %d\n", res);
					libusb_close(dev->usb_cam.dev);
					dev->usb_cam.dev = NULL;
					break;
				}
				// Open for the motor
				// the device immediately before camera is the motor on Xbox 360
				if ((ctx->enabled_subdevices & FREENECT_DEVICE_MOTOR) && !dev->usb_motor.dev && nr_ms_dev == nr_mot + 1)
				{
					// If the index given by the user matches our camera index
					res = libusb_open (devs[mot_idx], &dev->usb_motor.dev);
					if (res < 0 || !dev->usb_motor.dev)
					{
						FN_ERROR("Could not open motor: %d\n", res);
						dev->usb_motor.dev = NULL;
						break;
					}
					res = libusb_claim_interface (dev->usb_motor.dev, 0);
					if (res < 0) {
						FN_ERROR("Could not claim interface on motor: %d\n", res);
						libusb_close(dev->usb_motor.dev);
						dev->usb_motor.dev = NULL;
						break;
					}
				}
			}
			++nr_cam;
		}
		++nr_ms_dev;
	}

	libusb_free_device_list (devs, 1);  // free the list, unref the devices in it

	// Check that each subdevice is either opened or not enabled.
	if ( (dev->usb_cam.dev || !(ctx->enabled_subdevices & FREENECT_DEVICE_CAMERA))
		&& (dev->usb_motor.dev || !(ctx->enabled_subdevices & FREENECT_DEVICE_MOTOR)))
	{
		return 0;
	} else {
		if (dev->usb_cam.dev)
		{
			libusb_release_interface(dev->usb_cam.dev, 0);
			libusb_close(dev->usb_cam.dev);
		}
		if (dev->usb_motor.dev)
		{
			libusb_release_interface(dev->usb_motor.dev, 0);
			libusb_close(dev->usb_motor.dev);
		}
		return -1;
	}
}

int fnusb_close_subdevices(freenect_device *dev)
{
	if (dev->usb_cam.dev)
	{
		libusb_release_interface(dev->usb_cam.dev, 0);
		libusb_attach_kernel_driver(dev->usb_cam.dev, 0);
		libusb_close(dev->usb_cam.dev);
		dev->usb_cam.dev = NULL;
	}
	if (dev->usb_motor.dev)
	{
		libusb_release_interface(dev->usb_motor.dev, 0);
		libusb_close(dev->usb_motor.dev);
		dev->usb_motor.dev = NULL;
	}
	return 0;
}

static void iso_callback(struct libusb_transfer *xfer)
{
	int i;
	fnusb_isoc_stream *strm = (fnusb_isoc_stream*)xfer->user_data;
	freenect_context *ctx = strm->parent->parent->parent;

	if (strm->dead) {
		strm->dead_xfers++;
		FN_SPEW("EP %02x transfer complete, %d left\n", xfer->endpoint, strm->num_xfers - strm->dead_xfers);
		return;
	}

	switch(xfer->status) {
		case LIBUSB_TRANSFER_COMPLETED: // Normal operation.
		{
			uint8_t *buf = (uint8_t*)xfer->buffer;
			for (i=0; i<strm->pkts; i++) {
				strm->cb(strm->parent->parent, buf, xfer->iso_packet_desc[i].actual_length);
				buf += strm->len;
			}
			int res;
			res = libusb_submit_transfer(xfer);
			if (res != 0) {
				FN_ERROR("iso_callback(): failed to resubmit transfer after successful completion: %d\n", res);
				strm->dead_xfers++;
				if (res == LIBUSB_ERROR_NO_DEVICE) {
					strm->parent->device_dead = 1;
				}
			}
			break;
		}
		case LIBUSB_TRANSFER_NO_DEVICE:
		{
			// We lost the device we were talking to.  This is a large problem,
			// and one that we should eventually come up with a way to
			// properly propagate up to the caller.
			if(!strm->parent->device_dead) {
				FN_ERROR("USB device disappeared, cancelling stream %02x :(\n", xfer->endpoint);
			}
			strm->dead_xfers++;
			strm->parent->device_dead = 1;
			break;
		}
		case LIBUSB_TRANSFER_CANCELLED:
		{
			if(strm->dead) {
				FN_SPEW("EP %02x transfer cancelled\n", xfer->endpoint);
			} else {
				// This seems to be a libusb bug on OSX - instead of completing
				// the transfer with LIBUSB_TRANSFER_NO_DEVICE, the transfers
				// simply come back cancelled by the OS.  We can detect this,
				// though - the stream should be marked dead if we're
				// intentionally cancelling transfers.
				if(!strm->parent->device_dead) {
					FN_ERROR("Got cancelled transfer, but we didn't request it - device disconnected?\n");
				}
				strm->parent->device_dead = 1;
			}
			strm->dead_xfers++;
			break;
		}
		default:
		{
			// On other errors, resubmit the transfer - in particular, libusb
			// on OSX tends to hit random errors a lot.  If we don't resubmit
			// the transfers, eventually all of them die and then we don't get
			// any more data from the Kinect.
			FN_WARNING("Isochronous transfer error: %d\n", xfer->status);
			int res;
			res = libusb_submit_transfer(xfer);
			if (res != 0) {
				FN_ERROR("Isochronous transfer resubmission failed after unknown error: %d\n", res);
				strm->dead_xfers++;
				if (res == LIBUSB_ERROR_NO_DEVICE) {
					strm->parent->device_dead = 1;
				}
			}
			break;
		}
	}
}

int fnusb_start_iso(fnusb_dev *dev, fnusb_isoc_stream *strm, fnusb_iso_cb cb, int ep, int xfers, int pkts, int len)
{
	freenect_context *ctx = dev->parent->parent;
	int ret, i;

	strm->parent = dev;
	strm->cb = cb;
	strm->num_xfers = xfers;
	strm->pkts = pkts;
	strm->len = len;
	strm->buffer = (uint8_t*)malloc(xfers * pkts * len);
	strm->xfers = (struct libusb_transfer**)malloc(sizeof(struct libusb_transfer*) * xfers);
	strm->dead = 0;
	strm->dead_xfers = 0;

	uint8_t *bufp = strm->buffer;

	for (i=0; i<xfers; i++) {
		FN_SPEW("Creating EP %02x transfer #%d\n", ep, i);
		strm->xfers[i] = libusb_alloc_transfer(pkts);

		libusb_fill_iso_transfer(strm->xfers[i], dev->dev, ep, bufp, pkts * len, pkts, iso_callback, strm, 0);

		libusb_set_iso_packet_lengths(strm->xfers[i], len);

		ret = libusb_submit_transfer(strm->xfers[i]);
		if (ret < 0) {
			FN_WARNING("Failed to submit isochronous transfer %d: %d\n", i, ret);
			strm->dead_xfers++;
		}

		bufp += pkts*len;
	}

	return 0;

}

int fnusb_stop_iso(fnusb_dev *dev, fnusb_isoc_stream *strm)
{
	freenect_context *ctx = dev->parent->parent;
	int i;

	FN_FLOOD("fnusb_stop_iso() called\n");

	strm->dead = 1;

	for (i=0; i<strm->num_xfers; i++)
		libusb_cancel_transfer(strm->xfers[i]);
	FN_FLOOD("fnusb_stop_iso() cancelled all transfers\n");

	while (strm->dead_xfers < strm->num_xfers) {
		FN_FLOOD("fnusb_stop_iso() dead = %d\tnum = %d\n", strm->dead_xfers, strm->num_xfers);
		libusb_handle_events(ctx->usb.ctx);
	}

	for (i=0; i<strm->num_xfers; i++)
		libusb_free_transfer(strm->xfers[i]);
	FN_FLOOD("fnusb_stop_iso() freed all transfers\n");

	free(strm->buffer);
	free(strm->xfers);

	FN_FLOOD("fnusb_stop_iso() freed buffers and stream\n");
	memset(strm, 0, sizeof(*strm));
	FN_FLOOD("fnusb_stop_iso() done\n");
	return 0;
}

int fnusb_control(fnusb_dev *dev, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t wLength)
{
	return libusb_control_transfer(dev->dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, 0);
}
