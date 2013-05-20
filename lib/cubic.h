#ifndef _GUARD_CUBIC_H_
#define _GUARD_CUBIC_H_

#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

#include "libfreenect.h"
#include "libfreenect-registration.h"

typedef struct cubic_transform_t {
	double m00, m01, m02, m03;
	double m10, m11, m12, m13;
	double m20, m21, m22, m23;
	double yaw, pitch; // we don't support "roll" because Kinect cannot be setup so that will "roll" stable
	double x, y, z;
} cubic_transform_t;

typedef struct cubic_device_t {
	int id;
	freenect_device* device;
	pthread_mutex_t mutex;
	cubic_transform_t transform;
	uint16_t* depth;
	double ref_pix_size;
	double ref_distance;
} cubic_device_t;

struct cubic_t;

typedef struct cubic_t {
	freenect_context* context;
	cubic_device_t* devices;
	int count;
	size_t dims[3];
	double resolution;
	double refresh_rate;
	uint32_t* cube;
	void (*on_ready)(struct cubic_t*);
	pthread_t main, compute;
} cubic_t;

typedef struct {
	size_t dims[3]; // dimension, dimension x resolution is the scale we can analyze
	double resolution; // in terms of mm
	double refresh_rate;
	void (*on_ready)(struct cubic_t*);
} cubic_param_t;

// using open / close semantics because you can only have one cubic instance at the same time for the whole application
cubic_t* __attribute__((warn_unused_result)) cubic_open(int count, int ids[], cubic_param_t params);
void cubic_transform_adjust(cubic_t* cubic, int id, float yaw, float pitch, float x, float y, float z);
void cubic_close(cubic_t* cubic);

#endif
