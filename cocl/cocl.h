/*
 * Common OpenCL
 *
 */
#ifndef _COMMON_OPENCL_H_
#define _COMMON_OPENCL_H_

namespace cocl
{
    typedef cl_int              err_code;
    typedef cl_command_queue    CmdQueue;
    typedef cl_program          Program;
    typedef cl_context          Ctx;
    
    extern cl_device_id* Devices;
    extern Ctx  Context;
    extern CmdQueue cmdQueue;
       
    void FatalErr(int exitCode);
    err_code Init(bool shareGL );
    void    Release();
    err_code createCommandQueue(CmdQueue&); 
    err_code createProgramWithSource(Program& out,const char** source,size_t length,int kernel_count);
}

#endif
