#ifndef CRUIMAIN_H
#define CRUIMAIN_H

#include "crui.h"
#include "cruifolderwidget.h"
#include "cruihomewidget.h"
#include "cruireadwidget.h"
#include "cruipopup.h"

enum VIEW_MODE {
    MODE_HOME,
    MODE_FOLDER,
    MODE_READ
};

class CRUIScreenUpdateManagerCallback {
public:
    /// set animation fps (0 to disable) and/or update screen instantly
    virtual void setScreenUpdateMode(bool updateNow, int animationFps) = 0;
    virtual ~CRUIScreenUpdateManagerCallback() {}
};

class CRUIMainWidget : public CRUIWidget, public CRDirScanCallback, public CRUIScreenUpdateManagerCallback {
    CRUIHomeWidget * _home;
    CRUIFolderWidget * _folder;
    CRUIReadWidget * _read;
    CRUIPopupWindow * _popup;
    CRUIWidget * _currentWidget;
    VIEW_MODE _mode;
    lString8 _currentFolder;
    lString8 _pendingFolder;
    CRUIScreenUpdateManagerCallback * _screenUpdater;
    void setMode(VIEW_MODE mode);
public:
    /// draw now
    virtual void update();
    /// forward screen update request to external code
    virtual void setScreenUpdateMode(bool updateNow, int animationFps) {
        if (_screenUpdater)
            _screenUpdater->setScreenUpdateMode(updateNow, animationFps);
    }
    virtual void setScreenUpdater(CRUIScreenUpdateManagerCallback * screenUpdater) { _screenUpdater = screenUpdater; }
    virtual void onDirectoryScanFinished(CRDirCacheItem * item);
    virtual int getChildCount();
    virtual CRUIWidget * getChild(int index);
    /// measure dimensions
    virtual void measure(int baseWidth, int baseHeight);
    /// updates widget position based on specified rectangle
    virtual void layout(int left, int top, int right, int bottom);
    /// draws widget with its children to specified surface
    virtual void draw(LVDrawBuf * buf);
    /// motion event handler, returns true if it handled event
    virtual bool onTouchEvent(const CRUIMotionEvent * event);
    /// returns true if widget is child of this
    virtual bool isChild(CRUIWidget * widget);

    void openBook(lString8 pathname);
    void showFolder(lString8 folder);
    void showHome();

    void recreate();
    CRUIMainWidget();
    virtual ~CRUIMainWidget();
};

#endif // CRUIMAIN_H
