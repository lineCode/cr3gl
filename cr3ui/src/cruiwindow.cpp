#include "cruiwindow.h"
#include "crui.h"
#include "cruimain.h"

using namespace CRUI;

#define POPUP_ANIMATION_DURATION 200

//==========================================================

int CRUIWindowWidget::getChildCount() {
    return _children.length() + (_popupControl.popup ? 1 : 0);
}

CRUIWidget * CRUIWindowWidget::getChild(int index) {
    if (_popupControl.popup) {
        if (index == 0)
            return _popupControl.popup;
        else
            return _children.get(index - 1);
    } else {
        return _children.get(index);
    }
}

class CRUIPopupFrame : public CRUILinearLayout {
    PopupControl * _control;
    CRUIWidget * _body;
    CRUIWidget * _handle;
public:
    CRUIPopupFrame(PopupControl * control, CRUIWidget * body, int drawerLocation) : CRUILinearLayout(drawerLocation == ALIGN_TOP || drawerLocation == ALIGN_BOTTOM), _control(control), _handle(NULL) {
        _body = body;
        if (drawerLocation) {
            _handle = new CRUIWidget();
            if (drawerLocation == ALIGN_TOP) {
                _handle->setMinHeight(MIN_ITEM_PX / 2);
                _handle->setMaxHeight(MIN_ITEM_PX / 2);
                _handle->setLayoutParams(FILL_PARENT, WRAP_CONTENT);
                addChild(_handle);
                addChild(_body);
                setLayoutParams(FILL_PARENT, WRAP_CONTENT);
            } else if (drawerLocation == ALIGN_BOTTOM) {
                _handle->setMinHeight(MIN_ITEM_PX / 2);
                _handle->setMaxHeight(MIN_ITEM_PX / 2);
                _handle->setLayoutParams(FILL_PARENT, WRAP_CONTENT);
                addChild(_body);
                addChild(_handle);
                setLayoutParams(FILL_PARENT, WRAP_CONTENT);
            } else if (drawerLocation == ALIGN_LEFT) {
                _handle->setMinWidth(MIN_ITEM_PX / 2);
                _handle->setMaxWidth(MIN_ITEM_PX / 2);
                _handle->setLayoutParams(WRAP_CONTENT, FILL_PARENT);
                addChild(_handle);
                addChild(_body);
                setLayoutParams(WRAP_CONTENT, FILL_PARENT);
            } else if (drawerLocation == ALIGN_RIGHT) {
                _handle->setMinWidth(MIN_ITEM_PX / 2);
                _handle->setMaxWidth(MIN_ITEM_PX / 2);
                _handle->setLayoutParams(WRAP_CONTENT, FILL_PARENT);
                addChild(_body);
                addChild(_handle);
                setLayoutParams(WRAP_CONTENT, FILL_PARENT);
            }
            _handle->setStyle("POPUP_FRAME_HANDLE");
        } else {
            addChild(_body);
            setLayoutParams(WRAP_CONTENT, WRAP_CONTENT);
        }
        setStyle("POPUP_FRAME");
    }

    virtual void animate(lUInt64 millisPassed) {
        CR_UNUSED(millisPassed);
        lInt64 ts = GetCurrentTimeMillis();
        if (ts <= _control->startTs) {
            _control->progress = 0;
        } else if (ts < _control->endTs) {
            _control->progress = (int)((ts - _control->startTs) * 1000 / (_control->endTs - _control->startTs));
        } else {
            _control->progress = 1000;
        }
        if (_control->closing && _control->progress >= 1000) {
            _control->close();
        }
    }

    virtual bool isAnimating() {
        return _control->progress < 1000 || isAnimatingRecursive();
    }

    /// returns true if point is inside control (excluding margins)
    virtual bool isPointInside(int x, int y) {
        CR_UNUSED2(x, y);
        // to handle all touch events
        return true;
    }
    /// motion event handler, returns true if it handled event
    virtual bool onTouchEvent(const CRUIMotionEvent * event) {
        if (!_body->isPointInside(event->getX(), event->getY())) {
            if (event->getAction() == ACTION_UP)
                _control->animateClose();
            return true;
        }
        return true;
    }

    /// key event handler, returns true if it handled event
    virtual bool onKeyEvent(const CRUIKeyEvent * event) {
        if (event->key() == CR_KEY_BACK || event->key() == CR_KEY_ESC) {
            if (event->getType() == KEY_ACTION_RELEASE) {
                _control->animateClose();
            }
            return true;
        }
        return true;
    }
};

/// draws popup above content
void CRUIWindowWidget::drawPopup(LVDrawBuf * buf) {
    if (_popupControl.popup) {
        // outer space background
        buf->FillRect(_pos, _popupControl.getColor());
        lvRect rc;
        _popupControl.getRect(rc);
        _popupControl.popup->layout(rc.left, rc.top, rc.right, rc.bottom);
        _popupControl.popup->draw(buf);
    }
}

/// draws widget with its children to specified surface
void CRUIWindowWidget::draw(LVDrawBuf * buf) {
    CRUILinearLayout::draw(buf);
    drawPopup(buf);
}

/// start animation of popup closing
void PopupControl::animateClose() {
    if (!popup || closing)
        return;
    startTs = GetCurrentTimeMillis();
    endTs = startTs + POPUP_ANIMATION_DURATION;
    progress = 0;
    closing = true;
}

void PopupControl::layout(const lvRect & pos) {
    popup->measure(pos.width() - margins.left - margins.right, pos.height() - margins.top - margins.bottom);
    width = popup->getMeasuredWidth();
    height = popup->getMeasuredHeight();
    popup->layout(0, 0, width, height);
    lvRect rc = pos;
    lvRect srcrc = pos;
    if (align == ALIGN_TOP) {
        rc.bottom = rc.top + height;
        rc.left += margins.left;
        rc.right -= margins.right;
        srcrc = rc;
        srcrc.top -= height;
        srcrc.bottom -= height;
    } else if (align == ALIGN_BOTTOM) {
        rc.top = rc.bottom - height;
        rc.left += margins.left;
        rc.right -= margins.right;
        srcrc = rc;
        srcrc.top += height;
        srcrc.bottom += height;
    } else if (align == ALIGN_LEFT) {
        rc.right = rc.left + width;
        rc.top += margins.top;
        rc.bottom -= margins.bottom;
        srcrc = rc;
        srcrc.left -= width;
        srcrc.right -= width;
    } else if (align == ALIGN_RIGHT) {
        rc.left = rc.right - width;
        rc.top += margins.top;
        rc.bottom -= margins.bottom;
        srcrc = rc;
        srcrc.left += width;
        srcrc.right += width;
    } else {
        // center
        int dw = pos.width() - margins.left - margins.right - width;
        int dh = pos.height() - margins.top - margins.bottom - height;
        rc.left += dw / 2 + margins.left;
        rc.top += dh / 2 + margins.top;
        rc.right = rc.left + width;
        rc.bottom = rc.top+ height;
        srcrc = rc;
    }
    srcRect = srcrc;
    dstRect = rc;
}

lUInt32 PopupControl::getColor() {
    int p = closing ? 1000 - progress : progress;
    if (p <= 0) {
        return 0xFF000000 | outerColor;
    } else if (p >= 1000) {
        return outerColor;
    } else {
        int dstalpha = (outerColor >> 24) & 0xFF;
        int alpha = 255 + (dstalpha - 255) * p / 1000;
        if (alpha < 0)
            alpha = 0;
        else if (alpha > 255)
            alpha = 255;
        return (outerColor & 0xFFFFFF) | (alpha << 24);
    }
}

void PopupControl::getRect(lvRect & rc) {
    int p = closing ? 1000 - progress : progress;
    if (p <= 0) {
        rc = srcRect;
    } else if (p >= 1000) {
        rc = dstRect;
    } else {
        rc.left = srcRect.left + (dstRect.left - srcRect.left) * p / 1000;
        rc.right = srcRect.right + (dstRect.right - srcRect.right) * p / 1000;
        rc.top = srcRect.top + (dstRect.top - srcRect.top) * p / 1000;
        rc.bottom = srcRect.bottom + (dstRect.bottom - srcRect.bottom) * p / 1000;
    }

}

void CRUIWindowWidget::preparePopup(CRUIWidget * widget, int location, const lvRect & margins) {
    int handleLocation = 0;
    if (location == ALIGN_TOP)
        handleLocation = ALIGN_BOTTOM;
    else if (location == ALIGN_BOTTOM)
        handleLocation = ALIGN_TOP;
    else if (location == ALIGN_LEFT)
        handleLocation = ALIGN_RIGHT;
    else if (location == ALIGN_RIGHT)
        handleLocation = ALIGN_LEFT;
    CRUIPopupFrame * frame = new CRUIPopupFrame(&_popupControl, widget, handleLocation);
    widget = frame;
    _popupControl.close();
    _popupControl.popup = widget;
    _popupControl.align = location;
    _popupControl.margins = margins;
    _popupControl.layout(_pos);
    _popupControl.startTs = GetCurrentTimeMillis();
    _popupControl.endTs = _popupControl.startTs + POPUP_ANIMATION_DURATION;
    _popupControl.progress = 0;
    _popupControl.closing = false;
    _popupControl.outerColor = 0x80404040;
}

class CRUIListMenu : public CRUIListWidget, public CRUIListAdapter {
    CRUIWindowWidget * _window;
    CRUIActionList _actionList;
    CRUIHorizontalLayout * _itemLayout;
    CRUIImageWidget * _itemIcon;
    CRUITextWidget * _itemText;
public:
    CRUIListMenu(CRUIWindowWidget * window, const CRUIActionList & actionList) : _window(window), _actionList(actionList) {
        _itemIcon = new CRUIImageWidget();
        _itemText = new CRUITextWidget();
        _itemLayout = new CRUIHorizontalLayout();
        _itemLayout->addChild(_itemIcon);
        _itemLayout->addChild(_itemText);
        _itemText->setLayoutParams(FILL_PARENT, WRAP_CONTENT);
        _itemIcon->setStyle("MENU_ITEM_ICON");
        _itemText->setStyle("MENU_ITEM_TEXT");
        _itemLayout->setStyle("MENU_ITEM");
        setAdapter(this);
    }
    int getItemCount(CRUIListWidget * list) {
        CR_UNUSED(list);
        return _actionList.length();
    }

    CRUIWidget * getItemWidget(CRUIListWidget * list, int index) {
        CR_UNUSED(list);
        _itemIcon->setImage(_actionList[index]->icon_res);
        _itemText->setText(_actionList[index]->getName());
        return _itemLayout;
    }
    virtual bool onItemClickEvent(int itemIndex) {
        _window->onMenuItemAction(_actionList[itemIndex]);
        return true;
    }
};

/// close popup menu, and call onAction
bool CRUIWindowWidget::onMenuItemAction(const CRUIAction * action) {
    _popupControl.close();
    if (onAction(action))
        return true;
    return _main->onAction(action);
}

void CRUIWindowWidget::showMenu(const CRUIActionList & actionList, int location, lvRect & margins, bool asToolbar) {
    CRUIListMenu * menu = new CRUIListMenu(this, actionList);
    preparePopup(menu, location, margins);
}
