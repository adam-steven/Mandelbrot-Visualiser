//Interactive Mandelbrot
//Additions Used: glfw (opengl), glew (opengl), boost (barriers), stb_image.h (image reading)
//All threading was done in "mandelbrot.cpp" other files are for opengl and stb_image.h

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <complex>
#include <fstream>
#include <iostream>
#include <thread>
#include <boost/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <string>
#include <mutex>
#include <condition_variable>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Renderer.h"
#include "Texture.h"

using namespace std;

//The size of the image to generate.
const int WIDTH = 640;
const int HEIGHT = 480;

//The number of horizontal segments the image will be split into. (CHANGE THIS TO EDIT THE NUMBER OF THREADS)
const int SEGMENTS = 16;

//The number of times to iterate before we assume that a point isn't in the Mandelbrot set.
const int MAX_ITERATIONS = 1000;

//The image data.
uint32_t image[HEIGHT][WIDTH];

//Barrier for "write_tga" and "compute_mandelbrot" syncronization
boost::barrier syncBarrier(SEGMENTS + 1);

//User input variables
string quitCheck;
double inputs[4] = { -0.751085, -0.734975, 0.118378, 0.134488 };

//Conditional variable to stop the program running before the user has entered data
mutex inputLock;
condition_variable inputCV;
int completedSegments = SEGMENTS;

//Atomic bool to prevent "write_tga" and "opengl_display" from accessing the file at the same time
atomic<bool> imageChanged = true;

//Writes the image to a TGA file. (kept as a save)
void write_tga(const char *filename)
{
	//Loops thread until user quits
	do {
		syncBarrier.wait(); //Barrier waiting for the compute threads to finish

		//Check if the user quit
		if (quitCheck == "Y")
			break;

			ofstream outfile(filename, ofstream::binary);

			uint8_t header[18] = {
				0, // no image ID
				0, // no colour map
				2, // uncompressed 24-bit image
				0, 0, 0, 0, 0, // empty colour map specification
				0, 0, // X origin
				0, 0, // Y origin
				WIDTH & 0xFF, (WIDTH >> 8) & 0xFF, // width
				HEIGHT & 0xFF, (HEIGHT >> 8) & 0xFF, // height
				24, // bits per pixel
				0, // image descriptor
			};
			outfile.write((const char *)header, 18);

			for (int y = 0; y < HEIGHT; ++y)
			{
				for (int x = 0; x < WIDTH; ++x)
				{
					uint8_t pixel[3] = {
						image[y][x] & 0xFF, // blue channel
						(image[y][x] >> 8) & 0xFF, // green channel
						(image[y][x] >> 16) & 0xFF, // red channel
					};
					outfile.write((const char *)pixel, 3);
				}
			}

			outfile.close();
			if (!outfile)
			{
				 //An error has occurred at some point since we opened the file.
				cout << "Error writing to"  << filename << endl;
				exit(1);
			}
			else
				imageChanged = true;

	} while (quitCheck != "Y");
}

//Displays the generated TGA file to a window.
int opengl_display()
{
	GLFWwindow* window;

	//Initialises the glfw library
	if (!glfwInit())
		return -1;

	//Create a windowed mode window and its OpenGL context
	window = glfwCreateWindow(WIDTH, HEIGHT, "mandelbrot", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	//Make the window's context current
	glfwMakeContextCurrent(window);

	//Initialises the glew library
	if (glewInit() != GLEW_OK)
		cout << "GLEW_ERROR" << endl;

	{
		//The positions of the image relitive to the positions of the window
		float objectPositions[] = { -1.0f,-1.0f,0.0f,0.0f		,1.0f,-1.0f,1.0f,0.0f		,1.0f,1.0f,1.0f,1.0f		,-1.0f,1.0f,0.0f,1.0f };
		unsigned int indices[] = {0,1,2,	2,3,0};

		//Initialise classes 
		VertexArray va;
		VertexBuffer vb(objectPositions, 16 * sizeof(float));
		VertexBufferLayout layout;

		//Adds buffer to a vertex array
		layout.Push<float>(2); 
		layout.Push<float>(2);
		va.AddBuffer(vb, layout);

		//Initialise classes 
		IndexBuffer ib(indices, 6);
		Shader shader;
		Renderer renderer;

		//Loops until user quits or closes the window
		while (!glfwWindowShouldClose(window) && quitCheck != "Y")
		{
			//Checks if image has been changed 
			if (imageChanged)
			{
				renderer.Clear(); //Clears buffers to preset values

				//Initialises and binds the texture
				Texture texture("output.tga");
				texture.Bind();
				shader.SetUniform1i("u_Texture", 0);

				imageChanged = false;

				renderer.Draw(va, ib, shader); //Renders image from the array values
				glfwSwapBuffers(window); //Swap front and back buffers
			}
			glfwPollEvents(); //Poll for and process events
		}
	}
	glfwTerminate(); //Closes the the glfw library
	return 0;
}

//Render the Mandelbrot set segments into the image array.
void compute_mandelbrot(int yStart, int yEnd, int i)
{
	//Loops thread until user quits
	do {
		//Conditional variable to check if the user has entered new values
		{
			unique_lock<mutex> lk(inputLock);
			inputCV.wait(lk, [] {return completedSegments < SEGMENTS; });
		}
		inputCV.notify_one();//Signal to the other thread that the mutex is no longer locked

		//Check if the user quit
		if (quitCheck != "Y")
		{
			for (int y = yStart; y < yEnd; ++y)
			{
				for (int x = 0; x < WIDTH; ++x)
				{
					// Work out the point in the complex plane that
					// corresponds to this pixel in the output image.
					complex<double> c(inputs[0] + (x * (inputs[1] - inputs[0]) / WIDTH),
						inputs[2] + (y * (inputs[3] - inputs[2]) / HEIGHT));

					// Start off z at (0, 0).
					complex<double> z(0.0, 0.0);

					// Iterate z = z^2 + c until z moves more than 2 units
					// away from (0, 0), or we've iterated too many times.
					int iterations = 0;
					while (abs(z) < 2.0 && iterations < MAX_ITERATIONS)
					{
						z = (z * z) + c;

						++iterations;
					}

					//Add values to the image data
					image[y][x] = (((iterations % MAX_ITERATIONS) & 0xff) << 16) + (((iterations % MAX_ITERATIONS) & 0xff) << 8) + ((iterations % MAX_ITERATIONS) & 0xff);
				}
			}
			cout << "thread done " << i << endl;
		}

		//Resignal to notify "main" whan all the compute threads are done
		completedSegments += 1;
		inputCV.notify_one();
		
		syncBarrier.wait(); //Barrier waiting for the compute threads to finish

	} while (quitCheck != "Y");
}

//Retrieves user inputs and starts/joins threads 
int main(int argc, char *argv[])
{
	vector<thread *> threads;
	int segmentStart = 0;

	//Create all required threads 
	for (int i = 0; i < SEGMENTS; ++i) {
		threads.push_back(new thread(compute_mandelbrot, segmentStart, (segmentStart + HEIGHT / SEGMENTS), i));
		segmentStart += HEIGHT / SEGMENTS;
	}
	threads.push_back(new thread(write_tga, "output.tga"));
	threads.push_back(new thread(opengl_display));

	//Loops until user quits
	do {
		cout << endl << "Mandelbort Program :" << endl << "Default Values: Left -2.0, Right 1.0, Top 1.125, Bottom -1.125" << endl << "Zoomed Values: Left -0.751085, Right -0.734975, Top 0.118378, Bottom 0.134488" << endl << endl;

		cout << "Quit? (Y / N) : "; cin >> quitCheck;

		//Check if the user quit
		if (quitCheck != "Y")
		{
			cout << endl << "Enter Values :" << endl;
			cout << "Left : "; cin >> inputs[0];
			cout << "Right : "; cin >> inputs[1];
			cout << "Top : "; cin >> inputs[2];
			cout << "Bottom : "; cin >> inputs[3];
		}
		cout << endl;
	
		//Conditional variable to reset all required variables
		{
			lock_guard<mutex> lk(inputLock);
			completedSegments = 0;
			segmentStart = 0;
		}
		inputCV.notify_one(); //Signal to the other thread that the mutex is no longer locked
		//Conditional variable to check if all the compute threads are done
		{
			unique_lock<mutex> lk(inputLock);
			inputCV.wait(lk, [] {return completedSegments == SEGMENTS; });
		}
	} while (quitCheck != "Y");

	//Join all the threads
	for (thread *t : threads) {
		t->join();
		delete t;
	}

	return 0;
}


