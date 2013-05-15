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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "libfreenect.h"
#include "libfreenect-registration.h"

#include <pthread.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>

pthread_t freenect_thread;
volatile int die = 0;

int g_argc;
char **g_argv;

int window;

pthread_mutex_t gl_backbuf_mutex = PTHREAD_MUTEX_INITIALIZER;

// back: owned by libfreenect (implicit for depth)
// mid: owned by callbacks, "latest frame ready"
// front: owned by GL, "currently being drawn"
uint32_t *box_mid, *box_front;

freenect_context *f_ctx;
freenect_device *f_dev;

freenect_video_format requested_format = FREENECT_VIDEO_RGB;
freenect_video_format current_format = FREENECT_VIDEO_RGB;

pthread_cond_t gl_frame_cond = PTHREAD_COND_INITIALIZER;
int got_depth = 0;

static void draw_cube_at(double x, double y, double z)
{
	const static float radius = 0.05f;
	glBegin(GL_QUADS);
	// Draw The Cube Using quads
	glColor3f(0.0f, 1.0f, 0.0f); // Color Blue
	glVertex3f(x + radius, y + radius, z - radius); // Top Right Of The Quad (Top)
	glVertex3f(x - radius, y + radius, z - radius); // Top Left Of The Quad (Top)
	glVertex3f(x - radius, y + radius, z + radius); // Bottom Left Of The Quad (Top)
	glVertex3f(x + radius, y + radius, z + radius); // Bottom Right Of The Quad (Top)
	glColor3f(1.0f, 0.5f, 0.0f); // Color Orange
	glVertex3f(x + radius, y - radius, z - radius); // Top Right Of The Quad (Bottom)
	glVertex3f(x - radius, y - radius, z + radius); // Top Left Of The Quad (Bottom)
	glVertex3f(x - radius, y - radius, z - radius); // Bottom Left Of The Quad (Bottom)
	glVertex3f(x + radius, y - radius, z - radius); // Bottom Right Of The Quad (Bottom)
	glColor3f(1.0f, 0.0f, 0.0f); // Color Red    
	glVertex3f(x + radius, y + radius, z + radius); // Top Right Of The Quad (Front)
	glVertex3f(x - radius, y + radius, z + radius); // Top Left Of The Quad (Front)
	glVertex3f(x - radius, y - radius, z + radius); // Bottom Left Of The Quad (Front)
	glVertex3f(x + radius, y - radius, z + radius); // Bottom Right Of The Quad (Front)
	glColor3f(1.0f, 1.0f, 0.0f); // Color Yellow
	glVertex3f(x + radius, y - radius, z - radius); // Top Right Of The Quad (Back)
	glVertex3f(x - radius, y - radius, z - radius); // Top Left Of The Quad (Back)
	glVertex3f(x - radius, y + radius, z - radius); // Bottom Left Of The Quad (Back)
	glVertex3f(x + radius, y + radius, z - radius); // Bottom Right Of The Quad (Back)
	glColor3f(0.0f, 0.0f, 1.0f); // Color Blue
	glVertex3f(x - radius, y + radius, z + radius); // Top Right Of The Quad (Left)
	glVertex3f(x - radius, y + radius, z - radius); // Top Left Of The Quad (Left)
	glVertex3f(x - radius, y - radius, z - radius); // Bottom Left Of The Quad (Left)
	glVertex3f(x - radius, y - radius, z + radius); // Bottom Right Of The Quad (Left)
	glColor3f(1.0f, 0.0f, 1.0f); // Color Violet
	glVertex3f(x + radius, y + radius, z - radius); // Top Right Of The Quad (Right)
	glVertex3f(x + radius, y + radius, z + radius); // Top Left Of The Quad (Right)
	glVertex3f(x + radius, y - radius, z + radius); // Bottom Left Of The Quad (Right)
	glVertex3f(x + radius, y - radius, z - radius); // Bottom Right Of The Quad (Right)
	glEnd();
}

void DrawGLScene(void)
{
	pthread_mutex_lock(&gl_backbuf_mutex);

	// When using YUV_RGB mode, RGB frames only arrive at 15Hz, so we shouldn't force them to draw in lock-step.
	// However, this is CPU/GPU intensive when we are receiving frames in lockstep.
	if (current_format == FREENECT_VIDEO_YUV_RGB) {
		while (!got_depth) {
			pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
		}
	} else {
		while (!got_depth && requested_format != current_format) {
			pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
		}
	}

	if (requested_format != current_format) {
		pthread_mutex_unlock(&gl_backbuf_mutex);
		return;
	}

	if (got_depth) {
		uint32_t *tmp;
		tmp = box_front;
		box_front = box_mid;
		box_mid = tmp;
		got_depth = 0;
	}

	pthread_mutex_unlock(&gl_backbuf_mutex);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glLoadIdentity();
	gluLookAt(0.0f, 0.0f, 5.0f, 0, 0, 0, 0, 1, 0);
	glRotatef(-90, 1, 0, 0);

	int x, y, z;
	uint32_t* box = box_front;
	for (z = 0; z < 100; z++)
		for (x = 0; x < 100; x++)
		{
			for (y = 0; y < 50; y++)
				if (box[y] > 400)
					// draw the box because it is presented
					draw_cube_at((x - 50) * 0.1, (y - 25) * 0.1, z * 0.1);
			box += 50;
		}
	glFlush();

	glutSwapBuffers();
}

void keyPressed(unsigned char key, int x, int y)
{
	if (key == 27) {
		die = 1;
		pthread_join(freenect_thread, 0);
		glutDestroyWindow(window);
		free(box_mid);
		free(box_front);
		// Not pthread_exit because OSX leaves a thread lying around and doesn't exit
		exit(0);
	}
	if (key == 'f') {
		if (requested_format == FREENECT_VIDEO_IR_8BIT)
			requested_format = FREENECT_VIDEO_RGB;
		else if (requested_format == FREENECT_VIDEO_RGB)
			requested_format = FREENECT_VIDEO_YUV_RGB;
		else
			requested_format = FREENECT_VIDEO_IR_8BIT;
	}
}

void ReSizeGLScene(int Width, int Height)
{
	glViewport(0, 0, Width, Height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, 1.0, 1.5, 20.0);
	glMatrixMode(GL_MODELVIEW);
}

void InitGL(int Width, int Height)
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glShadeModel(GL_FLAT);
}

void *gl_threadfunc(void *arg)
{
	printf("GL thread\n");

	glutInit(&g_argc, g_argv);

	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(0, 0);

	window = glutCreateWindow("Kinect 360");

	glutDisplayFunc(&DrawGLScene);
	glutIdleFunc(&DrawGLScene);
	glutReshapeFunc(&ReSizeGLScene);
	glutKeyboardFunc(&keyPressed);

	InitGL(800, 600);

	glutMainLoop();

	return 0;
}

void depth_cb(freenect_device *dev, void *v_depth, uint32_t timestamp)
{
	int i, j;
	uint16_t *depth = (uint16_t*)v_depth;

	freenect_registration registration = freenect_copy_registration(dev);
	pthread_mutex_lock(&gl_backbuf_mutex);
	double ref_pix_size = registration.zero_plane_info.reference_pixel_size;
	double ref_distance = registration.zero_plane_info.reference_distance;
	memset(box_mid, 0, 100 * 100 * 50 * sizeof(uint32_t));
	for (i = 0; i < 480; i++)
	{
		for (j = 0; j < 640; j++)
		{
			double factor = 2 * ref_pix_size * depth[j] / ref_distance;
			int wx = (int)((j - 320 + 0.5) * factor / 100 + 0.5);
			int wy = (int)((i - 240 + 0.5) * factor / 100 + 0.5);
			int wz = (int)(depth[j] / 100 + 0.5);
			if (wz < 100 && wz >= 0 && wx < 50 && wx >= -50 && wy < 25 && wy >= -25)
				++box_mid[wz * 100 * 50 + (wx + 50) * 50 + (wy + 25)];
		}
		depth += 640;
	}
	got_depth++;
	pthread_cond_signal(&gl_frame_cond);
	pthread_mutex_unlock(&gl_backbuf_mutex);
}

void *freenect_threadfunc(void *arg)
{
	freenect_set_depth_callback(f_dev, depth_cb);
	freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, current_format));
	freenect_set_depth_mode(f_dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM));

	freenect_start_depth(f_dev);
	freenect_start_video(f_dev);

	printf("'f'-video format\n");

	while (!die && freenect_process_events(f_ctx) >= 0) {
		if (requested_format != current_format) {
			freenect_stop_video(f_dev);
			freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, requested_format));
			freenect_start_video(f_dev);
			current_format = requested_format;
		}
	}

	printf("\nshutting down streams...\n");

	freenect_stop_depth(f_dev);
	freenect_stop_video(f_dev);

	freenect_close_device(f_dev);
	freenect_shutdown(f_ctx);

	printf("-- done!\n");
	return 0;
}

int main(int argc, char **argv)
{
	int res;

	box_mid = (uint32_t*)malloc(100 * 100 * 50 * sizeof(uint32_t));
	box_front = (uint32_t*)malloc(100 * 100 * 50 * sizeof(uint32_t));

	g_argc = argc;
	g_argv = argv;

	if (freenect_init(&f_ctx, 0) < 0) {
		printf("freenect_init() failed\n");
		return -1;
	}

	freenect_set_log_level(f_ctx, FREENECT_LOG_DEBUG);
	freenect_select_subdevices(f_ctx, FREENECT_DEVICE_CAMERA);

	int nr_devices = freenect_num_devices(f_ctx);
	printf ("Number of devices found: %d\n", nr_devices);

	int user_device_number = 0;

	if (nr_devices < 1)
	{
		freenect_shutdown(f_ctx);
		return -1;
	}

	if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0)
	{
		printf("Could not open device\n");
		freenect_shutdown(f_ctx);
		return -1;
	}

	res = pthread_create(&freenect_thread, 0, freenect_threadfunc, 0);
	if (res)
	{
		printf("pthread_create failed\n");
		freenect_shutdown(f_ctx);
		return -1;
	}

	gl_threadfunc(0);

	return 0;
}
