#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>

#include <cubic.h>

static int window;

static pthread_mutex_t gl_backbuf_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t *back;

static void draw_cube_at(double x, double y, double z)
{
	const static float radius = 0.045f;
	glBegin(GL_QUADS);
	// Draw The Cube Using quads
	float green[] = {
		0.0f, 1.0f, 0.0f, 1.0f
	}; // Color Green
	float orange[] = {
		1.0f, 0.5f, 0.0f, 1.0f
	}; // Color Orange
	float red[] = {
		1.0f, 0.0f, 0.0f, 1.0f
	}; // Color Red
	float yellow[] = {
		1.0f, 1.0f, 0.0f, 1.0f
	}; // Color Yellow 
	float blue[] = {
		0.0f, 0.0f, 1.0f, 1.0f
	}; // Color Blue
	float violet[] = {
		1.0f, 0.0f, 1.0f, 1.0f
	}; // Color Violet
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
	glVertex3f(x + radius, y + radius, z - radius); // Top Right Of The Quad (Top)
	glVertex3f(x - radius, y + radius, z - radius); // Top Left Of The Quad (Top)
	glVertex3f(x - radius, y + radius, z + radius); // Bottom Left Of The Quad (Top)
	glVertex3f(x + radius, y + radius, z + radius); // Bottom Right Of The Quad (Top)
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, orange);
	glVertex3f(x + radius, y - radius, z - radius); // Top Right Of The Quad (Bottom)
	glVertex3f(x - radius, y - radius, z - radius); // Top Left Of The Quad (Bottom)
	glVertex3f(x - radius, y - radius, z + radius); // Bottom Left Of The Quad (Bottom)
	glVertex3f(x + radius, y - radius, z + radius); // Bottom Right Of The Quad (Bottom)
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
	glVertex3f(x + radius, y + radius, z + radius); // Top Right Of The Quad (Front)
	glVertex3f(x - radius, y + radius, z + radius); // Top Left Of The Quad (Front)
	glVertex3f(x - radius, y - radius, z + radius); // Bottom Left Of The Quad (Front)
	glVertex3f(x + radius, y - radius, z + radius); // Bottom Right Of The Quad (Front)
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, yellow);
	glVertex3f(x + radius, y - radius, z - radius); // Top Right Of The Quad (Back)
	glVertex3f(x - radius, y - radius, z - radius); // Top Left Of The Quad (Back)
	glVertex3f(x - radius, y + radius, z - radius); // Bottom Left Of The Quad (Back)
	glVertex3f(x + radius, y + radius, z - radius); // Bottom Right Of The Quad (Back)
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
	glVertex3f(x - radius, y + radius, z + radius); // Top Right Of The Quad (Left)
	glVertex3f(x - radius, y + radius, z - radius); // Top Left Of The Quad (Left)
	glVertex3f(x - radius, y - radius, z - radius); // Bottom Left Of The Quad (Left)
	glVertex3f(x - radius, y - radius, z + radius); // Bottom Right Of The Quad (Left)
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, violet);
	glVertex3f(x + radius, y + radius, z - radius); // Top Right Of The Quad (Right)
	glVertex3f(x + radius, y + radius, z + radius); // Top Left Of The Quad (Right)
	glVertex3f(x + radius, y - radius, z + radius); // Bottom Left Of The Quad (Right)
	glVertex3f(x + radius, y - radius, z - radius); // Bottom Right Of The Quad (Right)
	glEnd();
}

static void draw_environment(void)
{
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glLoadIdentity();
	gluLookAt(0, 0, -4, 0, 0, 0, 0, 1, 0);
	// glRotatef(-90, 1, 0, 0);
	glRotatef(60, 0, 1, 0);

	pthread_mutex_lock(&gl_backbuf_mutex);
	int x, y, z;
	uint32_t* cube = back;
	for (z = 0; z < 200; z++)
		for (y = 0; y < 100; y++)
		{
			for (x = 0; x < 200; x++)
				if (cube[x] >= 100)
					// draw the box because it is presented
					draw_cube_at(-(x - 100) * 0.1, -(y - 50) * 0.1, (z - 100) * 0.1); // change from left-hand coordinate to right-hand coordinate
			cube += 200;
		}
	pthread_mutex_unlock(&gl_backbuf_mutex);
	glFlush();

	glutSwapBuffers();
}

static void key_pressed(unsigned char key, int x, int y)
{
	if (key == 27) {
		glutDestroyWindow(window);
		free(back);
		// Not pthread_exit because OSX leaves a thread lying around and doesn't exit
		exit(0);
	}
}

static void resize_environment(int Width, int Height)
{
	glViewport(0, 0, Width, Height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, 1.0, 1.5, 20.0);
	glMatrixMode(GL_MODELVIEW);
}

static void environment_initialize(int Width, int Height)
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	float light_ambient[] = {
		0.2, 0.2, 0.2, 1.0
	};
	float light_diffuse[] = {
		1.0, 1.0, 1.0, 1.0
	};
	float light_position[] = {
		0.0, 0.0, 0.0, 1.0
	};
	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.1);
	glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.05);
	glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.0);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
}

static void cubic_on_ready(cubic_t* cubic)
{
	pthread_mutex_lock(&gl_backbuf_mutex);
	memcpy(back, cubic->cube, sizeof(uint32_t) * 200 * 200 * 100);
	pthread_mutex_unlock(&gl_backbuf_mutex);
}

int main(int argc, char **argv)
{
	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);
	glutInitWindowSize(640, 640);
	glutInitWindowPosition(0, 0);

	window = glutCreateWindow("Cubic");

	glutDisplayFunc(&draw_environment);
	glutIdleFunc(&draw_environment);
	glutReshapeFunc(&resize_environment);
	glutKeyboardFunc(&key_pressed);

	environment_initialize(640, 640);

	cubic_param_t params = {
		.dims = {
			200, 100, 200
		},
		.resolution = 50,
		.refresh_rate = 30,
		.on_ready = cubic_on_ready,
	};

	int ids[] = {
		0, 1, 2
	};

	back = (uint32_t*)malloc(200 * 200 * 100 * sizeof(uint32_t));

	cubic_t* cubic = cubic_open(3, ids, params);

	glutMainLoop();

	cubic_close(cubic);

	return 0;
}
