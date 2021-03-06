/*
 * glfont.cpp
 *
 *  Created on: Aug 5, 2013
 *      Author: vlopatin
 */

#include "glwrapper.h"

#include <crengine.h>
#include "glfont.h"
#include "glscene.h"
#include "gldrawbuf.h"

#if (USE_FREETYPE==1)

//#include <ft2build.h>

#ifdef ANDROID
#ifdef USE_FREETYPE2
#include "freetype2/config/ftheader.h"
#include "freetype2/freetype.h"
#else
#include "freetype/config/ftheader.h"
#include "freetype/freetype.h"
#endif
#else

#ifdef USE_FREETYPE2
#include <freetype2/config/ftheader.h>
#else
#include <freetype/config/ftheader.h>
#endif
//#include <ft2build.h>
#include FT_FREETYPE_H
//#include <freetype/freetype.h>
#endif

#if (USE_FONTCONFIG==1)
    #include <fontconfig/fontconfig.h>
#endif

#endif

class GLGlyphCachePage;
class GLGlyphCache;
class GLFont;

static GLGlyphCache * _glGlyphCache = NULL;

//#define GL_GLYPH_CACHE_PAGE_SIZE 1024
class GLGlyphCachePage
{
	GLGlyphCache * cache;
	LVGrayDrawBuf * drawbuf;
	int currentLine;
	int nextLine;
	int x;
	bool closed;
	bool needUpdateTexture;
    int tdx;
    int tdy;
    lUInt32 textureId;
public:
	GLGlyphCache * getCache() { return cache; }
	GLGlyphCachePage(GLGlyphCache * pcache) : cache(pcache), drawbuf(NULL), closed(false), needUpdateTexture(false), textureId(0) {
		// init free lines
		currentLine = nextLine = x = 0;
        tdx = tdy = CRGL->getMaxTextureSize();
        if (tdx > 1024) {
            tdx = tdy = 1024;
        }
    }
	virtual ~GLGlyphCachePage() {
		if (drawbuf)
			delete drawbuf;
        if (textureId != 0) {
            CRGL->deleteTexture(textureId);
        }
	}
	void updateTexture() {
		if (drawbuf == NULL)
			return; // no draw buffer!!!
	    if (textureId == 0) {
            //CRLog::debug("GLGlyphCache updateTexture - new texture");
            textureId = CRGL->genTexture();
            if (!textureId)
                return;
	    }
    	//CRLog::debug("updateTexture - setting image %dx%d", drawbuf->GetWidth(), drawbuf->GetHeight());
        if (!CRGL->setTextureImageAlpha(textureId, drawbuf->GetWidth(), drawbuf->GetHeight(), drawbuf->GetScanLine(0))) {
            CRGL->deleteTexture(textureId);
            textureId = 0;
            return;
        }
	    needUpdateTexture = false;
	    if (closed) {
	    	delete drawbuf;
	    	drawbuf = NULL;
	    }
	}
	void drawItem(GLGlyphCacheItem * item, int x, int y, lUInt32 color, lvRect * clip) {
		lUInt64 startTs = GetCurrentTimeMillis();
		if (needUpdateTexture)
			updateTexture();
		if (textureId != 0) {
			//CRLog::trace("drawing character at %d,%d", x, y);
            lvRect srcrc((int)(item->x0),
                         (int)(item->y0),
                         (int)(item->x1),
                         (int)(item->y1));
            lvRect dstrc(x,
                         y,
                         (int)(x + item->dx),
                         (int)(y + item->dy)
                         );
            if (clip) {
                int srcw = srcrc.width();
                int srch = srcrc.height();
                int dstw = dstrc.width();
                int dsth = dstrc.height();
                if (dstw) {
                    srcrc.left += clip->left * srcw / dstw;
                    srcrc.right -= clip->right * srcw / dstw;
                }
                if (dsth) {
                    srcrc.top += clip->top * srch / dsth;
                    srcrc.bottom -= clip->bottom * srch / dsth;
                }
                dstrc.left += clip->left;
                dstrc.right -= clip->right;
                dstrc.top += clip->top;
                dstrc.bottom -= clip->bottom;
            }
            if (!dstrc.isEmpty())
                CRGL->drawColorAndTextureRect(textureId, tdx, tdy, srcrc, dstrc, color, false);
        }
		if (LVGLPeekScene()) LVGLPeekScene()->updateCharacterDrawStats(GetCurrentTimeMillis() - startTs);
	}
	GLGlyphCacheItem * addItem(GLFont * font, LVFontGlyphCacheItem * glyph) {
		if (closed)
			return NULL;
		// next line if necessary
        if (x + glyph->bmp_width > tdx) {
			// move to next line
			currentLine = nextLine;
			x = 0;
		}
		// check if no room left for glyph height
        if (currentLine + glyph->bmp_height > tdy) {
			closed = true;
			updateTexture();
			return NULL;
		}
		GLGlyphCacheItem * cacheItem = new GLGlyphCacheItem();
        cacheItem->x0 = x;
        cacheItem->y0 = currentLine;
        cacheItem->x1 = (x + glyph->bmp_width);
        cacheItem->y1 = (currentLine + glyph->bmp_height);
		cacheItem->dx = glyph->bmp_width;
		cacheItem->dy = glyph->bmp_height;
		cacheItem->originX = glyph->origin_x;
		cacheItem->originY = glyph->origin_y;
		cacheItem->width = glyph->advance;
		cacheItem->page = this;
		cacheItem->font = font;
		// draw glyph to buffer, if non empty
		if (glyph->bmp_height && glyph->bmp_width) {
			//CRLog::trace("Creating new character %d for font %08x", (int)glyph->ch, (lUInt32)font);
			if (nextLine < currentLine + glyph->bmp_height)
				nextLine = currentLine + glyph->bmp_height;
			if (!drawbuf) {
                drawbuf = new LVGrayDrawBuf(tdx, tdy, 8, NULL);
				drawbuf->SetBackgroundColor(0x000000);
				drawbuf->SetTextColor(0xFFFFFF);
				drawbuf->Clear(0x000000);
			}
//#define BCH(x) (glyph->bmp[x] > 127 ?'1':'0')
//			CRLog::debug("Adding new glyph at %d,%d: %c%c%c%c%c%c%c%c", x, currentLine, BCH(0),BCH(1),BCH(2),BCH(3),BCH(4),BCH(5),BCH(6),BCH(7) );
			drawbuf->Draw(x, currentLine, glyph->bmp, glyph->bmp_width, glyph->bmp_height, NULL);
			x += glyph->bmp_width;
			needUpdateTexture = true;
		}
		return cacheItem;
	}
};

void GLGlyphCacheItem::draw(int x, int y, lUInt32 color, lvRect * clip) {
	page->drawItem(this, x, y, color, clip);
}

//============================================================================================
// GLGlyphCache implementation
#define LVGLMakeGlyphKey(ch, font) ((((lUInt64)ch) << 32) ^ ((lUInt64)font))

static void onLastSceneFinishGlyphCacheClear() {
    if (_glGlyphCache)
        _glGlyphCache->clear();
}

void GLGlyphCache::clear() {
    if (LVGLPeekScene()) {
        CRLog::info("Postpone clearing GL glyph cache until end drawing of scene");
        LVGLSetLastSceneCallback(onLastSceneFinishGlyphCacheClear);
        return;
    }
	CRLog::info("GLGlyphCache::clear() map size = %d, pages size = %d", _map.length(), _pages.length());
	LVHashTable<lUInt64, GLGlyphCacheItem*>::iterator iter = _map.forwardIterator();
	for (;;) {
		LVHashTable<lUInt64, GLGlyphCacheItem*>::pair * item = iter.next();
		if (!item)
			break;
		delete item->value;
		item->value = NULL;
	}
	_map.clear();
	_pages.clear();
}

void GLGlyphCache::clearFontGlyphs(GLFont * font) {
	LVHashTable<lUInt64, GLGlyphCacheItem*>::iterator iter = _map.forwardIterator();
	LVArray<lUInt64> keysForRemove(256, 0);
	for (;;) {
		LVHashTable<lUInt64, GLGlyphCacheItem*>::pair * item = iter.next();
		if (!item)
			break;
		if (item->value) {
			if (item->value->font == font)
				keysForRemove.add(item->key);
		}
	}
	for (int i = 0; i<keysForRemove.length(); i++) {
		GLGlyphCacheItem* removed = _map.get(keysForRemove[i]);
		_map.remove(keysForRemove[i]);
		if (removed)
			delete removed;
	}
}
GLGlyphCache::GLGlyphCache() : _map(32768) {

}
GLGlyphCache::~GLGlyphCache() {

}
GLGlyphCacheItem * GLGlyphCache::get(lChar16 ch, GLFont * font) {
	GLGlyphCacheItem *  res = _map.get(LVGLMakeGlyphKey(ch, font));
	return res;
}
GLGlyphCacheItem * GLGlyphCache::put(lChar16 ch, GLFont * font, LVFontGlyphCacheItem * glyph) {
	GLGlyphCachePage * page;
	if (_pages.length() == 0) {
		page = new GLGlyphCachePage(this);
		_pages.add(page);
	}
	page = _pages[_pages.length() - 1];
	GLGlyphCacheItem * item = page->addItem(font, glyph);
	if (!item) {
		page = new GLGlyphCachePage(this);
		_pages.add(page);
		item = page->addItem(font, glyph);
	}
	_map.set(LVGLMakeGlyphKey(ch, font), item);
	return item;
}


//=======================================================================================================
/// font manager interface class
class GLFontManager : public LVFontManager
{
protected:
	LVFontManager * _base;
	LVHashTable<LVFont *, LVFontRef> _mapByBase;
	LVHashTable<LVFont *, LVFontRef> _mapByGl;
	GLGlyphCache * _cache;
public:
    /// garbage collector frees unused fonts
    virtual void gc();
    /// returns most similar font
    virtual LVFontRef GetFont(int size, int weight, bool italic, css_font_family_t family, lString8 typeface, int documentId = -1);
    /// set fallback font face (returns true if specified font is found)
    virtual bool SetFallbackFontFace( lString8 face );
    /// get fallback font face (returns empty string if no fallback font is set)
    virtual lString8 GetFallbackFontFace();
    /// returns fallback font for specified size
    virtual LVFontRef GetFallbackFont(int /*size*/);
    /// registers font by name
    virtual bool RegisterFont( lString8 name );
    /// registers document font
    virtual bool RegisterDocumentFont(int /*documentId*/, LVContainerRef /*container*/, lString16 /*name*/, lString8 /*face*/, bool /*bold*/, bool /*italic*/);
    /// unregisters all document fonts
    virtual void UnregisterDocumentFonts(int /*documentId*/);
    /// initializes font manager
    virtual bool Init( lString8 path );
    /// get count of registered fonts
    virtual int GetFontCount();
    /// get hash of installed fonts and fallback font
    virtual lUInt32 GetFontListHash(int /*documentId*/);
    /// clear glyph cache
    virtual void clearGlyphCache();

    /// get antialiasing mode
    virtual int GetAntialiasMode();
    /// set antialiasing mode
    virtual void SetAntialiasMode( int mode );

    /// get kerning mode: true==ON, false=OFF
    virtual bool getKerning();

    /// get kerning mode: true==ON, false=OFF
    virtual void setKerning( bool kerningEnabled );

    /// notification from GLFont - instance is going to close
	virtual void removeFontInstance(LVFont * glFont);

    /// constructor
    GLFontManager(LVFontManager * base);
    /// destructor
    virtual ~GLFontManager();

    /// returns available typefaces
    virtual void getFaceList( lString16Collection & );

    /// sets current hinting mode
    virtual void SetHintingMode(hinting_mode_t /*mode*/);
    /// returns current hinting mode
    virtual hinting_mode_t  GetHintingMode();

    GLGlyphCache * getCache() {
    	return _cache;
    }

    /// sets current gamma level index
    virtual void SetGammaIndex( int gammaIndex ) {
        _base->SetGammaIndex(gammaIndex);
        _cache->clear();
    }

};

bool LVInitGLFontManager(LVFontManager * base) {
    if (fontMan && fontMan != base) {
        delete fontMan;
    }
    fontMan = new GLFontManager(base);
    return true;
}

class GLCharGlyphItem : public GLSceneItem {
	GLGlyphCacheItem * item;
	int x;
	int y;
	lUInt32 color;
	lvRect * clip;
public:
	GLCharGlyphItem(GLGlyphCacheItem * _item, int _x, int _y, lUInt32 _color, lvRect * _clip)
	: item(_item), x(_x), y(_y), color(_color), clip(_clip)
	{
	}
	virtual void draw() {
		item->draw(x, y, color, clip);
	}
	virtual ~GLCharGlyphItem() {
		if (clip)
			delete clip;
	}
};

/** \brief base class for fonts

    implements single interface for font of any engine
*/
class GLFont : public LVFont
{
	LVFontRef _base;
	GLFontManager * _fontMan;
public:

	GLFont(LVFontRef baseFont, GLFontManager * manager) {
		_base = baseFont;
		_fontMan = manager;
	}

	/// hyphenation character
    virtual lChar16 getHyphChar() { return _base->getHyphChar(); }

    /// hyphen width
    virtual int getHyphenWidth() { return _base->getHyphenWidth(); }

    /**
     * Max width of -/./,/!/? to use for visial alignment by width
     */
    virtual int getVisualAligmentWidth() { return _base->getVisualAligmentWidth(); }

    /** \brief get glyph info
        \param glyph is pointer to glyph_info_t struct to place retrieved info
        \return true if glyh was found
    */
    virtual bool getGlyphInfo( lUInt16 code, glyph_info_t * glyph, lChar16 def_char=0 ) { return _base->getGlyphInfo(code, glyph, def_char); }

    /** \brief measure text
        \param text is text string pointer
        \param len is number of characters to measure
        \param max_width is maximum width to measure line
        \param def_char is character to replace absent glyphs in font
        \param letter_spacing is number of pixels to add between letters
        \return number of characters before max_width reached
    */
    virtual lUInt16 measureText(
                        const lChar16 * text, int len,
                        lUInt16 * widths,
                        lUInt8 * flags,
                        int max_width,
                        lChar16 def_char,
                        int letter_spacing=0,
                        bool allow_hyphenation=true
                     ) {
    	return _base->measureText(text, len, widths, flags, max_width, def_char, letter_spacing, allow_hyphenation);
    }
    /** \brief measure text
        \param text is text string pointer
        \param len is number of characters to measure
        \return width of specified string
    */
    virtual lUInt32 getTextWidth(
                        const lChar16 * text, int len
        ) {
    	return _base->getTextWidth(text, len);
    }

//    /** \brief get glyph image in 1 byte per pixel format
//        \param code is unicode character
//        \param buf is buffer [width*height] to place glyph data
//        \return true if glyph was found
//    */
//    virtual bool getGlyphImage(lUInt16 code, lUInt8 * buf, lChar16 def_char=0) = 0;
    /** \brief get glyph item
        \param code is unicode character
        \return glyph pointer if glyph was found, NULL otherwise
    */
    virtual LVFontGlyphCacheItem * getGlyph(lUInt16 ch, lChar16 def_char=0) {
        FONT_GUARD
    	return _base->getGlyph(ch, def_char);
    }
    /// returns font baseline offset
    virtual int getBaseline() {
    	return _base->getBaseline();
    }
    /// returns font height including normal interline space
    virtual int getHeight() const {
    	return _base->getHeight();
    }
    /// returns font character size
    virtual int getSize() const {
    	return _base->getSize();
    }
    /// returns font weight
    virtual int getWeight() const {
    	return _base->getWeight();
    }
    /// returns italic flag
    virtual int getItalic() const {
    	return _base->getItalic();
    }
    /// returns char width
    virtual int getCharWidth( lChar16 ch, lChar16 def_char=0 ) {
    	return _base->getCharWidth(ch, def_char);
    }
    /// retrieves font handle
    virtual void * GetHandle() {
    	return _base->GetHandle();
    }
    /// returns font typeface name
    virtual lString8 getTypeFace() const {
    	return _base->getTypeFace();
    }
    /// returns font family id
    virtual css_font_family_t getFontFamily() const {
    	return _base->getFontFamily();
    }
    /// draws text string
    virtual void DrawTextString( LVDrawBuf * buf, int x, int y,
                       const lChar16 * text, int len,
                       lChar16 def_char, lUInt32 * palette = NULL, bool addHyphen = false,
                       lUInt32 flags=0, int letter_spacing=0 )
    {
		if (len <= 0)
			return;
        // workaround for no-rtti builds
    	GLDrawBuf * glbuf = buf->asGLDrawBuf(); //dynamic_cast<GLDrawBuf*>(buf);
    	if (glbuf) {
    		// use specific rendering for GL buffer
        	GLGlyphCache * cache = _fontMan->getCache();
            GLScene * scene = glbuf->getScene();// {LVGLPeekScene();
        	if (!scene) {
        		CRLog::error("DrawTextString - no current scene");
        		return;
        	}
			if (letter_spacing < 0 || letter_spacing > 50)
				letter_spacing = 0;
			lvRect clip;
			buf->GetClipRect( &clip );
			int _height = _base->getHeight();
			int _size = _base->getSize();
			int _baseline = _base->getBaseline();
			lUInt32 color = glbuf->GetTextColor();
            if ( y + _height < clip.top || y >= clip.bottom || x >= clip.right)
				return;

	#if (ALLOW_KERNING==1)
			bool use_kerning = _base->kerningEnabled();
	#endif
			int i;

			lChar16 previous = 0;
			//lUInt16 prev_width = 0;
			lChar16 ch;
			// measure character widths
			bool isHyphen = false;
			int x0 = x;
			for ( i=0; i<=len; i++) {
				if ( i==len && (!addHyphen || isHyphen) )
					break;
				if ( i<len ) {
					ch = text[i];
					if ( ch=='\t' )
						ch = ' ';
					isHyphen = (ch==UNICODE_SOFT_HYPHEN_CODE) && (i<len-1);
				} else {
					ch = UNICODE_SOFT_HYPHEN_CODE;
					isHyphen = 0;
				}
				int kerning = 0;
	#if (ALLOW_KERNING==1)
				if (use_kerning)
					kerning = _base->getKerningOffset(previous, ch, def_char);
	#endif
				GLGlyphCacheItem * item = cache->get(ch, this);
				if (!item) {
					LVFontGlyphCacheItem * glyph = getGlyph(ch, def_char);
					if (glyph) {
						item = cache->put(ch, this, glyph);
					}
				}
				if ( !item )
					continue;
				if ( (item && !isHyphen) || i>=len-1 ) { // avoid soft hyphens inside text string
					int w = item->width + (kerning >> 6);
					lvRect rc;
					rc.left = x + (kerning>>6) + item->originX;
					rc.top = (y + _baseline - item->originY);
					rc.right = rc.left + item->dx;
					rc.bottom = rc.top + item->dy;
//					clip.top = glbuf->GetHeight() - clip.top;
//					clip.bottom = glbuf->GetHeight() - clip.bottom;
					if (clip.intersects(rc)) {
						lvRect * clipInfo = NULL;
						if (!clip.isRectInside(rc))
							clipInfo = rc.clipBy(clip);
//						scene->add(new GLCharGlyphItem(item,
//								rc.left,
//								glbuf->GetHeight() - rc.top,
//								color, clipInfo));
                        scene->add(new GLCharGlyphItem(item,
                                rc.left,
                                rc.top,
                                color, clipInfo));
                    }
					x  += w + letter_spacing;
					previous = ch;
				}
			}
			if ( flags & LTEXT_TD_MASK ) {
				// text decoration: underline, etc.
				int h = _size > 30 ? 2 : 1;
				lUInt32 cl = buf->GetTextColor();
				if ( (flags & LTEXT_TD_UNDERLINE) || (flags & LTEXT_TD_BLINK) ) {
					int liney = y + _baseline + h;
					buf->FillRect( x0, liney, x, liney+h, cl );
				}
				if ( flags & LTEXT_TD_OVERLINE ) {
					int liney = y + h;
					buf->FillRect( x0, liney, x, liney+h, cl );
				}
				if ( flags & LTEXT_TD_LINE_THROUGH ) {
	//                int liney = y + _baseline - _size/4 - h/2;
					int liney = y + _baseline - _size*2/7;
					buf->FillRect( x0, liney, x, liney+h, cl );
				}
			}
        } else if (buf->isTiled()) {
            for (int ty = 0; ty < buf->getYtiles(); ty++) {
                for (int tx = 0; tx < buf->getXtiles(); tx++) {
                    LVDrawBuf * tile = buf->getTile(tx, ty);
                    lvRect tilerc;
                    buf->getTileRect(tilerc, tx, ty);
                    if (x > tilerc.right || y > tilerc.bottom || y < tilerc.top - getHeight())
                        continue;
                    DrawTextString(tile, x - tilerc.left, y - tilerc.top,
                                   text, len,
                                   def_char, palette, addHyphen,
                                   flags, letter_spacing);
                }
            }
        } else {
    		// use base font rendering for non-GL buffers
    		_base->DrawTextString(buf, x, y, text, len, def_char, palette, addHyphen, flags, letter_spacing);
    	}
    }

    /// get bitmap mode (true=monochrome bitmap, false=antialiased)
    virtual bool getBitmapMode() { return _base->getBitmapMode(); }
    /// set bitmap mode (true=monochrome bitmap, false=antialiased)
    virtual void setBitmapMode( bool mode ) { _base->setBitmapMode(mode); }

    /// get kerning mode: true==ON, false=OFF
    virtual bool getKerning() const { return _base->getKerning(); }
    /// get kerning mode: true==ON, false=OFF
    virtual void setKerning( bool kerning) { _base->setKerning(kerning); }

    /// sets current hinting mode
    virtual void setHintingMode(hinting_mode_t mode) { _base->setHintingMode(mode); }
    /// returns current hinting mode
    virtual hinting_mode_t  getHintingMode() const { return _base->getHintingMode(); }

    /// returns true if font is empty
    virtual bool IsNull() const {
    	return _base->IsNull();
    }
    virtual bool operator ! () const {
    	return _base->operator !();
    }
    virtual void Clear() {
    	_base->Clear();
    }
    virtual ~GLFont() { }
    /// set fallback font for this font
    void setFallbackFont( LVFontRef font ) { _base->setFallbackFont(font); }
    /// get fallback font for this font
    LVFont * getFallbackFont() { return _base->getFallbackFont(); }
};



/// garbage collector frees unused fonts
void GLFontManager::gc()
{
	// remove links from hash maps
	LVHashTable<LVFont *, LVFontRef>::iterator iter = _mapByBase.forwardIterator();
	LVArray<LVFont *> keysForRemove(32, 0);
	// prepare list of fonts to free
	for (;;) {
		LVHashTable<LVFont *, LVFontRef>::pair * item = iter.next();
		if (!item)
			break;
		if (item->value.getRefCount() <= 1) {
			keysForRemove.add(item->key);
		}
	}
	// free found unused fonts
	for (int i=0; i<keysForRemove.length(); i++)
		removeFontInstance(keysForRemove[i]);
	// free base font instances
	_base->gc();
}

/// returns most similar font
LVFontRef GLFontManager::GetFont(int size, int weight, bool italic, css_font_family_t family, lString8 typeface, int documentId)
{
	LVFontRef res = _base->GetFont(size, weight, italic, family, typeface, documentId);
	LVFontRef existing = _mapByBase.get(res.get());
	if (!existing) {
		LVFontRef f = LVFontRef(new GLFont(res, this));
		_mapByBase.set(res.get(), f);
		_mapByGl.set(f.get(), res);
		return f;
	} else {
		return existing;
	}
}

/// notification from GLFont - instance is going to close
void GLFontManager::removeFontInstance(LVFont * glFont) {
	LVFont * base = _mapByGl.get(glFont).get();
	_mapByGl.remove(glFont);
	_mapByBase.remove(base);
}

/// set fallback font face (returns true if specified font is found)
bool GLFontManager::SetFallbackFontFace( lString8 face )
{
	return _base->SetFallbackFontFace(face);
}

/// get fallback font face (returns empty string if no fallback font is set)
lString8 GLFontManager::GetFallbackFontFace()
{
	return _base->GetFallbackFontFace();
}

/// returns fallback font for specified size
LVFontRef GLFontManager::GetFallbackFont(int size)
{
	return _base->GetFallbackFont(size);
}

/// registers font by name
bool GLFontManager::RegisterFont( lString8 name )
{
	return _base->RegisterFont(name);
}

/// registers document font
bool GLFontManager::RegisterDocumentFont(int documentId, LVContainerRef container, lString16 name, lString8 face, bool bold, bool italic)
{
	return _base->RegisterDocumentFont(documentId, container, name, face, bold, italic);
}

/// unregisters all document fonts
void GLFontManager::UnregisterDocumentFonts(int documentId)
{
	_base->UnregisterDocumentFonts(documentId);
}

/// initializes font manager
bool GLFontManager::Init( lString8 path )
{
    CR_UNUSED(path);
    // nothing to do
	return true;
}

/// get count of registered fonts
int GLFontManager::GetFontCount() {
	return _base->GetFontCount();
}

/// get hash of installed fonts and fallback font
lUInt32 GLFontManager::GetFontListHash(int documentId)
{
	return _base->GetFontListHash(documentId);
}
/// clear glyph cache
void GLFontManager::clearGlyphCache()
{
	_base->clearGlyphCache();
	_cache->clear();
}

/// get antialiasing mode
int GLFontManager::GetAntialiasMode()
{
	return _base->GetAntialiasMode();
}
/// set antialiasing mode
void GLFontManager::SetAntialiasMode( int mode )
{
	_base->SetAntialiasMode(mode);
    _cache->clear();
}

/// get kerning mode: true==ON, false=OFF
bool GLFontManager::getKerning()
{
	return _base->getKerning();
}
/// get kerning mode: true==ON, false=OFF
void GLFontManager::setKerning( bool kerningEnabled )
{
	_base->setKerning(kerningEnabled);
}

/// constructor
GLFontManager::GLFontManager(LVFontManager * base) : LVFontManager(), _base(base), _mapByBase(1000), _mapByGl(1000)
{
	_cache = new GLGlyphCache();
    _glGlyphCache = _cache;
	CRLog::debug("Created GL Font Manager");
}

/// destructor
GLFontManager::~GLFontManager()
{
    _glGlyphCache = NULL;
	delete _cache;
	delete _base;
}

/// returns available typefaces
void GLFontManager::getFaceList( lString16Collection & result)
{
	_base->getFaceList(result);
}


/// sets current hinting mode
void GLFontManager::SetHintingMode(hinting_mode_t mode)
{
	_base->SetHintingMode(mode);
    _cache->clear();
}
/// returns current hinting mode
hinting_mode_t GLFontManager::GetHintingMode() {
	return _base->GetHintingMode();
}

