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
 * @file   main.cpp
 * @author Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>
 * @date   Wed Sep 30 12:21:51 2009
 * 
 * @brief  
 * 
 * 
 */

/**
 * @mainpage gVirtuS - A GPGPU transparent virtualization component
 *
 * @section Introduction
 * gVirtuS tries to fill the gap between in-house hosted computing clusters,
 * equipped with GPGPUs devices, and pay-for-use high performance virtual
 * clusters deployed  via public or private computing clouds. gVirtuS allows an
 * instanced virtual machine to access GPGPUs in a transparent way, with an
 * overhead  slightly greater than a real machine/GPGPU setup. gVirtuS is
 * hypervisor independent, and, even though it currently virtualizes nVIDIA CUDA
 * based GPUs, it is not limited to a specific brand technology. The performance
 * of the components of gVirtuS is assessed through a suite of tests in
 * different deployment scenarios, such as providing GPGPU power to cloud
 * computing based HPC clusters and sharing remotely hosted GPGPUs among HPC
 * nodes.
 * 
 * @section License
 * Copyright (C) 2009 - 2010
 *     Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <iostream>
#include <algorithm>
#include "ConfigFile.h"
#include "Communicator.h"
#include "Backend.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <dlfcn.h>
#include </usr/local/cuda/include/cuda.h>
#include </usr/local/cuda/include/cuda_runtime_api.h>

typedef CUresult (*cuInit_t)(unsigned int Flags);

typedef cudaError_t (*cudaSetDevice_t)(int device);
typedef cudaError_t (*cudaGetDeviceCount_t)(int *count);
typedef cudaError_t (*cudaDeviceReset_t)(void);

using namespace std;

void
signal_callback_handler(int signum) {
	printf("Caught signal %d\n",signum);

	void *libcuda = dlopen("/usr/lib/x86_64-linux-gnu/libcuda.so",
			RTLD_LAZY);
    if(libcuda == NULL) {
        cerr << "Error loading /usr/lib/x86_64-linux-gnu/libcuda.so: " << dlerror() << endl;
    	exit(signum);
    }

    cuInit_t init = (cuInit_t) ((uint64_t) dlsym(libcuda, "cuInit"));
	if(init == NULL) {
		dlclose(libcuda);
		cerr << "Error loading cuInit: CUDA driver function not found."
				<< endl;
		exit(signum);
	}

	void *lib = dlopen("/usr/local/cuda/lib64/libcudart.so",
			RTLD_LAZY);
    if(lib == NULL) {
        cerr << "Error loading /usr/local/cuda/lib64/libcudart.so: " << dlerror() << endl;
    	exit(signum);
    }

	cudaSetDevice_t setDevice = (cudaSetDevice_t) ((uint64_t) dlsym(lib, "cudaSetDevice"));
	if(setDevice == NULL) {
		dlclose(lib);
		cerr << "Error loading cudaSetDevice: CUDA RT function not found."
				<< endl;
		exit(signum);
	}

    cudaGetDeviceCount_t getDeviceCount = (cudaGetDeviceCount_t) ((uint64_t) dlsym(lib, "cudaGetDeviceCount"));
	if(getDeviceCount == NULL) {
		dlclose(lib);
		cerr << "Error loading cudaGetDeviceCount: CUDA RT function not found."
				<< endl;
		exit(signum);
	}

    cudaDeviceReset_t deviceReset = (cudaDeviceReset_t) ((uint64_t) dlsym(lib, "cudaDeviceReset"));
	if(deviceReset == NULL) {
		dlclose(lib);
		cerr << "Error loading cudaDeviceReset: CUDA RT function not found."
				<< endl;
		exit(signum);
	}

	int devCount = 0;
	if (getDeviceCount(&devCount) == 0) {
		for (int i = 0; i < devCount; ++i) {
			std::cout << "Setting device to " << setDevice (i) << std::endl;
			if (signum == 0) std::cout << "Initializing device: " << init(0) << std::endl;
			std::cout << "Resetting device: " << deviceReset () << std::endl;
		}
	}

	if (signum)	exit(signum);
}

vector<string> split(const string& s, const string& f) {
    vector<string> temp;
    if (f.empty()) {
        temp.push_back(s);
        return temp;
    }
    if (s.empty())
        return temp;
    typedef string::const_iterator iter;
    const iter::difference_type f_size(distance(f.begin(), f.end()));
    iter i(s.begin());
    for (iter pos; (pos = search(i, s.end(), f.begin(), f.end())) != s.end();) {
        temp.push_back(string(i, pos));
        advance(pos, f_size);
        i = pos;
    }
    temp.push_back(string(i, s.end()));
    return temp;
}

int main(int argc, char** argv) {
	//signal_callback_handler (0);
	//signal (SIGINT, signal_callback_handler);

    string conf = _CONFIG_FILE;
    if (argc == 2)
        conf = string(argv[1]);
    try {
        ConfigFile *cf = new ConfigFile(conf.c_str());
        Communicator *c = Communicator::Get(cf->Get("communicator"));
        vector<string> plugins = split(cf->Get("plugins"), ",");
        Backend b(plugins);
        b.Start(c);
        delete c;
    } catch (string &e) {
        cerr << "Exception: " << e << endl;
    } catch (const char *e) {
        cerr << "Exception: " << e << endl;
    }
    return 0;
}

