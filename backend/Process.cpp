/*
 * gVirtuS -- A GPGPU transparent virtualization component.
 *
 * Copyright (C) 2009-2010  The University of Napoli Parthenope at Naples.
 *
 * This file is part of gVirtuS.
 *
 * gVirtuS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gVirtuS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gVirtuS; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Written by: Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>,
 *             Department of Applied Science
 */

/**
 * @file   Process.cpp
 * @author Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>
 * @date   Wed Sep 30 10:45:40 2009
 *
 * @brief
 *
 *
 */

#include "Process.h"

#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <dlfcn.h>

#include <iostream>
#include </usr/local/cuda/include/cuda.h>
#include </usr/local/cuda/include/cuda_runtime_api.h>

typedef CUresult (*cuInit_t)(unsigned int Flags);

typedef cudaError_t (*cudaSetDevice_t)(int device);
typedef cudaError_t (*cudaGetDeviceCount_t)(int *count);
typedef cudaError_t (*cudaDeviceReset_t)(void);
typedef cudaError_t (*cudaFree_t) (void *devPtr);

using namespace std;

static GetHandler_t LoadModule(const char *name) {
    char path[4096];
    if (*name == '/')
        strcpy(path, name);
    else
        sprintf(path, _PLUGINS_DIR "/lib%s-backend.so", name);

    void *lib = dlopen(path, RTLD_LAZY);
    if(lib == NULL) {
        cerr << "Error loading " << path << ": " << dlerror() << endl;
        return NULL;
    }

    HandlerInit_t init = (HandlerInit_t) ((uint64_t) dlsym(lib, "HandlerInit"));
    if(init == NULL) {
        dlclose(lib);
        cerr << "Error loading " << name << ": HandlerInit function not found."
                << endl;
        return NULL;
    }

    if(init() != 0) {
        dlclose(lib);
        cerr << "Error loading " << name << ": HandlerInit failed."
                << endl;
        return NULL;
    }

    GetHandler_t sym = (GetHandler_t) ((uint64_t) dlsym(lib, "GetHandler"));
    if(sym == NULL) {
        dlclose(lib);
        cerr << "Error loading " << name << ": " << dlerror() << endl;
        return NULL;
    }

    cout << "Loaded module '" << name << "'." << endl;

    return sym;
}

Process::Process(const Communicator *communicator, vector<string> &plugins)
: Subprocess(), Observable() {
    mpCommunicator = const_cast<Communicator *> (communicator);
    mPlugins = plugins;
}

Process::~Process() {
    cout << "[Process " << GetPid() << "]: Destroyed." << endl;
}

void Process::Setup() {

}

static bool getstring(Communicator *c, string & s) {
    s = "";
    char ch = 0;
    while(c->Read(&ch, 1) == 1) {
        if(ch == 0) {
            return true;
        }
        s += ch;
    }
    return false;
}

void initialSetDevice (void) {
	void *libcuda = dlopen("/usr/lib/x86_64-linux-gnu/libcuda.so",
			RTLD_LAZY);
    if(libcuda == NULL) {
        cerr << "Error loading /usr/lib/x86_64-linux-gnu/libcuda.so: " << dlerror() << endl;
		return;
    }

    cuInit_t init = (cuInit_t) ((uint64_t) dlsym(libcuda, "cuInit"));
	if(init == NULL) {
		dlclose(libcuda);
		cerr << "Error loading cuInit: CUDA driver function not found."
				<< endl;
		return;
	}

	void *lib = dlopen("/usr/local/cuda/lib64/libcudart.so",
			RTLD_LAZY);
    if(lib == NULL) {
        cerr << "Error loading /usr/local/cuda/lib64/libcudart.so: " << dlerror() << endl;
        return;
    }

	cudaSetDevice_t setDevice = (cudaSetDevice_t) ((uint64_t) dlsym(lib, "cudaSetDevice"));
	if(setDevice == NULL) {
		dlclose(lib);
		cerr << "Error loading cudaSetDevice: CUDA RT function not found."
				<< endl;
		return;
	}

	cudaDeviceReset_t resetDevice = (cudaDeviceReset_t) ((uint64_t) dlsym(lib, "cudaDeviceReset"));
	if(resetDevice == NULL) {
		dlclose(lib);
		cerr << "Error loading cudaDeviceReset: CUDA RT function not found."
				<< endl;
		return;
	}

	cudaFree_t deviceFree = (cudaFree_t) ((uint64_t) dlsym(lib, "cudaFree"));
	if(deviceFree == NULL) {
		dlclose(lib);
		cerr << "Error loading cudaFree: CUDA RT function not found."
				<< endl;
		return;
	}

	std::cout << "Setting device initially to 0: " << setDevice(0) << std::endl;
	std::cout << "Initializing device: " << init(0) << std::endl;
	std::cout << "Resetting device: " << resetDevice() << std::endl;
	std::cout << "Freeing null pointer: " << deviceFree(0) << std::endl;
	dlclose(libcuda);
	dlclose (lib);
}

void Process::Execute(void * arg) {
    cout << "[Process " << GetPid() << "]: Started." << endl;

	//initialSetDevice();

    GetHandler_t h;
    for(vector<string>::iterator i = mPlugins.begin(); i != mPlugins.end();
            i++) {
        if((h = LoadModule((*i).c_str())) != NULL)
            mHandlers.push_back(h());
    }

    string routine;
    Buffer * input_buffer = new Buffer();
    while (getstring(mpCommunicator, routine)) {
        input_buffer->Reset(mpCommunicator);
        Handler *h = NULL;
        for(vector<Handler *>::iterator i = mHandlers.begin();
                i != mHandlers.end(); i++) {
            if((*i)->CanExecute(routine)) {
                h = *i;
                break;
            }
        }
        Result * result;
        if(h == NULL) {
            cout << "[Process " << GetPid() << "]: Requested unknown routine "
                    << routine << "." << endl;
            result = new Result(-1, new Buffer());
        } else
            result = h->Execute(routine, input_buffer);
        result->Dump(mpCommunicator);
        if (result->GetExitCode() != 0) {
            cout << "[Process " << GetPid() << "]: Requested '" << routine
                    << "' routine." << endl;
            cout << "[Process " << GetPid() << "]: Exit Code '"
                    << result->GetExitCode() << "'." << endl;
        }
        delete result;
    }
    delete input_buffer;
    Notify("process-ended");
    delete this;
}

