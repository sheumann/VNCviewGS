extern unsigned int fbHeight;
extern unsigned int fbWidth;

extern GrafPortPtr vncWindow;

void InitVNCWindow (void);

void SendFBUpdateRequest (BOOLEAN /*incremental*/, unsigned int /*x*/,
		unsigned int /*y*/, unsigned int /*width*/, unsigned int /*height*/);

void ConnectedEventLoop (void);

void DoSendClipboard (void);
void DoPointerEvent (void);

void ProcessKeyEvent (void);
void SendModifiers (void);
