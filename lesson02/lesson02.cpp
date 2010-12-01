#include <oclUtils.h>
#include <GL/glew.h>
#include <GLUT/glut.h>
#include <OpenGL/OpenGL.h>
#include "cocl/cocl.h"
// Rendering vars
const unsigned int window_width = 512;
const unsigned int window_height = 512;
const unsigned int mesh_width = 256;
const unsigned int mesh_height = 256;
// OpenCL variables
cl_program kernelProgram;
cl_kernel kernel;
const unsigned int vboSize = mesh_width * mesh_height * 4 * sizeof(float);
cl_mem vbo_cl;
char* cPathAndName = NULL;          // var for full paths to data, src, etc.
char* cSourceCL = NULL;             // Buffer to hold source for compilation 
bool g_GL_InterOp = true;
size_t globalWorkSize [] = {mesh_width, mesh_height};
// vbo variables
GLuint vbo;

// Sim and Auto-Verification parameters 
float anim = 0.0;
// mouse controls
int mouse_old_x, mouse_old_y;
int mouse_buttons = 0;
float rotate_x = 0.0, rotate_y = 0.0;
float translate_z = -3.0;
int iGLUTWindowHandle = 0;          // handle to the GLUT window
// Forward declaration
void InitGL(int argc,const char** argv);
void Cleanup(int err);
void createVBO(GLuint* vbo);
void DisplayGL();
void KeyboardGL(unsigned char key, int x, int y);
void mouse(int button, int state, int x, int y);
void motion(int x, int y);
void runKernel();
void (*cleanupFunc)(int) = &Cleanup;
// Main program
//*****************************************************************************
int main(int argc,const char** argv)
{
    cl_int err;

    // start logs
    shrSetLogFileName("lesson02.txt");
    shrLog("%s Starting...\n\n",argv[0]);

    //Initialize OpenGL items
    InitGL(argc,argv);

    //Initialize OpenCL 
    cocl::Init(true);

    // create a command-queue
    err = cocl::createCommandQueue(cocl::cmdQueue);
    shrCheckErrorEX(err, CL_SUCCESS, cleanupFunc);
    
    // Program Setup
    size_t program_length;
    cPathAndName = shrFindFilePath("simpleGL.cl", argv[0]);
    shrCheckErrorEX(cPathAndName != NULL, shrTRUE, cleanupFunc);
    cSourceCL = oclLoadProgSource(cPathAndName, "", &program_length);
    shrCheckErrorEX(cSourceCL != NULL, shrTRUE, cleanupFunc);

    // create the program
    err = cocl::createProgramWithSource(kernelProgram,(const char**)&cSourceCL,program_length,1);    
    shrCheckErrorEX(err, CL_SUCCESS, cleanupFunc);

    // build the program
    err= clBuildProgram(kernelProgram, 0, NULL, "-cl-fast-relaxed-math", NULL, NULL);
    if (err != CL_SUCCESS)
    {
        // write out standard error, Build Log and PTX, then cleanup and exit
        shrLogEx(LOGBOTH | ERRORMSG, err, STDERROR);
        oclLogBuildInfo(kernelProgram, oclGetFirstDev(cocl::Context));
        oclLogPtx(kernelProgram, oclGetFirstDev(cocl::Context), "oclSimpleGL.ptx");
        Cleanup(EXIT_FAILURE); 
    }

    // create kernel
    kernel = clCreateKernel ( kernelProgram, "sine_wave", &err);
    shrCheckErrorEX(err, CL_SUCCESS, cleanupFunc);

    // create VBO
    createVBO(&vbo);

    // set the kernel args values
    err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&vbo_cl);
    err |= clSetKernelArg(kernel, 1, sizeof(unsigned int), &mesh_width);
    err |= clSetKernelArg(kernel, 2, sizeof(unsigned int), &mesh_height);
    shrCheckErrorEX(err, CL_SUCCESS, cleanupFunc);

    // start main GLUT rendering loop
    glutMainLoop();

    // return path
    Cleanup(EXIT_FAILURE);

   
    return 0;
}

// Initialize GL
// ========================================================

void InitGL(int argc,const char** argv)
{

    // initialize GLUT 
    glutInit(&argc, (char**)argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowPosition (glutGet(GLUT_SCREEN_WIDTH)/2 - window_width/2, 
                            glutGet(GLUT_SCREEN_HEIGHT)/2 - window_height/2);
    glutInitWindowSize(window_width, window_height);
    iGLUTWindowHandle = glutCreateWindow("OpenCL/GL Interop (VBO)");
    
    // register GLUT callback functions
    glutDisplayFunc(DisplayGL);
    glutKeyboardFunc(KeyboardGL);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
	
    // initialize necessary OpenGL extensions
    glewInit();
    GLboolean bGLEW = glewIsSupported("GL_VERSION_2_0 GL_ARB_pixel_buffer_object"); 
    shrCheckErrorEX(bGLEW, shrTRUE, cleanupFunc);

    // default initialization
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glDisable(GL_DEPTH_TEST);

    // viewport
    glViewport(0, 0, window_width, window_height);

    // projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (GLfloat)window_width / (GLfloat) window_height, 0.1, 10.0);

    // set view matrix
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, translate_z);
    glRotatef(rotate_x, 1.0, 0.0, 0.0);
    glRotatef(rotate_y, 0.0, 1.0, 0.0);

}

// Display callback
// ========================================================
void DisplayGL ()
{
    anim += 0.01f;
    // run OpenCL kernel to generate vertex position
    runKernel();

    // clear graphics then render from the vbo
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexPointer(4, GL_FLOAT, 0, 0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glColor3f(1.0, 0.0, 0.0);
    glDrawArrays(GL_POINTS, 0, mesh_width * mesh_height);
    glDisableClientState(GL_VERTEX_ARRAY);

    //flip backbuffer to screen
    glutSwapBuffers();
    glutPostRedisplay();

}

// Run the OpenCL part of the computation
// ========================================================
void runKernel ()
{
    cl_int err = CL_SUCCESS;
    
    if(g_GL_InterOp)
    // map OpenGL buffer object for writing from OpenCL
    {
        glFinish();
        err = clEnqueueAcquireGLObjects(cocl::cmdQueue, 1, &vbo_cl, 0,0,0);
        shrCheckErrorEX(err, CL_SUCCESS, cleanupFunc);
    }
    
    // Set arg 3 and execute the kernel
    err = clSetKernelArg(kernel, 3, sizeof(float),&anim);
    err |= clEnqueueNDRangeKernel(cocl::cmdQueue, kernel, 2, NULL, globalWorkSize, NULL,0,0,0);
    shrCheckErrorEX(err, CL_SUCCESS, cleanupFunc);

    if(g_GL_InterOp)
    //unmap buffer object
    {
        err = clEnqueueReleaseGLObjects(cocl::cmdQueue, 1, &vbo_cl, 0,0,0);
        shrCheckErrorEX(err, CL_SUCCESS, cleanupFunc);
        clFinish(cocl::cmdQueue);
    }
    else
    //Explicit Copy
    {
        // map the PBO to copy data from the CL buffer via host
        glBindBufferARB(GL_ARRAY_BUFFER, vbo);

        // map the buffer object into client's memory
        void* ptr = glMapBufferARB(GL_ARRAY_BUFFER, GL_WRITE_ONLY_ARB);

        err = clEnqueueReadBuffer(cocl::cmdQueue, vbo_cl, CL_TRUE, 0, vboSize, ptr, 0, NULL, NULL);
        shrCheckErrorEX(err, CL_SUCCESS, cleanupFunc);

        glUnmapBufferARB(GL_ARRAY_BUFFER);
    }

}

// Create VBO
// ========================================================
void createVBO(GLuint * vbo)
{
    // create VBO
    // crate buffer object
    glGenBuffers(1, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    
    // initialize buffer object
    glBufferData(GL_ARRAY_BUFFER, vboSize, 0, GL_DYNAMIC_DRAW);
    if(g_GL_InterOp)
    // create OpenCL buffer from GL VBO
    {
        vbo_cl = clCreateFromGLBuffer(cocl::Context, CL_MEM_WRITE_ONLY, *vbo, NULL);
    }
    else
    // create standard OpenCL mem buffer
    {
        cl_int err;
        vbo_cl = clCreateBuffer(cocl::Context,CL_MEM_WRITE_ONLY, vboSize, NULL, &err);
        shrCheckErrorEX(err, CL_SUCCESS, cleanupFunc);
    }
}

// Function to clean up and exit
// ========================================================
void Cleanup(int exitCode)
{
    // Cleanup allocated objects
    shrLog("\nStarting Cleanup...\n\n");
    if(kernel)clReleaseKernel(kernel);
    if(kernelProgram)clReleaseProgram(kernelProgram);
    if(vbo)
    {
        glBindBuffer(1,vbo);
        glDeleteBuffers(1,&vbo);
        vbo = 0;
    }
    if(vbo_cl)clReleaseMemObject(vbo_cl);
    cocl::Release();
    if(iGLUTWindowHandle)glutDestroyWindow(iGLUTWindowHandle);
    if(cPathAndName)free(cPathAndName);
    if(cSourceCL)free(cSourceCL);
    
    // finalize logs and leave
    shrLogEx(LOGBOTH | CLOSELOG, 0, "oclSimpleGL.exe Exiting...\nPress <Enter> to Quit\n");
    exit(exitCode);
}

// Keyboard events handler
//*****************************************************************************
void KeyboardGL(unsigned char key, int x, int y)
{
    switch(key) 
    {
        case 'i': // toggle GL/CL Inter OP 
        case 'I': // toggle GL/CL Inter OP
            g_GL_InterOp = !g_GL_InterOp;
            break;
        case '\033': // escape quits
        case '\015': // Enter quits    
        case 'Q':    // Q quits
        case 'q':    // q (or escape) quits
            // Cleanup up and quit
	        Cleanup(EXIT_SUCCESS);
            break;
    }
}


// Mouse event handlers
//*****************************************************************************
void mouse(int button, int state, int x, int y)
{
    if (state == GLUT_DOWN) {
        mouse_buttons |= 1<<button;
    } else if (state == GLUT_UP) {
        mouse_buttons = 0;
    }

    mouse_old_x = x;
    mouse_old_y = y;

    glutPostRedisplay();
}

void motion(int x, int y)
{
    float dx, dy;
    dx = x - mouse_old_x;
    dy = y - mouse_old_y;

    if (mouse_buttons & 1) {
        rotate_x += dy * 0.2;
        rotate_y += dx * 0.2;
    } else if (mouse_buttons & 4) {
        translate_z += dy * 0.01;
    }

    mouse_old_x = x;
    mouse_old_y = y;

    // set view matrix
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, translate_z);
    glRotatef(rotate_x, 1.0, 0.0, 0.0);
    glRotatef(rotate_y, 0.0, 1.0, 0.0);
    glutPostRedisplay();
}
