#ifndef __XRD_LINK_H__
#define __XRD_LINK_H__
/******************************************************************************/
/*                                                                            */
/*                            X r d L i n k . h h                             */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

#include "XrdSys/XrdSysPthread.hh"

#include "Xrd/XrdJob.hh"
#include "Xrd/XrdLinkMatch.hh"
#include "Xrd/XrdProtocol.hh"
  
/******************************************************************************/
/*                       X r d L i n k   O p t i o n s                        */
/******************************************************************************/
  
#define XRDLINK_RDLOCK  0x0001
#define XRDLINK_NOCLOSE 0x0002

/******************************************************************************/
/*                      C l a s s   D e f i n i t i o n                       */
/******************************************************************************/
  
class XrdNetBuffer;
class XrdNetPeer;
class XrdPoll;

class XrdLink : XrdJob
{
public:
friend class XrdLinkScan;
friend class XrdPoll;
friend class XrdPollPoll;
friend class XrdPollDev;

static XrdLink *Alloc(XrdNetPeer &Peer, int opts=0);

int           Client(char *buff, int blen);

int           Close(int defer=0);

void          DoIt();

int           FDnum() {return FD;}

static XrdLink *fd2link(int fd)
                {if (fd < 0) fd = -fd; 
                 return (fd <= LTLast && LinkBat[fd] ? LinkTab[fd] : 0);
                }

static XrdLink *fd2link(int fd, unsigned int inst)
                {if (fd < 0) fd = -fd; 
                 if (fd <= LTLast && LinkBat[fd] && LinkTab[fd]
                 && LinkTab[fd]->Instance == inst) return LinkTab[fd];
                 return (XrdLink *)0;
                }

static XrdLink *Find(int &curr, XrdLinkMatch *who=0);

       int    getIOStats(long long &inbytes, long long &outbytes,
                              int  &numstall,     int  &numtardy)
                        { inbytes = BytesIn + BytesInTot;
                         outbytes = BytesOut+BytesOutTot;
                         numstall = stallCnt + stallCntTot;
                         numtardy = tardyCnt + tardyCntTot;
                         return InUse;
                        }

static int    getName(int &curr, char *bname, int blen, XrdLinkMatch *who=0);

XrdProtocol  *getProtocol() {return Protocol;} // opmutex must be locked

void          Hold(int lk) {(lk ? opMutex.Lock() : opMutex.UnLock());}

char         *ID;      // This is referenced a lot

unsigned int  Inst() {return Instance;}

int           isFlawed() {return Etext != 0;}

int           isInstance(unsigned int inst)
                        {return FD >= 0 && Instance == inst;}

const char   *Name(sockaddr *ipaddr=0)
                     {if (ipaddr) memcpy(ipaddr, &InetAddr, sizeof(sockaddr));
                      return (const char *)Lname;
                     }

const char   *Host(sockaddr *ipaddr=0)
                     {if (ipaddr) memcpy(ipaddr, &InetAddr, sizeof(sockaddr));
                      return (const char *)HostName;
                     }

int           Peek(char *buff, int blen, int timeout=-1);

int           Recv(char *buff, int blen);
int           Recv(char *buff, int blen, int timeout);

int           RecvAll(char *buff, int blen);

int           Send(const char *buff, int blen);
int           Send(const struct iovec *iov, int iocnt, int bytes=0);

int           setEtext(const char *text);

void          setID(const char *userid, int procid);

void          Serialize();                              // ASYNC Mode

void          setRef(int cnt);                          // ASYNC Mode

static int    Setup(int maxfd, int idlewait);

static int    Stats(char *buff, int blen, int do_sync=0);

       void   syncStats(int *ctime=0);

XrdProtocol  *setProtocol(XrdProtocol *pp);

       int    Terminate(const XrdLink *owner, int fdnum, unsigned int inst);

time_t        timeCon() {return conTime;}

int           UseCnt() {return InUse;}

              XrdLink();
             ~XrdLink() {}  // Is never deleted!

private:

void   Reset();

static XrdSysMutex   LTMutex;    // For the LinkTab only LTMutex->IOMutex allowed
static XrdLink     **LinkTab;
static char         *LinkBat;
static unsigned int  LinkAlloc;
static int           LTLast;
static const char   *TraceID;
static int           devNull;

// Statistical area (global and local)
//
static long long    LinkBytesIn;
static long long    LinkBytesOut;
static long long    LinkConTime;
static long long    LinkCountTot;
static int          LinkCount;
static int          LinkCountMax;
static int          LinkTimeOuts;
static int          LinkStalls;
       long long        BytesIn;
       long long        BytesInTot;
       long long        BytesOut;
       long long        BytesOutTot;
       int              stallCnt;
       int              stallCntTot;
       int              tardyCnt;
       int              tardyCntTot;
static XrdSysMutex  statsMutex;

// Identification section
//
struct sockaddr     InetAddr;
char                Uname[24];  // Uname and Lname must be adjacent!
char                Lname[232];
char               *HostName;
int                 HNlen;

XrdSysMutex         opMutex;
XrdSysMutex         rdMutex;
XrdSysMutex         wrMutex;
XrdSysSemaphore     IOSemaphore;
XrdLink            *Next;
XrdNetBuffer       *udpbuff;
XrdProtocol        *Protocol;
XrdProtocol        *ProtoAlt;
XrdPoll            *Poller;
struct pollfd      *PollEnt;
char               *Etext;
int                 FD;
unsigned int        Instance;
time_t              conTime;
int                 InUse;
int                 doPost;
char                LockReads;
char                KeepFD;
char                isEnabled;
char                isIdle;
char                inQ;
};
#endif
