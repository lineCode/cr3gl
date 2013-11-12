/*
 * cruireadwidget.cpp
 *
 *  Created on: Aug 21, 2013
 *      Author: vlopatin
 */

// uncomment to simulate slow render
//#define SLOW_RENDER_SIMULATION

#include "stringresource.h"
#include "cruireadwidget.h"
#include "crui.h"
#include "cruimain.h"
#include "gldrawbuf.h"
#include "fileinfo.h"
#include "cruiconfig.h"
#include "lvstsheet.h"
#include "hyphman.h"
#include <math.h>

using namespace CRUI;

lUInt32 applyAlpha(lUInt32 cl1, lUInt32 cl2, int alpha) {
	if (alpha <=0)
		return cl1;
	else if (alpha >= 255)
		return cl2;
    lUInt32 opaque = 256 - alpha;
    lUInt32 n1 = (((cl2 & 0xFF00FF) * alpha + (cl1 & 0xFF00FF) * opaque) >> 8) & 0xFF00FF;
    lUInt32 n2 = (((cl2 >> 8) & 0xFF00FF) * alpha + ((cl1 >> 8) & 0xFF00FF) * opaque) & 0xFF00FF00;
    return n1 | n2;
}

class CRUIReadMenu : public CRUIFrameLayout, CRUIOnClickListener, CRUIOnScrollPosCallback {
    CRUIReadWidget * _window;
    CRUIActionList _actionList;
    LVPtrVector<CRUIButton, false> _buttons;
    CRUILinearLayout * _scrollLayout;
    CRUITextWidget * _positionText;
    CRUISliderWidget * _scrollSlider;
    lvPoint _itemSize;
    int _btnCols;
    int _btnRows;
public:
    CRUIReadMenu(CRUIReadWidget * window, const CRUIActionList & actionList) : _window(window), _actionList(actionList) {
        for (int i = 0; i < _actionList.length(); i++) {
            const CRUIAction * action = _actionList[i];
            CRUIButton * button = new CRUIButton(action->getName(), action->icon_res.c_str(), true);
            button->setId(lString8::itoa(action->id));
            button->setOnClickListener(this);
            button->setStyle("BUTTON_NOBACKGROUND");
            button->setPadding(lvRect(PT_TO_PX(2), PT_TO_PX(2), PT_TO_PX(2), PT_TO_PX(2)));
            button->setFontSize(FONT_SIZE_XSMALL);
            ((CRUITextWidget*)button->childById("BUTTON_CAPTION")->setFontSize(FONT_SIZE_XSMALL))->setMaxLines(2);
            _buttons.add(button);
            addChild(button);
        }
        _scrollLayout = new CRUIVerticalLayout();
        _scrollLayout->setLayoutParams(FILL_PARENT, WRAP_CONTENT);
//        CRUIWidget * delimiter = new CRUIWidget();
//        delimiter->setBackground(0xC0000000);
//        delimiter->setMinHeight(PT_TO_PX(2));
//        delimiter->setMaxHeight(PT_TO_PX(2));
//        _scrollLayout->addChild(delimiter);
        _positionText = new CRUITextWidget();
        _positionText->setText(_window->getCurrentPositionDesc());
        _positionText->setPadding(lvRect(PT_TO_PX(8), MIN_ITEM_PX / 8, PT_TO_PX(2), 0));
        _positionText->setFontSize(FONT_SIZE_MEDIUM);
        _scrollLayout->addChild(_positionText);
        _scrollSlider = new CRUISliderWidget(0, 10000, _window->getCurrentPositionPercent());
        _scrollSlider->setScrollPosCallback(this);
        _scrollSlider->setMaxHeight(MIN_ITEM_PX * 3 / 4);
        _scrollLayout->addChild(_scrollSlider);
        _scrollLayout->setBackground("home_frame.9");
        addChild(_scrollLayout);
    }
    /// measure dimensions
    virtual void measure(int baseWidth, int baseHeight) {
        if (getVisibility() == GONE) {
            _measuredWidth = 0;
            _measuredHeight = 0;
            return;
        }
        CRUIImageRef icon = resourceResolver->getIcon(_actionList[0]->icon_res.c_str());
        LVFontRef font = currentTheme->getFontForSize(FONT_SIZE_XSMALL);
        int iconh = icon->originalHeight();
        int iconw = icon->originalWidth();
        int texth = font->getHeight() * 2;
        _itemSize.y = iconh + texth + PT_TO_PX(4);
        _itemSize.x = iconw * 120 / 100 + PT_TO_PX(4);
        int count = _actionList.length();
        int cols = baseWidth / _itemSize.x;
        if (cols < 1)
            cols = 1;
        int rows = (count + (cols - 1)) / cols;
        while (cols > 2 && (count + (cols - 1 - 1)) / (cols - 1) == rows)
            cols--;
        _btnCols = cols;
        _btnRows = rows; // + 1;
        _scrollLayout->measure(baseWidth, baseHeight);
        int width = baseWidth;
        int height = _btnRows * _itemSize.y + PT_TO_PX(3) + _scrollLayout->getMeasuredHeight();
        defMeasure(baseWidth, baseHeight, width, height);

    }

    /// updates widget position based on specified rectangle
    virtual void layout(int left, int top, int right, int bottom) {
        CRUIWidget::layout(left, top, right, bottom);
        int count = _actionList.length();
        lvRect rc = _pos;
        applyMargin(rc);
        applyPadding(rc);
        _scrollLayout->layout(rc.left, rc.bottom - _scrollLayout->getMeasuredHeight(), rc.right, rc.bottom);
        rc.bottom -= _scrollLayout->getMeasuredHeight();
        int rowh = rc.height() / _btnRows;
        lvRect rowrc = rc;
        for (int y = 0; y < _btnRows; y++) {
            int i0 = _btnCols * y;
            int rowlen = count - i0;
            if (rowlen > _btnCols)
                rowlen = _btnCols;
            rowrc.bottom = rowrc.top + rowh;
            lvRect btnrc = rowrc;
            int itemw = rowrc.width() / rowlen;
            for (int x = 0; x < rowlen; x++) {
                btnrc.right = btnrc.left + itemw;
                CRUIButton * button = _buttons[i0 + x];
                button->measure(btnrc.width(), btnrc.height());
                button->layout(btnrc.left, btnrc.top, btnrc.right, btnrc.bottom);
                btnrc.left += itemw;
            }
            rowrc.top += rowh;
        }
    }

    virtual bool onScrollPosChange(CRUISliderWidget * widget, int pos, bool manual) {
        CR_UNUSED(widget);
        if (!manual)
            return false;
        _window->goToPercent(pos);
        _positionText->setText(_window->getCurrentPositionDesc());
        return true;
    }

    virtual bool onClick(CRUIWidget * widget) {
        int id = widget->getId().atoi();
        const CRUIAction * action = NULL;
        for (int i = 0; i < _actionList.length(); i++) {
            if (id == _actionList[i]->id) {
                action = _actionList[i];
            }
        }
        if (action) {
            _window->onMenuItemAction(action);
        }
        return true;
    }
};


static bool isDocViewProp(const lString8 & key) {
    return key == PROP_FONT_FACE
            || key == PROP_FONT_COLOR
            || key == PROP_FONT_WEIGHT_EMBOLDEN
            || key == PROP_FONT_SIZE
            || key == PROP_FONT_FACE
            || key == PROP_BACKGROUND_COLOR
            || key == PROP_INTERLINE_SPACE
            || key == PROP_FLOATING_PUNCTUATION
            || key == PROP_BACKGROUND_IMAGE
            || key == PROP_BACKGROUND_IMAGE_ENABLED
            || key == PROP_BACKGROUND_IMAGE_CORRECTION_BRIGHTNESS
            || key == PROP_BACKGROUND_IMAGE_CORRECTION_CONTRAST
            || key == PROP_FONT_GAMMA_INDEX
            || key == PROP_FONT_ANTIALIASING
            || key == PROP_FONT_WEIGHT_EMBOLDEN
            || key == PROP_PAGE_MARGINS
            || key == PROP_FONT_HINTING;
}

void drawVGradient(LVDrawBuf * buf, lvRect & rc, lUInt32 colorTop, lUInt32 colorBottom) {
    buf->GradientRect(rc.left, rc.top, rc.right, rc.bottom, colorTop, colorTop, colorBottom, colorBottom);
}

static CRUIDocView * createDocView() {
	CRUIDocView * _docview = new CRUIDocView();
    _docview->setViewMode(DVM_SCROLL, 1);
    LVArray<int> sizes;
    for (int i = deviceInfo.shortSide / 40; i < deviceInfo.shortSide / 10 && i < 200; i++)
    	sizes.add(i);
	_docview->setFontSizes(sizes, false);
	_docview->setFontSize(deviceInfo.shortSide / 20);
	lvRect margins(deviceInfo.shortSide / 20, deviceInfo.shortSide / 20, deviceInfo.shortSide / 20, deviceInfo.shortSide / 20);
	_docview->setPageMargins(margins);
	return _docview;
}

CRUIReadWidget::CRUIReadWidget(CRUIMainWidget * main)
    : CRUIWindowWidget(main)
    , _pinchSettingPreview(NULL)
	, _isDragging(false)
	, _dragStartOffset(0)
    , _viewMode(DVM_PAGES)
    , _pageAnimation(PAGE_ANIMATION_SLIDE)
    , _locked(false)
	, _fileItem(NULL)
	, _lastPosition(NULL)
	, _startPositionIsUpdated(false)
	, _pinchOp(PINCH_OP_NONE)
{
    setId("READ");
    _docview = createDocView();
    _docview->setCallback(this);
    _docview->setViewMode(DVM_PAGES);
    _docview->setVisiblePageCount(2);
    _docview->setStatusFontSize(deviceInfo.shortSide / 25);
    _docview->setStatusMode(0, false, true, false, true, false, true, true);
}

CRUIReadWidget::~CRUIReadWidget() {
    if (_fileItem)
        delete _fileItem;
    if (_lastPosition)
        delete _lastPosition;
}

/// measure dimensions
void CRUIReadWidget::measure(int baseWidth, int baseHeight) {
    _measuredWidth = baseWidth;
    _measuredHeight = baseHeight;
}

/// updates widget position based on specified rectangle
void CRUIReadWidget::layout(int left, int top, int right, int bottom) {
    CRUIWindowWidget::layout(left, top, right, bottom);
    if (!_locked) {
        if (_docview->GetWidth() != right - left || _docview->GetHeight() != bottom - top) {
            _docview->Resize(right-left, bottom-top);
        }
    }
}

void CRUIReadWidget::prepareScroll(int direction) {
    if (renderIfNecessary()) {
        CRLog::trace("CRUIReadWidget::prepareScroll(%d)", direction);
        if (_viewMode == DVM_PAGES)
            _pagedCache.prepare(_docview, _docview->getCurPage(), _measuredWidth, _measuredHeight, direction, true, _pageAnimation);
        else
            _scrollCache.prepare(_docview, _docview->GetPos(), _measuredWidth, _measuredHeight, direction, true);
    }
}

/// draws widget with its children to specified surface
void CRUIReadWidget::draw(LVDrawBuf * buf) {
    _popupControl.updateLayout(_pos);
    if (_pinchOp && _pinchSettingPreview) {
    	_pinchSettingPreview->SetPos(0, false);
        buf->SetTextColor(_pinchSettingPreview->getTextColor());
        buf->SetBackgroundColor(_pinchSettingPreview->getBackgroundColor());
        _pinchSettingPreview->Draw(*buf, false);
    	_drawRequested = false;
    	return;
    }
    if (renderIfNecessary()) {
        //CRLog::trace("Document is ready, drawing");
        if (_viewMode == DVM_PAGES) {
            int direction = 0;
            int progress = 0;
            if (_scroll.isActive()) {
                direction = _scroll.dir() > 0 ? 1 : -1;
                progress = _scroll.progress();
                if (progress < 0)
                    progress = 0;
                else if (progress > 10000)
                    progress = 10000;
            } else if (_isDragging) {
                direction = _pagedCache.dir() > 0 ? 1 : -1;
                progress = (- (_dragPos.x - _dragStart.x) * direction) * 10000 / _pos.width();
            }
            _pagedCache.prepare(_docview, _docview->getCurPage(), _measuredWidth, _measuredHeight, direction, false, _pageAnimation);
            _pagedCache.draw(buf, _docview->getCurPage(), direction, progress, _pos.left);
        } else {
            _scrollCache.prepare(_docview, _docview->GetPos(), _measuredWidth, _measuredHeight, 0, false);
            _scrollCache.draw(buf, _docview->GetPos(), _pos.left, _pos.top);
        }
    } else {
        // document render in progress; draw just page background
        //CRLog::trace("Document is locked, just drawing background");
        _docview->drawPageBackground(*buf, 0, 0);
    }
    // scroll bottom and top gradients
    if (_viewMode != DVM_PAGES) {
        lvRect top = _pos;
        top.bottom = top.top + deviceInfo.shortSide / 60;
        lvRect top2 = _pos;
        top2.top = top.bottom;
        top2.bottom = top.top + deviceInfo.shortSide / 30;
        lvRect bottom = _pos;
        bottom.top = bottom.bottom - deviceInfo.shortSide / 60;
        lvRect bottom2 = _pos;
        bottom2.bottom = bottom.top;
        bottom2.top = bottom.bottom - deviceInfo.shortSide / 30;
        drawVGradient(buf, top, 0xA0000000, 0xE0000000);
        drawVGradient(buf, top2, 0xE0000000, 0xFF000000);
        drawVGradient(buf, bottom2, 0xFF000000, 0xE0000000);
        drawVGradient(buf, bottom, 0xE0000000, 0xA0000000);
    }
    // popup support
    if (_popupControl.popupBackground)
        _popupControl.popupBackground->draw(buf);
    if (_popupControl.popup)
        _popupControl.popup->draw(buf);
}

class BookLoadedNotificationTask : public CRRunnable {
    lString8 pathname;
    CRDocumentLoadCallback * callback;
    CRDocumentLoadCallback * callback2;
    bool success;
public:
    BookLoadedNotificationTask(lString8 _pathname, bool _success, CRDocumentLoadCallback * _callback, CRDocumentLoadCallback * _callback2) {
        pathname = _pathname;
        pathname.modify();
        callback = _callback;
        callback2 = _callback2;
        success = _success;
    }
    virtual void run() {
        CRLog::trace("BookLoadedNotificationTask.run()");
        callback2->onDocumentLoadFinished(pathname, success);
        callback->onDocumentLoadFinished(pathname, success);
    }
};

class BookRenderedNotificationTask : public CRRunnable {
    lString8 pathname;
    CRDocumentRenderCallback * callback;
    CRDocumentRenderCallback * callback2;
public:
    BookRenderedNotificationTask(lString8 _pathname, CRDocumentRenderCallback * _callback, CRDocumentRenderCallback * _callback2) {
        pathname = _pathname;
        pathname.modify();
        callback = _callback;
        callback2 = _callback2;
    }
    virtual void run() {
        CRLog::trace("BookRenderedNotificationTask.run()");
        callback2->onDocumentRenderFinished(pathname);
        callback->onDocumentRenderFinished(pathname);
    }
};

class OpenBookTask : public CRRunnable {
    lString8 _pathname;
    CRUIMainWidget * _main;
    CRUIReadWidget * _read;
public:
    OpenBookTask(lString16 pathname, CRUIMainWidget * main, CRUIReadWidget * read) : _main(main), _read(read) {
        _pathname = UnicodeToUtf8(pathname);
    }
    virtual void run() {
        CRLog::info("Loading book in background thread");
        bool success = _read->getDocView()->LoadDocument(Utf8ToUnicode(_pathname).c_str()) != 0;
        CRLog::info("Loading is finished %s", success ? "successfully" : "with error");
#ifdef SLOW_RENDER_SIMULATION
        concurrencyProvider->sleepMs(3000);
#endif
        if (!success) {
            _read->getDocView()->createDefaultDocument(lString16("Cannot open document"), lString16("Error occured while trying to open document"));
        }
        concurrencyProvider->executeGui(new BookLoadedNotificationTask(_pathname, success, _main, _read));
        CRLog::info("Rendering book in background thread");
        _read->getDocView()->Render();
        _read->restorePosition();
        _read->getDocView()->updateCache();
#ifdef SLOW_RENDER_SIMULATION
        concurrencyProvider->sleepMs(3000);
#endif
        CRLog::info("Render is finished");
        concurrencyProvider->executeGui(new BookRenderedNotificationTask(_pathname, _main, _read));
    }
};

class RenderBookTask : public CRRunnable {
    lString8 _pathname;
    CRUIMainWidget * _main;
    CRUIReadWidget * _read;
public:
    RenderBookTask(lString16 pathname, CRUIMainWidget * main, CRUIReadWidget * read) : _main(main), _read(read) {
        _pathname = UnicodeToUtf8(pathname);
    }
    virtual void run() {
        CRLog::info("Rendering in background thread");
        _read->getDocView()->Render();
        _read->getDocView()->updateCache();
#ifdef SLOW_RENDER_SIMULATION
        concurrencyProvider->sleepMs(3000);
#endif
        CRLog::info("Render in background thread is finished");
        concurrencyProvider->executeGui(new BookRenderedNotificationTask(_pathname, _main, _read));
    }
};

void CRUIReadWidget::closeBook() {
    updatePosition();
    clearImageCaches();
    if (_fileItem)
        delete _fileItem;
    if (_lastPosition)
        delete _lastPosition;
    _fileItem = NULL;
    _lastPosition = NULL;
    _docview->close();
}

bool CRUIReadWidget::restorePosition() {
    if (!_fileItem || !_fileItem->getBook())
        return false;
    BookDBBookmark * bmk = dirCache->loadLastPosition(_fileItem->getBook());
    if (bmk) {
        // found position
        ldomXPointer bm = _docview->getDocument()->createXPointer(lString16(bmk->startPos.c_str()));
        _docview->goToBookmark(bm);
        if (!_lastPosition)
            _lastPosition = bmk;
        else
            delete bmk;
        return true;
    }
    return false;
}

void CRUIReadWidget::beforeNavigationFrom() {
    updatePosition();
}

void CRUIReadWidget::updatePosition() {
    CRLog::trace("CRUIReadWidget::updatePosition()");
    if (!_fileItem || !_fileItem->getBook())
        return;
    ldomXPointer ptr = _docview->getBookmark();
    if ( ptr.isNull() )
        return;
    CRBookmark bm(ptr);
    lString16 comment;
    lString16 titleText;
    lString16 posText;
    bm.setType( bmkt_lastpos );
    if ( _docview->getBookmarkPosText( ptr, titleText, posText ) ) {
         bm.setTitleText( titleText );
         bm.setPosText( posText );
    }
    bm.setStartPos( ptr.toString() );
    int pos = ptr.toPoint().y;
    int fh = _docview->getDocument()->getFullHeight();
    int percent = fh > 0 ? (int)(pos * (lInt64)10000 / fh) : 0;
    if ( percent<0 )
        percent = 0;
    if ( percent>10000 )
        percent = 10000;
    bm.setPercent( percent );
    bm.setCommentText( comment );
    if (!_lastPosition)
        _lastPosition = new BookDBBookmark();
    _lastPosition->bookId = _fileItem->getBook()->id;
    _lastPosition->type = bm.getType();
    _lastPosition->percent = bm.getPercent();
    _lastPosition->shortcut = bm.getShortcut();
    _lastPosition->timestamp = GetCurrentTimeMillis();
    _lastPosition->startPos = UnicodeToUtf8(bm.getStartPos()).c_str();
    _lastPosition->endPos = UnicodeToUtf8(bm.getEndPos()).c_str();
    _lastPosition->titleText = UnicodeToUtf8(bm.getTitleText()).c_str();
    _lastPosition->posText = UnicodeToUtf8(bm.getPosText()).c_str();
    _lastPosition->commentText = UnicodeToUtf8(bm.getCommentText()).c_str();
    _lastPosition->startPos = UnicodeToUtf8(bm.getStartPos()).c_str();
    dirCache->saveLastPosition(_fileItem->getBook(), _lastPosition);
}

lString8 lastBookLang;
lString8 lastSettingsLang;
bool setHyph(lString8 bookLang, lString8 settingsLang) {
    if (bookLang == lastBookLang && settingsLang == lastSettingsLang) // don't set duplicate
        return false;
    lastBookLang = bookLang;
    lastSettingsLang = settingsLang;
    return crconfig.setHyphenationDictionary(bookLang, settingsLang);
}

bool CRUIReadWidget::openBook(const CRFileItem * file) {
    if (_locked)
        return false;
    if (!file)
        return false;
    closeBook();
    _locked = true;
    clearImageCaches();
    _main->showSlowOperationPopup();
    _fileItem = static_cast<CRFileItem*>(file->clone());
    _lastPosition = bookDB->loadLastPosition(file->getBook());
    lString8 bookLang(_fileItem->getBook() ? _fileItem->getBook()->language.c_str() : "");
    lString8 systemLang = crconfig.systemLanguage;
    setHyph(bookLang, systemLang);
    _main->executeBackground(new OpenBookTask(Utf8ToUnicode(getPathName()), _main, this));
    return true;
}

void CRUIReadWidget::onDocumentLoadFinished(lString8 pathname, bool success) {
    CR_UNUSED(pathname);
    if (!success) {
        if (_fileItem)
            delete _fileItem;
        if (_lastPosition)
            delete _lastPosition;
        _fileItem = NULL;
        _lastPosition = NULL;
    }
    // force update reading position - to refresh timestamp
    _startPositionIsUpdated = false;
}

void CRUIReadWidget::onDocumentRenderFinished(lString8 pathname) {
    CR_UNUSED(pathname);
    CRLog::trace("Render is finished - unlocking document");
    _locked = false;
    invalidate();
    clearImageCaches();
    if (!_startPositionIsUpdated) {
        // call update position to refresh last access timestamp
        _startPositionIsUpdated = true;
        updatePosition();
    }
    _main->update(true);
}

/// returns true if document is ready, false if background rendering is in progress
bool CRUIReadWidget::renderIfNecessary() {
    if (_locked) {
        CRLog::trace("Document is locked");
        return false;
    }
    if (_docview->GetWidth() != _pos.width() || _docview->GetHeight() != _pos.height()) {
        CRLog::trace("Changing docview size to %dx%d", _pos.width(), _pos.height());
        _docview->Resize(_pos.width(), _pos.height());
    }
    if (_docview->IsRendered())
        return true;
    CRLog::info("Render is required! Starting render task");
    _locked = true;
    clearImageCaches();
    _main->showSlowOperationPopup();
    _main->executeBackground(new RenderBookTask(Utf8ToUnicode(getPathName()), _main, this));
    return false;
}

#define SCROLL_SPEED_CALC_INTERVAL 2000
#define SCROLL_MIN_SPEED 3
#define SCROLL_FRICTION 13

/// overriden to treat popup as first child
int CRUIReadWidget::getChildCount() {
    int cnt = 0;
    if (_popupControl.popup)
        cnt++;
    if (_popupControl.popupBackground)
        cnt++;
    return cnt;
}

/// overriden to treat popup as first child
CRUIWidget * CRUIReadWidget::getChild(int index) {
    CR_UNUSED(index);
    if (index == 0) {
        if (_popupControl.popupBackground)
            return _popupControl.popupBackground;
        return _popupControl.popup;
    }
    return _popupControl.popup;
}

void CRUIReadWidget::animate(lUInt64 millisPassed) {
    if (_locked) {
        if (_scroll.isActive())
            _scroll.stop();
        return;
    }
    bool scrollWasActive = _scroll.isActive();
    CRUIWidget::animate(millisPassed);
    bool changed = _scroll.animate(millisPassed);
    if (changed) {
        if (_viewMode == DVM_PAGES) {
            if (_scroll.pos() >= _pos.width() - 1) {
                _scroll.stop();
            }
        } else {
            int oldpos = _docview->GetPos();
            //CRLog::trace("scroll animation: new position %d", _scroll.pos());
            _docview->SetPos(_scroll.pos(), false);
            if (oldpos == _docview->GetPos()) {
                //CRLog::trace("scroll animation - stopping at %d since set position not changed position", _scroll.pos());
                // stopping: bounds
                _scroll.stop();
            }
        }
    }
    if (scrollWasActive && !_scroll.isActive()) {
        if (_viewMode == DVM_PAGES) {
            CRLog::trace("flip stopped, old page: %d, new page: %d", _docview->getCurPage(), _pagedCache.getNewPage());
            _docview->goToPage(_pagedCache.getNewPage(), true);
//            if (_scroll.dir() > 0)
//                _docview->doCommand(DCMD_PAGEDOWN, 1);
//            else if (_scroll.dir() < 0)
//                _docview->doCommand(DCMD_PAGEUP, 1);
        }
        postUpdatePosition();
    }
}

class UpdatePositionEvent : public CRRunnable {
    CRUIReadWidget * _widget;
public:
    UpdatePositionEvent(CRUIReadWidget * widget) : _widget(widget) { }
    virtual void run() {
        _widget->updatePosition();
    }
};

void CRUIReadWidget::postUpdatePosition() {
    concurrencyProvider->executeGui(new UpdatePositionEvent(this));
}

bool CRUIReadWidget::isAnimating() {
    return _scroll.isActive() || CRUIWindowWidget::isAnimating();
}

void CRUIReadWidget::animateScrollTo(int newpos, int speed) {
    if (_locked)
        return;
    CRLog::trace("animateScrollTo( %d -> %d )", _docview->GetPos(), newpos);
    int delta = newpos - _docview->GetPos();
    prepareScroll(delta);
    _scroll.start(_docview->GetPos(), newpos, speed, SCROLL_FRICTION);
    invalidate();
    _main->update(true);
}

void CRUIReadWidget::animatePageFlip(int newpage, int speed) {
    if (_locked)
        return;
    int page = _docview->getCurPage();
    CRLog::trace("animatePageFlip( %d -> %d )", page, newpage);
    //_pagedCache.prepare(_docview->);
    int dir = newpage > page ? 1 : -1;
    if (_pageAnimation == PAGE_ANIMATION_NONE) {
        _docview->goToPage(newpage);
        invalidate();
        return;
    }
    prepareScroll(dir);
    _scroll.setDirection(dir);
    _scroll.start(0, _pos.width(), speed, SCROLL_FRICTION);
    invalidate();
    //_main->update(true);
}

bool CRUIReadWidget::doCommand(int cmd, int param) {
    if (_locked)
        return false;
    int pos = _docview->GetPos();
    int newpos = pos;
    int speed = 0;
    int page = _docview->getCurPage();
    int newpage = page;
    if (_viewMode == DVM_PAGES) {
        if (cmd == DCMD_LINEUP)
            cmd = DCMD_PAGEUP;
        else if (cmd == DCMD_LINEDOWN)
            cmd = DCMD_PAGEDOWN;
    }
    switch (cmd) {
    case DCMD_PAGEUP:
        if (_viewMode == DVM_PAGES) {
            if (_pageAnimation == PAGE_ANIMATION_NONE) {
                _docview->doCommand((LVDocCmd)cmd, 1);
            } else {
                newpage = page - _docview->getVisiblePageCount();
                speed = _pos.width() * 2;
            }
        } else {
            newpos = pos - _pos.height() * 9 / 10;
            speed = _pos.height() * 2;
        }
        break;
    case DCMD_PAGEDOWN:
        if (_viewMode == DVM_PAGES) {
            if (_pageAnimation == PAGE_ANIMATION_NONE) {
                _docview->doCommand((LVDocCmd)cmd, 1);
            } else {
                newpage = page + _docview->getVisiblePageCount();
                speed = _pos.width() * 2;
            }
        } else {
            newpos = pos + _pos.height() * 9 / 10;
            speed = _pos.height() * 2;
        }
        break;
    case DCMD_LINEUP:
        newpos = pos - _docview->getFontSize();
        speed = _pos.height() / 2;
        break;
    case DCMD_LINEDOWN:
        newpos = pos + _docview->getFontSize();
        speed = _pos.height() / 2;
        break;
    default:
        return _docview->doCommand((LVDocCmd)cmd, param);
    }
    if (_viewMode == DVM_PAGES) {
        if (page != newpage && newpage >= 0 && newpage < _docview->getPageCount() + _docview->getVisiblePageCount() - 1) {
            animatePageFlip(newpage, speed);
        }
    } else {
        if (pos != newpos) {
            animateScrollTo(newpos, speed);
        }
    }
    return true;
}

void CRUIReadWidget::clearImageCaches() {
	_scrollCache.clear();
    _pagedCache.clear();
}

bool CRUIReadWidget::onKeyEvent(const CRUIKeyEvent * event) {
    if (_locked)
        return false;
    int key = event->key();
	//CRLog::trace("CRUIReadWidget::onKeyEvent(%d  0x%x  popup.closing=%s   popup.progress=%d)", key, key, _popupControl.closing ? "yes" : "no", _popupControl.progress);
    if (_popupControl.popup) {
    	CRLog::trace("Popup is active - transferring key to window");
    	return CRUIWindowWidget::onKeyEvent(event);
    }
    if (event->getType() == KEY_ACTION_RELEASE) {
        if (_scroll.isActive())
            _scroll.stop();
        switch(key) {
        case CR_KEY_MENU:
        	showReaderMenu();
            invalidate();
            return true;
        default:
        	break;
        }
    }

    if (event->getType() == KEY_ACTION_PRESS) {
        if (_scroll.isActive())
            _scroll.stop();
        //CRLog::trace("keyDown(0x%04x) oldpos=%d", key,  _docview->GetPos());
        switch(key) {
        case CR_KEY_PGDOWN:
        case CR_KEY_SPACE:
            doCommand(DCMD_PAGEDOWN);
            invalidate();
            return true;
        case CR_KEY_PGUP:
            doCommand(DCMD_PAGEUP);
            invalidate();
            return true;
        case CR_KEY_HOME:
            _docview->doCommand(DCMD_BEGIN);
            invalidate();
            return true;
        case CR_KEY_END:
            _docview->doCommand(DCMD_END);
            invalidate();
            return true;
        case CR_KEY_UP:
            doCommand(DCMD_LINEUP, 1);
            invalidate();
            return true;
        case CR_KEY_DOWN:
            doCommand(DCMD_LINEDOWN, 1);
            invalidate();
            return true;
//        case CR_KEY_ESC:
//        case CR_KEY_BACK:
//            _main->back();
//            invalidate();
//            return true;
        case CR_KEY_MENU:
        	return true;
        default:
            break;
        }
    }
    //CRLog::trace("new pos=%d", _docview->GetPos());
    return CRUIWindowWidget::onKeyEvent(event);
}

int CRUIReadWidget::pointToTapZone(int x, int y) {
    int x0 = x / ((_pos.width() + 2) / 3);
    int y0 = y / ((_pos.height() + 2) / 3);
    if (x0 > 2) x0 = 2;
    if (x0 < 0) x0 = 0;
    if (y0 > 2) y0 = 2;
    if (y0 < 0) y0 = 0;
    return y0 * 3 + x0 + 1;
}

bool CRUIReadWidget::onTapZone(int zone, bool additionalAction) {
    lString8 settingName;
    if (additionalAction)
        settingName = PROP_APP_TAP_ZONE_ACTION_DOUBLE;
    else
        settingName = PROP_APP_TAP_ZONE_ACTION_NORMAL;
    settingName += lString8::itoa(zone);
    lString8 action = UnicodeToUtf8(_main->getSettings()->getStringDef(settingName.c_str()));
    if (!action.empty()) {
        const CRUIAction * a = CRUIActionByName(action.c_str());
        if (a != NULL) {
            return onAction(a);
        }
    }
    return false;
}

void CRUIReadWidget::startPinchOp(int op, int dx, int dy) {
	if (_pinchOp)
		return;
	_pinchOp = op;
	_pinchOpStartDx = dx;
	_pinchOpStartDy = dy;
    _pinchOpCurrentDx = dx;
    _pinchOpCurrentDy = dy;
    _pinchSettingPreview = createDocView();
    CRPropRef changed = _main->getSettings();
    CRPropRef docviewprops = LVCreatePropsContainer();
    //bool backgroundChanged = false;
    for (int i = 0; i < changed->getCount(); i++) {
        lString8 key(changed->getName(i));
        lString8 value(UnicodeToUtf8(changed->getValue(i)));
        if (isDocViewProp(key)) {
            docviewprops->setString(key.c_str(), value.c_str());
            if (key == PROP_FONT_COLOR) {
            	_pinchSettingPreview->setTextColor(changed->getColorDef(PROP_FONT_COLOR, 0));
            }
        }
    }
    _pinchSettingPreview->propsApply(docviewprops);
    lString16 title;
    switch(_pinchOp) {
    case PINCH_OP_HORIZONTAL:
    	title = _16(STR_PINCH_CHANGING_PAGE_MARGINS);
    	break;
    case PINCH_OP_VERTICAL:
    	title = _16(STR_PINCH_CHANGING_INTERLINE_SPACING);
    	break;
    case PINCH_OP_DIAGONAL:
    	title = _16(STR_PINCH_CHANGING_FONT_SIZE);
    	_pinchOpSettingValue = _docview->getFontSize();
    	break;
    }
    lString16 sampleText = _16(STR_SETTINGS_FONT_SAMPLE_TEXT);
    sampleText = sampleText + " " + sampleText;
    _pinchSettingPreview->createDefaultDocument(title, sampleText + "\n"
    		+ sampleText + "\n" + sampleText + "\n" + sampleText
    		+ "\n" + sampleText + "\n" + sampleText + "\n" + sampleText + "\n" + sampleText);
    _pinchSettingPreview->Resize(_pos.width(), _pos.height());
    CRLog::trace("startPinchOp %d   %d %d", _pinchOp, dx, dy);
    invalidate();
}

void CRUIReadWidget::updatePinchOp(int dx, int dy) {
	if (!_pinchOp)
		return;
    _pinchOpCurrentDx = dx;
    _pinchOpCurrentDy = dy;
    int delta = 0;
    int startSettingValue = 0;
    int newSettingValue = 0;
    switch(_pinchOp) {
    case PINCH_OP_HORIZONTAL:
		{
			startSettingValue = _main->getSettings()->getIntDef(PROP_PAGE_MARGINS, 100);
			delta = (dx) - (_pinchOpStartDx);
			int maxdiff = 2000 - 100;
			newSettingValue = startSettingValue + maxdiff * (-delta) * 120 / 100 / deviceInfo.shortSide;
			if (newSettingValue < 100)
				newSettingValue = 100;
			if (newSettingValue > 2000)
				newSettingValue = 2000;
			CRPropRef props = LVCreatePropsContainer();
			props->setInt(PROP_PAGE_MARGINS, newSettingValue);
			_pinchOpSettingValue = newSettingValue;
			_pinchSettingPreview->propsApply(props);
			invalidate();
		}
    	break;
    case PINCH_OP_VERTICAL:
		{
			startSettingValue = _main->getSettings()->getIntDef(PROP_INTERLINE_SPACE, 100);
			delta = (dy) - (_pinchOpStartDy);
			int maxdiff = 200 - 80;
			newSettingValue = startSettingValue + maxdiff * delta * 120 / 100 / deviceInfo.shortSide;
			if (newSettingValue < 80)
				newSettingValue = 80;
			if (newSettingValue > 200)
				newSettingValue = 200;
			CRPropRef props = LVCreatePropsContainer();
			props->setInt(PROP_INTERLINE_SPACE, newSettingValue);
			_pinchOpSettingValue = newSettingValue;
			_pinchSettingPreview->propsApply(props);
			invalidate();
		}
    	break;
    case PINCH_OP_DIAGONAL:
		{
			delta = (dx + dy) - (_pinchOpStartDx + _pinchOpStartDy);
			int maxdiff = crconfig.maxFontSize - crconfig.minFontSize;
			startSettingValue = _docview->getFontSize();

			if (delta > 0) {
				newSettingValue = startSettingValue + maxdiff * delta * 120 / 100 / deviceInfo.shortSide;
				CRLog::trace("Zoom in %d -> %d", startSettingValue, newSettingValue);
			} else {
				newSettingValue = startSettingValue + maxdiff * delta * 120 / 100  / deviceInfo.shortSide;
				//newSettingValue = startSettingValue - startSettingValue * ((-delta) * 2 / deviceInfo.shortSide);
				CRLog::trace("Zoom out %d -> %d", startSettingValue, newSettingValue);
			}
			if (newSettingValue < crconfig.minFontSize)
				newSettingValue = crconfig.minFontSize;
			if (newSettingValue > crconfig.maxFontSize)
				newSettingValue = crconfig.maxFontSize;
			if (_pinchSettingPreview->getFontSize() != newSettingValue) {
				_pinchOpSettingValue = newSettingValue;
				_pinchSettingPreview->setFontSize(newSettingValue);
				invalidate();
			}
			break;
		}
    }
    CRLog::trace("updatePinchOp %d   %d %d", _pinchOp, dx, dy);
}

void CRUIReadWidget::endPinchOp(int dx, int dy, bool cancel) {
	if (!_pinchOp)
		return;
    CRLog::trace("endPinchOp %d   %d %d", _pinchOp, dx, dy);
	if (_pinchSettingPreview) {
		delete _pinchSettingPreview;
		_pinchSettingPreview = NULL;
	}
	if (!cancel) {
		switch(_pinchOp) {
		case PINCH_OP_HORIZONTAL:
			_main->initNewSettings()->setInt(PROP_PAGE_MARGINS, _pinchOpSettingValue);
			_main->applySettings();
			break;
		case PINCH_OP_VERTICAL:
			_main->initNewSettings()->setInt(PROP_INTERLINE_SPACE, _pinchOpSettingValue);
			_main->applySettings();
			break;
		case PINCH_OP_DIAGONAL:
			_main->initNewSettings()->setInt(PROP_FONT_SIZE, _pinchOpSettingValue);
			_main->applySettings();
			break;
		}
	}
	_pinchOp = PINCH_OP_NONE;
    invalidate();
}

/// returns true to allow parent intercept this widget which is currently handled by this widget
bool CRUIReadWidget::allowInterceptTouchEvent(const CRUIMotionEvent * event) {
    CR_UNUSED(event);
	if (_isDragging || _pinchOp)
		return false;
	return true;
}

/// motion event handler, returns true if it handled event
bool CRUIReadWidget::onTouchEvent(const CRUIMotionEvent * event) {
    if (_locked)
        return false;
    int action = event->getAction();
    if (action != ACTION_MOVE && event->count() > 1)
    	CRLog::trace("CRUIReadWidget::onTouchEvent multitouch %d pointers action = %d", event->count(), action);
    //CRLog::trace("CRUIListWidget::onTouchEvent %d (%d,%d)", action, event->getX(), event->getY());
    int dx = event->getX() - event->getStartX();
    int dy = event->getY() - event->getStartY();
    int pinchDx = event->getPinchDx();
    int pinchDy = event->getPinchDy();
    int delta = dy; //isVertical() ? dy : dx;
    int delta2 = dx; //isVertical() ? dy : dx;
    //CRLog::trace("CRUIListWidget::onTouchEvent %d (%d,%d) dx=%d, dy=%d, delta=%d, itemIndex=%d [%d -> %d]", action, event->getX(), event->getY(), dx, dy, delta, index, _dragStartOffset, _scrollOffset);
//    if (event->isCancelRequested())
//        return true;
    switch (action) {
    case ACTION_DOWN:
        if (_scroll.isActive()) {
            _scroll.stop();
            if (_viewMode == DVM_PAGES)
                _docview->goToPage(_pagedCache.getNewPage());
            event->cancelAllPointers();
            _isDragging = false;
            invalidate();
            return true;
        }
        _isDragging = false;
        _dragStart.x = event->getX();
        _dragStart.y = event->getY();
        _dragPos = _dragStart;
        if (_viewMode == DVM_PAGES)
            _dragStartOffset = _dragStart.x;
        else
            _dragStartOffset = _docview->GetPos();
        if (_scroll.isActive())
            _scroll.stop();
        invalidate();
        //CRLog::trace("list DOWN");
        break;
    case ACTION_UP:
        {
            invalidate();
            if (_pinchOp) {
            	endPinchOp(pinchDx, pinchDy, false);
            	event->cancelAllPointers();
            } else if (_isDragging) {
                lvPoint speed = event->getSpeed(SCROLL_SPEED_CALC_INTERVAL);
                if (_viewMode == DVM_PAGES) {
                    int progress = myAbs(_dragPos.x - _dragStart.x);
                    int spd = myAbs(speed.x);
                    if (spd < _pos.width())
                        spd =_pos.width();
                    _scroll.setDirection(_pagedCache.dir());
                    _scroll.start(0, _pos.width(), spd, SCROLL_FRICTION);
                    _scroll.setPos(progress);
                    CRLog::trace("Starting page flip with speed %d", _scroll.speed());
                    _isDragging = false;
                } else {
                    if (speed.y < -SCROLL_MIN_SPEED || speed.y > SCROLL_MIN_SPEED) {
                        _scroll.start(_docview->GetPos(), -speed.y, SCROLL_FRICTION);
                        CRLog::trace("Starting scroll with speed %d", _scroll.speed());
                    }
                }
            	event->cancelAllPointers();
            } else {
            	int x = event->getX();
            	int y = event->getY();
                bool longTap = (event->getDownDuration() > 500);
                bool twoFinigersTap = false;
                if (event->count() == 2) {
                	int dx1 = myAbs(event->getX(0) - event->getStartX(0));
                	int dy1 = myAbs(event->getY(0) - event->getStartY(0));
                	int dx2 = myAbs(event->getX(1) - event->getStartX(1));
                	int dy2 = myAbs(event->getY(1) - event->getStartY(1));
                	if (dx1 < DRAG_THRESHOLD && dy1 < DRAG_THRESHOLD && dx2 < DRAG_THRESHOLD && dy2 < DRAG_THRESHOLD) {
                		twoFinigersTap = true;
                		x = (x + event->getX(1)) / 2;
                		y = (y + event->getY(1)) / 2;
                	}
                }
                int zone = pointToTapZone(event->getX(), event->getY());
                event->cancelAllPointers();
                //bool twoFingersTap = (event->count() == 2) && event->get
                //onTapZone(zone, twoFinigersTap);
                onTapZone(zone, longTap || twoFinigersTap);
            }
            _dragStartOffset = 0; //NO_DRAG;
            _isDragging = false;
//            setScrollOffset(_scrollOffset);
//            if (itemIndex != -1) {
//                //CRLog::trace("UP ts=%lld downTs=%lld downDuration=%lld", event->getEventTimestamp(), event->getDownEventTimestamp(), event->getDownDuration());
//                bool isLong = event->getDownDuration() > LONG_TOUCH_THRESHOLD; // 0.5 seconds threshold
//                if (isLong && onItemLongClickEvent(itemIndex))
//                    return true;
//                onItemClickEvent(itemIndex);
//            }
        }
        // fire onclick
        //CRLog::trace("list UP");
        break;
    case ACTION_FOCUS_IN:
//        if (isDragging)
//            setScrollOffset(_dragStartOffset - delta);
//        else
//            _selectedItem = index;
        //invalidate();
        //CRLog::trace("list FOCUS IN");
        break;
    case ACTION_FOCUS_OUT:
//        if (isDragging)
//            setScrollOffset(_dragStartOffset - delta);
//        else
//            _selectedItem = -1;
        //invalidate();
        return false; // to continue tracking
        //CRLog::trace("list FOCUS OUT");
        break;
    case ACTION_CANCEL:
        if (_pinchOp) {
        	endPinchOp(pinchDx, pinchDy, true);
        }
        _isDragging = false;
        //setScrollOffset(_scrollOffset);
        //CRLog::trace("list CANCEL");
        break;
    case ACTION_MOVE:
    	if (_pinchOp) {
    		updatePinchOp(pinchDx, pinchDy);
    	} else if (!_isDragging && event->count() == 2) {
			int ddx0 = myAbs(event->getStartX(0) - event->getStartX(1));
			int ddy0 = myAbs(event->getStartY(0) - event->getStartY(1));
			int ddx1 = myAbs(event->getX(0) - event->getX(1));
			int ddy1 = myAbs(event->getY(0) - event->getY(1));
			int op0, op1;
			if (ddx0 > ddy0 * 3)
				op0 = PINCH_OP_HORIZONTAL;
			else if (ddy0 > ddx0 * 3)
				op0 = PINCH_OP_VERTICAL;
			else
				op0 = PINCH_OP_DIAGONAL;
			if (ddx1 > ddy1 * 3)
				op1 = PINCH_OP_HORIZONTAL;
			else if (ddy1 > ddx1 * 3)
				op1 = PINCH_OP_VERTICAL;
			else
				op1 = PINCH_OP_DIAGONAL;
			int ddd = myAbs(pinchDx) + myAbs(pinchDy);
			if (op0 == op1 && ddd > DRAG_THRESHOLD_X * 2 / 3) {
				startPinchOp(op0, pinchDx, pinchDy);
			}
        } else if (_viewMode != DVM_PAGES && !_isDragging && ((delta > DRAG_THRESHOLD) || (-delta > DRAG_THRESHOLD))) {
            _isDragging = true;
            _docview->SetPos(_dragStartOffset - delta, false);
            prepareScroll(-delta);
            invalidate();
            //_main->update(true);
        } else if (_viewMode == DVM_PAGES && !_isDragging && ((delta2 > DRAG_THRESHOLD) || (-delta2 > DRAG_THRESHOLD))) {
            if (_pageAnimation == PAGE_ANIMATION_NONE) {
                if (delta2 < 0)
                    _docview->doCommand(DCMD_PAGEDOWN, 1);
                else
                    _docview->doCommand(DCMD_PAGEUP, 1);
                event->cancelAllPointers();
                invalidate();
                return true;
            }
            _isDragging = true;
            prepareScroll(-delta2);
            invalidate();
            //_main->update(true);
        } else if (_isDragging) {
            _dragPos.x = event->getX();
            _dragPos.y = event->getY();
            if (_viewMode == DVM_PAGES) {
                // will be handled in Draw
            } else {
                _docview->SetPos(_dragStartOffset - delta, false);
            }
            invalidate();
            //_main->update(true);
        } else if (!_isDragging) {
        	if (event->count() == 2) {
        		int ddx0 = myAbs(event->getStartX(0) - event->getStartX(1));
        		int ddy0 = myAbs(event->getStartY(0) - event->getStartY(1));
        		int ddx1 = myAbs(event->getX(0) - event->getX(1));
        		int ddy1 = myAbs(event->getY(0) - event->getY(1));
        		int op0, op1;
        		if (ddx0 > ddy0 / 2)
        			op0 = PINCH_OP_HORIZONTAL;
        		else if (ddy0 > ddx0 / 2)
        			op0 = PINCH_OP_VERTICAL;
        		else
        			op0 = PINCH_OP_DIAGONAL;
        		if (ddx1 > ddy1 / 2)
        			op1 = PINCH_OP_HORIZONTAL;
        		else if (ddy1 > ddx1 / 2)
        			op1 = PINCH_OP_VERTICAL;
        		else
        			op0 = PINCH_OP_DIAGONAL;
        		int ddd = myAbs(pinchDx) + myAbs(pinchDy);
        		if (op0 == op1 && ddd > DRAG_THRESHOLD_X * 2 / 3) {
        			startPinchOp(op0, pinchDx, pinchDy);
        		}
        	} else {
        		if ((delta2 > DRAG_THRESHOLD_X) || (-delta2 > DRAG_THRESHOLD_X)) {
        			_main->startDragging(event, false);
        		}
            }
        }
        // ignore
        //CRLog::trace("list MOVE");
        break;
    default:
        return CRUIWidget::onTouchEvent(event);
    }
    return true;
}

void CRUIReadWidget::goToPosition(lString16 path) {
    ldomXPointer pt = _docview->getDocument()->createXPointer(path);
    _docview->goToBookmark(pt);
    clearImageCaches();
}

// formats percent value 0..10000  as  XXX.XX%
static lString16 formatPercent(int percent) {
    char s[100];
    sprintf(s, "%d.%02d%%", percent / 100, percent % 100);
    return Utf8ToUnicode(s);
}

lString16 CRUIReadWidget::getCurrentPositionDesc() {
    int pos = getCurrentPositionPercent();
    lString16 str;
    str += formatPercent(pos);
    str += "  ";
    ldomXPointer ptr = _docview->getBookmark();
    if (!ptr.isNull()) {
        lString16 titleText;
        lString16 posText;
        if ( _docview->getBookmarkPosText( ptr, titleText, posText ) ) {
            str += titleText;
        }
    }
    return str;
}

int CRUIReadWidget::getCurrentPositionPercent() {
    return _docview->getPosPercent();
}

void CRUIReadWidget::goToPercent(int percent) {
    int maxpos = _docview->GetFullHeight() - _docview->GetHeight();
    if (maxpos < 0)
        maxpos = 0;
    int p = (int)(percent * (lInt64)maxpos / 10000);
    _docview->SetPos(p, false);
    if (_viewMode == DVM_PAGES)
        _pagedCache.prepare(_docview, _docview->getCurPage(), _measuredWidth, _measuredHeight, 0, false, _pageAnimation);
    else
        _scrollCache.prepare(_docview, p, _pos.width(), _pos.height(), 0, false);
    invalidate();
}

bool CRUIReadWidget::hasTOC() {
    LVTocItem * toc = _docview->getToc();
    return toc && toc->getChildCount();
}

void CRUIReadWidget::showTOC() {
    if (!hasTOC())
        return;
    CRUITOCWidget * widget = new CRUITOCWidget(_main, this);
    _main->showTOC(widget);
}

void CRUIReadWidget::showReaderMenu() {
    CRLog::trace("showReaderMenu");
    CRUIActionList actions;
    actions.add(ACTION_BACK);
    actions.add(ACTION_SETTINGS);
    if (_main->getSettings()->getBoolDef(PROP_NIGHT_MODE, false))
        actions.add(ACTION_DAY_MODE);
    else
        actions.add(ACTION_NIGHT_MODE);
    //actions.add(ACTION_GOTO_PERCENT);
    CRLog::trace("checking TOC");
    //if (hasTOC())
        actions.add(ACTION_TOC);
    actions.add(ACTION_HELP);
    actions.add(ACTION_EXIT);
    lvRect margins;
    CRUIReadMenu * menu = new CRUIReadMenu(this, actions);
    CRLog::trace("showing popup");
    preparePopup(menu, ALIGN_BOTTOM, margins);
}

/// override to handle menu or other action
bool CRUIReadWidget::onAction(const CRUIAction * action) {
    if (!action)
        return false;
    if (action->cmd) {
        doCommand(action->cmd, action->param);
        return true;
    }
    switch (action->id) {
    case CMD_BACK:
        _main->back();
        return true;
    case CMD_TOC:
        showTOC();
        return true;
    case CMD_MENU:
        showReaderMenu();
        return true;
    case CMD_SETTINGS:
        _main->showSettings(lString8("@settings/reader"));
        return true;
    default:
        return _main->onAction(action);
    }
    return false;
}

/// applies properties, returns list of not recognized properties
CRPropRef CRUIDocView::propsApply(CRPropRef props) {
    //CRPropRef oldSettings = propsGetCurrent();
    CRPropRef newSettings = propsGetCurrent() | props;
    CRPropRef forDocview = LVCreatePropsContainer();
    bool backgroundChanged = false;
    //bool needClearCache = false;
    for (int i = 0; i < props->getCount(); i++) {
        lString8 key(props->getName(i));
        //lString8 value(UnicodeToUtf8(props->getValue(i)));
        if (key == PROP_PAGE_MARGINS) {
        	int marginPercent = props->getIntDef(key.c_str(), 5000);
        	int hmargin = deviceInfo.shortSide * marginPercent / 10000;
        	lvRect margins(hmargin, hmargin / 2, hmargin, hmargin / 2);
        	setPageMargins(margins);
        	requestRender();
        } else if (key == PROP_FONT_ANTIALIASING) {
            int antialiasingMode = props->getIntDef(PROP_FONT_ANTIALIASING, 2);
            if (antialiasingMode == 1) {
                antialiasingMode = 2;
            }
            if (fontMan->GetAntialiasMode() != antialiasingMode) {
                fontMan->SetAntialiasMode(antialiasingMode);
            }
            requestRender();
        } else if (key == PROP_FONT_HINTING) {
            bool bytecode = props->getBoolDef(PROP_FONT_HINTING, 1);
            int hintingMode = bytecode ? HINTING_MODE_BYTECODE_INTERPRETOR : HINTING_MODE_AUTOHINT;
            if ((int)fontMan->GetHintingMode() != hintingMode && hintingMode >= 0 && hintingMode <= 2) {
                //CRLog::debug("Setting hinting mode to %d", mode);
                fontMan->SetHintingMode((hinting_mode_t)hintingMode);
            }
            requestRender();
        } else if (key == PROP_FONT_GAMMA_INDEX) {
            int gammaIndex = props->getIntDef(PROP_FONT_GAMMA_INDEX, 15);
            int oldGammaIndex = fontMan->GetGammaIndex();
            if (oldGammaIndex != gammaIndex) {
                fontMan->SetGammaIndex(gammaIndex);
            }
        } else {
            forDocview->setString(key.c_str(), props->getValue(i));
        }
        if (key == PROP_BACKGROUND_COLOR
                || key == PROP_BACKGROUND_IMAGE
                || key == PROP_BACKGROUND_IMAGE_ENABLED
                || key == PROP_BACKGROUND_IMAGE_CORRECTION_BRIGHTNESS
                || key == PROP_BACKGROUND_IMAGE_CORRECTION_CONTRAST) {
            propsGetCurrent()->setString(key.c_str(), props->getValue(i));
            backgroundChanged = true;
            //needClearCache = true;
        }
    }
    CRPropRef res = LVDocView::propsApply(forDocview);
    if (backgroundChanged) {
        //setBackground(resourceResolver->getBackgroundImage(newSettings));
        setBackground(resourceResolver->getBackgroundImage(propsGetCurrent()));
    }
    return res;
}

// apply changed settings
void CRUIReadWidget::applySettings(CRPropRef changed, CRPropRef oldSettings, CRPropRef newSettings) {
    CR_UNUSED2(oldSettings, newSettings);
    CRPropRef docviewprops = LVCreatePropsContainer();
    //bool backgroundChanged = false;
    //bool needClearCache = false;
    for (int i = 0; i < changed->getCount(); i++) {
        lString8 key(changed->getName(i));
        lString8 value(UnicodeToUtf8(changed->getValue(i)));
        //CRLog::trace("%s = %s", key.c_str(), value.c_str());
        if (isDocViewProp(key)) {
            docviewprops->setString(key.c_str(), value.c_str());
            if (key == PROP_FONT_COLOR) {
                _docview->setTextColor(changed->getColorDef(PROP_FONT_COLOR, 0));
                //needClearCache = true;
            }
        }
        if (key == PROP_BACKGROUND_COLOR
                || key == PROP_BACKGROUND_IMAGE
                || key == PROP_BACKGROUND_IMAGE_ENABLED
                || key == PROP_BACKGROUND_IMAGE_CORRECTION_BRIGHTNESS
                || key == PROP_BACKGROUND_IMAGE_CORRECTION_CONTRAST) {
            //backgroundChanged = true;
            //needClearCache = true;
        }
        if (key == PROP_HYPHENATION_DICT) {
            setHyph(lastBookLang, value);
            _docview->requestRender();
            //needClearCache = true;
            invalidate();
        }
        if (key == PROP_PAGE_VIEW_MODE) {
            int n = value.atoi();
            if (n == 0)
                _docview->setViewMode(DVM_SCROLL);
            else if (n == 1)
                _docview->setViewMode(DVM_PAGES, 1);
            else
                _docview->setViewMode(DVM_PAGES, 2);
            _viewMode = _docview->getViewMode();
        }
        if (key == PROP_PAGE_VIEW_ANIMATION) {
            _pageAnimation = (PageFlipAnimation)value.atoi();
            if (_pageAnimation < PAGE_ANIMATION_NONE || _pageAnimation > PAGE_ANIMATION_3D)
                _pageAnimation = PAGE_ANIMATION_SLIDE;
        }
    }
//    if (backgroundChanged) {
//        _docview->setBackground(resourceResolver->getBackgroundImage(newSettings));
//    }
    //if (needClearCache) {
    clearImageCaches();
    //}
    if (docviewprops->getCount())
        _docview->propsApply(docviewprops);
}

/// on starting file loading
void CRUIReadWidget::OnLoadFileStart( lString16 filename ) {
    CR_UNUSED(filename);
}

/// format detection finished
void CRUIReadWidget::OnLoadFileFormatDetected(doc_format_t fileFormat) {
    lString8 cssFile = crconfig.cssDir + LVDocFormatCssFileName(fileFormat);

    lString8 css;
    if (!LVLoadStylesheetFile(Utf8ToUnicode(cssFile), css)) {
        // todo: fallback
    }
    _docview->setStyleSheet(css);
}

/// file loading is finished successfully - drawCoveTo() may be called there
void CRUIReadWidget::OnLoadFileEnd() {

}

/// first page is loaded from file an can be formatted for preview
void CRUIReadWidget::OnLoadFileFirstPagesReady() {

}

/// file progress indicator, called with values 0..100
void CRUIReadWidget::OnLoadFileProgress( int percent) {
    CR_UNUSED(percent);
}

/// document formatting started
void CRUIReadWidget::OnFormatStart() {

}

/// document formatting finished
void CRUIReadWidget::OnFormatEnd() {
	invalidate();
}

/// format progress, called with values 0..100
void CRUIReadWidget::OnFormatProgress(int percent) {
    CR_UNUSED(percent);
}

/// format progress, called with values 0..100
void CRUIReadWidget::OnExportProgress(int percent) {
    CR_UNUSED(percent);
}

/// file load finiished with error
void CRUIReadWidget::OnLoadFileError(lString16 message) {
    CR_UNUSED(message);
}

/// Override to handle external links
void CRUIReadWidget::OnExternalLink(lString16 url, ldomNode * node) {
    CR_UNUSED2(url, node);
}

/// Called when page images should be invalidated (clearImageCache() called in LVDocView)
void CRUIReadWidget::OnImageCacheClear() {
//    class ClearCache : public CRRunnable {
//        CRUIReadWidget * _widget;
//    public:
//        ClearCache(CRUIReadWidget * widget) : _widget(widget) {}
//        virtual void run() {
//            _widget->clearImageCaches();
//        }
//    };
//    concurrencyProvider->executeGui(new ClearCache(this));
}

/// return true if reload will be processed by external code, false to let internal code process it
bool CRUIReadWidget::OnRequestReload() {
    return false;
}




//================================================================
// Scroll Mode page image cache

CRUIReadWidget::ScrollModePageCache::ScrollModePageCache() : minpos(0), maxpos(0), dx(0), dy(0), tdx(0), tdy(0) {

}

#define MIN_TEX_SIZE 64
#define MAX_TEX_SIZE 4096
static int nearestPOT(int n) {
	for (int i = MIN_TEX_SIZE; i <= MAX_TEX_SIZE; i++) {
		if (n <= i)
			return i;
	}
	return MIN_TEX_SIZE;
}

LVDrawBuf * CRUIReadWidget::ScrollModePageCache::createBuf() {
    return new GLDrawBuf(dx, tdy, 32, true);
}

void CRUIReadWidget::ScrollModePageCache::setSize(int _dx, int _dy) {
    if (dx != _dx || dy != _dy) {
        clear();
        dx = _dx;
        dy = _dy;
        tdx = nearestPOT(dx);
        tdy = nearestPOT(dy);
    }
}

/// ensure images are prepared
void CRUIReadWidget::ScrollModePageCache::prepare(LVDocView * _docview, int _pos, int _dx, int _dy, int direction, bool force) {
    setSize(_dx, _dy);
    if (_pos >= minpos && _pos + dy <= maxpos && !force)
        return; // already prepared
    int y0 = direction == 0 ? _pos : (direction > 0 ? (_pos - dy / 4) : (_pos - dy * 5 / 4));
    int y1 = direction == 0 ? _pos : (direction > 0 ? (_pos + dy + dy * 5 / 4) : (_pos + dy + dy / 4));
    if (y0 < 0)
    	y0 = 0;
    if (y1 > _docview->GetFullHeight() + dy)
        y1 = _docview->GetFullHeight() + dy;
    int pos0 = y0 / tdy * tdy;
    int pos1 = (y1 + tdy - 1) / tdy * tdy;
    int pageCount = (pos1 - pos0) / tdy + 1;
    for (int i = pages.length() - 1; i >= 0; i--) {
        ScrollModePage * p = pages[i];
        if (!p->intersects(y0, y1)) {
            pages.remove(i);
            delete p;
        }
    }
    for (int i = 0; i < pageCount; i++) {
        int pos = pos0 + i * tdy;
        bool found = false;
        for (int k = pages.length() - 1; k >= 0; k--) {
            if (pages[k]->pos == pos) {
                found = true;
                break;
            }
        }
        if (!found) {
            ScrollModePage * page = new ScrollModePage();
            page->dx = dx;
            page->dy = tdy;
            page->pos = pos;
            page->drawbuf = createBuf();
            LVDrawBuf * buf = page->drawbuf; //dynamic_cast<GLDrawBuf*>(page->drawbuf);
            buf->beforeDrawing();
            int oldpos = _docview->GetPos();
            _docview->SetPos(pos, false, true);
            buf->SetTextColor(_docview->getTextColor());
            buf->SetBackgroundColor(_docview->getBackgroundColor());
            _docview->Draw(*buf, false);
            _docview->SetPos(oldpos, false, true);
            buf->afterDrawing();
            pages.add(page);
            CRLog::trace("new page cache item %d..%d", page->pos, page->pos + page->dy);
        }
    }
    minpos = maxpos = -1;
    for (int k = 0; k < pages.length(); k++) {
        //CRLog::trace("page cache item [%d] %d..%d", k, pages[k]->pos, pages[k]->pos + pages[k]->dy);
        if (minpos == -1 || minpos > pages[k]->pos) {
            minpos = pages[k]->pos;
        }
        if (maxpos == -1 || maxpos < pages[k]->pos + pages[k]->dy) {
            maxpos = pages[k]->pos + pages[k]->dy;
        }
    }
}

void CRUIReadWidget::ScrollModePageCache::draw(LVDrawBuf * dst, int pos, int x, int y) {
	CRLog::trace("ScrollModePageCache::draw()");
    // workaround for no-rtti builds
	GLDrawBuf * glbuf = dst->asGLDrawBuf(); //dynamic_cast<GLDrawBuf*>(buf);
    if (glbuf) {
        //glbuf->beforeDrawing();
        for (int k = pages.length() - 1; k >= 0; k--) {
            if (pages[k]->intersects(pos, pos + dy)) {
                // draw fragment
                int y0 = pages[k]->pos - pos;
                pages[k]->drawbuf->DrawTo(glbuf, x, y + y0, 0, NULL);
            }
        }
        //glbuf->afterDrawing();
    }
}

void CRUIReadWidget::ScrollModePageCache::clear() {
    pages.clear();
    minpos = 0;
    maxpos = 0;
}


//=============================================================================
//  Paged mode

CRUIReadWidget::PagedModePageCache::PagedModePageCache() : numPages(0), pageCount(0), dx(0), dy(0), tdx(0), tdy(0), newPage(0) {

}

void CRUIReadWidget::PagedModePageCache::clear() {
	//CRLog::trace("CRUIReadWidget::PagedModePageCache::clear");
    pages.clear();
}

LVDrawBuf * CRUIReadWidget::PagedModePageCache::createBuf() {
    return new GLDrawBuf(dx, tdy, 32, true);
}

void CRUIReadWidget::PagedModePageCache::setSize(int _dx, int _dy, int _numPages, int _pageCount) {
    if (dx != _dx || dy != _dy || numPages != _numPages || pageCount != _pageCount) {
        clear();
        numPages = _numPages;
        pageCount = _pageCount;
        dx = _dx;
        dy = _dy;
        tdx = nearestPOT(dx);
        tdy = nearestPOT(dy);
    }
}

CRUIReadWidget::PagedModePage * CRUIReadWidget::PagedModePageCache::findPage(int page) {
	for (int i = 0; i < pages.length(); i++) {
        if (pages[i]->pageNumber == page && !pages[i]->back)
			return pages[i];
	}
	return NULL;
}

CRUIReadWidget::PagedModePage * CRUIReadWidget::PagedModePageCache::findPageBack(int page) {
    for (int i = 0; i < pages.length(); i++) {
        if (pages[i]->pageNumber == page && pages[i]->back)
            return pages[i];
    }
    // fallback
    //return findPage(page);
    return NULL;
}

void CRUIReadWidget::PagedModePageCache::clearExcept(int page1, int page2) {
	for (int i = pages.length() - 1; i >= 0; i--) {
		if (pages[i]->pageNumber != page1 && pages[i]->pageNumber != page2) {
			//CRLog::trace("Clearing page image %d", pages[i]->pageNumber);
			delete pages.remove(i);
		}
	}
}

void CRUIReadWidget::PagedModePageCache::preparePage(LVDocView * _docview, int pageNumber, bool back) {
	if (pageNumber < 0)
		return;
    if (back && findPageBack(pageNumber))
        return; // already prepared
    if (!back && findPage(pageNumber))
		return; // already prepared
    //CRLog::trace("Preparing page image for page %d", pageNumber);
    PagedModePage * page = new PagedModePage();
    page->dx = dx;
    page->dy = dy;
    page->tdx = tdx;
    page->tdy = tdy;
    page->pageNumber = pageNumber;
    page->numPages = numPages;
    page->drawbuf = createBuf();
    page->back = back;
    LVDrawBuf * buf = page->drawbuf; //dynamic_cast<GLDrawBuf*>(page->drawbuf);
    buf->beforeDrawing();
    buf->SetTextColor(_docview->getTextColor());
    buf->SetBackgroundColor(_docview->getBackgroundColor());
    lvRect rc(0, 0, dx, dy);
    int oldPage = _docview->getCurPage();
    if (oldPage != pageNumber)
        _docview->goToPage(pageNumber);
    //_docview->Draw(*buf, -1, pageNumber, false, false);
    if (back)
        _docview->drawPageBackground(*buf, 0, 0);
    else
        _docview->Draw(*buf, false);
    if (oldPage != pageNumber)
        _docview->goToPage(oldPage);
    int sdx = dx / 10 / _docview->getVisiblePageCount();
    lUInt32 cl1 = 0xE0000000;
    lUInt32 cl2 = 0xFF000000;
    buf->GradientRect(0, 0, sdx, dy, cl1, cl2, cl2, cl1);
    buf->GradientRect(dx - sdx, 0, dx, dy, cl2, cl1, cl1, cl2);
    if (_docview->getVisiblePageCount() == 2) {
        buf->GradientRect(dx / 2, 0, dx / 2 + sdx, dy, cl1, cl2, cl2, cl1);
        buf->GradientRect(dx / 2 - sdx, 0, dx / 2, dy, cl2, cl1, cl1, cl2);
    }
    if (_docview->getVisiblePageCount() == 2) {
        lvRect rc1 = rc;
        rc1.right = dx / 2;
        buf->DrawFrame(rc1, 0xC0404040, 1);
        rc1.shrink(1);
        buf->DrawFrame(rc1, 0xE0404040, 1);
        rc1 = rc;
        rc1.left = dx / 2;
        buf->DrawFrame(rc1, 0xC0404040, 1);
        rc1.shrink(1);
        buf->DrawFrame(rc1, 0xE0404040, 1);
    } else {
        buf->DrawFrame(rc, 0xC0404040, 1);
        rc.shrink(1);
        buf->DrawFrame(rc, 0xE0404040, 1);
    }
    buf->afterDrawing();
    pages.add(page);
}

/// ensure images are prepared
void CRUIReadWidget::PagedModePageCache::prepare(LVDocView * _docview, int _page, int _dx, int _dy, int _direction, bool force, int _pageAnimation) {
    CR_UNUSED(force);
    setSize(_dx, _dy, _docview->getVisiblePageCount(), _docview->getPageCount());
    pageAnimation = _pageAnimation;
    if (_direction)
        direction = _direction;
    int thisPage = _page; // current page
    if (numPages == 2)
    	thisPage = thisPage & ~1;
    int nextPage = -1;
    if (direction > 0) {
    	nextPage = thisPage + numPages;
        if (nextPage >= pageCount + numPages - 1)
    		nextPage = -1;
    } else if (direction < 0) {
    	nextPage = thisPage - numPages;
    	if (nextPage < 0)
    		nextPage = -1; // no page
    }
    if (nextPage >= 0)
        newPage = nextPage;
    if (findPage(thisPage) && (nextPage == -1 || findPage(nextPage)))
    	return; // already prepared
    clearExcept(thisPage, nextPage);
    preparePage(_docview, thisPage);
    preparePage(_docview, nextPage);
    if (pageAnimation == PAGE_ANIMATION_3D && direction > 0)
        preparePage(_docview, thisPage, true);
    if (pageAnimation == PAGE_ANIMATION_3D && direction < 0)
        preparePage(_docview, nextPage, true);
}

void CRUIReadWidget::PagedModePageCache::drawFolded(LVDrawBuf * buf, PagedModePage * page, int srcx1, int srcx2, int dstx1, int dstx2, float angle1, float angle2) {
    lUInt32 shadowAlpha = 64;
    float dangle = (angle2 - angle1);
    if (dangle < 0)
        dangle = -dangle;
    if (dangle < 0.01f) {
        buf->DrawFragment(page->drawbuf, srcx1, 0, srcx2 - srcx1, dy, dstx1, 0, dstx2 - dstx1, dy, 0);
    } else {
        // TODO
        int steps = (int)(dangle / 0.15f + 1);
        float sa1 = (float)sin(angle1);
        float sa2 = (float)sin(angle2);
        for (int step = 0; step < steps; step++) {
            float a1 = angle1 + (angle2 - angle1) * step / steps;
            float a2 = angle1 + (angle2 - angle1) * (step + 1) / steps;
            int alpha1 = 255 - (int)(shadowAlpha * sin(a1));
            int alpha2 = 255 - (int)(shadowAlpha * sin(a2));
            int sx1 = srcx1 + (srcx2 - srcx1) * step / steps;
            int sx2 = srcx1 + (srcx2 - srcx1) * (step + 1) / steps;
            int dx1 = (int)(dstx1 + (dstx2 - dstx1) * ((float)sin(a1) - sa1) / (sa2 - sa1) + 0.5f);
            int dx2 = (int)(dstx1 + (dstx2 - dstx1) * ((float)sin(a2) - sa1) / (sa2 - sa1) + 0.5f);
            buf->DrawFragment(page->drawbuf, sx1, 0, sx2 - sx1, dy, dx1, 0, dx2 - dx1, dy, 0);
            lUInt32 shadowcl1 = (alpha1 << 24) | 0x000000;
            lUInt32 shadowcl2 = (alpha2 << 24) | 0x000000;
            buf->GradientRect(dx1, 0, dx2, dy, shadowcl1, shadowcl2, shadowcl2, shadowcl1);
        }
    }
}

void CRUIReadWidget::PagedModePageCache::drawFolded(LVDrawBuf * buf, PagedModePage * page1, PagedModePage * page1back, PagedModePage * page2, int progress, int diam, int x) {
    float m_pi_2 = (float)M_PI / 2;
    float fdiam = 60 * dx / 100.0f;
    float fradius = fdiam / 2;
    float halfc = m_pi_2 * fdiam;
    float quarterc = halfc / 2;
    float downx = (progress * (dx + halfc) / 10000);
    float d = 0; // left flat part of current page other side
    if (downx > halfc)
        d = downx - halfc;
    float a = dx - downx; // left flat part of current page
    float avisible = d > 0 ? a - d : a;
    float bangle = downx > quarterc ? m_pi_2 : downx * m_pi_2 / quarterc;
    float bx = downx > quarterc ? quarterc : downx;
    float b = downx > quarterc ? fradius : (float)sin(bangle) * fradius;
    float e = 0; // right flat part of next page
    //if (downx > fradius)
    e = downx - b;
    float c = 0;
    float cx = 0;
    float cangle = 0;
    if (downx > quarterc) {
        if (downx > halfc) {
            cangle = m_pi_2;
            cx = quarterc;
        } else {
            cx = downx - quarterc;
            cangle = cx * m_pi_2 / quarterc;
        }
        c = fradius - (float)cos(cangle) * fradius;
    }

    int shadowdx = (int)(fradius);
    lUInt32 shadowcl1 = 0xD0000000;
    lUInt32 shadowcl2 = 0xFF000000;


    int ia = (int)a;
    int iavisible = (int)avisible;
    if (iavisible > 0)
        drawFolded(buf, page1, 0, iavisible, 0 + x, iavisible + x, 0, 0);
    int ie = (int)e;
    if (ie > 0) {
        drawFolded(buf, page2, dx - ie, dx, dx - ie + x, dx + x, 0, 0);
        buf->GradientRect(dx - ie + x, 0, dx - ie + x + shadowdx, dy, shadowcl1, shadowcl2, shadowcl2, shadowcl1);
    }
    int ib = (int)b;
    int ibx = (int)bx;
    //int idownx = (int)downx;
    if (ib > 0) {
        drawFolded(buf, page1, ia, ia + ibx, ia + x, dx - ie + x, 0, bangle);
    }
    int ic = (int)(c + 0.5f);
    int icx = (int)(cx + 0.5f);
    int id = (int)(d + 0.5f);
    if (ic > 0) {
        drawFolded(buf, page1back, dx - id, dx - id - icx, dx - ie - ic + x, dx - ie + x, m_pi_2 - cangle, m_pi_2);
    }
    if (id > 0)
        drawFolded(buf, page1back, dx, dx - id, dx - downx - id + x, dx - downx + x, 0, 0);
}

/// draw
void CRUIReadWidget::PagedModePageCache::draw(LVDrawBuf * dst, int pageNumber, int direction, int progress, int x) {
    CR_UNUSED2(direction, progress);
    //CRLog::trace("PagedModePageCache::draw(page=%d, progress=%d dir=%d)", pageNumber, progress, direction);
    // workaround for no-rtti builds
    GLDrawBuf * glbuf = dst->asGLDrawBuf(); //dynamic_cast<GLDrawBuf*>(buf);
    if (glbuf) {
        if (progress < 0)
            progress = 0;
        if (progress > 10000)
            progress = 10000;
        //glbuf->beforeDrawing();
        int nextPage = pageNumber;
        if (direction > 0)
            nextPage += numPages;
        else if (direction < 0)
            nextPage -= numPages;
        if (nextPage >= pageCount + numPages - 1)
            nextPage = pageNumber;
        if (nextPage < 0)
            nextPage = pageNumber;
        CRUIReadWidget::PagedModePage * page = findPage(pageNumber);
        CRUIReadWidget::PagedModePage * page2 = ((nextPage != pageNumber) && (pageAnimation != PAGE_ANIMATION_NONE)) ? findPage(nextPage) : NULL;
        if (page2 && page) {
            // animation
            int ddx = dx * progress / 10000;
            int shadowdx = dx / 20;
            lUInt32 shadowcl1 = 0xD0000000;
            lUInt32 shadowcl2 = 0xFF000000;
            int alpha = 255 - 255 * progress / 10000;
            if (alpha < 0)
                alpha = 0;
            else if (alpha > 255)
                alpha = 255;
            if (direction > 0) {
                //
                if (pageAnimation == PAGE_ANIMATION_SLIDE) {
                    page2->drawbuf->DrawTo(glbuf, x + 0, 0, 0, NULL);
                    page->drawbuf->DrawTo(glbuf, x + 0 - ddx, 0, 0, NULL);
                    glbuf->GradientRect(x + dx - ddx, 0, x + dx - ddx + shadowdx, dy, shadowcl1, shadowcl2, shadowcl2, shadowcl1);
                } else if (pageAnimation == PAGE_ANIMATION_SLIDE2) {
                    page2->drawbuf->DrawTo(glbuf, x + 0 + dx - ddx, 0, 0, NULL);
                    page->drawbuf->DrawTo(glbuf, x + 0 - ddx, 0, 0, NULL);
                } else if (pageAnimation == PAGE_ANIMATION_FADE) {
                    page->drawbuf->DrawTo(glbuf, x + 0, 0, 0, NULL);
                    page2->drawbuf->DrawTo(glbuf, x + 0, 0, alpha << 16, NULL);
                } else if (pageAnimation == PAGE_ANIMATION_3D) {
                    CRUIReadWidget::PagedModePage * page_back = findPageBack(pageNumber);
                    if (!page_back)
                        page_back = page;
                    drawFolded(glbuf, page, page_back, page2, progress, 30, x);
//                    page2->drawbuf->DrawTo(glbuf, x + 0, 0, 0, NULL);
//                    glbuf->DrawFragment(page->drawbuf, 0, 0, dx, dy, x + 0, 0, dx - ddx, dy, 0);
//                    glbuf->GradientRect(x + dx - ddx, 0, x + dx - ddx + shadowdx, dy, shadowcl1, shadowcl2, shadowcl2, shadowcl1);
                }
            } else if (direction < 0) {
                //
                if (pageAnimation == PAGE_ANIMATION_SLIDE) {
                    page->drawbuf->DrawTo(glbuf, x + 0, 0, 0, NULL);
                    page2->drawbuf->DrawTo(glbuf, x + 0 - dx + ddx, 0, 0, NULL);
                    if (ddx < shadowdx)
                        shadowcl1 = (0xFF - ddx * 0x3F / shadowdx) << 24;
                    glbuf->GradientRect(x + 0 + ddx, 0, x + 0 + ddx + shadowdx, dy, shadowcl1, shadowcl2, shadowcl2, shadowcl1);
                } else if (pageAnimation == PAGE_ANIMATION_SLIDE2) {
                    page->drawbuf->DrawTo(glbuf, x + 0 + ddx, 0, 0, NULL);
                    page2->drawbuf->DrawTo(glbuf, x + 0 - dx + ddx, 0, 0, NULL);
                } else if (pageAnimation == PAGE_ANIMATION_FADE) {
                    page->drawbuf->DrawTo(glbuf, x + 0, 0, 0, NULL);
                    page2->drawbuf->DrawTo(glbuf, x + 0, 0, alpha << 16, NULL);
                } else if (pageAnimation == PAGE_ANIMATION_3D) {
                    CRUIReadWidget::PagedModePage * page_back = findPageBack(nextPage);
                    if (!page_back)
                        page_back = page2;
                    drawFolded(glbuf, page2, page_back, page, 10000 - progress, 30, x);
//                    page->drawbuf->DrawTo(glbuf, x + 0, 0, 0, NULL);
//                    glbuf->DrawFragment(page2->drawbuf, 0, 0, dx, dy, x + 0, 0, ddx, dy, 0);
//                    if (ddx < shadowdx)
//                        shadowcl1 = (0xFF - ddx * 0x3F / shadowdx) << 24;
//                    glbuf->GradientRect(x + 0 + ddx, 0, x + 0 + ddx + shadowdx, dy, shadowcl1, shadowcl2, shadowcl2, shadowcl1);
                }
            }
        } else {
            // no animation
            if (page) {
                // simple draw current page
                page->drawbuf->DrawTo(glbuf, x, 0, 0, NULL);
            }
        }
        //glbuf->afterDrawing();
    }
}





static void addTocItems(LVPtrVector<LVTocItem, false> & toc, LVTocItem * item) {
    if (item->getParent())
        toc.add(item);
    for (int i = 0; i < item->getChildCount(); i++)
        addTocItems(toc, item->getChild(i));
}

CRUITOCWidget::CRUITOCWidget(CRUIMainWidget * main, CRUIReadWidget * read) : CRUIWindowWidget(main), _readWidget(read) {
    _title = new CRUITitleBarWidget(lString16(), this, this, false);
    _title->setTitle(STR_READER_TOC);
    _body->addChild(_title);
    _list = new CRUIListWidget(true, this);
    _list->setOnItemClickListener(this);
    _list->setStyle("SETTINGS_ITEM_LIST");
    _body->addChild(_list);
    addTocItems(_toc, read->getDocView()->getToc());
    _itemWidget = new CRUIHorizontalLayout();
    _itemWidget->setMinHeight(MIN_ITEM_PX * 2 / 3);
    _itemWidget->setPadding(PT_TO_PX(2));
    _chapter = new CRUITextWidget();
    _page = new CRUITextWidget();
    _itemWidget->addChild(_chapter);
    _itemWidget->addChild(_page);
    _page->setAlign(ALIGN_RIGHT|ALIGN_VCENTER);
    _chapter->setAlign(ALIGN_LEFT|ALIGN_VCENTER);
    _chapter->setLayoutParams(FILL_PARENT, WRAP_CONTENT);
    _itemWidget->setStyle("LIST_ITEM");
}

int CRUITOCWidget::getItemCount(CRUIListWidget * list) {
    CR_UNUSED(list);
    return _toc.length();
}

CRUIWidget * CRUITOCWidget::getItemWidget(CRUIListWidget * list, int index) {
    CR_UNUSED(list);
    LVTocItem * item = _toc[index];
    _chapter->setText(item->getName());
    _page->setText(formatPercent(item->getPercent()));
    lvRect padding;
    padding.left = (item->getLevel() - 1) * MIN_ITEM_PX / 3;
    _chapter->setPadding(padding);
    return _itemWidget;
}

// list item click
bool CRUITOCWidget::onListItemClick(CRUIListWidget * widget, int itemIndex) {
    CR_UNUSED(widget);
    LVTocItem * item = _toc[itemIndex];
    _readWidget->goToPosition(item->getPath());
    onAction(CMD_BACK);
    return true;
}

bool CRUITOCWidget::onClick(CRUIWidget * widget) {
    if (widget->getId() == "BACK")
        onAction(CMD_BACK);
    return true;
}

bool CRUITOCWidget::onLongClick(CRUIWidget * widget) {
    if (widget->getId() == "BACK")
        onAction(CMD_BACK);
    return true;
}

/// handle menu or other action
bool CRUITOCWidget::onAction(const CRUIAction * action) {
    switch (action->id) {
    case CMD_BACK:
        _main->back();
        return true;
    default:
        break;
    }
    return false;
}

