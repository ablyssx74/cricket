/*
 * Copyright 2026, Kris Beazley supermusicthingy@epluribusunix.net
 * All rights reserved. Distributed under the terms of the MIT license.
 */

// DCC (Direct Client-to-Client) file transfers for Cricket.
//
// Supports DCC SEND in both directions, active and passive ("reverse", with a
// token) variants, and DCC RESUME / ACCEPT. Listen ports come from a small
// pool; each pool port is forwarded on the home router by a PortMapper
// (NAT-PMP / PCP / UPnP). When the reachability check says we can't be reached
// from the internet, outgoing offers switch to passive DCC automatically.
//
// The chat window (the "host") owns the IRC connections, so DccManager never
// writes to a server itself: it posts MSG_DCC_SEND_CTCP to the host, and
// MSG_DCC_LOG for lines that belong in a server log.

#ifndef CRICKET_DCC_H
#define CRICKET_DCC_H

#include <Locker.h>
#include <Looper.h>
#include <Messenger.h>
#include <String.h>
#include <SupportDefs.h>

#include <vector>

namespace cricket {
class PortMapper;
}

// Messages DccManager posts to the host window.
enum {
	// "server" (pointer), "target" (string), "ctcp" (string, without the \x01 framing)
	MSG_DCC_SEND_CTCP = 'dcSC',
	// "server" (pointer, may be NULL for "active buffer"), "text" (string)
	MSG_DCC_LOG = 'dcLG'
};

struct DccSettings {
	BString downloadDir;        // where received files go
	uint16  firstPort = 59200;  // first listen port of the pool
	int32   portCount = 5;      // pool size (simultaneous "waiting for peer" transfers)
	bool    usePortMapping = true;   // ask the router (UPnP / NAT-PMP / PCP) to forward the pool
	BString externalIP;         // manual override of the advertised address ("" = automatic)
	bool    forcePassive = false;    // always offer files with passive DCC
};

enum DccDirection {
	DCC_SEND = 0,
	DCC_RECEIVE
};

enum DccState {
	DCC_STATE_OFFERED = 0,  // incoming offer, waiting for the user to accept
	DCC_STATE_WAITING,      // waiting for the peer (to connect, reply or ACCEPT)
	DCC_STATE_CONNECTING,
	DCC_STATE_ACTIVE,
	DCC_STATE_DONE,
	DCC_STATE_FAILED,
	DCC_STATE_CANCELLED
};

// A copy of one transfer's state, safe to use outside the manager's lock.
struct DccTransferInfo {
	int32        id;
	DccDirection direction;
	DccState     state;
	BString      nick;
	BString      fileName;
	BString      path;
	off_t        size;
	off_t        bytesDone;
	bool         passive;
	BString      error;
	bigtime_t    startTime;  // when data started flowing (0 = not yet)
	bigtime_t    endTime;
	off_t        startBytes; // bytesDone when data started (resume offset)
};

class DccManager : public BLooper {
public:
	explicit DccManager(const BMessenger& host);
	virtual ~DccManager();

	// (Re)configure. Restarts the port mappers when the port pool or the
	// port-mapping switch changed. Call from any thread.
	void ApplySettings(const DccSettings& settings);

	// Cancels every transfer, removes the router mappings and waits for all
	// worker threads. Call once before quitting the looper.
	void Shutdown();

	// --- Called from the host window's thread ---

	// Handles a CTCP DCC request received from `nick` on `server`. `ctcp` is
	// the CTCP body without \x01 framing ("DCC SEND ..."). Returns true if it
	// was a DCC message (whether or not it was acted on).
	bool HandleCtcp(void* server, const BString& nick, const BString& ctcp);

	// Offers the file at `path` to `nick` on `server`.
	void OfferFile(void* server, const BString& nick, const BString& path);

	// The server node is going away (disconnected / removed).
	void ForgetServer(void* server);

	void ShowTransfersWindow();

	// --- Used by the transfers window ---
	std::vector<DccTransferInfo> Snapshot();
	BString StatusText();
	void Cancel(int32 id);
	void ClearFinished();
	BString DownloadDir();

	virtual void MessageReceived(BMessage* message);

private:
	struct Transfer;

	Transfer* _Find(int32 id);            // caller holds fLock
	Transfer* _NewTransfer(DccDirection dir, void* server, const BString& nick);
	void      _StartThread(Transfer* t);  // caller holds fLock
	static int32 _ThreadEntry(void* data);
	void      _RunSend(int32 id);
	void      _RunReceive(int32 id);
	void      _Finish(int32 id, DccState state, const char* error);

	void      _HandleIncomingSend(void* server, const BString& nick,
	              const BString& fileName, const BString& host, uint16 port,
	              off_t size, const BString& token);
	void      _PromptForOffer(int32 id, bool canResume);
	void      _AcceptOffer(int32 id, bool resume);

	bool      _AllocListenPort(uint16& outPort, int& outFd);  // caller holds fLock
	void      _ReleaseListenPort(uint16 port);                // caller holds fLock

	BString   _AdvertisedIP();   // "" when we think we can't be reached
	bool      _UsePassive();
	uint32    _LocalIPv4();      // host byte order, 0 if unknown

	void      _StartPortMappers();  // caller holds fLock
	void      _StopPortMappers();   // must NOT hold fLock (joins threads)

	void      _SendCtcp(void* server, const BString& target, const BString& ctcp);
	void      _Log(void* server, const BString& text);

	BMessenger   fHost;
	BLocker      fLock;
	DccSettings  fSettings;
	std::vector<Transfer*> fTransfers;
	int32        fNextId;

	std::vector<bool>                 fPortBusy;
	std::vector<bool>                 fPortMapped;  // per pool port, from PORT_MAP_REPORT
	std::vector<cricket::PortMapper*> fMappers;

	// Port-mapping / reachability status (from PORT_MAP_REPORT), guarded by fLock
	BString      fMapStatus;
	BString      fReachStatus;
	int          fReachable;     // 1 yes, 0 no (CGNAT/double NAT), -1 unknown
	BString      fInternetIP;
	BString      fRouterIP;      // router's WAN address from the mapper
	int32        fMappedCount;

	BMessenger   fWindow;        // transfers window, if open
	bool         fShuttingDown;
};

extern DccManager* gDccManager;

#endif
