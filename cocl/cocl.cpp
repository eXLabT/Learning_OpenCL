/*
 *
 *
 */
#include <oclUtils.h>
#include "cocl.h"
#include <OpenGL/OpenGL.h>

namespace cocl
{
    cl_device_id* Devices = NULL;
    Ctx  Context;
    CmdQueue cmdQueue;
    
    err_code createContext(Ctx& out,bool shareGL );
    void (*errFunc)(int) = &FatalErr;
    void FatalErr(int exitCode)
    {
        Release();
    }
    err_code Init(bool shareGL)
    {
        err_code err = 0;
    cl_platform_id platformID;
    cl_uint devCount;
    // Get the platform ID
    err |= oclGetPlatformID(&platformID);
    shrCheckErrorEX(err, CL_SUCCESS, errFunc);

    // Get the number of GPU devices avaliable to the platform
    err |= clGetDeviceIDs(platformID, CL_DEVICE_TYPE_GPU, 0, NULL, &devCount);
    shrCheckErrorEX(err, CL_SUCCESS, errFunc);

    // Retrieve device IDs list
    cocl::Devices = new cl_device_id [devCount];
    err |= clGetDeviceIDs (platformID, CL_DEVICE_TYPE_GPU, devCount,cocl::Devices,NULL);
    shrCheckErrorEX(err, CL_SUCCESS, errFunc);
        // create context
        err |= createContext(Context,shareGL);
        return err;
    }
   void    Release()
   {
    if(cocl::cmdQueue)clReleaseCommandQueue(cocl::cmdQueue);
    if(cocl::Context)clReleaseContext(cocl::Context);
    if(cocl::Devices)delete(cocl::Devices);
    
   }
   err_code createContext(Ctx& out,bool shareGL)
   {
        err_code err;
        if(shareGL)
        {
#if defined (__APPLE__)
        CGLContextObj cglContext = CGLGetCurrentContext();
        CGLShareGroupObj CGLShareGroup = CGLGetShareGroup(cglContext);
        cl_context_properties props[] = 
        {
            CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
            (cl_context_properties)CGLShareGroup,
            0
        };
        out = clCreateContext(props,0,0,NULL,NULL,&err);
        shrCheckErrorEX(err, CL_SUCCESS, errFunc);
#else
    #pragma error "Only support Mac Platform Now"
#endif
        }
        else
        {
        out = clCreateContext(0,1,&Devices[0],NULL,NULL,&err);
        }

        return err;
   }
   err_code createCommandQueue(CmdQueue& out_cmdQueue) 
   {
        cl_int err;
        out_cmdQueue = clCreateCommandQueue(Context,Devices[0],0,&err);
        return err;
   }
   
   err_code createProgramWithSource(Program& out_program,const char** source,size_t length,int kernel_count)
   {
        err_code err;
        out_program = clCreateProgramWithSource(Context,kernel_count,source,&length,&err);
        return err;
   }
}
