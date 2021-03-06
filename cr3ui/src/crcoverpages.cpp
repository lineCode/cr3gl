
#include "crcoverpages.h"
#include "lvdocview.h"
#include "epubfmt.h"
#include "pdbfmt.h"
#include "lvqueue.h"
#include "cruiwidget.h"
#include "cruiconfig.h"

//#define USE_GL_COVERPAGE_CACHE 0 // no GL context under this thread anyway

//#if USE_GL_COVERPAGE_CACHE
//#include "gldrawbuf.h"
//#endif

enum CoverCachingType {
    COVER_CACHED,   // cached in directory
    COVER_FROMBOOK, // read from book directly (it's fast enough for format)
    COVER_EMPTY     // no cover image in book
};

/// draw book cover to drawbuf
void CRDrawBookCover(LVDrawBuf * drawbuf, lString8 fontFace, CRDirEntry * book, LVImageSourceRef image, int bpp = 32);
bool LVBookFileExists(lString8 fname);

class CRCoverFileCache {
public:
    class Entry {
    public:
        lString8 pathname;
        int type;
        lString8 cachedFile;
        int size;
        Entry(lString8 fn, int _type) : pathname(fn), type(_type), size(0) {
        }
    };
private:
    CRMutexRef _mutex;
    LVQueue<Entry*> _cache;
    lString16 _dir;
    lString16 _filename;
    int _maxitems;
    int _maxfiles;
    int _maxsize;
    int _nextId;
    lString8 generateNextFilename();
    lString8 saveToCache(LVStreamRef stream);
    void checkSize();
    Entry * add(const lString8 & pathname, int type, int sz, lString8 & fn);
    Entry * put(const lString8 & pathname, int type, LVStreamRef stream);
    void clear();
    bool save();
    lString16 getFilename(Entry * item);
    Entry * find(const lString8 & pathname);
    Entry * scan(const lString8 & pathname);
    LVStreamRef getStream(Entry * item);
    bool knownCachedFile(const lString8 & fn);
public:

    // mutex-protected externally available methods

    void cacheDownloadedImage(const lString8 & fn, LVStreamRef stream);

    bool isCached(const lString8 & fn);
    /// get or scan cover stream for path
    LVStreamRef getStream(const lString8 & pathname);

    // initialization / finalization
    bool open();
    CRCoverFileCache(lString16 dir, int maxitems = 1000, int maxfiles = 200, int maxsize = 16*1024*1024);
    ~CRCoverFileCache() { save(); clear(); }
};

extern CRCoverFileCache * coverCache;

class CRCoverImageCache {
public:
    class Entry {
    public:
        CRDirEntry * book;
        int dx;
        int dy;
        LVDrawBuf * image;
        Entry(CRDirEntry * _book, int _dx, int _dy, LVDrawBuf * _image) : book(_book->clone()), dx(_dx), dy(_dy), image(_image) {}
        ~Entry() { if (image) delete image; }
    };
private:
    LVQueue<Entry*> _cache;
    int _maxitems;
    int _maxsize;
    void checkSize();
    Entry * put(CRDirEntry * _book, int _dx, int _dy, LVDrawBuf * _image);
    /// override to use non-standard draw buffers (e.g. OpenGL)
    virtual LVColorDrawBuf * createDrawBuf(int dx, int dy);
public:
    void clear();
    Entry * draw(CRCoverPageManager * _manager, CRDirEntry * _book, int dx, int dy);
    Entry * find(CRDirEntry * _book, int dx, int dy);
    CRCoverImageCache(int maxitems = 1000, int maxsize = 16*1024*1024);
    virtual ~CRCoverImageCache() { clear(); }
};

extern CRCoverImageCache * coverImageCache;

class CoverTask {
public:
    CRDirEntry * book;
    int dx;
    int dy;
    CRRunnable * callback;
    CoverTask(CRDirEntry * _book, int _dx, int _dy, CRRunnable * _callback) : book(_book->clone()), dx(_dx), dy(_dy), callback(_callback) {}
    virtual ~CoverTask() {
        delete book;
    }
    bool isSame(CRDirEntry * _book) {
        return _book->getCoverPathName() == book->getCoverPathName();
    }
    bool isSame(CRDirEntry * _book, int _dx, int _dy) {
        return ((!_dx && !_dy) || (dx == _dx && dy == _dy)) && isSame(_book);
    }
};

void CRDrawBookCover(LVDrawBuf * drawbuf, lString8 fontFace, CRDirEntry * book, LVImageSourceRef image, int bpp)
{
    //CRLog::debug("drawBookCover called");
    if (crconfig.einkMode) {
        drawbuf->SetBackgroundColor(0xFFFFFF);
        drawbuf->FillRect(0, 0, drawbuf->GetWidth(), drawbuf->GetHeight(), 0xFFFFFF);
    }
    lString16 title = book->getTitle();
    lString16 authors = book->getAuthorNames(false);
    lString16 seriesName = book->getSeriesNameOnly();
    int seriesNumber = book->getSeriesNumber();
    if (title.empty() && authors.empty())
        title = Utf8ToUnicode(book->getFileName());
    if (drawbuf != NULL) {
        int factor = 1;
        int dx = drawbuf->GetWidth();
        int dy = drawbuf->GetHeight();
        int MIN_WIDTH = 300;
        int MIN_HEIGHT = 400;
        if (dx < MIN_WIDTH || dy < MIN_HEIGHT) {
            if (dx * 2 < MIN_WIDTH || dy * 2 < MIN_HEIGHT) {
                dx *= 3;
                dy *= 3;
                factor = 3;
            } else {
                dx *= 2;
                dy *= 2;
                factor = 2;
            }
        }
        LVDrawBuf * drawbuf2 = drawbuf;
        if (factor > 1)
            drawbuf2 = new LVColorDrawBuf(dx, dy, drawbuf->GetBitsPerPixel());

        if (bpp >= 16) {
            // native color resolution
            LVDrawBookCover(*drawbuf2, image, fontFace, title, authors, seriesName, seriesNumber);
            image.Clear();
        } else {
            LVGrayDrawBuf grayBuf(drawbuf2->GetWidth(), drawbuf2->GetHeight(), bpp);
            LVDrawBookCover(grayBuf, image, fontFace, title, authors, seriesName, seriesNumber);
            image.Clear();
            grayBuf.DrawTo(drawbuf2, 0, 0, 0, NULL);
            //CRUIDrawTo(&grayBuf, drawbuf2, 0, 0);
        }
        if (factor > 1) {
            drawbuf->DrawRescaled(drawbuf2, 0, 0, drawbuf->GetWidth(), drawbuf->GetHeight(), 0);
            delete drawbuf2;
        }
    }
    //CRLog::debug("drawBookCover finished");
}

bool LVBookFileExists(lString8 fname) {
    lString16 fn = Utf8ToUnicode(fname);
    lString16 arc;
    lString16 file;
    if (LVSplitArcName(fn, arc, file)) {
        return LVFileExists(arc);
    } else {
        return LVFileExists(fn);
    }
}

LVStreamRef LVGetBookCoverStream(lString8 _path) {
    lString16 path = Utf8ToUnicode(_path);
    lString16 arcname, item;
    LVStreamRef res;
    LVContainerRef arc;
    if (!LVSplitArcName(path, arcname, item)) {
        // not in archive
        LVStreamRef stream = LVOpenFileStream(path.c_str(), LVOM_READ);
        if (!stream.isNull()) {
            arc = LVOpenArchieve(stream);
            if (!arc.isNull()) {
                // ZIP-based format
                if (DetectEpubFormat(stream)) {
                    // EPUB
                    // extract coverpage from epub
                    res = GetEpubCoverpage(arc);
                    return res;
                }
            } else {
                doc_format_t fmt;
                if (DetectPDBFormat(stream, fmt)) {
                    res = GetPDBCoverpage(stream);
                    return res;
                }
            }
        }
    } else {
        LVStreamRef arcstream = LVOpenFileStream(arcname.c_str(), LVOM_READ);
        if (!arcstream.isNull()) {
            arc = LVOpenArchieve(arcstream);
            if (!arc.isNull()) {
                LVStreamRef stream = arc->OpenStream(item.c_str(), LVOM_READ);
                if (!stream.isNull()) {
                    doc_format_t fmt;
                    if (DetectPDBFormat(stream, fmt)) {
                        res = GetPDBCoverpage(stream);
                        return res;
                    }
                }
            }
        }
    }
    return res;
}

LVStreamRef LVScanBookCover(lString8 _path, int & type) {
    type = COVER_EMPTY;
    if (_path.empty())
        return LVStreamRef();
    lString16 path = Utf8ToUnicode(_path);
    CRLog::debug("scanBookCoverInternal(%s) called", LCSTR(path));
    lString16 arcname, item;
    LVStreamRef res;
    LVContainerRef arc;
    if (!LVSplitArcName(path, arcname, item)) {
        // not in archive
        LVStreamRef stream = LVOpenFileStream(path.c_str(), LVOM_READ);
        if (!stream.isNull()) {
            arc = LVOpenArchieve(stream);
            if (!arc.isNull()) {
                // ZIP-based format
                if (DetectEpubFormat(stream)) {
                    // EPUB
                    // extract coverpage from epub
                    res = GetEpubCoverpage(arc);
                    if (!res.isNull())
                        type = COVER_FROMBOOK;
                }
            } else {
                res = GetFB2Coverpage(stream);
                if (res.isNull()) {
                    doc_format_t fmt;
                    if (DetectPDBFormat(stream, fmt)) {
                        res = GetPDBCoverpage(stream);
                        if (!res.isNull())
                            type = COVER_FROMBOOK;
                    }
                } else {
                    type = COVER_CACHED;
                }
            }
        }
    } else {
        CRLog::debug("scanBookCoverInternal() : is archive, item=%s, arc=%d", LCSTR(item), LCSTR(arcname));
        LVStreamRef arcstream = LVOpenFileStream(arcname.c_str(), LVOM_READ);
        if (!arcstream.isNull()) {
            arc = LVOpenArchieve(arcstream);
            if (!arc.isNull()) {
                LVStreamRef stream = arc->OpenStream(item.c_str(), LVOM_READ);
                if (!stream.isNull()) {
                    CRLog::debug("scanBookCoverInternal() : archive stream opened ok, parsing");
                    res = GetFB2Coverpage(stream);
                    if (res.isNull()) {
                        doc_format_t fmt;
                        if (DetectPDBFormat(stream, fmt)) {
                            res = GetPDBCoverpage(stream);
                            if (!res.isNull())
                                type = COVER_FROMBOOK;
                        }
                    } else {
                        type = COVER_CACHED;
                    }
                }
            }
        }
    }
    if (!res.isNull())
        CRLog::debug("scanBookCoverInternal() : returned cover page stream");
    else
        CRLog::debug("scanBookCoverInternal() : cover page data not found");
    return res;
}



CRCoverFileCache * coverCache = NULL;

CRCoverFileCache::CRCoverFileCache(lString16 dir, int maxitems, int maxfiles, int maxsize) : _dir(dir), _maxitems(maxitems), _maxfiles(maxfiles), _maxsize(maxsize), _nextId(0) {
	CRLog::info("Created CRCoverFileCache - maxItems=%d maxFiles=%d maxSize=%d at %s", _maxitems, _maxfiles, _maxsize, LCSTR(dir));
    LVAppendPathDelimiter(_dir);
    _filename = _dir;
    _filename += "covercache.ini";
    LVCreateDirectory(_dir);
    _mutex = concurrencyProvider->createMutex();
}

lString8 CRCoverFileCache::generateNextFilename() {
    char s[32];
    _nextId++;
    sprintf(s, "%08d.img", _nextId);
    return lString8(s);
}

lString8 CRCoverFileCache::saveToCache(LVStreamRef stream) {
    if (stream.isNull())
        return lString8::empty_str;
    lString8 fn = generateNextFilename();
    lString16 fn16 = _dir + Utf8ToUnicode(fn);
    LVStreamRef out = LVOpenFileStream(fn16.c_str(), LVOM_WRITE);
    if (out.isNull())
        return lString8::empty_str;
    lvsize_t sz = stream->GetSize();
    lvsize_t saved = LVPumpStream(out.get(), stream.get());
    if (sz != saved)
        return lString8::empty_str;
    return fn;
}

CRCoverFileCache::Entry * CRCoverFileCache::add(const lString8 & pathname, int type, int sz, lString8 & fn) {
    Entry * p = new Entry(pathname, type);
    p->size = sz;
    p->cachedFile = fn;
    _cache.pushBack(p);
    return p;
}

void CRCoverFileCache::cacheDownloadedImage(const lString8 & fn, LVStreamRef stream) {
    if (!stream.isNull() && stream->GetSize() != 0) {
        put(fn, COVER_CACHED, stream);
    } else {
        put(fn, COVER_EMPTY, stream);
    }
}

CRCoverFileCache::Entry * CRCoverFileCache::scan(const lString8 & pathname) {
    int type = COVER_EMPTY;
    LVStreamRef stream = LVScanBookCover(pathname, type);
    return put(pathname, type, stream);
}

CRCoverFileCache::Entry * CRCoverFileCache::put(const lString8 & pathname, int type, LVStreamRef stream) {
    Entry * existing = find(pathname);
    if (existing) {
    	CRLog::error("CRCoverFileCache::put - existing item found for %s", pathname.c_str());
        return existing;
    }
    CRGuard guard(_mutex); CR_UNUSED(guard);
    if (stream.isNull() || stream->GetSize() == 0)
        type = COVER_EMPTY;
    int sz = !stream.isNull() ? (int)stream->GetSize() : 0;
    Entry * p = new Entry(pathname, type);
    if (type == COVER_CACHED) {
        lString8 cached = saveToCache(stream);
        if (!cached.empty()) {
            p->size = sz;
            p->cachedFile = cached;
        } else {
            p->type = COVER_EMPTY;
        }
    }
    CRLog::trace("Adding item %s type=%d size=%d", p->pathname.c_str(), p->type, p->size);
    _cache.pushFront(p);
    checkSize();
    save();
    return p;
}

lString16 CRCoverFileCache::getFilename(Entry * item) {
    return _dir + Utf8ToUnicode(item->cachedFile);
}

void CRCoverFileCache::checkSize() {
    int totalBooks = 0;
    int totalFiles = 0;
    int totalSize = 0;
    for (LVQueue<Entry*>::Iterator iterator = _cache.iterator(); iterator.next(); ) {
        Entry * item = iterator.get();
        if (item->type == COVER_CACHED) {
            totalSize += item->size;
            totalFiles++;
        }
        totalBooks++;
        if ((totalBooks > _maxitems || totalFiles > _maxfiles || totalSize > _maxsize) && (totalBooks > 10)) {
            iterator.remove();
            //CRLog::trace("CRCoverFileCache::checkSize() - totalBooks:%d totalFiles:%d totalSize:%d Removing cover file item %s", totalBooks, totalFiles, totalSize, item->pathname.c_str());
            if (item->type == COVER_CACHED)
                LVDeleteFile(getFilename(item));
            delete item;
        }
    }
}

bool CRCoverFileCache::isCached(const lString8 & fn) {
    CRGuard guard(_mutex); CR_UNUSED(guard);
    for (LVQueue<Entry*>::Iterator iterator = _cache.iterator(); iterator.next(); ) {
        Entry * item = iterator.get();
        if (item->pathname == fn) {
            iterator.moveToHead();
            return true;
        }
    }
    return false;
}

bool CRCoverFileCache::knownCachedFile(const lString8 & fn) {
    for (LVQueue<Entry*>::Iterator iterator = _cache.iterator(); iterator.next(); ) {
        Entry * item = iterator.get();
        if (item->cachedFile == fn)
            return true;
    }
    return false;
}

CRCoverFileCache::Entry * CRCoverFileCache::find(const lString8 & pathname) {
    CRGuard guard(_mutex); CR_UNUSED(guard);
    for (LVQueue<Entry*>::Iterator iterator = _cache.iterator(); iterator.next(); ) {
        Entry * item = iterator.get();
        if (item->pathname == pathname) {
            iterator.moveToHead();
            return item;
        }
    }
    return NULL;
}

void CRCoverFileCache::clear() {
    for (LVQueue<Entry*>::Iterator iterator = _cache.iterator(); iterator.next(); ) {
        Entry * item = iterator.remove();
        delete item;
    }
}

LVStreamRef CRCoverFileCache::getStream(const lString8 & pathname) {
    //CRLog::trace("CRCoverFileCache::getStream %s", pathname.c_str());
    Entry * item = find(pathname);
    if (!item) {
        //CRLog::trace("CRCoverFileCache::getStream Cache item %s not found, scanning", pathname.c_str());
        item = scan(pathname);
//        if (!item)
//        	CRLog::trace("item %s scanned, no coverpage found", pathname.c_str());
//        else
//        	CRLog::trace("item %s scanned, type=%d size=%d", item->pathname.c_str(), item->type, item->size);
    }
    if (!item)
        return LVStreamRef();
    return getStream(item);
}


LVStreamRef CRCoverFileCache::getStream(Entry * item) {
    //CRLog::trace("CRCoverFileCache::getStream(type = %d)", item->type);
    if (item->type == COVER_EMPTY)
        return LVStreamRef();
    if (item->type == COVER_CACHED)
        return LVOpenFileStream(getFilename(item).c_str(), LVOM_READ);
    //CRLog::trace("Calling LVGetBookCoverStream(%s)", item->pathname.c_str());
    return LVGetBookCoverStream(item->pathname);
}

bool CRCoverFileCache::open() {
    LVStreamRef in = LVOpenFileStream(_filename.c_str(), LVOM_READ);
    if (in.isNull())
        return false;
    int sz = (int)in->GetSize();
    lString8 buf;
    buf.append(sz, ' ');
    lvsize_t bytesRead;
    in->Read(buf.modify(), buf.length(), &bytesRead);
    if ((int)bytesRead != buf.length())
        return false;
    lString8Collection lines;
    lines.split(buf, lString8("\n"));
    if (lines.length() < 1)
        return false;
    _nextId = lines[0].atoi();
    for (int i = 1; i < lines.length(); i++) {
        lString8 line = lines[i];
        int p = line.rpos("=");
        if (p > 0) {
            lString8 pathname = line.substr(0, p);
            bool isDownloaded = pathname.startsWith("http://") || pathname.startsWith("https://");
            lString8 params = line.substr(p + 1);
            if (params.length() > 0 && params.length() < 100) {
                int t = 0;
                int sz = 0;
                char s[100];
                if (sscanf(params.c_str(), "%d,%d,%s", &t, &sz, s) != 3)
                    return false;
                lString16 coverfile = _dir + Utf8ToUnicode(s);
                if (t == COVER_CACHED && !LVFileExists(coverfile)) {
                    // cover file deleted
                    continue;
                }
                if (t != COVER_CACHED && t != COVER_EMPTY && t != COVER_FROMBOOK)
                    continue;
                if (!isDownloaded && !LVBookFileExists(pathname)) {
                    // book file deleted
                    if (t == COVER_CACHED)
                        LVDeleteFile(coverfile);
                    continue;
                }
                lString8 s8(s);
                add(pathname, t, sz, s8);
            }
        }
    }
    /// delete unknown files from cache directory
    LVContainerRef dir = LVOpenDirectory(_dir.c_str(), L"*.img");
    if (!dir.isNull()) {
        for (int i = 0; i<dir->GetObjectCount(); i++) {
            const LVContainerItemInfo * item = dir->GetObjectInfo(i);
            if (!item->IsContainer()) {
                lString8 fn = UnicodeToUtf8(item->GetName());
                if (!fn.endsWith(".img"))
                    continue;
                if (!knownCachedFile(fn))
                    LVDeleteFile(_dir + item->GetName());
            }
        }
    }
    return true;
}

bool CRCoverFileCache::save() {
    LVStreamRef out = LVOpenFileStream(_filename.c_str(), LVOM_WRITE);
    if (out.isNull())
        return false;
    lString8 buf;
    buf << lString8::itoa(_nextId) << "\n";
    for (LVQueue<Entry*>::Iterator iterator = _cache.iterator(); iterator.next(); ) {
        Entry * item = iterator.get();
        buf << item->pathname << "=" << lString8::itoa(item->type) << "," << lString8::itoa(item->size) << "," << item->cachedFile << "\n";
    }
    lvsize_t bytesWritten = 0;
    out->Write(buf.c_str(), buf.length(), &bytesWritten);
    return (int)bytesWritten == buf.length();
}










static lUInt32 getAvgColor(LVColorDrawBuf * buf, lvRect & rc, int borderSize = 0) {
    int stats[0x1000];
    memset(stats, 0, sizeof(int) * 0x1000);
    for (int y = rc.top; y < rc.bottom; y++) {
        bool hborder = !borderSize || y <= rc.top + borderSize || y > rc.bottom - borderSize;
        lUInt32 * row = (lUInt32*)buf->GetScanLine(y);
        for (int x = rc.left; x < rc.right; x++) {
            bool vborder = !borderSize || x <= rc.left + borderSize || x > rc.right - borderSize;
            if (hborder || vborder) {
                lUInt32 cl = row[x];
                if ((cl & 0xFF000000) == 0) {
                    // convert to 12 bit color
                    int cl12 = ((cl >> 12) & 0xF00) | ((cl >> 8) & 0xF0) | ((cl >> 4) & 0xF);
                    stats[cl12]++;
                }
            }
        }
    }
    int maxcount = stats[0];
    lUInt32 cl12 = 0;
    for (int i = 1; i < 0x1000; i++) {
        if (maxcount < stats[i]) {
            maxcount = stats[i];
            cl12 = i;
        }
    }
    return ((cl12 & 0xF00) << 12) | ((cl12 & 0xF0) << 8) | ((cl12 & 0xF) << 4);
}

// distance between two colors
static inline int calcDist(lUInt32 c1, lUInt32 c2) {
    int dr = ((c1 >> 16) & 0xFF) - ((c2 >> 16) & 0xFF);
    int dg = ((c1 >> 8) & 0xFF) - ((c2 >> 8) & 0xFF);
    int db = ((c1 >> 0) & 0xFF) - ((c2 >> 0) & 0xFF);
    if (dr < 0) dr = -dr;
    if (dg < 0) dg = -dg;
    if (db < 0) db = -db;
    return dr + dg + db;
}

// returns 0 if very close to src color (full transform), 255 if very close to neutral color
static inline int calcDistAlpha(int ds, int dn) {
    return (ds + dn > 5) ? 255 * ds / (ds + dn) : 255;
}

static inline int calcMult(int sc, int dc) {
    return sc > (dc >> 4) ? 256 * dc / sc : 256 * 16;
}

static inline int correctComponent(int c, int mult, int alpha) {
    if (alpha >= 250)
        return c; // no transform, very close to neutral
    int cm = (c * mult) >> 8;
    int res = ((cm * (255-alpha)) + (c * alpha)) >> 8;
    if (res < 0)
        return 0;
    else if (res > 255)
        return 255;
    return res;
}

static void correctColors(LVColorDrawBuf * buf, lUInt32 srcColor, lUInt32 dstColor, lUInt32 neutralColor) {
    int sr = (srcColor >> 16) & 0xFF;
    int sg = (srcColor >> 8) & 0xFF;
    int sb = (srcColor >> 0) & 0xFF;
    int dr = (dstColor >> 16) & 0xFF;
    int dg = (dstColor >> 8) & 0xFF;
    int db = (dstColor >> 0) & 0xFF;
    int mr = calcMult(sr, dr);
    int mg = calcMult(sg, dg);
    int mb = calcMult(sb, db);
    for (int y = 0; y < buf->GetHeight(); y++) {
        lUInt32 * row = (lUInt32*)buf->GetScanLine(y);
        for (int x = 0; x < buf->GetWidth(); x++) {
            lUInt32 cl = row[x];
            if ((cl & 0xFF000000) < 0xF0000000) {
                int ds = calcDist(cl, srcColor); // distance to src color
                int dn = calcDist(cl, neutralColor); // distance to neutral color
                int alpha = calcDistAlpha(ds, dn);
                int r = correctComponent((cl >> 16) & 0xFF, mr, alpha);
                int g = correctComponent((cl >> 8) & 0xFF, mg, alpha);
                int b = correctComponent((cl >> 0) & 0xFF, mb, alpha);
                row[x] = (cl & 0xFF000000) | (r << 16) | (g << 8) | b;
            }
        }
    }
}

CRCoverImageCache::Entry * CRCoverImageCache::draw(CRCoverPageManager * _manager, CRDirEntry * _book, int dx, int dy) {
    CRLog::trace("CRCoverImageCache::draw called for %s", _book->getCoverPathName().c_str());
    CRENGINE_GUARD;
    LVStreamRef stream = coverCache->getStream(_book->getCoverPathName());
    LVImageSourceRef image;
    if (!stream.isNull() && stream->GetSize() != 0) {
        image = LVCreateStreamImageSource(stream);
    }

    // TODO: fix font face

    LVColorDrawBuf * drawbuf = createDrawBuf(dx, dy);
    drawbuf->beforeDrawing();

    lvRect clientRect;
    lUInt32 bookImageColor;
    lUInt32 neutralColor;
    if (_manager->drawBookTemplate(drawbuf, clientRect, bookImageColor, neutralColor)) {
        // has book template
        LVColorDrawBuf * buf = createDrawBuf(clientRect.width(), clientRect.height());
        buf->beforeDrawing();
        CRDrawBookCover(buf, lString8("Arial"), _book, image, crconfig.einkMode ? 4 : 32);
        buf->afterDrawing();

        lvRect rc(0, 0, buf->GetWidth(), buf->GetHeight());
        lUInt32 coverImageColor = getAvgColor(buf, rc, rc.width() / 10);
        correctColors(drawbuf, bookImageColor, coverImageColor, neutralColor);

        //CRUIDrawTo(buf, drawbuf, clientRect.left, clientRect.top);
        buf->DrawTo(drawbuf, clientRect.left, clientRect.top, 0, NULL);
        delete buf;
    } else {
        // does not have book template
        CRDrawBookCover(drawbuf, lString8("Arial"), _book, image, crconfig.einkMode ? 4 : 32);
    }

    drawbuf->afterDrawing();
    return put(_book, dx, dy, drawbuf);
}

CRCoverImageCache::Entry * CRCoverImageCache::find(CRDirEntry * _book, int dx, int dy) {
    for (LVQueue<Entry*>::Iterator iterator = _cache.iterator(); iterator.next(); ) {
        Entry * item = iterator.get();
        if (item->book->getCoverPathName() == _book->getCoverPathName() && item->dx == dx && item->dy == dy) {
            iterator.moveToHead();
            return item;
        }
    }
    return NULL;
}

CRCoverImageCache::Entry * CRCoverImageCache::put(CRDirEntry * _book, int _dx, int _dy, LVDrawBuf * _image) {
    Entry * existing = find(_book, _dx, _dy);
    if (existing) {
        return existing;
    }
    Entry * p = new Entry(_book->clone(), _dx, _dy, _image);
    _cache.pushFront(p);
    checkSize();
    return p;
}

void CRCoverImageCache::checkSize() {
    int totalItems = 0;
    int totalSize = 0;
    for (LVQueue<Entry*>::Iterator iterator = _cache.iterator(); iterator.next(); ) {
        Entry * item = iterator.get();
        totalSize += item->dx * item->dy * 4;
        totalItems++;
        if ((totalItems > _maxitems || totalSize > _maxsize) && totalItems > 10) {
            CRLog::trace("CRCoverImageCache::checkSize() removing item %s %dx%d", item->book->getCoverPathName().c_str(), item->dx, item->dy);
            iterator.remove();
            delete item;
        }
    }
}

LVColorDrawBuf * CRCoverImageCache::createDrawBuf(int dx, int dy) {
    LVColorDrawBuf * res = new LVColorDrawBuf(dx, dy, 32);
    res->Clear(0xFF000000);
    return res;
}

void CRCoverImageCache::clear() {
    CRLog::info("CRCoverImageCache::clear()");
    for (LVQueue<Entry*>::Iterator iterator = _cache.iterator(); iterator.next(); ) {
        Entry * item = iterator.remove();
        delete item;
    }
}

CRCoverImageCache::CRCoverImageCache(int maxitems, int maxsize) : _maxitems(maxitems), _maxsize(maxsize){
    CRLog::info("CRCoverImageCache:: maxitems=%d maxsize=%d", maxitems, maxsize);
}

CRCoverImageCache * coverImageCache = NULL;

void CRCoverPageManager::allTasksFinished() {
    // already under lock in background thread
    if (_allTasksFinishedCallback) {
        concurrencyProvider->executeGui(_allTasksFinishedCallback); // callback will be deleted in GUI thread
        _allTasksFinishedCallback = NULL;
    }
}

/// set book image to draw covers on - instead of plain cover images
void CRCoverPageManager::setCoverPageTemplate(LVImageSourceRef image, const lvRect & clientRect, const lvRect & neutralRect) {
    _bookImage = image;
    _bookImageClientRect = clientRect;
    _bookImageNeutralRect = neutralRect;
    clearImageCache();
}

int CRCoverPageManager::BookImageCache::find(int dx, int dy) {
    for (int i = 0; i < items.length(); i++) {
        if (items[i]->dx == dx && items[i]->dy == dy)
            return i;
    }
    return -1;
}

#define MAX_BOOK_IMAGE_CACHE_SIZE 3
CRCoverPageManager::BookImage * CRCoverPageManager::BookImageCache::get(LVImageSourceRef img, const lvRect & rc, const lvRect & neutralRc, int dx, int dy) {
    int index = find(dx, dy);
    if (index >= 0) {
        if (index > 0)
            items.move(0, index);
        return items[0];
    }
    if (items.length() >= MAX_BOOK_IMAGE_CACHE_SIZE)
        delete items.remove(items.length() - 1);
    items.insert(0, new CRCoverPageManager::BookImage(img, rc, neutralRc, dx, dy));
    return items[0];
}

CRCoverPageManager::BookImage::BookImage(LVImageSourceRef img, const lvRect & rc, const lvRect & neutralRc, int _dx, int _dy) : dx(_dx), dy(_dy) {
    clientRc.left = rc.left * dx / img->GetWidth();
    clientRc.top = rc.top * dy / img->GetHeight();
    clientRc.right = rc.right * dx / img->GetWidth();
    clientRc.bottom = rc.bottom * dy / img->GetHeight();
    lvRect nrc;
    nrc.left = neutralRc.left * dx / img->GetWidth();
    nrc.top = neutralRc.top * dy / img->GetHeight();
    nrc.right = neutralRc.right * dx / img->GetWidth();
    nrc.bottom = neutralRc.bottom * dy / img->GetHeight();
    buf = new LVColorDrawBuf(dx, dy, 32);
    buf->Clear(0xFF000000); // transparent
    buf->Draw(img, 0, 0, dx, dy, false);
    color = getAvgColor(buf, clientRc);
    neutralColor = getAvgColor(buf, nrc);
}

void CRCoverPageManager::setAllTasksFinishedCallback(CRRunnable * allTasksFinishedCallback) {
    // called from GUI thread
    CRGuard guard(_monitor);
    if (_stopped || (!_taskIsRunning && _queue.length() == 0)) {
        // call immediately
        allTasksFinishedCallback->run();
        delete allTasksFinishedCallback;
    }
    /// set callback
    if (_allTasksFinishedCallback)
        delete _allTasksFinishedCallback;
    _allTasksFinishedCallback = allTasksFinishedCallback;
}


void CRCoverPageManager::stop() {
    if (_stopped)
        return;
    {
        CRGuard guard(_monitor);
        CRLog::trace("CRCoverPageManager::stop() - under lock");
        _stopped = true;
        CRLog::trace("CRCoverPageManager::stop() deleting all tasks from queue");
        while (_queue.length() > 0) {
            CoverTask * p = _queue.popFront();
            delete p;
        }
        CRLog::trace("CRCoverPageManager::stop() all tasks are deleted, calling notifyAll on monitor");
        _monitor->notifyAll();
    }
    CRLog::trace("CRCoverPageManager::stop() calling Join");
    _thread->join();
    CRLog::trace("CRCoverPageManager::stop() joined");
}

void CRCoverPageManager::run() {
    CRLog::info("CRCoverPageManager::run thread started");
    // testing tizen concurrency

    for (;;) {
        if (_stopped)
            break;
        CoverTask * task = NULL;
        {
            //CRLog::trace("CRCoverPageManager::run :: wait for lock");
            CRGuard guard(_monitor);
            if (_stopped)
                break;
            if (_paused) {
                CRLog::trace("CRCoverPageManager::run :: paused. Waiting...");
                _monitor->wait();
                continue;
            }
            //CRLog::trace("CRCoverPageManager::run :: lock acquired");
            if (_queue.length() <= 0) {
                allTasksFinished();
                //CRLog::trace("CRCoverPageManager::run :: calling monitor wait");
                _monitor->wait();
                //CRLog::trace("CRCoverPageManager::run :: done monitor wait");
            }
            if (_stopped)
                break;
            task = _queue.popFront();
            if (task)
                _taskIsRunning = true;
            else
                allTasksFinished();
        }
        /// execute w/o lock
        if (task) {

            //CRLog::trace("CRCoverPageManager: searching in cache");
            CRCoverImageCache::Entry * entry = coverImageCache->find(task->book, task->dx, task->dy);
            if (!entry) {
                //CRLog::trace("CRCoverPageManager: rendering new coverpage image ");
                entry = coverImageCache->draw(this, task->book, task->dx, task->dy);
            }
            //CRLog::trace("CRCoverPageManager: calling ready callback");
            if (task->callback)
                concurrencyProvider->executeGui(task->callback); // callback will be deleted in GUI thread
            delete task;
            _taskIsRunning = false;
        }
        // process next event
    }
    CRLog::info("CRCoverPageManager thread finished");
}

LVDrawBuf * CRCoverPageManager::getIfReady(CRDirEntry * _book, int dx, int dy)
{
    //CRLog::trace("CRCoverPageManager::getIfReady :: wait for lock");
    CRGuard guard(_monitor);
    //CRLog::trace("CRCoverPageManager::getIfReady :: lock acquired");
    CRCoverImageCache::Entry * existing = coverImageCache->find(_book, dx, dy);
    if (existing) {
        //CRLog::trace("CRCoverPageManager::getIfReady - found existing image for %s %dx%d", _book->getPathName().c_str(), dx, dy);
        return existing->image;
    }
    //CRLog::trace("CRCoverPageManager::getIfReady - not found %s %dx%d", _book->getPathName().c_str(), dx, dy);
    return NULL;
}

/// draws book template and tells its client rect - returns false if book template is not set
bool CRCoverPageManager::drawBookTemplate(LVDrawBuf * buf, lvRect & clientRect, lUInt32 & avgColor, lUInt32 & neutralColor) {
    CRGuard guard(_monitor);
    if (_bookImage.isNull())
        return false;
    BookImage * bookImage = _bookImageCache.get(_bookImage, _bookImageClientRect, _bookImageNeutralRect, buf->GetWidth(), buf->GetHeight());
    bookImage->buf->DrawTo(buf, 0, 0, 0, NULL);
    clientRect = bookImage->clientRc;
    avgColor = bookImage->color;
    neutralColor = bookImage->neutralColor;
    return true;
}

void CRCoverPageManager::prepare(CRDirEntry * _book, int dx, int dy, CRRunnable * readyCallback, ExternalImageSourceCallback * downloadCallback)
{
    //CRLog::trace("CRCoverPageManager::prepare :: wait for lock");
    CRGuard guard(_monitor);
    //CRLog::trace("CRCoverPageManager::prepare %s %dx%d", _book->getPathName().c_str(), dx, dy);
    if (_stopped) {
        CRLog::error("Ignoring new task since cover page manager is stopped");
        return;
    }
    CRCoverImageCache::Entry * existing = coverImageCache->find(_book, dx, dy);
    if (existing) {
        // we are in GUI thread now
        if (readyCallback) {
            readyCallback->run();
            delete readyCallback;
        }
        return;
    }
    lString8 coverPath = _book->getCoverPathName();
    bool isExternal = coverPath.startsWith("http://") || coverPath.startsWith("https://");
    _queue.iterator();
    for (LVQueue<CoverTask*>::Iterator iterator = _queue.iterator(); iterator.next(); ) {
        CoverTask * item = iterator.get();
        if (item->isSame(_book, dx, dy)) {
            //CRLog::trace("CRCoverPageManager::prepare %s %dx%d -- there is already such task in queue", _book->getPathName().c_str(), dx, dy);
            // already has the same task in queue
            iterator.moveToHead();
            return;
        }
    }
    if (isExternal) {
        for (LVQueue<CoverTask*>::Iterator iterator = _externalSourceQueue.iterator(); iterator.next(); ) {
            CoverTask * item = iterator.get();
            if (item->isSame(_book, dx, dy)) {
                //CRLog::trace("CRCoverPageManager::prepare %s %dx%d -- there is already such task in queue", _book->getPathName().c_str(), dx, dy);
                // already has the same task in queue
                iterator.moveToHead();
                return;
            }
        }
    }
    CRDirEntry * book = _book;
    _book = _book->clone();
    CoverTask * task = new CoverTask(_book, dx, dy, readyCallback);
    if (isExternal && downloadCallback) {
        if (coverCache->isCached(coverPath)) {
            // we already have image file cached, just draw it
            CRLog::trace("Cover %s is found in cache; just drawing", coverPath.c_str());
            _queue.pushBack(task);
        } else {
            CRLog::trace("Cover %s is not found in cache; requesting download", coverPath.c_str());
            // request for download externally
            _externalSourceQueue.pushBack(task);
            downloadCallback->onRequestImageDownload(book);
        }
    } else {
        _queue.pushBack(task);
    }
    _monitor->notify();
    //CRLog::trace("CRCoverPageManager::prepare - added new task %s %dx%d", _book->getPathName().c_str(), dx, dy);
}

/// once external image source downloaded image, call this method to set image file and continue coverpage preparation
void CRCoverPageManager::setExternalImage(CRDirEntry * _book, LVStreamRef & stream) {
    CRGuard guard(_monitor);
    CR_UNUSED(guard);
    if (_stopped) {
        CRLog::error("Ignoring new task since cover page manager is stopped");
        return;
    }
    bool changed = false;
    for (LVQueue<CoverTask*>::Iterator iterator = _externalSourceQueue.iterator(); iterator.next(); ) {
        CoverTask * item = iterator.get();
        if (item->isSame(_book)) {
            //CRLog::trace("CRCoverPageManager::prepare %s %dx%d -- there is already such task in queue", _book->getPathName().c_str(), dx, dy);
            // already has the same task in queue
            if (!changed) {
                coverCache->cacheDownloadedImage(_book->getCoverPathName(), stream);
                changed = true;
            }
            CoverTask * task = iterator.remove();
            _queue.pushBack(task);
            if (!iterator.get())
                break;
        }
    }
    if (changed)
        _monitor->notify();
}

/// cancels all pending coverpage tasks
void CRCoverPageManager::cancelAll() {
    CRGuard guard(_monitor);
    CR_UNUSED(guard);
    CRLog::trace("CRCoverPageManager::cancelAll");
    for (LVQueue<CoverTask*>::Iterator iterator = _queue.iterator(); iterator.next(); ) {
        CoverTask * item = iterator.get();
        iterator.remove();
        delete item->callback;
        delete item;
    }
    for (LVQueue<CoverTask*>::Iterator iterator = _externalSourceQueue.iterator(); iterator.next(); ) {
        CoverTask * item = iterator.get();
        iterator.remove();
        delete item->callback;
        delete item;
    }
}

/// removes all cached images from memory
void CRCoverPageManager::clearImageCache() {
    CRGuard guard(_monitor);
    CR_UNUSED(guard);
    CRLog::trace("CRCoverPageManager::clearImageCache()");
    coverImageCache->clear();
    _bookImageCache.clear();
}

void CRCoverPageManager::cancel(CRDirEntry * _book, int dx, int dy)
{
    CRGuard guard(_monitor);
    CR_UNUSED(guard);
    CRLog::trace("CRCoverPageManager::cancel %s %dx%d", _book->getCoverPathName().c_str(), dx, dy);
    for (LVQueue<CoverTask*>::Iterator iterator = _queue.iterator(); iterator.next(); ) {
        CoverTask * item = iterator.get();
        if (item->isSame(_book, dx, dy)) {
            iterator.remove();
            delete item->callback;
            delete item;
            break;
        }
    }
    for (LVQueue<CoverTask*>::Iterator iterator = _externalSourceQueue.iterator(); iterator.next(); ) {
        CoverTask * item = iterator.get();
        if (item->isSame(_book)) {
            iterator.remove();
            delete item->callback;
            delete item;
            break;
        }
    }
}

CRCoverPageManager::CRCoverPageManager() : _stopped(false), _allTasksFinishedCallback(NULL), _taskIsRunning(false), _paused(false) {
    _monitor = concurrencyProvider->createMonitor();
    _thread = concurrencyProvider->createThread(this);
}

CRCoverPageManager::~CRCoverPageManager() {
    //stop();
	CRLog::trace("CRCoverPageManager::~CRCoverPageManager");

	CRLog::trace("CRCoverPageManager::~CRCoverPageManager deleting monitor");
	_monitor.clear();
	CRLog::trace("CRCoverPageManager::~CRCoverPageManager deleting thread");
	_thread.clear();
}

/// start thread
void CRCoverPageManager::start() {
    _thread->start();
}

/// pause thread
void CRCoverPageManager::pause() {
    {
        CRGuard guard(_monitor);
        CR_UNUSED(guard);
        if (_paused)
            return;
        CRLog::trace("CRCoverPageManager::pause()");
        _paused = true;
    }
    while (true) {
        {
            CRGuard guard(_monitor);
            CR_UNUSED(guard);
            if (!_taskIsRunning || _stopped) {
                CRLog::trace("No active coverpage task");
                break;
            }
            CRLog::trace("Waiting for current coverpage task completion");
        }
        concurrencyProvider->sleepMs(100);
    }
}

/// resume thread
void CRCoverPageManager::resume() {
    CRGuard guard(_monitor);
    CR_UNUSED(guard);
    if (!_paused)
        return;
    CRLog::trace("CRCoverPageManager::resume()");
    _paused = false;
    _monitor->notify();
}

CRCoverPageManager * coverPageManager = NULL;

void CRSetupCoverpageManager(lString16 coverCacheDir, int maxitems, int maxfiles, int maxsize, int maxRenderCacheItems, int maxRenderCacheBytes) {
    CRStopCoverpageManager();
    coverCache = new CRCoverFileCache(coverCacheDir, maxitems, maxfiles, maxsize);
    coverCache->open();
    coverImageCache = new CRCoverImageCache(maxRenderCacheItems, maxRenderCacheBytes);
    coverPageManager = new CRCoverPageManager();
}

void CRPauseCoverpageManager() {
    if (coverPageManager) {
        coverPageManager->pause();
    }
}

void CRResumeCoverpageManager() {
    if (coverPageManager) {
        coverPageManager->resume();
    }
}

void CRStartCoverpageManager() {
    if (coverPageManager) {
        coverPageManager->start();
    }
}

void CRStopCoverpageManager() {
    if (coverPageManager) {
    	CRLog::info("Calling coverPageManager->stop()");
        coverPageManager->stop();
    	CRLog::info("Returned from coverPageManager->stop(), calling delete coverPageManager");
        delete coverPageManager;
        coverPageManager = NULL;
    }
    if (coverImageCache) {
    	CRLog::trace("Deleting coverImageCache");
        delete coverImageCache;
        coverImageCache = NULL;
    }
    if (coverCache) {
    	CRLog::trace("Deleting coverCache");
        delete coverCache;
        coverCache = NULL;
    }

}
