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
 * @file   TapCommunicator.cpp
 * @author Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>
 * @date   Thu Oct 8 12:08:33 2009
 *
 * @brief
 *
 *
 */

#include "TapCommunicator.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <stdint.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cerrno>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <net/route.h>
#include <netinet/in.h>

using namespace std;

#define TAP_MAGIC 95549

static int gVerbose = 1; // Set to true to turn on logging messages.

#define LOG(msg) \
  if (gVerbose) \
    { \
      std::cout << __FUNCTION__ << "(): " << msg << std::endl;   \
    }

#define ABORT(msg, printErrno) \
  std::cout << __FILE__ << ": fatal error at line " << __LINE__ << ": " << __FUNCTION__ << "(): " << msg << std::endl; \
  if (printErrno) \
    { \
      std::cout << "    errno = " << errno << " (" << std::strerror (errno) << ")" << std::endl; \
    } \
  std::exit (-1);

#define ABORT_IF(cond, msg, printErrno) \
  if (cond) \
    { \
      ABORT (msg, printErrno); \
    }

//
// Lots of the following helper code taken from corresponding functions in src/node.
//
#define ASCII_DOT (0x2e)
#define ASCII_ZERO (0x30)
#define ASCII_a (0x41)
#define ASCII_z (0x5a)
#define ASCII_A (0x61)
#define ASCII_Z (0x7a)
#define ASCII_COLON (0x3a)
#define ASCII_ZERO (0x30)

//static char
//AsciiToLowCase (char c)
//{
//  if (c >= ASCII_a && c <= ASCII_z) {
//      return c;
//    } else if (c >= ASCII_A && c <= ASCII_Z) {
//      return c + (ASCII_a - ASCII_A);
//    } else {
//      return c;
//    }
//}
//
//static void
//AsciiToMac48 (const char *str, uint8_t addr[6])
//{
//  int i = 0;
//  while (*str != 0 && i < 6)
//    {
//      uint8_t byte = 0;
//      while (*str != ASCII_COLON && *str != 0)
//        {
//          byte <<= 4;
//          char low = AsciiToLowCase (*str);
//          if (low >= ASCII_a)
//            {
//              byte |= low - ASCII_a + 10;
//            }
//          else
//            {
//              byte |= low - ASCII_ZERO;
//            }
//          str++;
//        }
//      addr[i] = byte;
//      i++;
//      if (*str == 0)
//        {
//          break;
//        }
//      str++;
//    }
//}

static uint32_t
AsciiToIpv4 (const char *address)
{
  uint32_t host = 0;
  while (true) {
      uint8_t byte = 0;
      while (*address != ASCII_DOT &&
             *address != 0) {
          byte *= 10;
          byte += *address - ASCII_ZERO;
          address++;
        }
      host <<= 8;
      host |= byte;
      if (*address == 0) {
          break;
        }
      address++;
    }
  return host;
}

static sockaddr
CreateInetAddress (uint32_t networkOrder)
{
  union {
    struct sockaddr any_socket;
    struct sockaddr_in si;
  } s;
  s.si.sin_family = AF_INET;
  s.si.sin_port = 0; // unused
  s.si.sin_addr.s_addr = htonl (networkOrder);
  return s.any_socket;
}

int TapCommunicator::tap_value = 0;
short TapCommunicator::port_value = 50;

TapCommunicator::TapCommunicator(const std::string& communicator)
{
    mPort = port_value;
    std::ostringstream oss;
    oss << "128.0.0." << tap_value + 1;
    mHostname = oss.str();
    struct hostent *ent = gethostbyname(mHostname.c_str());
    if (ent == NULL)
        throw "TapCommunicator: Can't resolve hostname '" + mHostname + "'.";
    mInAddrSize = ent->h_length;
    mInAddr = new char[mInAddrSize];
    memcpy(mInAddr, *ent->h_addr_list, mInAddrSize);
}

TapCommunicator::TapCommunicator(const char *hostname, short port)
{
    std::ostringstream oss;
    oss << "128.0.0." << tap_value + 1;
    mHostname = oss.str();
    struct hostent *ent = gethostbyname(mHostname.c_str());
    if (ent == NULL)
        throw "TapCommunicator: Can't resolve hostname '" + mHostname + "'.";
    mInAddrSize = ent->h_length;
    mInAddr = new char[mInAddrSize];
    memcpy(mInAddr, *ent->h_addr_list, mInAddrSize);
    mPort = port_value;
}

TapCommunicator::TapCommunicator(int fd, const char *hostname)
{
    mSocketFd = fd;
    InitializeStream();
}

TapCommunicator::~TapCommunicator()
{
    delete[] mInAddr;
}

int TapCommunicator::CreateTap (const char *dev, const char *gw, const char *ip, const char *mac, const char *mode, const char *netmask)
{
  //
  // Creation and management of Tap devices is done via the tun device
  //
  int tap = open ("/dev/net/tun", O_RDWR);
  ABORT_IF (tap == -1, "Could not open /dev/net/tun", true);

  //
  // Allocate a tap device, making sure that it will not send the tun_pi header.
  // If we provide a null name to the ifr.ifr_name, we tell the kernel to pick
  // a name for us (i.e., tapn where n = 0..255.
  //
  // If the device does not already exist, the system will create one.
  //
  struct ifreq ifr;
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strcpy (ifr.ifr_name, dev);
  int status = ioctl (tap, TUNSETIFF, (void *) &ifr);
  ABORT_IF (status == -1, "Could not allocate tap device", true);

  std::string tapDeviceName = (char *)ifr.ifr_name;
  LOG ("Allocated TAP device " << tapDeviceName);

  //
  // Operating mode "2" corresponds to USE_LOCAL and "3" to USE_BRIDGE mode.
  // This means that we expect that the user will have named, created and
  // configured a network tap that we are just going to use.  So don't mess
  // up his hard work by changing anything, just return the tap fd.
  //
  if (strcmp (mode, "2") == 0 || strcmp (mode, "3") == 0)
    {
      LOG ("Returning precreated tap ");
      return tap;
    }

  //
  // Set the hardware (MAC) address of the new device
  //
//  ifr.ifr_hwaddr.sa_family = 1; // this is ARPHRD_ETHER from if_arp.h
//  AsciiToMac48 ("90:b1:1c:8e:1c:80", (uint8_t*)ifr.ifr_hwaddr.sa_data);
//  status = ioctl (tap, SIOCSIFHWADDR, &ifr);
//  ABORT_IF (status == -1, "Could not set MAC address", true);
//  LOG ("Set device MAC address to " << mac);

  status = ioctl(tap, TUNSETPERSIST, 0);

  int fd = socket (AF_INET, SOCK_DGRAM, 0);

  //
  // Bring the interface up.
  //
  status = ioctl (fd, SIOCGIFFLAGS, &ifr);
  ABORT_IF (status == -1, "Could not get flags for interface", true);
  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
  status = ioctl (fd, SIOCSIFFLAGS, &ifr);
  ABORT_IF (status == -1, "Could not bring interface up", true);
  LOG ("Device is up");

  //
  // Set the IP address of the new interface/device.
  //
  ifr.ifr_addr = CreateInetAddress (AsciiToIpv4 (ip));
  status = ioctl (fd, SIOCSIFADDR, &ifr);
  ABORT_IF (status == -1, "Could not set IP address", true);
  LOG ("Set device IP address to " << ip);

  //
  // Set the net mask of the new interface/device
  //
  ifr.ifr_netmask = CreateInetAddress (AsciiToIpv4 (netmask));
  status = ioctl (fd, SIOCSIFNETMASK, &ifr);
  ABORT_IF (status == -1, "Could not set net mask", true);
  LOG ("Set device Net Mask to " << netmask);

  return tap;
}

void TapCommunicator::Serve() {
    struct sockaddr_in socket_addr;

    std::ostringstream oss;
    oss << "128.0.0." << tap_value + 1;
    mHostname = oss.str();
    struct hostent *ent = gethostbyname(mHostname.c_str());
    if (ent == NULL)
        throw "TapCommunicator: Can't resolve hostname '" + mHostname + "'.";
    mInAddrSize = ent->h_length;
    mInAddr = new char[mInAddrSize];
    memcpy(mInAddr, *ent->h_addr_list, mInAddrSize);
    mPort = port_value;
    char tapname[7];
    sprintf (tapname, "tap%d", tap_value);
    CreateTap (tapname, "", mHostname.c_str(), "", "1", "255.255.255.0");

    if ((mSocketFd = socket (AF_INET, SOCK_STREAM, 0)) == 0)
        throw "TapCommunicator: Can't create socket.";

    memset((char *) &socket_addr, 0, sizeof(struct sockaddr_in));

    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons(port_value);
    socket_addr.sin_addr.s_addr = inet_addr(mHostname.c_str());

    char on = 1;
    setsockopt(mSocketFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));

    int result = bind(mSocketFd, (struct sockaddr *) & socket_addr,
            sizeof (struct sockaddr_in));
    if( result != 0) {
    	std::cout << "Result: " << strerror(errno) << std::endl;
        throw " TapCommunicator: Can't bind socket.";
    }

    // Proactively create next tap for next simulated node
    ++tap_value;

    if (listen(mSocketFd, 5) != 0)
        throw "TapCommunicator: Can't listen from socket.";
}

const Communicator * const TapCommunicator::Accept() const {
    unsigned client_socket_fd;
    struct sockaddr_in client_socket_addr;
    unsigned client_socket_addr_size;
    client_socket_addr_size = sizeof (struct sockaddr_in);
    if ((client_socket_fd = accept(mSocketFd,
            (sockaddr *) & client_socket_addr,
            &client_socket_addr_size)) == 0)
        throw "TapCommunicator: Error while accepting connection.";

    return new TapCommunicator(client_socket_fd,
            inet_ntoa(client_socket_addr.sin_addr));
}

void TapCommunicator::Connect() {
    struct sockaddr_in remote;

    if ((mSocketFd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        throw "TapCommunicator: Can't create socket.";

    remote.sin_family = AF_INET;
    remote.sin_port = htons(port_value);
    memcpy(&remote.sin_addr, mInAddr, mInAddrSize);

    if (connect(mSocketFd, (struct sockaddr *) & remote,
            sizeof (struct sockaddr_in)) != 0)
        throw "TapCommunicator: Can't connect to socket.";
    InitializeStream();
    ++tap_value;
}

void TapCommunicator::Close() {
	if (mSocketFd != -1)
	{
		close (mSocketFd);
		mSocketFd = -1;
	}
}

size_t TapCommunicator::Read(char* buffer, size_t size) {
    mpInput->read(buffer, size);
    if (mpInput->bad() || mpInput->eof())
        return 0;
    return size;
}

size_t TapCommunicator::Write(const char* buffer, size_t size) {
    mpOutput->write(buffer, size);
    return size;
}

void TapCommunicator::Sync() {
    mpOutput->flush();
}

void TapCommunicator::InitializeStream() {
	mpInputBuf = new __gnu_cxx::stdio_filebuf<char>(mSocketFd, ios_base::in);
	mpOutputBuf = new __gnu_cxx::stdio_filebuf<char>(mSocketFd, ios_base::out);
	mpInput = new istream(mpInputBuf);
	mpOutput = new ostream(mpOutputBuf);
}

