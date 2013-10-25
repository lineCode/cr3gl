#include "CR3Renderer.h"
#include "gldrawbuf.h"
#include "glfont.h"
#include <crengine.h>
#include "crui.h"
#include "fileinfo.h"
#include "cruimain.h"
#include "cruiwidget.h"
#include "CoolReader.h"

using namespace CRUI;

const GLfloat ONEP = GLfloat(+1.0f);
const GLfloat ONEN = GLfloat(-1.0f);
const GLfloat ZERO = GLfloat( 0.0f);

CR3Renderer::CR3Renderer(CoolReaderApp * app)
	: _app(app)
	, _backbuffer(NULL)
	//, _updateRequested(false)
	, __controlWidth(0)
	, __controlHeight(0)
	, __angle(0)
	, __player(NULL)
	, __playerStarted(true)
	, __playerDrawOnce(0)
{
	_eventManager = new CRUIEventManager();
	_eventAdapter = new CRUIEventAdapter(_eventManager);
	_widget = new CRUIMainWidget();
	_eventManager->setRootWidget(_widget);
	_widget->setScreenUpdater(this);
	_widget->setPlatform(this);
}

CR3Renderer::~CR3Renderer(void)
{

}

bool
CR3Renderer::InitializeGl(void)
{
	// TODO:
	// Initialize GL status. 

	glShadeModel(GL_SMOOTH);
	glViewport(0, 0, GetTargetControlWidth(), GetTargetControlHeight());
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrthof(0, GetTargetControlWidth(), GetTargetControlHeight(), 0, -1.0f, 1.0f);
	glClearColor(1, 1, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	return true;
}

bool
CR3Renderer::TerminateGl(void)
{
	// TODO:
	// Terminate and reset GL status. 
	return true;
}

#define USE_BACKBUFFER 1
bool
CR3Renderer::Draw(void)
{
	CRLog::debug("CR3Renderer::Draw is called");
//	if (__playerDrawOnce > 0) {
//		__playerDrawOnce--;
//		if (__playerDrawOnce == 1 && __playerStarted) {
//			CRLog::debug("CR3Renderer::Draw stopping player");
//			__playerStarted = false;
//			__player->Pause();
//			return true;
//		}
//	}

	//_updateRequested = false;

	glShadeModel(GL_SMOOTH);
	glViewport(0, 0, GetTargetControlWidth(), GetTargetControlHeight());
	glEnable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrthof(0, GetTargetControlWidth(), GetTargetControlHeight(), 0, -1.0f, 1.0f);
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	GLDrawBuf buf(GetTargetControlWidth(), GetTargetControlHeight(), 32, false);
#if USE_BACKBUFFER == 1
	if (!_backbuffer) {
		_backbuffer = new GLDrawBuf(GetTargetControlWidth(), GetTargetControlHeight(), 32, true);
	} else if (_backbuffer->GetWidth() != GetTargetControlWidth() || _backbuffer->GetHeight() != GetTargetControlHeight()) {
		delete _backbuffer;
		_backbuffer = new GLDrawBuf(GetTargetControlWidth(), GetTargetControlHeight(), 32, true);
	}

	_backbuffer->beforeDrawing();
#endif

	lvRect pos = _widget->getPos();
	bool sizeChanged = false;
	if (pos.width() != __controlWidth || pos.height() != __controlHeight) {
		sizeChanged = true;
	}

	bool needLayout, needDraw, animating;
	CRUICheckUpdateOptions(_widget, needLayout, needDraw, animating);
	_widget->invalidate();
	if (needLayout || sizeChanged) {
		//CRLog::trace("need layout");
		_widget->measure(__controlWidth, __controlHeight);
		_widget->layout(0, 0, _widget->getMeasuredWidth(), _widget->getMeasuredHeight());
		needDraw = true;
	}
	if (needDraw) {
		//CRLog::trace("need draw");
#if USE_BACKBUFFER == 1
		_widget->draw(_backbuffer);
#else
		buf.beforeDrawing();
		_widget->draw(&buf);
#endif
	}
#if USE_BACKBUFFER == 1
	_backbuffer->afterDrawing();

	buf.beforeDrawing();
	_backbuffer->DrawTo(&buf, 0, 0, 0, NULL);
#endif
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glFlush();

	buf.afterDrawing();


	//CRLog::debug("CR3Renderer::Draw exiting");
	return true;
}

bool
CR3Renderer::Pause(void)
{
	// TODO:
	// Do something necessary when Plyaer is paused. 

	return true;
}

bool
CR3Renderer::Resume(void)
{
	// TODO:
	// Do something necessary when Plyaer is resumed. 

	return true;
}

int
CR3Renderer::GetTargetControlWidth(void)
{
	return __controlWidth;
}

int
CR3Renderer::GetTargetControlHeight(void)
{
	return __controlHeight;
}

void
CR3Renderer::SetTargetControlWidth(int width)
{
	__controlWidth = width;
}

void
CR3Renderer::SetTargetControlHeight(int height)
{
	__controlHeight = height;
}

/// CRUIScreenUpdateManagerCallback implementation - exit application
void CR3Renderer::exitApp() {
	_app->Terminate();
}

/// minimize app or show Home Screen
void CR3Renderer::minimizeApp() {
	/// TODO: just hide
	_app->Terminate();

}


/// set animation fps (0 to disable) and/or update screen instantly
void CR3Renderer::setScreenUpdateMode(bool updateNow, int animationFps) {
	CRLog::trace("setScreenUpdateMode(%s, %d fps)", (updateNow ? "update now" : "no update"), animationFps);
	if ((animationFps != 0) != __playerStarted) {
		if (!__playerStarted) {
			__player->SetFps(30);
			__player->Resume();
			__playerStarted = true;
		} else {
			__player->SetFps(-30);
			__player->Pause();
			__playerStarted = false;
		}
	}
	if (!__playerStarted && updateNow) {
		__player->Redraw();
	}
//	if (!animationFps) {
////		if (__playerDrawOnce < 2)
////			__playerDrawOnce = 2;
////		if (__playerStarted) {
//////			__playerDrawOnce = true;
////			//CRLog::trace("Pausing player");
//////			__player->Pause();
//////			__playerStarted = false;
////		}
//		if (updateNow) {
//			__playerDrawOnce = 3;
//			if (!__playerStarted) {
//				__playerStarted = true;
//				__player->SetFps(30);//);
//				__player->Resume();
//				__player->Redraw();
//			}
////			//CRLog::trace("Updating player");
////			if (!_updateRequested) {
////				_updateRequested = true;
////				__player->Redraw();
////			}
//		}
//	} else {
//		__playerDrawOnce = 0;
//		if (!__playerStarted) {
//			//CRLog::trace("Resuming player");
//			__playerStarted = true;
//			__player->SetFps(30);//);
//			__player->Resume();
//		}
//	}
}
