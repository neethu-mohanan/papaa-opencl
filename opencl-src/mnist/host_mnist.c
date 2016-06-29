#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CL/cl.h>
#include <CL/cl_ext.h>
#include "papi.h"
#include "lenet5_model.h"
#include "pgm.h"

#define HSTRIDE 2
#define VSTRIDE 2

typedef float DTYPE;

int main()
{
	cl_event event,event1,event2,event3;
	int err, i=0,j =0,Hstride=HSTRIDE,Vstride=VSTRIDE;
	register long long ptimer1=0;
	register long long ptimer2=0;
        pgm_t input_pgm;

        int ipgm_img_width,l1_width,l2_width;
	int ipgm_img_height,l1_height,l2_height;

	cl_device_id device_id;             // compute device id 
	cl_context context;                 // compute context
	cl_command_queue commands;          // compute command queue
	cl_program program;                 // compute program
	cl_kernel kernel[5];                // compute kernel
	
	//OpenCL device memory for matrices
	cl_mem d_image, d_filter1, d_conv1, d_bias1, d_pool1, d_filter2, d_conv2, d_bias2, d_pool2;
	DTYPE *h_image, *h_filter1, *h_conv1, *h_bias1, *h_pool1, *h_filter2, *h_conv2, *h_bias2, *h_pool2;
        cl_mem d_weights1, d_output1, d_cbias1, d_cbias2, d_weights2, d_output;
        DTYPE  *h_weights1, *h_cbias1, *h_output1, *h_cbias2, *h_weights2, *h_output;

	readPGM(&input_pgm,"../../imgs/mnist_test_img_0.pgm");
	ipgm_img_width  = input_pgm.width;
	ipgm_img_height = input_pgm.height;
	printf("cl:main program:img_width %d\n", ipgm_img_width);
	printf("cl:main program:img_height %d\n", ipgm_img_height);

 	//Allocate host memory for matrices
	unsigned int size_image = ipgm_img_width*ipgm_img_height;
	unsigned int mem_size_image = sizeof(DTYPE) * size_image;
        h_image  = (DTYPE*)malloc(mem_size_image * CONV1_NO_INPUTS);
	for(j=0;j<CONV1_NO_INPUTS;j++)
	{
	   for(i=0;i<size_image;i++)
	   {
	   	h_image[(i+(j*size_image))] = (DTYPE) input_pgm.buf[i]/255;
	   }
	}
	
	unsigned int size_filter1 = CONV1_FILTER_WIDTH*CONV1_FILTER_HEIGHT;
	unsigned int mem_size_filter1 = sizeof(DTYPE) * size_filter1;
	h_filter1 = (DTYPE*) conv1_weights;
	   
	unsigned int size_conv1 = ipgm_img_width * ipgm_img_height;
	unsigned int mem_size_conv1 = sizeof(DTYPE) * size_conv1;
	h_conv1 = (DTYPE*) malloc(mem_size_conv1*CONV1_NO_OUTPUTS);
	 
	unsigned int size_bias1 = 1; //1 bias value for 1 output map 
	unsigned int mem_size_bias1 = sizeof(DTYPE) * size_bias1;
	h_bias1 = (DTYPE*) conv1_bias;
	
	l1_width  = (ipgm_img_width-CONV1_FILTER_WIDTH+1)/Hstride; 
	l1_height = (ipgm_img_height-CONV1_FILTER_WIDTH+1)/Vstride;
        unsigned int size_pool1 = l1_width * l1_height;
        unsigned int mem_size_pool1 = sizeof(DTYPE) * size_pool1;
        h_pool1 = (DTYPE*) malloc(mem_size_pool1*CONV1_NO_OUTPUTS);
	
	unsigned int size_filter2 = CONV2_FILTER_WIDTH*CONV2_FILTER_HEIGHT;
	unsigned int mem_size_filter2 = sizeof(DTYPE) * size_filter2;
	h_filter2 = (DTYPE*) conv2_weights;
	   
	unsigned int size_conv2 = l1_width * l1_height;
	unsigned int mem_size_conv2 = sizeof(DTYPE) * size_conv2;
	h_conv2 = (DTYPE*) malloc(mem_size_conv2*CONV2_NO_OUTPUTS);
	 
	unsigned int size_bias2 = 1; //1 bias value for 1 output map 
	unsigned int mem_size_bias2 = sizeof(DTYPE) * size_bias2;
	h_bias1 = (DTYPE*) conv2_bias;
	
	l2_width  = (l1_width-CONV2_FILTER_WIDTH +1)/Hstride;
	l2_height = (l1_height-CONV2_FILTER_HEIGHT+1)/Vstride;
        unsigned int size_pool2 = l2_width*l2_height;
        unsigned int mem_size_pool2 = sizeof(DTYPE) * size_pool2;
        h_pool2 = (DTYPE*) malloc(mem_size_pool2*CONV2_NO_OUTPUTS);

        unsigned int size_weights1 = IP1_NO_INPUTS*IP1_NO_OUTPUTS;
        unsigned int mem_size_weights1 = sizeof(DTYPE) * size_weights1;
        h_weights1 = (DTYPE*) ip1_weights;

        unsigned int size_output1 = IP1_NO_OUTPUTS;
        unsigned int mem_size_output1 = sizeof(DTYPE) * size_output1;
        h_output1 = (DTYPE*) malloc(mem_size_output1);

        unsigned int size_cbias1 = IP1_NO_OUTPUTS;
        unsigned int mem_size_cbias1 = sizeof(DTYPE) * size_cbias1;
        h_cbias1 = (DTYPE*) ip1_bias;

        unsigned int size_weights2 = IP2_NO_INPUTS*IP2_NO_OUTPUTS;
        unsigned int mem_size_weights2 = sizeof(DTYPE) * size_weights2;
        h_weights2 = (DTYPE*) ip2_weights;

        unsigned int size_output = IP2_NO_OUTPUTS;
        unsigned int mem_size_output = sizeof(DTYPE) * size_output;
        h_output = (DTYPE*) malloc(mem_size_output);

        unsigned int size_cbias2 = IP2_NO_OUTPUTS;
        unsigned int mem_size_cbias2 = sizeof(DTYPE) * size_cbias2;
        h_cbias2 = (DTYPE*) ip2_bias;


        cl_uint dev_cnt = 0;
	clGetPlatformIDs(0, 0, &dev_cnt);

	cl_platform_id platform_ids[5];
	
	clGetPlatformIDs(dev_cnt, platform_ids, NULL);
	for(i=0;i<dev_cnt;i++)
	{
#ifdef DEVICE_GPU
	   err = clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
#else
	   err = clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_CPU, 1, &device_id, NULL);
#endif
	   if(err == CL_SUCCESS)
		break;
	}
	if (err != CL_SUCCESS)
	{
	   if(err == CL_INVALID_PLATFORM)
	    printf("CL_INVALID_PLATFORM\n");
	   if(err == CL_INVALID_DEVICE_TYPE)
	    printf("CL_INVALID_DEVICE_TYPE\n");
	   if(err == CL_INVALID_VALUE)
	    printf("CL_INVALID_VALUE\n");
	   if(err == CL_DEVICE_NOT_FOUND)
	    printf("CL_DEVICE_NOT_FOUND\n");
	   printf("Error: Failed to create a device group!\n");
	    return EXIT_FAILURE;
   	}

	   // Create a compute context 
	   context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
	   if (!context)
	   {
	       printf("Error: Failed to create a compute context!\n");
	       return EXIT_FAILURE;
	   }
	
	   // Create a command commands
	   commands = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err);
	   if (!commands)
	   {
	       printf("Error: Failed to create a command commands!\n");
	       return EXIT_FAILURE;
	   }
	
	   // Create the compute program from the source file
	   char *KernelSource;
	   long lFileSize;
	   lFileSize = LoadOpenCLKernel("kernels.cl", &KernelSource);
	   if( lFileSize < 0L ) {
	       perror("File read failed");
	       return 1;
	   }
	
	   program = clCreateProgramWithSource(context, 1, (const char **) & KernelSource, NULL, &err);
	   if (!program)
	   {
	       printf("Error: Failed to create compute program!\n");
	       return EXIT_FAILURE;
	   }
	
	   // Build the program executable
	   err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	   if (err != CL_SUCCESS)
	   {
	       size_t len;
	       char buffer[2048];
	       printf("Error: Failed to build program executable!\n");
	       clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
	       printf("%s\n", buffer);
	       exit(1);
	   }
	
	   kernel[0] = clCreateKernel(program, "filter3D", &err);
	   if (!kernel[0] || err != CL_SUCCESS)
	   {
	       printf("Error: Failed to create compute kernel!\n");
	       exit(1);
	   }

	   kernel[1] = clCreateKernel(program, "maxpool3D", &err);
	   if (!kernel[1] || err != CL_SUCCESS)
	   {
	       printf("Error: Failed to create compute kernel!\n");
	       exit(1);
	   }

	   kernel[2] = clCreateKernel(program, "iplayer", &err);
	   if (!kernel[2] || err != CL_SUCCESS)
	   {
	       printf("Error: Failed to create compute kernel!\n");
	       exit(1);
	   }

	   kernel[3] = clCreateKernel(program, "relu_layer", &err);
	   if (!kernel[3] || err != CL_SUCCESS)
	   {
	       printf("Error: Failed to create compute kernel!\n");
	       exit(1);
	   }

	   kernel[4] = clCreateKernel(program, "softmax", &err);
	   if (!kernel[4] || err != CL_SUCCESS)
	   {
	       printf("Error: Failed to create compute kernel!\n");
	       exit(1);
	   }

 	   // Create the input and output arrays in device memory for our calculation
       	   d_image    = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , mem_size_image*CONV1_NO_INPUTS, h_image, &err);
       	   d_filter1  = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , mem_size_filter1*CONV1_NO_INPUTS*CONV1_NO_OUTPUTS, h_filter1, &err);
       	   d_conv1    = clCreateBuffer(context, CL_MEM_WRITE_ONLY, mem_size_conv1*CONV1_NO_OUTPUTS, NULL, &err);
       	   d_bias1    = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , mem_size_bias1*CONV1_NO_OUTPUTS, h_bias1, &err);
	   d_pool1    = clCreateBuffer(context, CL_MEM_READ_WRITE, mem_size_pool1*CONV1_NO_OUTPUTS, NULL, &err);
       	   d_filter2  = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , mem_size_filter2*CONV1_NO_INPUTS*CONV1_NO_OUTPUTS, h_filter2, &err);
       	   d_conv2    = clCreateBuffer(context, CL_MEM_READ_WRITE, mem_size_conv2*CONV1_NO_OUTPUTS, NULL, &err);
       	   d_bias2    = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , mem_size_bias2*CONV1_NO_OUTPUTS, h_bias2, &err);
           //d_pool2    = clCreateBuffer(context, CL_MEM_READ_WRITE, mem_size_pool2*CONV2_NO_OUTPUTS, NULL, &err);
       	   //d_weights1 = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, mem_size_weights1, h_weights1, &err);
          // d_output1  = clCreateBuffer(context, CL_MEM_READ_WRITE, mem_size_output1, NULL, &err);
          // d_cbias1   = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , mem_size_cbias1, h_cbias1, &err);
          // d_weights2 = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , mem_size_weights2, h_weights2, &err);
          // d_output   = clCreateBuffer(context, CL_MEM_WRITE_ONLY, mem_size_output, NULL, &err);
          // d_cbias2   = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR , mem_size_cbias2, h_cbias2, &err);
 
	   if (!d_image || !d_filter1|| !d_conv1|| !d_bias1 || !d_pool1||!d_filter2|| !d_conv2|| !d_bias2 ) //|| !d_pool2||!d_weights1|| !d_output1|| !d_cbias1 ||!d_weights2|| !d_output|| !d_cbias2 )
       	   {
       	      printf("Error: Failed to allocate device memory!\n");
       	      exit(1);
       	   }    

       	   //Launch OpenCL kernel
       	   size_t conv1local[3], conv1global[3], conv2local[3], conv2global[3], pool1local[3], pool1global[3], pool2local[3], pool2global[3],relu_loc, relu_global, ip1_local,ip1_global,ip2_local, ip2_global, smax_local, smax_global;

       	   ptimer1 = PAPI_get_virt_usec();

       	   int filter_width1  = CONV1_FILTER_WIDTH;
       	   int filter_height1 = CONV1_FILTER_HEIGHT;
       	   int in_maps1       = CONV1_NO_INPUTS;
       
       	   err  = clSetKernelArg(kernel[0], 0, sizeof(cl_mem), (void *)&d_image);
       	   err |= clSetKernelArg(kernel[0], 1, sizeof(cl_mem), (void *)&d_filter1);
       	   err |= clSetKernelArg(kernel[0], 2, sizeof(cl_mem), (void *)&d_conv1);
       	   err |= clSetKernelArg(kernel[0], 3, sizeof(int), (void *)&filter_width1);
       	   err |= clSetKernelArg(kernel[0], 4, sizeof(int), (void *)&filter_height1);
       	   err |= clSetKernelArg(kernel[0], 5, sizeof(int), (void *)&in_maps1);
       	   err |= clSetKernelArg(kernel[0], 6, sizeof(cl_mem), (void*)&d_bias1);
       	
       	   if (err != CL_SUCCESS)
	   {
       	        printf("Error: Failed to set kernel arguments! %d\n", err);	
       	        exit(1);
           }

       	   conv1local[0] = 2;
       	   conv1local[1] = 2;
       	   conv1local[2] = 1;
       
       	   conv1global[0] = ipgm_img_width;
       	   conv1global[1] = ipgm_img_height;
       	   conv1global[2] = CONV1_NO_OUTPUTS;
       	   
	  /*Enqueue task for parallel execution*/
       	   err = clEnqueueNDRangeKernel(commands, kernel[0], 3, NULL, conv1global, conv1local, 0, NULL, &event);
       	   if (err != CL_SUCCESS)
       	   {
	        if(err == CL_INVALID_WORK_ITEM_SIZE)
	       	 	printf("CL_INVALID_WORK_ITEM_SIZE \n");
	        if(err == CL_INVALID_WORK_GROUP_SIZE)
	        	printf("CL_INVALID_WORK_GROUP_SIZE \n");
	        printf("Error: Failed to execute kernel! %d\n", err);
	        exit(1);
	   }

      	   clFinish(commands);	

	   /*Retrieve result from device*/
           err = clEnqueueReadBuffer(commands, d_conv1, CL_TRUE, 0, mem_size_conv1*CONV1_NO_OUTPUTS, h_conv1, 0, NULL, NULL);
           if (err != CL_SUCCESS)
	   {
	        printf("Error: Failed to read output array! %d\n", err);
	        exit(1);
	   }

           long m; 
           l1_width   = ipgm_img_width-CONV1_FILTER_WIDTH+1;
	   l1_height  = ipgm_img_height-CONV1_FILTER_HEIGHT+1;
	   DTYPE* temp = (DTYPE*) malloc(l1_width*l1_height*sizeof(DTYPE)*CONV1_NO_OUTPUTS);

	   for(i=0;i<CONV1_NO_OUTPUTS;i++)
           {
	      for(m=0;m<l1_height;m++)
	      {
		memcpy(temp+(i*l1_width*l1_height)+(m*l1_width),h_conv1+(i*ipgm_img_height*ipgm_img_width)+(m*ipgm_img_width),l1_width*sizeof(DTYPE));
	      }
	   }
	   
       	   clReleaseMemObject(d_image);
           clReleaseMemObject(d_filter1);
           clReleaseMemObject(d_conv1);
           clReleaseMemObject(d_bias1);

       	   d_conv1  = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR ,sizeof(DTYPE)*l1_width*l1_height*CONV1_NO_OUTPUTS, temp, &err);
           int pool_width  = Hstride;
           int pool_height = Vstride;
	   l1_width /= Hstride;
	   l1_height /= Vstride;

           err  = clSetKernelArg(kernel[1], 0, sizeof(cl_mem), (void *)&d_conv1);
           err |= clSetKernelArg(kernel[1], 1, sizeof(cl_mem), (void *)&d_pool1);
           err |= clSetKernelArg(kernel[1], 2, sizeof(int), (void *)&pool_width);
           err |= clSetKernelArg(kernel[1], 3, sizeof(int), (void *)&pool_height);
           err |= clSetKernelArg(kernel[1], 4, sizeof(int), (void *)&ipgm_img_width);
           err |= clSetKernelArg(kernel[1], 5, sizeof(int), (void *)&ipgm_img_height);
           err |= clSetKernelArg(kernel[1], 6, sizeof(int), (void *)&Hstride);
           err |= clSetKernelArg(kernel[1], 7, sizeof(int), (void *)&Vstride);

           if (err != CL_SUCCESS) {
                printf("Error: Failed to set kernel arguments! %d\n", err);
                exit(1);
                   }
           pool1local[0] = 2;
           pool1local[1] = 2;
           pool1local[2] = 1;

           pool1global[0] = l1_width;
           pool1global[1] = l1_height;
           pool1global[2] = CONV1_NO_OUTPUTS;


           /*Enqueue task for parallel execution*/
           err = clEnqueueNDRangeKernel(commands, kernel[1], 3, NULL, pool1global, pool1local, 0, NULL, &event);
           if (err != CL_SUCCESS)
           {
                if(err == CL_INVALID_WORK_ITEM_SIZE)
                printf("CL_INVALID_WORK_ITEM_SIZE \n");
                if(err == CL_INVALID_WORK_GROUP_SIZE)
                        printf("CL_INVALID_WORK_GROUP_SIZE \n");
                printf("Error: Failed to execute kernel! %d\n", err);
                exit(1);
	   }

	   clFinish(commands);

           int filter_width  = CONV2_FILTER_WIDTH;
           int filter_height = CONV2_FILTER_HEIGHT;
           int in_maps       = CONV2_NO_INPUTS;

           err  = clSetKernelArg(kernel[0], 0, sizeof(cl_mem), (void *)&d_pool1);
           err |= clSetKernelArg(kernel[0], 1, sizeof(cl_mem), (void *)&d_filter2);
           err |= clSetKernelArg(kernel[0], 2, sizeof(cl_mem), (void *)&d_conv2);
           err |= clSetKernelArg(kernel[0], 3, sizeof(int), (void *)&filter_width);
           err |= clSetKernelArg(kernel[0], 4, sizeof(int), (void *)&filter_height);
           err |= clSetKernelArg(kernel[0], 5, sizeof(int), (void *)&in_maps);
           err |= clSetKernelArg(kernel[0], 6, sizeof(cl_mem), (void*)&d_bias2);

           if (err != CL_SUCCESS) {
                printf("Error: Failed to set kernel arguments! %d\n", err);
                exit(1);
                   }
           conv2local[0] = 2;
           conv2local[1] = 2;
           conv2local[2] = 1;

           conv2global[0] = l1_width;
           conv2global[1] = l1_height;
           conv2global[2] = CONV2_NO_OUTPUTS;

           /*Enqueue task for parallel execution*/
           err = clEnqueueNDRangeKernel(commands, kernel[0], 3, NULL, conv2global, conv2local, 0, NULL, &event);
           if (err != CL_SUCCESS)
           {
                if(err == CL_INVALID_WORK_ITEM_SIZE)
                        printf("CL_INVALID_WORK_ITEM_SIZE \n");
                if(err == CL_INVALID_WORK_GROUP_SIZE)
                        printf("CL_INVALID_WORK_GROUP_SIZE \n");
                if(err == CL_OUT_OF_RESOURCES)
                        printf("CL_OUT_OF_RESOURCES \n");
                if(err == CL_OUT_OF_HOST_MEMORY)
                        printf("CL_OUT_OF_HOST_MEMORY \n");
                if(err == CL_MEM_OBJECT_ALLOCATION_FAILURE)
                        printf("CL_MEM_OBJECT_ALLOCATION_FAILURE \n");
                if(err == CL_INVALID_GLOBAL_WORK_SIZE)
                        printf("CL_INVALID_GLOBAL_WORK_SIZE \n");
                if(err == CL_INVALID_KERNEL_ARGS)
                        printf("CL_INVALID_KERNEL_ARGS \n");
                printf("Error: Failed to execute kernel! %d \n", err);
                exit(1);
           }
           clFinish(commands);

           /*Retrieve result from device*/
           err = clEnqueueReadBuffer(commands, d_conv2, CL_TRUE, 0, mem_size_conv2*CONV2_NO_OUTPUTS, h_conv2, 0, NULL, NULL);
           if (err != CL_SUCCESS)
           {
                printf("Error: Failed to read output array! %d\n", err);
                exit(1);
           }

           l2_width   = l1_width-CONV2_FILTER_WIDTH+1;
           l2_height  = l1_height-CONV2_FILTER_HEIGHT+1;
           DTYPE* temp1 = (DTYPE*) malloc(l2_width*l2_height*sizeof(DTYPE)*CONV2_NO_OUTPUTS);

           for(i=0;i<CONV2_NO_OUTPUTS;i++)
           {
              for(m=0;m<l2_height;m++)
              {
                memcpy(temp1+(i*l2_width*l2_height)+(m*l2_width),h_conv2+(i*l1_height*l1_width)+(m*l1_width),l2_width*sizeof(DTYPE));
              }
           }

           clReleaseMemObject(d_pool1);
           clReleaseMemObject(d_filter2);
           clReleaseMemObject(d_conv2);
           clReleaseMemObject(d_bias2);

           d_conv2  = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR ,sizeof(DTYPE)*l2_width*l2_height*CONV2_NO_OUTPUTS, temp1, &err);
	   l2_width /=Hstride;
	   l2_height /= Vstride;

           err  = clSetKernelArg(kernel[1], 0, sizeof(cl_mem), (void *)&d_conv2);
           err |= clSetKernelArg(kernel[1], 1, sizeof(cl_mem), (void *)&d_pool2);
           err |= clSetKernelArg(kernel[1], 2, sizeof(int), (void *)&pool_width);
           err |= clSetKernelArg(kernel[1], 3, sizeof(int), (void *)&pool_height);
           err |= clSetKernelArg(kernel[1], 4, sizeof(int), (void *)&l1_width);
           err |= clSetKernelArg(kernel[1], 5, sizeof(int), (void *)&l1_height);
           err |= clSetKernelArg(kernel[1], 6, sizeof(int), (void *)&Hstride);
           err |= clSetKernelArg(kernel[1], 7, sizeof(int), (void *)&Vstride);

           if (err != CL_SUCCESS) {
                printf("Error: Failed to set kernel arguments! %d\n", err);
                exit(1);
                   }
           pool2local[0] = 2;
           pool2local[1] = 2;
           pool2local[2] = 1;

           pool2global[0] = l2_width;
           pool2global[1] = l2_height;
           pool2global[2] = CONV1_NO_OUTPUTS;


           /*Enqueue task for parallel execution*/
           err = clEnqueueNDRangeKernel(commands, kernel[1], 3, NULL, pool1global, pool1local, 0, NULL, &event);
           if (err != CL_SUCCESS)
           {
                if(err == CL_INVALID_WORK_ITEM_SIZE)
                printf("CL_INVALID_WORK_ITEM_SIZE \n");
                if(err == CL_INVALID_WORK_GROUP_SIZE)
                        printf("CL_INVALID_WORK_GROUP_SIZE \n");
                printf("Error: Failed to execute kernel! %d\n", err);
                exit(1);
           }

           clFinish(commands);

           int inputs1 = IP1_NO_INPUTS;
           int inputs2 = IP2_NO_INPUTS;

           err  = clSetKernelArg(kernel[2], 0, sizeof(cl_mem), (void *)&d_pool2);
           err |= clSetKernelArg(kernel[2], 1, sizeof(cl_mem), (void *)&d_weights1);
           err |= clSetKernelArg(kernel[2], 2, sizeof(cl_mem), (void *)&d_output1);
           err |= clSetKernelArg(kernel[2], 3, sizeof(int), (void *)&inputs1);
           err |= clSetKernelArg(kernel[2], 4, sizeof(cl_mem), (void*)&d_cbias1);

           err |= clSetKernelArg(kernel[3], 0, sizeof(cl_mem), (void*)&d_output1);

           err  = clSetKernelArg(kernel[2], 0, sizeof(cl_mem), (void *)&d_output1);
           err |= clSetKernelArg(kernel[2], 1, sizeof(cl_mem), (void *)&d_weights2);
           err |= clSetKernelArg(kernel[2], 2, sizeof(cl_mem), (void *)&d_output);
           err |= clSetKernelArg(kernel[2], 3, sizeof(int), (void *)&inputs2);
           err |= clSetKernelArg(kernel[2], 4, sizeof(cl_mem), (void*)&d_cbias2);

           err |= clSetKernelArg(kernel[4], 0, sizeof(cl_mem), (void*)&d_output);

           if (err != CL_SUCCESS) {
                printf("Error: Failed to set kernel arguments! %d\n", err);
                exit(1);
                   }

           ip1_local= 4;
           ip1_global = IP1_NO_OUTPUTS;

           ptimer1 = PAPI_get_virt_usec();
           /*Enqueue task for parallel execution*/
           err = clEnqueueNDRangeKernel(commands, kernel[2], 1, NULL, &ip1_global, &ip1_local, 0, NULL, &event);
           if (err != CL_SUCCESS)
           {
                printf("Error: Failed to execute kernel! %d \n", err);
                exit(1);
           }

           err = clEnqueueNDRangeKernel(commands,kernel[3],1,NULL,&ip1_global,&ip1_local,1,&event, &event1);
           if(err!= CL_SUCCESS)
           {
                printf("Error: Failed to execute kernel %d \n", err);
                exit(1);
           }

           ip2_local  = 2;
           ip2_global = IP2_NO_OUTPUTS;

           err = clEnqueueNDRangeKernel(commands, kernel[2], 1, NULL, &ip2_global, &ip2_local, 1,&event1, &event2);
           if (err != CL_SUCCESS)
           {
                printf("Error: Failed to execute kernel! %d \n", err);
                exit(1);
           }

           smax_local=IP2_NO_OUTPUTS;
           smax_global=IP2_NO_OUTPUTS;

           err = clEnqueueNDRangeKernel(commands, kernel[4], 1, NULL, &smax_global, &smax_local, 1, &event2, &event3);
           if (err != CL_SUCCESS)
           {
                printf("Error: Failed to execute kernel! %d \n", err);
                exit(1);
           }

           ptimer2 = PAPI_get_virt_usec();
           printf("cl:main timing:PAPI clEnqueueNDRangeKernel %llu us\n",(ptimer2-ptimer1));
           clFinish(commands);
           cl_ulong time_start, time_end;
           double total_time;
           clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
           clGetEventProfilingInfo(event3, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);
           total_time = time_end - time_start;
           printf("cl:main timing:opencl clEnqueueNDRangeKernel %0.3f us\n", total_time / 1000.0);

           /*Retrieve result from device*/
           err = clEnqueueReadBuffer(commands, d_output, CL_TRUE, 0, mem_size_output, h_output, 0, NULL, NULL);
           if (err != CL_SUCCESS)
           {
                printf("Error: Failed to read output array! %d\n", err);
                exit(1);
           }

           int idx=-1;
           float result = -1.0;
           printf("Output Probabilities \n");
           for(i=0;i<IP2_NO_OUTPUTS;i++)
           {
                printf("%f,",h_output[i]);
                if(h_output[i]>result)
                {
                   result = h_output[i];
                   idx = i;
                }
           }
           printf("\n");


           printf("The digit in the image is %d \n",idx);
           destroyPGM(&input_pgm);
	   
	   free(h_image);
	   free(h_output);
	   free(temp);
	   free(temp1);

	   clReleaseMemObject(d_image);
	   clReleaseMemObject(d_output);

	   clReleaseProgram(program);
	   clReleaseKernel(kernel[0]);
	   clReleaseKernel(kernel[1]);
	   clReleaseKernel(kernel[2]);
	   clReleaseKernel(kernel[3]);
	   clReleaseKernel(kernel[4]);
	   clReleaseCommandQueue(commands);
	   clReleaseContext(context);

	   return 0;
}