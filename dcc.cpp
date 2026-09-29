/*
 * Copyright 2026, Kris Beazley supermusicthingy@epluribusunix.net
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "dcc.h"

#include "PortMapper.h"

#include <Alert.h>
#include <Application.h>
#include <Autolock.h>
#include <Button.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Invoker.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <MessageRunner.h>
#include <Mime.h>
#include <Notification.h>
#include <Path.h>
#include <Roster.h>
#include <ScrollView.h>
#include <StringView.h>
#include <Window.h>

#include <arpa/inet.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

DccManager* gDccManager = nullptr;

// Private messages
enum {
	MSG_DCC_ALERT_REPLY   = 'dcAR',  // "id", "resume" (bool), "which" (from BAlert)
	MSG_DCC_TRANSFER_DONE = 'dcTD',  // "id"
	MSG_DCC_REFRESH       = 'dcRF',  // transfers window timer
	MSG_DCC_CANCEL        = 'dcCN',
	MSG_DCC_CLEAR         = 'dcCL',
	MSG_DCC_OPEN_FOLDER   = 'dcOF',
	MSG_DCC_OPEN_ITEM     = 'dcOI',
	MSG_DCC_ACTIVATE      = 'dcAC'
};

static const bigtime_t kAcceptTimeout  = 180 * 1000000LL;  // peer has 3 minutes to connect
static const bigtime_t kConnectTimeout = 30 * 1000000LL;
static const bigtime_t kStallTimeout   = 120 * 1000000LL;  // no data for 2 minutes = dead
static const bigtime_t kFinalAckWait   = 30 * 1000000LL;
static const int32     kMaxPendingPerNick = 3;
static const size_t    kChunkSize = 64 * 1024;


// =============================================================================
// Helpers
// =============================================================================

namespace {

BString
FormatSize(off_t bytes)
{
	char buf[64];
	if (bytes < 1024)
		snprintf(buf, sizeof(buf), "%lld B", (long long)bytes);
	else if (bytes < 1024 * 1024)
		snprintf(buf, sizeof(buf), "%.1f KiB", bytes / 1024.0);
	else if (bytes < 1024LL * 1024 * 1024)
		snprintf(buf, sizeof(buf), "%.1f MiB", bytes / (1024.0 * 1024.0));
	else
		snprintf(buf, sizeof(buf), "%.2f GiB", bytes / (1024.0 * 1024.0 * 1024.0));
	return BString(buf);
}


bool
IsNumber(const BString& s)
{
	if (s.Length() == 0)
		return false;
	for (int32 i = 0; i < s.Length(); i++) {
		if (s[i] < '0' || s[i] > '9')
			return false;
	}
	return true;
}


// An address field in a DCC request: a decimal IPv4 number, a dotted quad or
// an IPv6 literal.
bool
LooksLikeAddress(const BString& s)
{
	if (IsNumber(s))
		return true;
	for (int32 i = 0; i < s.Length(); i++) {
		char c = s[i];
		bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')
			|| c == '.' || c == ':';
		if (!ok)
			return false;
	}
	return s.Length() > 0;
}


BString
IPv4ToString(uint32 hostOrder)
{
	char buf[24];
	snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (hostOrder >> 24) & 0xFF,
		(hostOrder >> 16) & 0xFF, (hostOrder >> 8) & 0xFF, hostOrder & 0xFF);
	return BString(buf);
}


// Dotted quad -> host-order number, 0 if it isn't one.
uint32
ParseIPv4(const BString& s)
{
	struct in_addr addr;
	if (inet_pton(AF_INET, s.String(), &addr) != 1)
		return 0;
	return ntohl(addr.s_addr);
}


bool
IsPrivateIPv4(uint32 ip)
{
	uint32 a = (ip >> 24) & 0xFF, b = (ip >> 16) & 0xFF;
	return a == 10 || a == 127 || a == 0
		|| (a == 172 && b >= 16 && b <= 31)
		|| (a == 192 && b == 168)
		|| (a == 100 && b >= 64 && b <= 127)   // carrier-grade NAT
		|| (a == 169 && b == 254);
}


// The address as it appears in a DCC request -> something connect() can use.
BString
DccHostToString(const BString& field)
{
	if (IsNumber(field))
		return IPv4ToString((uint32)strtoul(field.String(), nullptr, 10));
	return field;
}


// Keeps only a safe bare file name: no directories, no control characters,
// no leading dots (hidden files / "..").
BString
SanitizeFileName(const BString& name)
{
	BString base(name);
	int32 slash = base.FindLast('/');
	int32 backslash = base.FindLast('\\');
	int32 cut = slash > backslash ? slash : backslash;
	if (cut >= 0)
		base.Remove(0, cut + 1);

	BString clean;
	for (int32 i = 0; i < base.Length(); i++) {
		unsigned char c = (unsigned char)base[i];
		if (c < 0x20 || c == 0x7F || c == ':')
			continue;
		clean.Append((char)c, 1);
	}
	clean.Trim();
	while (clean.StartsWith("."))
		clean.Remove(0, 1);
	if (clean.Length() > 200)
		clean.Truncate(200);
	if (clean.Length() == 0)
		clean = "dcc_file";
	return clean;
}


bool
PathExists(const BString& path)
{
	struct stat st;
	return stat(path.String(), &st) == 0;
}


// "dir/name", or "dir/name (1).ext" etc. if that already exists.
BString
UniquePath(const BString& dir, const BString& name)
{
	BString path(dir);
	path << "/" << name;
	if (!PathExists(path))
		return path;

	BString base(name), ext;
	int32 dot = name.FindLast('.');
	if (dot > 0) {
		name.CopyInto(base, 0, dot);
		name.CopyInto(ext, dot, name.Length() - dot);
	}
	for (int n = 1; n < 1000; n++) {
		path = dir;
		path << "/" << base << " (" << n << ")" << ext;
		if (!PathExists(path))
			return path;
	}
	path = dir;
	path << "/" << base << " (" << system_time() << ")" << ext;
	return path;
}


BString
QuoteFileName(const BString& name)
{
	BString q(name);
	q.ReplaceAll('"', '\'');
	if (q.FindFirst(' ') >= 0) {
		q.Prepend("\"");
		q.Append("\"");
	}
	return q;
}


// Splits "DCC <TYPE> <file name> <args...>". The file name may be quoted; if
// it isn't, a name containing spaces is recovered by taking the expected
// number of trailing arguments.
bool
ParseDccRequest(const BString& ctcp, BString& type, BString& name, std::vector<BString>& args)
{
	BString rest(ctcp);
	rest.Trim();
	if (rest.ICompare("DCC ", 4) != 0)
		return false;
	rest.Remove(0, 4);
	rest.Trim();

	int32 sp = rest.FindFirst(' ');
	if (sp < 0) {
		type = rest;
		type.ToUpper();
		return true;
	}
	rest.CopyInto(type, 0, sp);
	type.ToUpper();
	rest.Remove(0, sp + 1);
	rest.Trim();

	auto split = [](const BString& s, std::vector<BString>& out) {
		int32 pos = 0;
		while (pos < s.Length()) {
			while (pos < s.Length() && s[pos] == ' ')
				pos++;
			if (pos >= s.Length())
				break;
			int32 end = s.FindFirst(' ', pos);
			if (end < 0)
				end = s.Length();
			BString tok;
			s.CopyInto(tok, pos, end - pos);
			out.push_back(tok);
			pos = end;
		}
	};

	if (rest.StartsWith("\"")) {
		int32 close = rest.FindFirst('"', 1);
		if (close < 0)
			return false;
		rest.CopyInto(name, 1, close - 1);
		BString tail;
		rest.CopyInto(tail, close + 1, rest.Length() - close - 1);
		split(tail, args);
		return true;
	}

	std::vector<BString> tokens;
	split(rest, tokens);
	int n = (int)tokens.size();
	int argCount = 0;
	if (type == "SEND") {
		// ip port size [token]
		argCount = (n >= 5 && LooksLikeAddress(tokens[n - 4]) && IsNumber(tokens[n - 3])
			&& IsNumber(tokens[n - 2]) && IsNumber(tokens[n - 1])) ? 4 : 3;
	} else if (type == "RESUME" || type == "ACCEPT") {
		// port position [token]
		argCount = (n >= 4 && IsNumber(tokens[n - 3]) && IsNumber(tokens[n - 2])
			&& IsNumber(tokens[n - 1])) ? 3 : 2;
	} else {
		argCount = n > 0 ? n - 1 : 0;
	}
	if (n < argCount + 1)
		return false;
	for (int i = 0; i < n - argCount; i++) {
		if (i > 0)
			name << " ";
		name << tokens[i];
	}
	for (int i = n - argCount; i < n; i++)
		args.push_back(tokens[i]);
	return true;
}


void
SetNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


// Waits up to `timeout` for fd to become readable (write=false) or writable,
// in short slices so `cancel` is noticed. Returns 1 ready, 0 timeout, -1 cancel/error.
int
WaitFd(int fd, bool write, bigtime_t timeout, volatile bool* cancel)
{
	bigtime_t deadline = system_time() + timeout;
	while (true) {
		if (cancel != nullptr && *cancel)
			return -1;
		bigtime_t left = deadline - system_time();
		if (left <= 0)
			return 0;
		if (left > 250000)
			left = 250000;
		fd_set set;
		FD_ZERO(&set);
		FD_SET(fd, &set);
		struct timeval tv;
		tv.tv_sec = left / 1000000;
		tv.tv_usec = left % 1000000;
		int rv = write ? select(fd + 1, nullptr, &set, nullptr, &tv)
			: select(fd + 1, &set, nullptr, nullptr, &tv);
		if (rv > 0)
			return 1;
		if (rv < 0 && errno != EINTR)
			return -1;
	}
}


// Sends all of buf on a non-blocking socket. False on error, stall or cancel.
bool
SendAll(int fd, const char* buf, size_t len, volatile bool* cancel,
	bigtime_t stallTimeout = kStallTimeout)
{
	while (len > 0) {
		ssize_t n = send(fd, buf, len, 0);
		if (n > 0) {
			buf += n;
			len -= (size_t)n;
			continue;
		}
		if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			return false;
		if (WaitFd(fd, true, stallTimeout, cancel) != 1)
			return false;
	}
	return true;
}


bool
WriteAll(int fd, const char* buf, size_t len)
{
	while (len > 0) {
		ssize_t n = write(fd, buf, len);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return false;
		}
		buf += n;
		len -= (size_t)n;
	}
	return true;
}


// Non-blocking connect to host:port with a timeout; returns fd or -1.
int
ConnectTo(const BString& host, uint16 port, volatile bool* cancel, BString& error)
{
	char portStr[8];
	snprintf(portStr, sizeof(portStr), "%u", port);
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo* res = nullptr;
	if (getaddrinfo(host.String(), portStr, &hints, &res) != 0 || res == nullptr) {
		error = "Could not resolve the peer's address";
		return -1;
	}

	int result = -1;
	for (struct addrinfo* ai = res; ai != nullptr && result < 0; ai = ai->ai_next) {
		int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0)
			continue;
		SetNonBlocking(fd);
		int rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
		if (rc != 0 && errno != EINPROGRESS) {
			close(fd);
			continue;
		}
		if (rc != 0) {
			if (WaitFd(fd, true, kConnectTimeout, cancel) != 1) {
				close(fd);
				continue;
			}
			int soErr = 0;
			socklen_t len = sizeof(soErr);
			if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soErr, &len) < 0 || soErr != 0) {
				close(fd);
				continue;
			}
		}
		result = fd;
	}
	freeaddrinfo(res);
	if (result < 0 && error.Length() == 0) {
		error = "Could not connect to the peer at ";
		error << host << ":" << port;
	}
	return result;
}

}  // namespace


// =============================================================================
// Transfer record
// =============================================================================

struct DccManager::Transfer {
	DccTransferInfo info;
	void*     server = nullptr;
	BString   token;               // passive DCC token
	BString   remoteHost;
	uint16    remotePort = 0;
	uint16    listenPort = 0;
	int       listenFd = -1;
	int       sockFd = -1;
	off_t     position = 0;        // resume offset
	bool      connectMode = false; // SEND: connect to the peer (passive reply) instead of accepting
	bool      listenMode = false;  // RECEIVE: accept a connection (passive offer) instead of connecting
	bool      resumeRequested = false;
	thread_id thread = -1;
	bool      threadRunning = false;
	volatile bool cancel = false;
};

struct DccThreadArgs {
	DccManager* manager;
	int32       id;
};


// =============================================================================
// Transfers window
// =============================================================================

namespace {

class DccTransferItem : public BListItem {
public:
	explicit DccTransferItem(const DccTransferInfo& info) : fInfo(info) {}

	void SetInfo(const DccTransferInfo& info) { fInfo = info; }
	const DccTransferInfo& Info() const { return fInfo; }

	virtual void Update(BView* owner, const BFont* font)
	{
		BListItem::Update(owner, font);
		font_height fh;
		font->GetHeight(&fh);
		fLineHeight = ceilf(fh.ascent + fh.descent + fh.leading);
		SetHeight(fLineHeight * 3 + 14);
	}

	virtual void DrawItem(BView* owner, BRect frame, bool complete)
	{
		rgb_color bg = IsSelected() ? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR)
			: ui_color(B_LIST_BACKGROUND_COLOR);
		rgb_color fg = IsSelected() ? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR)
			: ui_color(B_LIST_ITEM_TEXT_COLOR);
		owner->SetLowColor(bg);
		owner->FillRect(frame, B_SOLID_LOW);
		owner->SetHighColor(fg);

		font_height fh;
		owner->GetFontHeight(&fh);
		float x = frame.left + 8;
		float y = frame.top + 4 + fh.ascent;

		// Line 1: direction, file name and peer
		BString title;
		title << (fInfo.direction == DCC_SEND ? "\xE2\x86\x91 " : "\xE2\x86\x93 ")  // ↑ / ↓
			<< fInfo.fileName << "  \xE2\x80\x94  "
			<< (fInfo.direction == DCC_SEND ? "to " : "from ") << fInfo.nick;
		BFont bold;
		owner->GetFont(&bold);
		BFont regular(bold);
		bold.SetFace(B_BOLD_FACE);
		owner->SetFont(&bold);
		owner->DrawString(title.String(), BPoint(x, y));
		owner->SetFont(&regular);

		// Line 2: progress bar
		float barTop = frame.top + 8 + fLineHeight;
		BRect bar(x, barTop, frame.right - 8, barTop + fLineHeight - 4);
		owner->SetHighColor(tint_color(bg, B_DARKEN_2_TINT));
		owner->StrokeRect(bar);
		float fraction = 0;
		if (fInfo.size > 0)
			fraction = (float)((double)fInfo.bytesDone / (double)fInfo.size);
		else if (fInfo.state == DCC_STATE_DONE)
			fraction = 1;
		if (fraction > 1)
			fraction = 1;
		if (fraction > 0) {
			BRect fill = bar.InsetByCopy(1, 1);
			fill.right = fill.left + (fill.Width() * fraction);
			rgb_color barColor = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
			if (fInfo.state == DCC_STATE_FAILED || fInfo.state == DCC_STATE_CANCELLED)
				barColor = tint_color(ui_color(B_FAILURE_COLOR), B_LIGHTEN_1_TINT);
			else if (fInfo.state == DCC_STATE_DONE)
				barColor = ui_color(B_SUCCESS_COLOR);
			owner->SetHighColor(barColor);
			owner->FillRect(fill);
		}

		// Line 3: status text
		owner->SetHighColor(fg);
		owner->DrawString(StatusLine().String(), BPoint(x, frame.bottom - 4 - fh.descent));
	}

	BString StatusLine() const
	{
		BString s;
		const char* peer = fInfo.nick.String();
		switch (fInfo.state) {
			case DCC_STATE_OFFERED:
				s << "Waiting for you to accept (" << FormatSize(fInfo.size) << ")";
				break;
			case DCC_STATE_WAITING:
				if (fInfo.direction == DCC_SEND)
					s << "Waiting for " << peer << " to accept" << (fInfo.passive ? " (passive DCC)" : "");
				else
					s << "Waiting for " << peer << " to connect";
				break;
			case DCC_STATE_CONNECTING:
				s << "Connecting to " << peer << "\xE2\x80\xA6";
				break;
			case DCC_STATE_ACTIVE:
			case DCC_STATE_DONE: {
				s << FormatSize(fInfo.bytesDone);
				if (fInfo.size > 0) {
					s << " of " << FormatSize(fInfo.size);
					char pct[16];
					snprintf(pct, sizeof(pct), " (%d%%)", (int)(100.0 * fInfo.bytesDone / fInfo.size));
					s << pct;
				}
				bigtime_t end = fInfo.state == DCC_STATE_DONE ? fInfo.endTime : system_time();
				double secs = fInfo.startTime > 0 ? (end - fInfo.startTime) / 1000000.0 : 0;
				double rate = secs > 0.5 ? (fInfo.bytesDone - fInfo.startBytes) / secs : 0;
				if (rate > 0) {
					s << " \xC2\xB7 " << FormatSize((off_t)rate) << "/s";
					if (fInfo.state == DCC_STATE_ACTIVE && fInfo.size > fInfo.bytesDone) {
						int left = (int)((fInfo.size - fInfo.bytesDone) / rate);
						char eta[32];
						if (left >= 3600)
							snprintf(eta, sizeof(eta), " \xC2\xB7 %dh %02dm left", left / 3600, (left / 60) % 60);
						else
							snprintf(eta, sizeof(eta), " \xC2\xB7 %dm %02ds left", left / 60, left % 60);
						s << eta;
					}
				}
				if (fInfo.state == DCC_STATE_DONE)
					s << " \xC2\xB7 Done";
				break;
			}
			case DCC_STATE_FAILED:
				s << "Failed: " << fInfo.error;
				break;
			case DCC_STATE_CANCELLED:
				s << (fInfo.error.Length() > 0 ? fInfo.error.String() : "Cancelled");
				break;
		}
		return s;
	}

private:
	DccTransferInfo fInfo;
	float fLineHeight = 12;
};


class DccTransfersWindow : public BWindow {
public:
	explicit DccTransfersWindow(DccManager* manager)
		: BWindow(BRect(120, 120, 680, 460), "DCC Transfers", B_TITLED_WINDOW,
			B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
		  fManager(manager),
		  fRunner(nullptr)
	{
		fStatus = new BStringView("status", "");
		fStatus->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
		fList = new BListView("transfers", B_SINGLE_SELECTION_LIST);
		fList->SetInvocationMessage(new BMessage(MSG_DCC_OPEN_ITEM));
		BScrollView* scroll = new BScrollView("scroll", fList, 0, false, true);
		scroll->SetExplicitMinSize(BSize(460, 220));

		BButton* cancel = new BButton("cancel", "Cancel Transfer", new BMessage(MSG_DCC_CANCEL));
		BButton* clear = new BButton("clear", "Clear Finished", new BMessage(MSG_DCC_CLEAR));
		BButton* folder = new BButton("folder", "Open Downloads Folder", new BMessage(MSG_DCC_OPEN_FOLDER));

		BLayoutBuilder::Group<>(this, B_VERTICAL, 6)
			.SetInsets(10)
			.Add(fStatus)
			.Add(scroll, 1.0)
			.AddGroup(B_HORIZONTAL, 6)
				.Add(cancel)
				.Add(clear)
				.AddGlue()
				.Add(folder)
			.End();

		Refresh();
		BMessage tick(MSG_DCC_REFRESH);
		fRunner = new BMessageRunner(BMessenger(this), &tick, 500000);
	}

	virtual ~DccTransfersWindow()
	{
		delete fRunner;
		while (fList->CountItems() > 0)
			delete fList->RemoveItem((int32)0);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case MSG_DCC_REFRESH:
				Refresh();
				break;
			case MSG_DCC_ACTIVATE:
				Activate(true);
				break;
			case MSG_DCC_CANCEL: {
				DccTransferItem* item = dynamic_cast<DccTransferItem*>(fList->ItemAt(fList->CurrentSelection()));
				if (item != nullptr)
					fManager->Cancel(item->Info().id);
				break;
			}
			case MSG_DCC_CLEAR:
				fManager->ClearFinished();
				Refresh();
				break;
			case MSG_DCC_OPEN_FOLDER: {
				BString dir = fManager->DownloadDir();
				create_directory(dir.String(), 0755);
				entry_ref ref;
				if (get_ref_for_path(dir.String(), &ref) == B_OK)
					be_roster->Launch(&ref);
				break;
			}
			case MSG_DCC_OPEN_ITEM: {
				DccTransferItem* item = dynamic_cast<DccTransferItem*>(fList->ItemAt(fList->CurrentSelection()));
				if (item == nullptr || item->Info().path.Length() == 0)
					break;
				if (item->Info().direction == DCC_RECEIVE && item->Info().state != DCC_STATE_DONE)
					break;
				entry_ref ref;
				if (get_ref_for_path(item->Info().path.String(), &ref) == B_OK)
					be_roster->Launch(&ref);
				break;
			}
			default:
				BWindow::MessageReceived(message);
		}
	}

private:
	void Refresh()
	{
		std::vector<DccTransferInfo> infos = fManager->Snapshot();
		fStatus->SetText(fManager->StatusText().String());

		// Drop items for transfers that no longer exist.
		for (int32 i = fList->CountItems() - 1; i >= 0; i--) {
			DccTransferItem* item = static_cast<DccTransferItem*>(fList->ItemAt(i));
			bool found = false;
			for (const DccTransferInfo& info : infos) {
				if (info.id == item->Info().id) {
					found = true;
					break;
				}
			}
			if (!found)
				delete fList->RemoveItem(i);
		}
		// Update existing items, append new ones.
		for (const DccTransferInfo& info : infos) {
			DccTransferItem* existing = nullptr;
			for (int32 i = 0; i < fList->CountItems(); i++) {
				DccTransferItem* item = static_cast<DccTransferItem*>(fList->ItemAt(i));
				if (item->Info().id == info.id) {
					existing = item;
					break;
				}
			}
			if (existing != nullptr) {
				existing->SetInfo(info);
				fList->InvalidateItem(fList->IndexOf(existing));
			} else {
				fList->AddItem(new DccTransferItem(info));
			}
		}
	}

	DccManager*     fManager;
	BStringView*    fStatus;
	BListView*      fList;
	BMessageRunner* fRunner;
};

}  // namespace


// =============================================================================
// DccManager
// =============================================================================

DccManager::DccManager(const BMessenger& host)
	:
	BLooper("DCC manager"),
	fHost(host),
	fLock("DCC transfers"),
	fNextId(1),
	fReachable(-1),
	fMappedCount(0),
	fShuttingDown(false)
{
	Run();
}


DccManager::~DccManager()
{
	for (Transfer* t : fTransfers)
		delete t;
}


void
DccManager::ApplySettings(const DccSettings& settings)
{
	bool restartMappers;
	{
		BAutolock lock(fLock);
		restartMappers = settings.firstPort != fSettings.firstPort
			|| settings.portCount != fSettings.portCount
			|| settings.usePortMapping != fSettings.usePortMapping
			|| (fMappers.empty() && settings.usePortMapping);
		fSettings = settings;
		if (fSettings.portCount < 1)
			fSettings.portCount = 1;
		if (fSettings.portCount > 20)
			fSettings.portCount = 20;
		if ((int32)fPortBusy.size() != fSettings.portCount)
			fPortBusy.assign(fSettings.portCount, false);
	}
	if (!restartMappers)
		return;

	_StopPortMappers();
	BAutolock lock(fLock);
	_StartPortMappers();
}


void
DccManager::_StartPortMappers()
{
	fMappedCount = 0;
	fPortMapped.assign(fSettings.portCount, false);
	fRouterIP = "";
	fReachable = -1;
	fInternetIP = "";
	fReachStatus = "";
	if (!fSettings.usePortMapping || fShuttingDown) {
		fMapStatus = "Router port forwarding is off.";
		return;
	}
	fMapStatus = "Looking for a UPnP / NAT-PMP router\xE2\x80\xA6";
	for (int32 i = 0; i < fSettings.portCount; i++) {
		// Only the first mapper runs the internet reachability probe.
		cricket::PortMapper* mapper = new cricket::PortMapper(BMessenger(this),
			(uint16)(fSettings.firstPort + i), "Cricket DCC", i == 0);
		if (i == 0) {
			// Learn our public IP now rather than only after a successful
			// mapping: it's needed to spot peers on this machine / LAN.
			mapper->ProbeReachability();
		}
		mapper->Start();
		fMappers.push_back(mapper);
	}
}


void
DccManager::_StopPortMappers()
{
	std::vector<cricket::PortMapper*> mappers;
	{
		BAutolock lock(fLock);
		mappers.swap(fMappers);
		fMappedCount = 0;
	}
	for (cricket::PortMapper* m : mappers)
		m->RequestStop();
	for (cricket::PortMapper* m : mappers)
		delete m;  // Stop() joins; the mapping is removed from the router on the way out
}


void
DccManager::Shutdown()
{
	std::vector<thread_id> threads;
	{
		BAutolock lock(fLock);
		fShuttingDown = true;
		for (Transfer* t : fTransfers) {
			t->cancel = true;
			if (t->sockFd >= 0)
				shutdown(t->sockFd, SHUT_RDWR);
			if (t->threadRunning && t->thread >= 0)
				threads.push_back(t->thread);
		}
	}
	for (thread_id tid : threads) {
		status_t result;
		wait_for_thread(tid, &result);
	}
	_StopPortMappers();

	// The transfers window reads from us, so make sure it is gone (not just
	// asked to go) before the manager can be deleted.
	BLooper* looper = nullptr;
	fWindow.Target(&looper);
	if (looper != nullptr && looper->Lock())
		looper->Quit();
	fWindow = BMessenger();
}


DccManager::Transfer*
DccManager::_Find(int32 id)
{
	for (Transfer* t : fTransfers) {
		if (t->info.id == id)
			return t;
	}
	return nullptr;
}


DccManager::Transfer*
DccManager::_NewTransfer(DccDirection dir, void* server, const BString& nick)
{
	Transfer* t = new Transfer;
	t->info.id = fNextId++;
	t->info.direction = dir;
	t->info.state = DCC_STATE_WAITING;
	t->info.nick = nick;
	t->info.size = 0;
	t->info.bytesDone = 0;
	t->info.passive = false;
	t->info.startTime = 0;
	t->info.endTime = 0;
	t->info.startBytes = 0;
	t->server = server;
	fTransfers.push_back(t);
	return t;
}


void
DccManager::_SendCtcp(void* server, const BString& target, const BString& ctcp)
{
	BMessage msg(MSG_DCC_SEND_CTCP);
	msg.AddPointer("server", server);
	msg.AddString("target", target);
	msg.AddString("ctcp", ctcp);
	fHost.SendMessage(&msg, (BHandler*)nullptr, 1000000);
}


void
DccManager::_Log(void* server, const BString& text)
{
	BMessage msg(MSG_DCC_LOG);
	msg.AddPointer("server", server);
	msg.AddString("text", text);
	fHost.SendMessage(&msg, (BHandler*)nullptr, 1000000);
}


uint32
DccManager::_LocalIPv4()
{
	// Ask the routing layer which source address it would use for an
	// off-link destination. No packet is sent.
	uint32 result = 0;
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return 0;
	sockaddr_in dst;
	memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_port = htons(9);
	dst.sin_addr.s_addr = inet_addr("8.8.8.8");
	if (connect(s, (sockaddr*)&dst, sizeof(dst)) == 0) {
		sockaddr_in local;
		socklen_t len = sizeof(local);
		if (getsockname(s, (sockaddr*)&local, &len) == 0)
			result = ntohl(local.sin_addr.s_addr);
	}
	close(s);
	return result;
}


BString
DccManager::_AdvertisedIP()
{
	BAutolock lock(fLock);
	if (fSettings.externalIP.Length() > 0 && ParseIPv4(fSettings.externalIP) != 0)
		return fSettings.externalIP;
	if (fReachable == 1 && fInternetIP.Length() > 0)
		return fInternetIP;
	if (fMappedCount > 0 && fReachable != 0 && fRouterIP.Length() > 0) {
		uint32 router = ParseIPv4(fRouterIP);
		if (router != 0 && !IsPrivateIPv4(router))
			return fRouterIP;
	}
	uint32 local = _LocalIPv4();
	if (local != 0 && !IsPrivateIPv4(local))
		return IPv4ToString(local);
	return "";
}


bool
DccManager::_IsOwnPublicAddress(const BString& host)
{
	uint32 ip = ParseIPv4(host);
	if (ip == 0 || IsPrivateIPv4(ip))
		return false;
	BAutolock lock(fLock);
	return (fInternetIP.Length() > 0 && ParseIPv4(fInternetIP) == ip)
		|| (fRouterIP.Length() > 0 && ParseIPv4(fRouterIP) == ip)
		|| (fSettings.externalIP.Length() > 0 && ParseIPv4(fSettings.externalIP) == ip);
}


int
DccManager::_ConnectPeer(const BString& host, uint16 port, volatile bool* cancel,
	BString& error, bool& usedLoopback)
{
	usedLoopback = false;
	int fd = ConnectTo(host, port, cancel, error);
	if (fd >= 0 || (cancel != nullptr && *cancel))
		return fd;

	// The peer advertised our own public IP, so it sits on this machine (or
	// behind the same router). Connecting to our public address from inside
	// only works with "hairpin NAT" plus a forwarded port, which clients like
	// Vision don't set up -- but on the same machine the loopback works.
	if (_IsOwnPublicAddress(host)) {
		BString loopError;
		fd = ConnectTo("127.0.0.1", port, cancel, loopError);
		if (fd >= 0) {
			usedLoopback = true;
			error = "";
			return fd;
		}
		error << " (the peer uses your own public address; it is probably on your LAN, "
			"and your router doesn't loop connections back)";
	}
	return -1;
}


bool
DccManager::_UsePassive()
{
	{
		BAutolock lock(fLock);
		if (fSettings.forcePassive)
			return true;
		if (fSettings.externalIP.Length() > 0)
			return false;
		if (fReachable == 0)
			return true;
	}
	return _AdvertisedIP().Length() == 0;
}


bool
DccManager::_AllocListenPort(uint16& outPort, int& outFd)
{
	for (int32 i = 0; i < (int32)fPortBusy.size(); i++) {
		if (fPortBusy[i])
			continue;
		uint16 port = (uint16)(fSettings.firstPort + i);
		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
			return false;
		int yes = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(fd, 1) != 0) {
			close(fd);
			continue;  // in use by something else, try the next one
		}
		SetNonBlocking(fd);
		fPortBusy[i] = true;
		outPort = port;
		outFd = fd;
		return true;
	}
	return false;
}


void
DccManager::_ReleaseListenPort(uint16 port)
{
	int32 i = (int32)port - (int32)fSettings.firstPort;
	if (i >= 0 && i < (int32)fPortBusy.size())
		fPortBusy[i] = false;
}


void
DccManager::_StartThread(Transfer* t)
{
	DccThreadArgs* args = new DccThreadArgs{this, t->info.id};
	t->thread = spawn_thread(_ThreadEntry, "DCC transfer", B_NORMAL_PRIORITY, args);
	if (t->thread < 0) {
		delete args;
		t->info.state = DCC_STATE_FAILED;
		t->info.error = "Could not start a transfer thread";
		return;
	}
	t->threadRunning = true;
	resume_thread(t->thread);
}


int32
DccManager::_ThreadEntry(void* data)
{
	DccThreadArgs* args = static_cast<DccThreadArgs*>(data);
	DccManager* self = args->manager;
	int32 id = args->id;
	delete args;

	DccDirection dir = DCC_SEND;
	{
		BAutolock lock(self->fLock);
		Transfer* t = self->_Find(id);
		if (t == nullptr)
			return 0;
		dir = t->info.direction;
	}
	if (dir == DCC_SEND)
		self->_RunSend(id);
	else if (dir == DCC_CHAT)
		self->_RunChat(id);
	else
		self->_RunReceive(id);

	BAutolock lock(self->fLock);
	Transfer* t = self->_Find(id);
	if (t != nullptr)
		t->threadRunning = false;
	return 0;
}


void
DccManager::_Finish(int32 id, DccState state, const char* error)
{
	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t == nullptr)
			return;
		if (t->cancel && state == DCC_STATE_FAILED) {
			state = DCC_STATE_CANCELLED;
			error = nullptr;  // the cause is the user, not whatever the socket said
		}
		t->info.state = state;
		if (error != nullptr && t->info.error.Length() == 0)
			t->info.error = error;
		t->info.endTime = system_time();
		if (t->sockFd >= 0) {
			close(t->sockFd);
			t->sockFd = -1;
		}
		if (t->listenFd >= 0) {
			close(t->listenFd);
			t->listenFd = -1;
			_ReleaseListenPort(t->listenPort);
		}
	}
	BMessage done(MSG_DCC_TRANSFER_DONE);
	done.AddInt32("id", id);
	PostMessage(&done);
}


// Accepts one connection on a transfer's listen socket, then frees the pool port.
static int
AcceptPeer(int listenFd, volatile bool* cancel)
{
	if (WaitFd(listenFd, false, kAcceptTimeout, cancel) != 1)
		return -1;
	sockaddr_storage peer;
	socklen_t len = sizeof(peer);
	int fd = accept(listenFd, (sockaddr*)&peer, &len);
	if (fd >= 0)
		SetNonBlocking(fd);
	return fd;
}


void
DccManager::_RunSend(int32 id)
{
	volatile bool* cancel;
	bool connectMode;
	BString host, path;
	uint16 port;
	int listenFd;
	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t == nullptr)
			return;
		cancel = &t->cancel;
		connectMode = t->connectMode;
		host = t->remoteHost;
		port = t->remotePort;
		listenFd = t->listenFd;
		path = t->info.path;
		if (connectMode)
			t->info.state = DCC_STATE_CONNECTING;
	}

	int sock;
	BString error;
	bool usedLoopback = false;
	if (connectMode) {
		sock = _ConnectPeer(host, port, cancel, error, usedLoopback);
	} else {
		sock = AcceptPeer(listenFd, cancel);
		if (sock < 0)
			error = "The peer never connected (timed out)";
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t != nullptr && t->listenFd >= 0) {
			close(t->listenFd);
			t->listenFd = -1;
			_ReleaseListenPort(t->listenPort);
		}
	}
	if (sock < 0) {
		_Finish(id, DCC_STATE_FAILED, error.String());
		return;
	}

	off_t position, size;
	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t == nullptr) {
			close(sock);
			return;
		}
		t->sockFd = sock;
		position = t->position;  // read now: a RESUME may have arrived while waiting
		size = t->info.size;
		t->info.state = DCC_STATE_ACTIVE;
		t->info.bytesDone = position;
		t->info.startBytes = position;
		t->info.startTime = system_time();
	}

	int fd = open(path.String(), O_RDONLY);
	if (fd < 0) {
		_Finish(id, DCC_STATE_FAILED, "Could not open the file");
		return;
	}
	if (position > 0 && lseek(fd, position, SEEK_SET) != position) {
		close(fd);
		_Finish(id, DCC_STATE_FAILED, "Could not seek to the resume position");
		return;
	}

	char* buffer = new char[kChunkSize];
	char ackBuf[4];
	int ackFill = 0;
	uint32 lastAck = 0;
	auto drainAcks = [&]() -> bool {
		// Returns false if the peer closed the connection.
		while (true) {
			char tmp[256];
			ssize_t n = recv(sock, tmp, sizeof(tmp), 0);
			if (n > 0) {
				for (ssize_t i = 0; i < n; i++) {
					ackBuf[ackFill++] = tmp[i];
					if (ackFill == 4) {
						lastAck = ((uint32)(uint8)ackBuf[0] << 24) | ((uint32)(uint8)ackBuf[1] << 16)
							| ((uint32)(uint8)ackBuf[2] << 8) | (uint32)(uint8)ackBuf[3];
						ackFill = 0;
					}
				}
				continue;
			}
			if (n == 0)
				return false;
			return true;  // EAGAIN: nothing more for now
		}
	};

	off_t sent = position;
	bool ok = true;
	const char* failure = nullptr;
	while (sent < size) {
		if (*cancel) {
			ok = false;
			break;
		}
		size_t want = kChunkSize;
		if ((off_t)want > size - sent)
			want = (size_t)(size - sent);
		ssize_t n = read(fd, buffer, want);
		if (n <= 0) {
			ok = false;
			failure = "Could not read the file";
			break;
		}
		if (!SendAll(sock, buffer, (size_t)n, cancel)) {
			ok = false;
			failure = "The connection was lost";
			break;
		}
		sent += n;
		if (!drainAcks()) {
			// Receiver hung up early. Fine if it already has everything.
			if (sent < size) {
				ok = false;
				failure = "The peer closed the connection";
			}
			break;
		}
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t != nullptr)
			t->info.bytesDone = sent;
	}
	delete[] buffer;
	close(fd);

	if (ok) {
		// Give the receiver a moment to acknowledge everything (acks carry the
		// low 32 bits of the total byte count) before closing.
		bigtime_t deadline = system_time() + kFinalAckWait;
		while (lastAck != (uint32)(size & 0xFFFFFFFF) && system_time() < deadline && !*cancel) {
			if (WaitFd(sock, false, 250000, cancel) == 1 && !drainAcks())
				break;
		}
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t != nullptr)
			t->info.bytesDone = sent;
	}
	_Finish(id, ok ? DCC_STATE_DONE : DCC_STATE_FAILED, failure);
}


void
DccManager::_RunReceive(int32 id)
{
	volatile bool* cancel;
	bool listenMode;
	BString host, path;
	uint16 port;
	int listenFd;
	off_t position, size;
	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t == nullptr)
			return;
		cancel = &t->cancel;
		listenMode = t->listenMode;
		host = t->remoteHost;
		port = t->remotePort;
		listenFd = t->listenFd;
		path = t->info.path;
		position = t->position;
		size = t->info.size;
		t->info.state = listenMode ? DCC_STATE_WAITING : DCC_STATE_CONNECTING;
	}

	int sock;
	BString error;
	if (listenMode) {
		sock = AcceptPeer(listenFd, cancel);
		if (sock < 0)
			error = "The sender never connected (timed out)";
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t != nullptr && t->listenFd >= 0) {
			close(t->listenFd);
			t->listenFd = -1;
			_ReleaseListenPort(t->listenPort);
		}
	} else {
		bool usedLoopback = false;
		sock = _ConnectPeer(host, port, cancel, error, usedLoopback);
		if (usedLoopback) {
			void* server = nullptr;
			BString nick;
			{
				BAutolock lock(fLock);
				Transfer* t = _Find(id);
				if (t != nullptr) {
					server = t->server;
					nick = t->info.nick;
				}
			}
			_Log(server, BString("--- [DCC] ") << nick << " advertised your own public address; "
				"connected on this machine (127.0.0.1) instead.");
		}
	}
	if (sock < 0) {
		_Finish(id, DCC_STATE_FAILED, error.String());
		return;
	}

	int flags = O_WRONLY | O_CREAT | (position > 0 ? 0 : O_TRUNC);
	int fd = open(path.String(), flags, 0644);
	if (fd < 0) {
		close(sock);
		_Finish(id, DCC_STATE_FAILED, "Could not create the file in the download folder");
		return;
	}
	if (position > 0) {
		if (ftruncate(fd, position) != 0 || lseek(fd, position, SEEK_SET) != position) {
			close(fd);
			close(sock);
			_Finish(id, DCC_STATE_FAILED, "Could not resume the file");
			return;
		}
	}

	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t != nullptr) {
			t->sockFd = sock;
			t->info.state = DCC_STATE_ACTIVE;
			t->info.bytesDone = position;
			t->info.startBytes = position;
			t->info.startTime = system_time();
		}
	}

	char* buffer = new char[kChunkSize];
	off_t received = position;
	const char* failure = nullptr;
	bool ok = true;
	while (size <= 0 || received < size) {
		int ready = WaitFd(sock, false, kStallTimeout, cancel);
		if (ready != 1) {
			ok = false;
			failure = ready == 0 ? "The transfer stalled (no data for 2 minutes)" : nullptr;
			break;
		}
		ssize_t n = recv(sock, buffer, kChunkSize, 0);
		if (n == 0)
			break;  // sender closed
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
				continue;
			ok = false;
			failure = "The connection was lost";
			break;
		}
		if (size > 0 && received + n > size)
			n = (ssize_t)(size - received);  // never write past the announced size
		if (!WriteAll(fd, buffer, (size_t)n)) {
			ok = false;
			failure = "Could not write to disk (disk full?)";
			break;
		}
		received += n;

		// Acknowledge: total bytes received so far, low 32 bits, network order.
		uint32 ack = htonl((uint32)(received & 0xFFFFFFFF));
		if (!SendAll(sock, (const char*)&ack, sizeof(ack), cancel)) {
			// Some senders close right after the last byte; that's not an error.
			if (size <= 0 || received < size) {
				ok = false;
				failure = "The connection was lost";
			}
			break;
		}

		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t != nullptr)
			t->info.bytesDone = received;
	}
	delete[] buffer;
	close(fd);

	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t != nullptr)
			t->info.bytesDone = received;
	}

	if (ok && size > 0 && received < size) {
		BString msg;
		msg << "The sender closed the connection after " << FormatSize(received)
			<< " of " << FormatSize(size);
		_Finish(id, DCC_STATE_FAILED, msg.String());
		return;
	}
	if (ok)
		update_mime_info(path.String(), false, true, false);
	_Finish(id, ok ? DCC_STATE_DONE : DCC_STATE_FAILED, failure);
}


// -----------------------------------------------------------------------------
// Incoming CTCP DCC
// -----------------------------------------------------------------------------

bool
DccManager::HandleCtcp(void* server, const BString& nick, const BString& ctcp)
{
	if (ctcp.ICompare("DCC ", 4) != 0)
		return false;

	BString type, name;
	std::vector<BString> args;
	if (!ParseDccRequest(ctcp, type, name, args)) {
		_Log(server, BString("--- [DCC] Ignored a malformed DCC request from ") << nick);
		return true;
	}

	if (type == "SEND") {
		if (args.size() < 3) {
			_Log(server, BString("--- [DCC] Ignored a malformed DCC SEND from ") << nick);
			return true;
		}
		BString host = DccHostToString(args[0]);
		uint32 portValue = (uint32)strtoul(args[1].String(), nullptr, 10);
		off_t size = (off_t)strtoll(args[2].String(), nullptr, 10);
		BString token = args.size() >= 4 ? args[3] : BString();
		if (portValue > 65535) {
			_Log(server, BString("--- [DCC] Ignored a DCC SEND with an invalid port from ") << nick);
			return true;
		}
		uint16 port = (uint16)portValue;

		// A reply to one of our passive offers: the receiver is listening,
		// so connect to it and start sending.
		if (token.Length() > 0 && port != 0) {
			BAutolock lock(fLock);
			for (Transfer* t : fTransfers) {
				if (t->info.direction == DCC_SEND && t->info.passive
					&& t->info.state == DCC_STATE_WAITING && t->token == token
					&& t->info.nick.ICompare(nick) == 0) {
					if (port < 1024) {
						t->info.state = DCC_STATE_FAILED;
						t->info.error = "The peer asked us to connect to a privileged port";
						return true;
					}
					t->remoteHost = host;
					t->remotePort = port;
					t->connectMode = true;
					_StartThread(t);
					return true;
				}
			}
		}
		_HandleIncomingSend(server, nick, name, host, port, size, token);
		return true;
	}

	if (type == "RESUME") {
		// The receiver of one of our offers wants to continue a partial file.
		if (args.size() < 2)
			return true;
		uint16 port = (uint16)strtoul(args[0].String(), nullptr, 10);
		off_t pos = (off_t)strtoll(args[1].String(), nullptr, 10);
		BString token = args.size() >= 3 ? args[2] : BString();
		BAutolock lock(fLock);
		for (Transfer* t : fTransfers) {
			if (t->info.direction != DCC_SEND || t->info.state != DCC_STATE_WAITING
				|| t->info.nick.ICompare(nick) != 0)
				continue;
			bool match = t->info.passive ? (token.Length() > 0 && t->token == token)
				: (port != 0 && t->listenPort == port);
			if (!match)
				continue;
			if (pos <= 0 || pos >= t->info.size)
				return true;
			t->position = pos;
			BString reply("DCC ACCEPT ");
			reply << QuoteFileName(t->info.fileName) << " " << (t->info.passive ? 0 : t->listenPort)
				<< " " << (int64)pos;
			if (t->info.passive)
				reply << " " << t->token;
			_SendCtcp(t->server, t->info.nick, reply);
			return true;
		}
		return true;
	}

	if (type == "ACCEPT") {
		// The sender agreed to our RESUME request.
		if (args.size() < 2)
			return true;
		uint16 port = (uint16)strtoul(args[0].String(), nullptr, 10);
		off_t pos = (off_t)strtoll(args[1].String(), nullptr, 10);
		BAutolock lock(fLock);
		for (Transfer* t : fTransfers) {
			if (t->info.direction == DCC_RECEIVE && t->resumeRequested
				&& t->info.state == DCC_STATE_WAITING && t->remotePort == port
				&& t->info.nick.ICompare(nick) == 0) {
				t->resumeRequested = false;
				if (pos < 0 || pos > t->position) {
					// The sender would skip data we don't have; refuse rather than
					// produce a corrupt file.
					t->info.state = DCC_STATE_FAILED;
					t->info.error = "The sender answered the resume request with a bad offset";
					t->info.endTime = system_time();
					return true;
				}
				t->position = pos;  // at or before our size: we truncate to it
				_StartThread(t);
				return true;
			}
		}
		return true;
	}

	if (type == "CHAT") {
		// DCC CHAT chat <ip> <port> [token]
		if (args.size() < 2) {
			_Log(server, BString("--- [DCC] Ignored a malformed DCC CHAT from ") << nick);
			return true;
		}
		BString host = DccHostToString(args[0]);
		uint32 portValue = (uint32)strtoul(args[1].String(), nullptr, 10);
		BString token = args.size() >= 3 ? args[2] : BString();
		if (portValue > 65535)
			return true;
		uint16 port = (uint16)portValue;

		// Reply to our passive chat offer: the peer is listening, connect to it.
		if (token.Length() > 0 && port != 0) {
			BAutolock lock(fLock);
			for (Transfer* t : fTransfers) {
				if (t->info.direction == DCC_CHAT && t->info.passive
					&& t->info.state == DCC_STATE_WAITING && !t->threadRunning
					&& t->token == token && t->info.nick.ICompare(nick) == 0) {
					if (port < 1024)
						return true;
					t->remoteHost = host;
					t->remotePort = port;
					t->listenMode = false;
					_StartThread(t);
					return true;
				}
			}
		}
		_HandleIncomingChat(server, nick, host, port, token);
		return true;
	}

	_Log(server, BString("--- [DCC] Ignored an unsupported DCC ") << type << " request from " << nick);
	return true;
}


void
DccManager::_HandleIncomingSend(void* server, const BString& nick, const BString& fileName,
	const BString& host, uint16 port, off_t size, const BString& token)
{
	bool passive = (port == 0);
	if (!passive && port < 1024) {
		_Log(server, BString("--- [DCC] Refused a file offer from ") << nick
			<< ": it pointed at a privileged port (" << port << ").");
		return;
	}
	if (passive && token.Length() == 0) {
		_Log(server, BString("--- [DCC] Ignored a DCC SEND without a port from ") << nick);
		return;
	}
	if (size < 0) {
		_Log(server, BString("--- [DCC] Ignored a DCC SEND with an invalid size from ") << nick);
		return;
	}

	int32 id;
	bool canResume = false;
	{
		BAutolock lock(fLock);
		if (fShuttingDown)
			return;
		int32 pending = 0;
		for (Transfer* t : fTransfers) {
			if (t->info.state == DCC_STATE_OFFERED && t->info.nick.ICompare(nick) == 0)
				pending++;
		}
		if (pending >= kMaxPendingPerNick)
			return;  // flood guard: don't stack up more prompts from the same nick

		Transfer* t = _NewTransfer(DCC_RECEIVE, server, nick);
		t->info.state = DCC_STATE_OFFERED;
		t->info.fileName = SanitizeFileName(fileName);
		t->info.size = size;
		t->info.passive = passive;
		t->token = token;
		t->remoteHost = host;
		t->remotePort = port;
		id = t->info.id;

		// Offer to resume when a smaller file of the same name is already there
		// (active offers only; passive resume is rarely supported).
		if (!passive && size > 0) {
			BString existing(fSettings.downloadDir);
			existing << "/" << t->info.fileName;
			struct stat st;
			if (stat(existing.String(), &st) == 0 && S_ISREG(st.st_mode)
				&& st.st_size > 0 && st.st_size < size)
				canResume = true;
		}
	}

	_Log(server, BString("--- [DCC] ") << nick << " offers to send you \"" << SanitizeFileName(fileName)
		<< "\" (" << FormatSize(size) << ").");
	_PromptForOffer(id, canResume);
}


void
DccManager::_PromptForOffer(int32 id, bool canResume)
{
	BString text;
	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t == nullptr)
			return;
		text << t->info.nick << " wants to send you a file:\n\n"
			<< t->info.fileName << "\n" << FormatSize(t->info.size) << "\n\n";
		if (t->info.passive)
			text << "(Passive DCC: you will listen for the sender's connection.)\n\n";
		else
			text << "From " << t->remoteHost << ":" << t->remotePort << "\n\n";
		if (canResume)
			text << "A partial copy is already in your download folder; "
				"\"Resume\" continues it.\n\n";
		text << "Only accept files from people you trust.";
	}

	BAlert* alert = canResume
		? new BAlert("DCC File Offer", text.String(), "Decline", "Resume", "Accept",
			B_WIDTH_AS_USUAL, B_OFFSET_SPACING, B_INFO_ALERT)
		: new BAlert("DCC File Offer", text.String(), "Decline", "Accept", nullptr,
			B_WIDTH_AS_USUAL, B_OFFSET_SPACING, B_INFO_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	BMessage* reply = new BMessage(MSG_DCC_ALERT_REPLY);
	reply->AddInt32("id", id);
	reply->AddBool("resume", canResume);
	alert->Go(new BInvoker(reply, BMessenger(this)));
}


void
DccManager::_AcceptOffer(int32 id, bool resume)
{
	void* server;
	BString nick, fileName, passiveReply, resumeRequest;
	bool passive;
	bool failed = false;
	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t == nullptr || t->info.state != DCC_STATE_OFFERED)
			return;
		server = t->server;
		nick = t->info.nick;
		fileName = t->info.fileName;
		passive = t->info.passive;

		create_directory(fSettings.downloadDir.String(), 0755);
		if (resume) {
			BString existing(fSettings.downloadDir);
			existing << "/" << t->info.fileName;
			struct stat st;
			if (stat(existing.String(), &st) == 0 && st.st_size < t->info.size) {
				t->info.path = existing;
				t->position = st.st_size;
			} else {
				resume = false;
			}
		}
		if (!resume)
			t->info.path = UniquePath(fSettings.downloadDir, t->info.fileName);

		if (resume) {
			// Ask the sender to continue from our current size; the transfer
			// starts once its DCC ACCEPT arrives.
			t->resumeRequested = true;
			t->info.state = DCC_STATE_WAITING;
			resumeRequest << "DCC RESUME " << QuoteFileName(t->info.fileName) << " "
				<< t->remotePort << " " << (int64)t->position;
		} else if (passive) {
			// Reverse DCC: we listen, and tell the sender where to connect.
			uint16 port;
			int fd;
			if (!_AllocListenPort(port, fd)) {
				t->info.state = DCC_STATE_FAILED;
				t->info.error = "No free DCC port (all listen ports are busy)";
				failed = true;
			} else {
				t->listenPort = port;
				t->listenFd = fd;
				t->listenMode = true;
				t->info.state = DCC_STATE_WAITING;
			}
		} else {
			_StartThread(t);
		}
	}
	if (failed) {
		_Log(server, BString("--- [DCC] Could not accept \"") << fileName << "\" from " << nick
			<< ": all DCC listen ports are busy.");
		return;
	}

	if (resume) {
		_SendCtcp(server, nick, resumeRequest);
		_Log(server, BString("--- [DCC] Asking ") << nick << " to resume \"" << fileName << "\".");
		return;
	}

	if (passive) {
		BString ip = _AdvertisedIP();
		if (ip.Length() == 0) {
			uint32 local = _LocalIPv4();
			ip = IPv4ToString(local);
			_Log(server, BString("--- [DCC] Warning: you don't seem to be reachable from the internet, "
				"and neither is ") << nick << " (passive DCC). The transfer will probably only work on a LAN.");
		}
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t == nullptr)
			return;
		passiveReply << "DCC SEND " << QuoteFileName(t->info.fileName) << " " << ParseIPv4(ip) << " "
			<< t->listenPort << " " << (int64)t->info.size << " " << t->token;
		_SendCtcp(server, nick, passiveReply);
		_StartThread(t);
	}
}


// -----------------------------------------------------------------------------
// Outgoing offers
// -----------------------------------------------------------------------------

void
DccManager::OfferFile(void* server, const BString& nick, const BString& path)
{
	struct stat st;
	if (stat(path.String(), &st) != 0 || !S_ISREG(st.st_mode)) {
		_Log(server, BString("--- [DCC] Can't send \"") << path << "\": not a readable file.");
		return;
	}
	if (access(path.String(), R_OK) != 0) {
		_Log(server, BString("--- [DCC] Can't send \"") << path << "\": permission denied.");
		return;
	}

	BPath p(path.String());
	BString fileName = SanitizeFileName(p.Leaf() != nullptr ? p.Leaf() : path.String());
	bool passive = _UsePassive();
	BString ip = _AdvertisedIP();
	if (!passive && ip.Length() == 0)
		passive = true;

	BString ctcp;
	int32 id;
	{
		BAutolock lock(fLock);
		if (fShuttingDown)
			return;
		Transfer* t = _NewTransfer(DCC_SEND, server, nick);
		t->info.fileName = fileName;
		t->info.path = path;
		t->info.size = st.st_size;
		t->info.passive = passive;
		id = t->info.id;

		if (passive) {
			// Reverse DCC: port 0 plus a token; the receiver replies with where
			// to connect.
			char token[16];
			snprintf(token, sizeof(token), "%u", (unsigned)((system_time() / 7 + id * 7919) % 100000000));
			t->token = token;
			uint32 advertised = ParseIPv4(ip);
			if (advertised == 0)
				advertised = _LocalIPv4();
			ctcp << "DCC SEND " << QuoteFileName(fileName) << " " << advertised << " 0 "
				<< (int64)st.st_size << " " << t->token;
		} else {
			uint16 port;
			int fd;
			if (!_AllocListenPort(port, fd)) {
				t->info.state = DCC_STATE_FAILED;
				t->info.error = "No free DCC port (all listen ports are busy)";
			} else {
				t->listenPort = port;
				t->listenFd = fd;
				ctcp << "DCC SEND " << QuoteFileName(fileName) << " " << ParseIPv4(ip) << " "
					<< port << " " << (int64)st.st_size;
				_StartThread(t);
			}
		}
	}

	if (ctcp.Length() == 0) {
		_Log(server, BString("--- [DCC] Can't send \"") << fileName
			<< "\": all DCC listen ports are busy. Try again when a transfer finishes.");
		return;
	}
	_SendCtcp(server, nick, ctcp);
	_Log(server, BString("--- [DCC] Offering \"") << fileName << "\" (" << FormatSize(st.st_size)
		<< ") to " << nick << (passive ? " using passive DCC" : "") << ". Waiting for them to accept\xE2\x80\xA6");
	if (passive && !fSettings.forcePassive) {
		_Log(server, BString("--- [DCC] You don't seem to be reachable from the internet, so this uses "
			"passive DCC. Clients without passive DCC support (Vision, for one) can't receive it. "
			"If ") << nick << " is on your own network, set Server Settings \xE2\x86\x92 DCC \xE2\x86\x92 "
			"External IP override to this computer's LAN address (or 127.0.0.1 for the same computer).");
	}
	(void)id;
	ShowTransfersWindow();
}


void
DccManager::ForgetServer(void* server)
{
	BAutolock lock(fLock);
	for (Transfer* t : fTransfers) {
		if (t->server == server)
			t->server = nullptr;
	}
}


// -----------------------------------------------------------------------------
// DCC CHAT
// -----------------------------------------------------------------------------

void
DccManager::_PostChat(uint32 what, void* server, const BString& nick, const char* key,
	const BString& value, bool action)
{
	BMessage msg(what);
	msg.AddPointer("server", server);
	msg.AddString("nick", nick);
	if (key != nullptr)
		msg.AddString(key, value);
	if (what == MSG_DCC_CHAT_LINE)
		msg.AddBool("action", action);
	fHost.SendMessage(&msg, (BHandler*)nullptr, 1000000);
}


void
DccManager::OfferChat(void* server, const BString& nick)
{
	bool passive = _UsePassive();
	BString ip = _AdvertisedIP();
	if (!passive && ip.Length() == 0)
		passive = true;

	BString ctcp;
	{
		BAutolock lock(fLock);
		if (fShuttingDown)
			return;
		for (Transfer* t : fTransfers) {
			if (t->info.direction == DCC_CHAT && t->server == server
				&& t->info.nick.ICompare(nick) == 0
				&& (t->info.state == DCC_STATE_WAITING || t->info.state == DCC_STATE_CONNECTING
					|| t->info.state == DCC_STATE_ACTIVE)) {
				_Log(server, BString("--- [DCC] A DCC CHAT with ") << nick << " is already open or pending.");
				return;
			}
		}
		Transfer* t = _NewTransfer(DCC_CHAT, server, nick);
		t->info.fileName = "DCC CHAT";
		t->info.passive = passive;
		if (passive) {
			char token[16];
			snprintf(token, sizeof(token), "%u", (unsigned)((system_time() / 7 + t->info.id * 7919) % 100000000));
			t->token = token;
			uint32 advertised = ParseIPv4(ip);
			if (advertised == 0)
				advertised = _LocalIPv4();
			ctcp << "DCC CHAT chat " << advertised << " 0 " << t->token;
		} else {
			uint16 port;
			int fd;
			if (!_AllocListenPort(port, fd)) {
				t->info.state = DCC_STATE_FAILED;
				t->info.error = "No free DCC port (all listen ports are busy)";
			} else {
				t->listenPort = port;
				t->listenFd = fd;
				t->listenMode = true;
				ctcp << "DCC CHAT chat " << ParseIPv4(ip) << " " << port;
				_StartThread(t);
			}
		}
	}
	if (ctcp.Length() == 0) {
		_Log(server, BString("--- [DCC] Can't start a DCC CHAT: all DCC listen ports are busy."));
		return;
	}
	_SendCtcp(server, nick, ctcp);
	_Log(server, BString("--- [DCC] Offering a DCC CHAT to ") << nick
		<< (passive ? " (passive DCC)" : "") << ". Waiting for them to accept\xE2\x80\xA6");
}


void
DccManager::_HandleIncomingChat(void* server, const BString& nick, const BString& host,
	uint16 port, const BString& token)
{
	bool passive = (port == 0);
	if (!passive && port < 1024) {
		_Log(server, BString("--- [DCC] Refused a DCC CHAT from ") << nick
			<< ": it pointed at a privileged port (" << port << ").");
		return;
	}
	if (passive && token.Length() == 0)
		return;

	int32 id;
	BString text;
	{
		BAutolock lock(fLock);
		if (fShuttingDown)
			return;
		int32 pending = 0;
		for (Transfer* t : fTransfers) {
			if (t->info.state == DCC_STATE_OFFERED && t->info.nick.ICompare(nick) == 0)
				pending++;
		}
		if (pending >= kMaxPendingPerNick)
			return;

		Transfer* t = _NewTransfer(DCC_CHAT, server, nick);
		t->info.state = DCC_STATE_OFFERED;
		t->info.fileName = "DCC CHAT";
		t->info.passive = passive;
		t->token = token;
		t->remoteHost = host;
		t->remotePort = port;
		id = t->info.id;

		text << nick << " wants to chat with you directly (DCC CHAT).\n\n"
			<< "The chat bypasses the IRC server and connects your computers directly, "
			"so each of you learns the other's IP address.";
		if (!passive)
			text << "\n\nFrom " << host << ":" << port;
	}

	_Log(server, BString("--- [DCC] ") << nick << " offers a DCC CHAT.");
	BAlert* alert = new BAlert("DCC Chat Offer", text.String(), "Decline", "Accept", nullptr,
		B_WIDTH_AS_USUAL, B_OFFSET_SPACING, B_INFO_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	BMessage* reply = new BMessage(MSG_DCC_ALERT_REPLY);
	reply->AddInt32("id", id);
	reply->AddBool("chat", true);
	alert->Go(new BInvoker(reply, BMessenger(this)));
}


void
DccManager::_AcceptChat(int32 id)
{
	void* server = nullptr;
	BString nick, reply;
	bool passive;
	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t == nullptr || t->info.state != DCC_STATE_OFFERED)
			return;
		server = t->server;
		nick = t->info.nick;
		passive = t->info.passive;
		t->info.state = DCC_STATE_WAITING;
		if (!passive) {
			t->listenMode = false;
			_StartThread(t);
			return;
		}
		uint16 port;
		int fd;
		if (!_AllocListenPort(port, fd)) {
			t->info.state = DCC_STATE_FAILED;
			t->info.error = "No free DCC port";
		} else {
			t->listenPort = port;
			t->listenFd = fd;
			t->listenMode = true;
		}
	}

	// Passive offer: we listen and tell the peer where to connect.
	BString ip = _AdvertisedIP();
	if (ip.Length() == 0)
		ip = IPv4ToString(_LocalIPv4());
	BAutolock lock(fLock);
	Transfer* t = _Find(id);
	if (t == nullptr || t->listenFd < 0) {
		_Log(server, BString("--- [DCC] Could not accept the DCC CHAT from ") << nick
			<< ": all DCC listen ports are busy.");
		return;
	}
	reply << "DCC CHAT chat " << ParseIPv4(ip) << " " << t->listenPort << " " << t->token;
	_SendCtcp(server, nick, reply);
	_StartThread(t);
}


void
DccManager::_RunChat(int32 id)
{
	volatile bool* cancel;
	bool listenMode;
	BString host, nick;
	uint16 port;
	int listenFd;
	void* server;
	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t == nullptr)
			return;
		cancel = &t->cancel;
		listenMode = t->listenMode;
		host = t->remoteHost;
		port = t->remotePort;
		listenFd = t->listenFd;
		nick = t->info.nick;
		server = t->server;
		t->info.state = listenMode ? DCC_STATE_WAITING : DCC_STATE_CONNECTING;
	}

	int sock;
	BString error;
	if (listenMode) {
		sock = AcceptPeer(listenFd, cancel);
		if (sock < 0)
			error = "they never connected (timed out)";
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t != nullptr && t->listenFd >= 0) {
			close(t->listenFd);
			t->listenFd = -1;
			_ReleaseListenPort(t->listenPort);
		}
	} else {
		bool usedLoopback = false;
		sock = _ConnectPeer(host, port, cancel, error, usedLoopback);
	}
	if (sock < 0) {
		if (!*cancel)
			_Log(server, BString("--- [DCC] The DCC CHAT with ") << nick << " could not be opened: " << error << ".");
		_Finish(id, DCC_STATE_FAILED, error.String());
		return;
	}

	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t != nullptr) {
			t->sockFd = sock;
			t->info.state = DCC_STATE_ACTIVE;
			t->info.startTime = system_time();
			server = t->server;
		}
	}
	_PostChat(MSG_DCC_CHAT_OPENED, server, nick);

	// Read newline-terminated lines until either side hangs up.
	BString pending;
	char buffer[4096];
	const char* reason = "the other side closed the chat";
	while (!*cancel) {
		int ready = WaitFd(sock, false, 3600 * 1000000LL, cancel);
		if (ready == 0)
			continue;   // idle for an hour; chats may sit quietly
		if (ready < 0)
			break;
		ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
		if (n == 0)
			break;
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
				continue;
			reason = "the connection was lost";
			break;
		}
		pending.Append(buffer, (int32)n);
		int32 nl;
		while ((nl = pending.FindFirst('\n')) >= 0 || pending.Length() > 8192) {
			BString line;
			if (nl < 0) {
				nl = 8192;  // no newline in a long run: pass it on in pieces
				pending.CopyInto(line, 0, nl);
				pending.Remove(0, nl);
			} else {
				pending.CopyInto(line, 0, nl);
				pending.Remove(0, nl + 1);
			}
			line.RemoveAll("\r");
			bool action = false;
			if (line.StartsWith("\x01" "ACTION ")) {
				line.Remove(0, 8);
				line.RemoveAll("\x01");
				action = true;
			} else if (line.StartsWith("\x01")) {
				continue;  // other CTCP inside a chat: ignore
			}
			BAutolock lock(fLock);
			Transfer* t = _Find(id);
			server = t != nullptr ? t->server : nullptr;
			lock.Unlock();
			_PostChat(MSG_DCC_CHAT_LINE, server, nick, "text", line, action);
		}
	}
	if (*cancel)
		reason = "you closed the chat";
	{
		BAutolock lock(fLock);
		Transfer* t = _Find(id);
		if (t != nullptr)
			server = t->server;
	}
	_PostChat(MSG_DCC_CHAT_CLOSED, server, nick, "reason", reason);
	_Finish(id, *cancel ? DCC_STATE_CANCELLED : DCC_STATE_DONE, nullptr);
}


bool
DccManager::ChatSend(void* server, const BString& nick, const BString& text, bool action)
{
	BString line(text);
	line.RemoveAll("\r");
	line.RemoveAll("\n");
	if (action) {
		line.Prepend("\x01" "ACTION ");
		line.Append("\x01");
	}
	line.Append("\n");

	BAutolock lock(fLock);
	for (Transfer* t : fTransfers) {
		if (t->info.direction == DCC_CHAT && t->server == server && t->info.state == DCC_STATE_ACTIVE
			&& t->sockFd >= 0 && t->info.nick.ICompare(nick) == 0) {
			// Short stall limit: this runs on the chat window's thread.
			return SendAll(t->sockFd, line.String(), line.Length(), &t->cancel, 5 * 1000000LL);
		}
	}
	return false;
}


void
DccManager::CloseChat(void* server, const BString& nick)
{
	BAutolock lock(fLock);
	for (Transfer* t : fTransfers) {
		if (t->info.direction != DCC_CHAT || t->server != server || t->info.nick.ICompare(nick) != 0)
			continue;
		if (t->info.state == DCC_STATE_DONE || t->info.state == DCC_STATE_FAILED
			|| t->info.state == DCC_STATE_CANCELLED)
			continue;
		t->cancel = true;
		if (t->sockFd >= 0)
			shutdown(t->sockFd, SHUT_RDWR);
		if (!t->threadRunning) {
			t->info.state = DCC_STATE_CANCELLED;
			t->info.endTime = system_time();
			if (t->listenFd >= 0) {
				close(t->listenFd);
				t->listenFd = -1;
				_ReleaseListenPort(t->listenPort);
			}
		}
	}
}


// -----------------------------------------------------------------------------
// Window support
// -----------------------------------------------------------------------------

std::vector<DccTransferInfo>
DccManager::Snapshot()
{
	BAutolock lock(fLock);
	std::vector<DccTransferInfo> out;
	for (Transfer* t : fTransfers) {
		if (t->info.direction != DCC_CHAT)
			out.push_back(t->info);
	}
	return out;
}


BString
DccManager::StatusText()
{
	BString text;
	BAutolock lock(fLock);
	text << fMapStatus;
	if (fReachStatus.Length() > 0)
		text << "  " << fReachStatus;
	if (fSettings.forcePassive)
		text << "  (Passive DCC is forced on.)";
	return text;
}


void
DccManager::Cancel(int32 id)
{
	BAutolock lock(fLock);
	Transfer* t = _Find(id);
	if (t == nullptr)
		return;
	if (t->info.state == DCC_STATE_DONE || t->info.state == DCC_STATE_FAILED
		|| t->info.state == DCC_STATE_CANCELLED)
		return;
	t->cancel = true;
	if (t->sockFd >= 0)
		shutdown(t->sockFd, SHUT_RDWR);
	if (!t->threadRunning) {
		// Not started yet (offer pending, or waiting for a passive reply / ACCEPT).
		t->info.error = t->info.state == DCC_STATE_OFFERED ? "Declined" : "Cancelled";
		t->info.state = DCC_STATE_CANCELLED;
		t->info.endTime = system_time();
		if (t->listenFd >= 0) {
			close(t->listenFd);
			t->listenFd = -1;
			_ReleaseListenPort(t->listenPort);
		}
	}
}


void
DccManager::ClearFinished()
{
	BAutolock lock(fLock);
	for (size_t i = fTransfers.size(); i-- > 0;) {
		Transfer* t = fTransfers[i];
		bool finished = t->info.state == DCC_STATE_DONE || t->info.state == DCC_STATE_FAILED
			|| t->info.state == DCC_STATE_CANCELLED;
		if (finished && !t->threadRunning) {
			delete t;
			fTransfers.erase(fTransfers.begin() + i);
		}
	}
}


BString
DccManager::DownloadDir()
{
	BAutolock lock(fLock);
	return fSettings.downloadDir;
}


void
DccManager::ShowTransfersWindow()
{
	BAutolock lock(fLock);
	if (fShuttingDown)
		return;
	if (fWindow.IsValid()) {
		fWindow.SendMessage(MSG_DCC_ACTIVATE);
		return;
	}
	DccTransfersWindow* window = new DccTransfersWindow(this);
	fWindow = BMessenger(window);
	window->Show();
}


void
DccManager::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case PORT_MAP_REPORT: {
			int32 state = 0, internalPort = 0, externalPort = 0, reachable = -2;
			message->FindInt32("state", &state);
			message->FindInt32("internal_port", &internalPort);
			message->FindInt32("external_port", &externalPort);
			const char* text = nullptr;
			message->FindString("message", &text);

			BAutolock lock(fLock);
			if (message->FindInt32("reachable", &reachable) == B_OK) {
				fReachable = reachable;
				const char* ip = nullptr;
				if (message->FindString("internet_ip", &ip) == B_OK && ip != nullptr)
					fInternetIP = ip;
				if (reachable == 1)
					fReachStatus = "Reachable from the internet.";
				else if (reachable == 0)
					fReachStatus = "Not reachable from the internet; using passive DCC.";
				else
					fReachStatus = "Internet reachability unknown.";
				break;
			}

			int32 index = internalPort - fSettings.firstPort;
			std::vector<bool>& mapped = fPortMapped;
			if ((int32)mapped.size() != fSettings.portCount)
				mapped.assign(fSettings.portCount, false);
			if (index >= 0 && index < (int32)mapped.size()) {
				if (state == cricket::PORT_MAP_STATE_MAPPED) {
					mapped[index] = true;
					const char* ip = nullptr;
					if (message->FindString("external_ip", &ip) == B_OK && ip != nullptr && ip[0] != '\0')
						fRouterIP = ip;
				} else if (state == cricket::PORT_MAP_STATE_LOST || state == cricket::PORT_MAP_STATE_FAILED
					|| state == cricket::PORT_MAP_STATE_REMOVED) {
					mapped[index] = false;
				}
			}
			fMappedCount = 0;
			for (bool m : mapped)
				fMappedCount += m ? 1 : 0;

			const char* method = nullptr;
			message->FindString("method", &method);
			if (fMappedCount > 0) {
				fMapStatus = "Router port forwarding active";
				if (method != nullptr && method[0] != '\0')
					fMapStatus << " (" << method << ")";
				fMapStatus << ": " << fMappedCount << " of " << fSettings.portCount << " DCC ports";
				if (fRouterIP.Length() > 0)
					fMapStatus << ", external address " << fRouterIP;
				fMapStatus << ".";
			} else if (index == 0 && text != nullptr) {
				fMapStatus = text;
			}
			break;
		}

		case MSG_DCC_ALERT_REPLY: {
			int32 id = 0, which = 0;
			bool canResume = false, isChat = false;
			message->FindInt32("id", &id);
			message->FindInt32("which", &which);
			message->FindBool("resume", &canResume);
			message->FindBool("chat", &isChat);
			if (isChat && which != 0) {
				_AcceptChat(id);
				break;
			}
			if (which == 0) {
				void* server = nullptr;
				BString nick, file;
				{
					BAutolock lock(fLock);
					Transfer* t = _Find(id);
					if (t == nullptr || t->info.state != DCC_STATE_OFFERED)
						break;
					t->info.state = DCC_STATE_CANCELLED;
					t->info.error = "Declined";
					t->info.endTime = system_time();
					server = t->server;
					nick = t->info.nick;
					file = t->info.fileName;
				}
				if (isChat)
					_Log(server, BString("--- [DCC] Declined a DCC CHAT from ") << nick << ".");
				else
					_Log(server, BString("--- [DCC] Declined \"") << file << "\" from " << nick << ".");
				break;
			}
			bool resume = canResume && which == 1;
			ShowTransfersWindow();
			_AcceptOffer(id, resume);
			break;
		}

		case MSG_DCC_TRANSFER_DONE: {
			int32 id = 0;
			message->FindInt32("id", &id);
			DccTransferInfo info;
			void* server = nullptr;
			{
				BAutolock lock(fLock);
				Transfer* t = _Find(id);
				if (t == nullptr)
					break;
				info = t->info;
				server = t->server;
			}
			if (info.direction == DCC_CHAT)
				break;
			BString line("--- [DCC] ");
			if (info.state == DCC_STATE_DONE) {
				double secs = info.startTime > 0 ? (info.endTime - info.startTime) / 1000000.0 : 0;
				line << (info.direction == DCC_SEND ? "Sent \"" : "Received \"") << info.fileName << "\" "
					<< (info.direction == DCC_SEND ? "to " : "from ") << info.nick << " ("
					<< FormatSize(info.bytesDone);
				if (secs > 0.5)
					line << ", " << FormatSize((off_t)((info.bytesDone - info.startBytes) / secs)) << "/s";
				line << ")";
				if (info.direction == DCC_RECEIVE)
					line << ", saved to " << info.path;
				line << ".";

				if (info.direction == DCC_RECEIVE) {
					BNotification note(B_INFORMATION_NOTIFICATION);
					note.SetGroup("Cricket IRC");
					note.SetTitle("DCC download complete");
					BString content;
					content << info.fileName << " from " << info.nick;
					note.SetContent(content.String());
					entry_ref ref;
					if (get_ref_for_path(info.path.String(), &ref) == B_OK)
						note.SetOnClickFile(&ref);
					note.Send();
				}
			} else if (info.state == DCC_STATE_CANCELLED) {
				line << "Transfer of \"" << info.fileName << "\" with " << info.nick << " was cancelled.";
			} else {
				line << "Transfer of \"" << info.fileName << "\" with " << info.nick << " failed: "
					<< info.error << ".";
			}
			_Log(server, line);
			break;
		}

		default:
			BLooper::MessageReceived(message);
	}
}
