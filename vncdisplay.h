extern unsigned int fbHeight;
extern unsigned int fbWidth;

extern BOOLEAN displayInProgress;

extern unsigned int rectX;
extern unsigned int rectY;
extern unsigned int rectWidth;
extern unsigned int rectHeight;

#define encodingRaw         0
#define encodingCopyRect    1
#define encodingRRE         2
#define encodingCoRRE       4
#define encodingHextile     5
#define encodingZRLE        16
#define encodingCursor      0xffffff11
#define encodingDesktopSize 0xffffff21

extern GrafPortPtr vncWindow;

/* VNC session window dimensions */
extern unsigned int winHeight;
extern unsigned int winWidth;

/* On the next 2 structs, only certain values are permanently zero.
 * Others are changed later.
 */
extern struct LocInfo srcLocInfo;

/* Used by multiple encodings */
extern Rect srcRect;
extern unsigned char *pixTransTbl;

extern BOOLEAN checkBounds; /* Adjust drawing to stay in bounds */

void InitVNCWindow (void);

void SendFBUpdateRequest (BOOLEAN /*incremental*/, unsigned int /*x*/,
        unsigned int /*y*/, unsigned int /*width*/, unsigned int /*height*/);

void ConnectedEventLoop (void);

void NextRect (void);
