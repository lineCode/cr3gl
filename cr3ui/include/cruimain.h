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
    lString8Collection _folderStack;
    lUInt64 _lastAnimationTs;

    void setMode(VIEW_MODE mode);

    struct AnimatinControl {
        bool active;
        bool manual;
        bool deleteOldWidget;
        VIEW_MODE oldMode;
        VIEW_MODE newMode;
        CRUIWidget * oldWidget;
        CRUIWidget * newWidget;
        int direction;
        int duration;
        int progress;
        lvPoint startPoint;
        lUInt64 startTs;
        AnimatinControl() : active(false) {}
    };
    AnimatinControl _animation;

    void startAnimation(CRUIWidget * newWidget, VIEW_MODE newMode, int direction, int duration, bool deleteOldWidget, bool manual);
    void stopAnimation();
public:
    virtual void animate(lUInt64 millisPassed);
    virtual bool isAnimating();

    /// draw now
    virtual void update();

    /// forward screen update request to external code
    virtual void setScreenUpdateMode(bool updateNow, int animationFps) {
        if (_screenUpdater)
            _screenUpdater->setScreenUpdateMode(updateNow, animationFps);
    }
    virtual void setScreenUpdater(CRUIScreenUpdateManagerCallback * screenUpdater) { _screenUpdater = screenUpdater; }


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
    /// motion event handler - before children, returns true if it handled event
    virtual bool onTouchEventPreProcess(const CRUIMotionEvent * event);

    /// return true if drag operation is intercepted
    virtual bool startDragging(const CRUIMotionEvent * event, bool vertical);

    /// returns true if widget is child of this
    virtual bool isChild(CRUIWidget * widget);

    void showSlowOperationPopup();
    void hideSlowOperationPopup();

    void openBook(lString8 pathname);
    void showFolder(lString8 folder);
    void showHome();
    void back();

    virtual void onDirectoryScanFinished(CRDirCacheItem * item);

    void recreate();
    CRUIMainWidget();
    virtual ~CRUIMainWidget();
};

#endif // CRUIMAIN_H
