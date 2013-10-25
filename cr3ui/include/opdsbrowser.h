/*
 * opdsbrowser.h
 *
 *  Created on: Oct 25, 2013
 *      Author: lve
 */

#ifndef OPDSBROWSER_H_
#define OPDSBROWSER_H_

#include "cruilist.h"
#include "fileinfo.h"
#include "cruiwindow.h"

class CRUITitleBarWidget;
//class CRUIOpdsItemListWidget;

class CRUIMainWidget;

class CRUIOpdsBrowserWidget : public CRUIWindowWidget, public CRUIOnListItemClickListener, public CRUIOnClickListener, public CRUIOnLongClickListener {
	CRUITitleBarWidget * _title;
	//CRUIOpdsItemListWidget * _fileList;
    CRDirContentItem * _dir;
public:
    /// returns true if all coverpages are available, false if background tasks are submitted
    virtual bool requestAllVisibleCoverpages();
    /// motion event handler, returns true if it handled event
    virtual bool onTouchEvent(const CRUIMotionEvent * event);
    virtual bool onKeyEvent(const CRUIKeyEvent * event);
    virtual bool onClick(CRUIWidget * widget);
    virtual bool onLongClick(CRUIWidget * widget);
    virtual bool onListItemClick(CRUIListWidget * widget, int itemIndex);
    virtual void setDirectory(CRDirContentItem * _dir);
    /// override to handle menu or other action
    virtual bool onAction(const CRUIAction * action);
    /// override to handle menu or other action - by id
    virtual bool onAction(int actionId) { return CRUIWindowWidget::onAction(actionId); }
    CRUIOpdsBrowserWidget(CRUIMainWidget * main);
	virtual ~CRUIOpdsBrowserWidget();
};



#endif /* OPDSBROWSER_H_ */
