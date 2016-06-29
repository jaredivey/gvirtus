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
 * @file   TapCommunicator.h
 * @author Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>
 * @date   Thu Oct 8 12:08:33 2009
 * 
 * @brief  
 * 
 * 
 */

#ifndef _TAPCOMMUNICATOR_H
#define	_TAPCOMMUNICATOR_H

#include <ext/stdio_filebuf.h>

#include "Communicator.h"

/**
 * TapCommunicator implements a Communicator for the TCP/IP socket.
 */
class TapCommunicator : public Communicator {
public:
    TapCommunicator(const std::string & communicator);
    TapCommunicator(const char *hostname, short port);
    TapCommunicator(int fd, const char *hostname);
    virtual ~TapCommunicator();
    int CreateTap (const char *dev, const char *gw, const char *ip, const char *mac, const char *mode, const char *netmask);
    void Serve();
    const Communicator * const Accept() const;
    void Connect();
    size_t Read(char *buffer, size_t size);
    size_t Write(const char *buffer, size_t size);
    void Sync();
    void Close();
private:
    void InitializeStream();
    std::istream *mpInput;
    std::ostream *mpOutput;
    std::string mHostname;
    char * mInAddr;
    int mInAddrSize;
    short mPort;
    int mSocketFd;
    __gnu_cxx::stdio_filebuf<char> *mpInputBuf;
    __gnu_cxx::stdio_filebuf<char> *mpOutputBuf;
    static int tap_value;
    static short port_value;
};

#endif	/* _TCPCOMMUNICATOR_H */

