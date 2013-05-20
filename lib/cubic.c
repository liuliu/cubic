#include "cubic.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#define CUBIC_PI (3.141592653589793)
#define KINECT_WIDTH (640)
#define KINECT_HEIGHT (480)

static void cubic_feedback(freenect_device *dev, void *depth, uint32_t timestamp)
{
	cubic_device_t* device = (cubic_device_t*)freenect_get_user(dev);
	freenect_registration registration = freenect_copy_registration(dev);
	pthread_mutex_lock(&device->mutex);
	device->ref_pix_size = registration.zero_plane_info.reference_pixel_size;
	device->ref_distance = registration.zero_plane_info.reference_distance;
	memcpy(device->depth, depth, sizeof(uint16_t) * KINECT_WIDTH * KINECT_HEIGHT);
	pthread_mutex_unlock(&device->mutex);
	freenect_destroy_registration(&registration);
}

static void cubic_depth_to_cube(uint16_t* depth, double resolution, size_t dims[static 3], double ref_pix_size, double ref_distance, cubic_transform_t transform, uint32_t* cube)
{
	int i, j;
	for (i = 0; i < KINECT_HEIGHT; i++)
	{
		for (j = 0; j < KINECT_WIDTH; j++)
		{
			double z = depth[j] + ref_distance;
			double factor = 2 * ref_pix_size * z / ref_distance;
			double x = (j - KINECT_WIDTH / 2 + 0.5) * factor;
			double y = (i - KINECT_HEIGHT / 2 + 0.5) * factor;
			uint32_t wx = (uint32_t)((x * transform.m00 + y * transform.m01 + z * transform.m02 + transform.m03) / resolution + 0.5 * dims[0] + 0.5);
			uint32_t wy = (uint32_t)((x * transform.m10 + y * transform.m11 + z * transform.m12 + transform.m13) / resolution + 0.5 * dims[1] + 0.5);
			uint32_t wz = (uint32_t)((x * transform.m20 + y * transform.m21 + z * transform.m22 + transform.m23) / resolution + 0.5 * dims[2] + 0.5);
			if (wx < dims[0] && wy < dims[1] && wz < dims[2])
				++cube[wz * dims[0] * dims[1] + wy * dims[0] + wx];
		}
		depth += KINECT_WIDTH;
	}
}

static void* cubic_compute(void* data)
{
	cubic_t* cubic = (cubic_t*)data;
	int i;
	struct timeval ltv, ctv;
	gettimeofday(&ltv, 0);
	for (;;)
	{
		memset(cubic->cube, 0, sizeof(uint32_t) * cubic->dims[0] * cubic->dims[1] * cubic->dims[2]);
		for (i = 0; i < cubic->count; i++)
		{
			pthread_mutex_lock(&cubic->devices[i].mutex);
			cubic_depth_to_cube(cubic->devices[i].depth, cubic->resolution, cubic->dims, cubic->devices[i].ref_pix_size, cubic->devices[i].ref_distance, cubic->devices[i].transform, cubic->cube);
			pthread_mutex_unlock(&cubic->devices[i].mutex);
		}
		cubic->on_ready(cubic);
		gettimeofday(&ctv, 0);
		int64_t usec = 1000000 / cubic->refresh_rate - (ctv.tv_usec - ltv.tv_usec + (ctv.tv_sec - ltv.tv_sec) * 1000000);
		if (usec > 0)
			usleep(usec);
		gettimeofday(&ltv, 0);
	}
	return 0;
}

static void* cubic_main(void* data)
{
	int i;
	cubic_t* cubic = (cubic_t*)data;

	for (i = 0; i < cubic->count; i++)
	{
		freenect_set_depth_callback(cubic->devices[i].device, cubic_feedback);
		freenect_set_depth_mode(cubic->devices[i].device, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM));
		freenect_start_depth(cubic->devices[i].device);
	}

	while (freenect_process_events(cubic->context) >= 0);

	for (i = 0; i < cubic->count; i++)
	{
		freenect_stop_depth(cubic->devices[i].device);
		freenect_close_device(cubic->devices[i].device);
	}
	freenect_shutdown(cubic->context);

	return 0;
}

void cubic_transform_adjust(cubic_t* cubic, int id, float yaw, float pitch, float x, float y, float z)
{
	cubic->devices[id].transform.yaw = yaw;
	cubic->devices[id].transform.pitch = pitch;
	cubic->devices[id].transform.x = x;
	cubic->devices[id].transform.y = y;
	cubic->devices[id].transform.z = z;
	cubic->devices[id].transform.m00 = cosf(pitch);
	cubic->devices[id].transform.m01 = sinf(yaw) * sinf(pitch);
	cubic->devices[id].transform.m02 = cosf(yaw) * sinf(pitch);
	cubic->devices[id].transform.m03 = x;
	cubic->devices[id].transform.m10 = 0;
	cubic->devices[id].transform.m11 = cosf(yaw);
	cubic->devices[id].transform.m12 = -sinf(yaw);
	cubic->devices[id].transform.m13 = y;
	cubic->devices[id].transform.m20 = -sinf(pitch);
	cubic->devices[id].transform.m21 = sinf(yaw) * cosf(pitch);
	cubic->devices[id].transform.m22 = cosf(yaw) * cosf(pitch);
	cubic->devices[id].transform.m23 = z;
}

cubic_t* cubic_open(int count, int ids[], cubic_param_t params)
{
	cubic_t* cubic = (cubic_t*)malloc(sizeof(cubic_t) + sizeof(cubic_device_t) * count + sizeof(uint32_t) * params.dims[0] * params.dims[1] * params.dims[2] + sizeof(uint16_t) * KINECT_WIDTH * KINECT_HEIGHT * count);
	cubic->on_ready = params.on_ready;
	cubic->resolution = params.resolution;
	cubic->dims[0] = params.dims[0];
	cubic->dims[1] = params.dims[1];
	cubic->dims[2] = params.dims[2];
	cubic->refresh_rate = params.refresh_rate;
	cubic->devices = (cubic_device_t*)(cubic + 1);
	cubic->cube = (uint32_t*)(cubic->devices + count);
	uint16_t* depth = (uint16_t*)(cubic->cube + params.dims[0] * params.dims[1] * params.dims[2]);
	freenect_init(&cubic->context, 0);
	freenect_set_log_level(cubic->context, FREENECT_LOG_WARNING);
	freenect_select_subdevices(cubic->context, FREENECT_DEVICE_CAMERA);
	cubic->count = count;
	int i;
	for (i = 0; i < cubic->count; i++)
	{
		cubic->devices[i].id = ids[i];
		freenect_open_device(cubic->context, &cubic->devices[i].device, cubic->devices[i].id);
		freenect_set_user(cubic->devices[i].device, &cubic->devices[i]);
		// we defaulting to clockwise Kinects
		cubic_transform_adjust(cubic, i, 0, i * CUBIC_PI / 3, 0, 0, 0);
		cubic->devices[i].depth = depth;
		pthread_mutex_init(&cubic->devices[i].mutex, 0);
		depth += KINECT_WIDTH * KINECT_HEIGHT;
	}
	// we need another compute thread to do it, because main thread are used for processing events,
	// and we cannot put any computing on it otherwise will lose frame
	pthread_create(&cubic->compute, 0, cubic_compute, cubic);
	pthread_create(&cubic->main, 0, cubic_main, cubic);
	return cubic;
}

void cubic_close(cubic_t* cubic)
{
}
