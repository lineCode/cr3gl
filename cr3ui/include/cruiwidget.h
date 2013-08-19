/*
 * cruiwidget.h
 *
 *  Created on: Aug 15, 2013
 *      Author: vlopatin
 */

#ifndef CRUIWIDGET_H_
#define CRUIWIDGET_H_

#include "cruievent.h"

/// base class for all UI elements
class CRUIWidget {
protected:
	lString8 _id;
	lString8 _styleId;
	lUInt32 _state;
	lvRect _pos;
	lvRect _margin;
	lvRect _padding;
	int _layoutWidth;
	int _layoutHeight;
	int _minWidth;
	int _maxWidth;
	int _minHeight;
	int _maxHeight;
	int _measuredWidth;
	int _measuredHeight;
	CRUIWidget * _parent;
	CRUIImageRef _background;
	bool _layoutRequested;
	bool _drawRequested;
	LVFontRef _font;
	lUInt8 _fontSize;
	lUInt32 _textColor;
	lUInt32 _align;
	CRUIOnTouchEventListener * _onTouchListener;
	CRUIOnClickListener * _onClickListener;
	CRUIOnLongClickListener * _onLongClickListener;

	/// measure dimensions
	virtual void defMeasure(int baseWidth, int baseHeight, int contentWidth, int contentHeight);
	/// correct rectangle bounds according to alignment
	virtual void applyAlign(lvRect & rc, int contentWidth, int contentHeight);

	bool setClipRect(LVDrawBuf * buf, lvRect & rc);

public:

	CRUIWidget();
	virtual ~CRUIWidget();

	/// returns true if point is inside control (excluding margins)
	virtual bool isPointInside(int x, int y);
	/// returns true if widget is child of this
	virtual bool isChild(CRUIWidget * widget);

	/// motion event handler, returns true if it handled event
	virtual bool onTouchEvent(const CRUIMotionEvent * event);
	/// click handler, returns true if it handled event
	virtual bool onClickEvent();
	/// long click handler, returns true if it handled event
	virtual bool onLongClickEvent();

	virtual CRUIOnTouchEventListener * setOnTouchListener(CRUIOnTouchEventListener * listener);
	virtual CRUIOnTouchEventListener * getOnTouchListener() { return _onTouchListener; }
	virtual CRUIOnClickListener * setOnClickListener(CRUIOnClickListener * listener);
	virtual CRUIOnClickListener * getOnClickListener() { return _onClickListener; }
	virtual CRUIOnLongClickListener * setOnLongClickListener(CRUIOnLongClickListener * listener);
	virtual CRUIOnLongClickListener * getOnLongClickListener() { return _onLongClickListener; }

	virtual lvPoint getTileOffset() const { return lvPoint(); }
	const lString8 & getId() { return _id; }
	CRUIWidget * setId(const lString8 & id) { _id = id; return this; }
	lUInt32 getState() { return _state; }
	lUInt32 getState(lUInt32 mask) { return _state & mask; }
	CRUIWidget * setState(lUInt32 state) { if (_state != state) { _state = state; invalidate(); } return this; }
	CRUIWidget * setState(lUInt32 state, lUInt32 mask) { return setState((_state & ~mask) | (state & mask)); }

	virtual lUInt32 getAlign();
	virtual lUInt32 getHAlign() { return getAlign() & CRUI::ALIGN_MASK_HORIZONTAL; }
	virtual lUInt32 getVAlign() { return getAlign() & CRUI::ALIGN_MASK_VERTICAL; }
	virtual CRUIWidget * setAlign(lUInt32 align) { _align = align; requestLayout(); return this; }

	int getLayoutWidth() { return _layoutWidth; }
	int getLayoutHeight() { return _layoutHeight; }
	CRUIWidget * setLayoutParams(int width, int height) { _layoutWidth = width; _layoutHeight = height; requestLayout(); return this; }

	CRUIWidget * setPadding(int w) { _padding.left = _padding.top = _padding.right = _padding.bottom = w; requestLayout(); return this; }
	CRUIWidget * setMargin(int w) { _margin.left = _margin.top = _margin.right = _margin.bottom = w; requestLayout(); return this; }
	CRUIWidget * setPadding(const lvRect & rc) { _padding = rc; requestLayout(); return this; }
	CRUIWidget * setMargin(const lvRect & rc) { _margin = rc; requestLayout(); return this; }
	CRUIWidget * setMinWidth(int v) { _minWidth = v; requestLayout(); return this; }
	CRUIWidget * setMaxWidth(int v) { _maxWidth = v; requestLayout(); return this; }
	CRUIWidget * setMinHeight(int v) { _minHeight = v; requestLayout(); return this; }
	CRUIWidget * setMaxHeight(int v) { _maxHeight = v; requestLayout(); return this; }

	virtual void getMargin(lvRect & rc);
	virtual void getPadding(lvRect & rc);
	virtual void applyPadding(lvRect & rc);
	virtual void applyMargin(lvRect & rc);
	virtual const lvRect & getPadding();
	virtual const lvRect & getMargin();
	virtual int getMinHeight();
	virtual int getMaxHeight();
	virtual int getMaxWidth();
	virtual int getMinWidth();

	CRUIWidget * setStyle(lString8 styleId) { _styleId = styleId; return this; }
	CRUIStyle * getStyle(bool forState = false);

	virtual CRUIWidget * setText(lString16 text) { return this; }

	virtual CRUIWidget * setFont(LVFontRef font) { _font = font; requestLayout(); return this; }
	virtual CRUIWidget * setTextColor(lUInt32 color) { _textColor = color; requestLayout(); return this; }
	virtual CRUIWidget * setBackground(CRUIImageRef background) { _background = background; requestLayout(); return this; }
	virtual CRUIWidget * setBackground(lUInt32 color) { _background = CRUIImageRef(new CRUISolidFillImage(color)); requestLayout(); return this; }
	virtual CRUIImageRef getBackground();
	virtual LVFontRef getFont();
	virtual lUInt32 getTextColor();




	virtual int getChildCount() { return 0; }
	virtual CRUIWidget * childById(const lString8 & id);
	virtual CRUIWidget * childById(const char * id);
	virtual CRUIWidget * getChild(int index) { return NULL; }
	virtual CRUIWidget * addChild(CRUIWidget * child) { return NULL; }
	virtual CRUIWidget * removeChild(int index) { return NULL; }
	virtual CRUIWidget * setParent(CRUIWidget * parent) { _parent = parent; return this; }
	/// returns parent widget pointer, NULL if it's top level widget
	virtual CRUIWidget * getParent() { return _parent; }



	virtual bool isLayoutRequested() { return _layoutRequested; }
	virtual void requestLayout(bool updateParent = true) {
		_layoutRequested = true;
		if (updateParent && _parent)
			_parent->requestLayout(true);
	}

	virtual bool isDrawRequested() { return _drawRequested; }
	virtual void invalidate() {
		_drawRequested = true;
	}

	/// measure dimensions
	virtual void measure(int baseWidth, int baseHeight);
	/// updates widget position based on specified rectangle
	virtual void layout(int left, int top, int right, int bottom);

	int getMeasuredWidth() { return _measuredWidth; }
	int getMeasuredHeight() { return _measuredHeight; }

	/// draws widget with its children to specified surface
	virtual void draw(LVDrawBuf * buf);
};

/// will set needLayout to true if any widget in tree starting from specified requires layout, set needRedraw if any widget is invalidated
void CRUICheckUpdateOptions(CRUIWidget * widget, bool & needLayout, bool & needRedraw);

#endif /* CRUIWIDGET_H_ */