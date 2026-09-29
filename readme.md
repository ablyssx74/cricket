
<h1>
  <img src="https://github.com/user-attachments/assets/2c1377ea-27d6-4b01-bd1d-fdda59530fe3" width="48" height="48" align="top" alt="App Icon">
  cricket
</h1>

### A Native Multi-Server IRC Client for Haiku OS.

### Notable Features
* CertFP  / SASL Fully Supported.
* Gemini AI Real-time Language Translator.
* Aspell Spell Checking.
* Loads all URLs with Firefox if Firefox is set as preferred in FileTypes.
* Smart Timestamp Display.
* Checks Online for Updates & Sends Notification if Available.
* Color Highlight keywords.
* Ignore keywords.
* Emots.
* DCC file transfers (send and receive, passive DCC, resume), with automatic
  router port forwarding (UPnP / NAT-PMP / PCP).



### DCC file transfers
* Right-click a user and choose **Send File to …**, or type `/dcc send <nick> [path]`.
* `/dcc` (or **DCC Transfers…** on a server's right-click menu) opens the transfers window.
* Incoming files always ask first and are saved to `~/Downloads` (change it in
  Server Settings → DCC). Partial downloads can be resumed.
* Cricket listens on 5 ports starting at 59200 and asks your router to forward
  them. If you still can't be reached from the internet (e.g. carrier-grade
  NAT), it offers files with passive DCC instead. DCC CHAT isn't supported yet.
* Set `CRICKET_PORTMAP_DEBUG=1` to see what the port mapper is doing.

### Build 64bit / 32bit
```
make release

```

### Linux (KDE Plasma / Wayland)
A native Qt 6 port lives in [`linux/`](linux/README.md). On CachyOS / Arch:
```
cd linux/packaging && makepkg -si
```

### Screenshots
<img align="left" width="700" height="400" alt="Image" src="https://github.com/user-attachments/assets/d4c47f8c-17c5-455f-8a46-cb59d696a829" />



