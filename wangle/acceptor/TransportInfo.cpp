/*
 *  Copyright (c) 2015, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include <wangle/acceptor/TransportInfo.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <folly/io/async/AsyncSocket.h>

using std::chrono::microseconds;
using std::map;
using std::string;

namespace wangle {

bool TransportInfo::initWithSocket(const folly::AsyncSocket* sock) {
#if defined(__linux__) || defined(__FreeBSD__)
  if (!TransportInfo::readTcpInfo(&tcpinfo, sock)) {
    tcpinfoErrno = errno;
    return false;
  }
  rtt = microseconds(tcpinfo.tcpi_rtt);
  cwnd = tcpinfo.tcpi_snd_cwnd;
  mss = tcpinfo.tcpi_snd_mss;
  /* The ratio of packet retransmission (rtx) is a good indicator of network
   * bandwidth condition. Unfortunately, the number of segmentOut is not
   * available in current tcpinfo.  To workaround this limitation, totalBytes
   * and MSS are used to estimate it.
   */
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 17
  if (tcpinfo.tcpi_total_retrans == 0) {
    rtx = 0;
  } else if (tcpinfo.tcpi_total_retrans > 0 && tcpinfo.tcpi_snd_mss > 0 &&
      totalBytes > 0) {
    // numSegmentOut is the underestimation of the number of tcp packets sent
    double numSegmentOut = double(totalBytes) / tcpinfo.tcpi_snd_mss;
    // so rtx is the overestimation of actual packet retransmission rate
    rtx = tcpinfo.tcpi_total_retrans / numSegmentOut;
  } else {
    rtx = -1;
  }
#else
    rtx = -1;
#endif  // __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 17
  validTcpinfo = true;
#else
  tcpinfoErrno = EINVAL;
  rtt = microseconds(-1);
  rtx = -1;
  cwnd = -1;
  mss = -1;
#endif
  return true;
}

int64_t TransportInfo::readRTT(const folly::AsyncSocket* sock) {
#if defined(__linux__) || defined(__FreeBSD__)
  struct tcp_info tcpinfo;
  if (!TransportInfo::readTcpInfo(&tcpinfo, sock)) {
    return -1;
  }
  return tcpinfo.tcpi_rtt;
#else
  return -1;
#endif
}

#if defined(__linux__) || defined(__FreeBSD__)
bool TransportInfo::readTcpInfo(struct tcp_info* tcpinfo,
                                const folly::AsyncSocket* sock) {
  socklen_t len = sizeof(struct tcp_info);
  if (!sock) {
    return false;
  }
  if (getsockopt(sock->getFd(), IPPROTO_TCP,
                 TCP_INFO, (void*) tcpinfo, &len) < 0) {
    VLOG(4) << "Error calling getsockopt(): " << strerror(errno);
    return false;
  }
  return true;
}
#endif

} // namespace wangle
