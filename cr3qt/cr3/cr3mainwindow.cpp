#include "cr3mainwindow.h"
#include <QtCore/QCoreApplication>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLPaintDevice>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QClipboard>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QAuthenticator>
#include <QSslError>
#include <QDesktopServices>
#include <QtSpeech>

#include "gldrawbuf.h"

#define DESKTOP_DPI 120

CRUIQtTextToSpeech::CRUIQtTextToSpeech()
    : _ttsCallback(NULL), _currentVoice(NULL)
    , _defaultVoice(NULL), _isSpeaking(false)
    , _rate(50)
{
    _speechManager = new QtSpeech(this);
    _speechManager->setRate(_rate);
    //const VoiceName & currentVoice = _speechManager->name();
    QtSpeech::VoiceNames voiceList = _speechManager->voices();
    CRLog::debug("TTS Voices available: %d", voiceList.length());
    for (int i = 0; i < voiceList.length(); i++) {
        lString8 id(voiceList[i].id.toUtf8().constData());
        lString8 name(voiceList[i].name.toUtf8().constData());
        lString8 lang(voiceList[i].lang.toUtf8().constData());
        CRLog::debug("TTS Voice: id:'%s'' name:'%s' lang: %s",  id.c_str(), name.c_str(), lang.c_str());
        CRUITextToSpeechVoice * voice = new CRUITextToSpeechVoice(id, name, lang);
        _voices.add(voice);
        if (voiceList[i].id == _speechManager->name().id) {
            _currentVoice = _defaultVoice = voice;
            CRLog::info("Current TTS Voice: '%s' lang: %s", name.c_str(), lang.c_str());
        }
    }
    //if (voiceList.length())
    //    _speechManager->tell(QString("Hello"));
    //tell(lString16(L"Hello"));
}

CRUITextToSpeechCallback * CRUIQtTextToSpeech::getTextToSpeechCallback() {
    return _ttsCallback;
}

void CRUIQtTextToSpeech::setTextToSpeechCallback(CRUITextToSpeechCallback * callback) {
    _ttsCallback = callback;
}

void CRUIQtTextToSpeech::getAvailableVoices(LVPtrVector<CRUITextToSpeechVoice, false> & list) {
    list.clear();
    for(int i = 0; i < _voices.length(); i++)
        list.add(_voices[i]);
}

CRUITextToSpeechVoice * CRUIQtTextToSpeech::getCurrentVoice() {
    return _currentVoice;
}

CRUITextToSpeechVoice * CRUIQtTextToSpeech::getDefaultVoice() {
    return _defaultVoice;
}

class CRUITtsFinishedEvent : public CRRunnable {
    CRUITextToSpeechCallback * _callback;
public:
    CRUITtsFinishedEvent(CRUITextToSpeechCallback * callback)
        : _callback(callback)
    {
        CRLog::trace("CRUITtsFinishedEvent created");
    }

    virtual void run() {
        CRLog::trace("CRUITtsFinishedEvent::run()");
        _callback->onSentenceFinished();
    }
};

bool CRUIQtTextToSpeech::isSpeaking() {
    return _isSpeaking;
}

void CRUIQtTextToSpeech::stop() {
    _isSpeaking = false;
    _speechManager->stop();
}

void CRUIQtTextToSpeech::sentenceFinished() {
    _isSpeaking = false;
    //CRLog::trace("CRUIQtTextToSpeech::sentenceFinished()");
    if (_ttsCallback)
        concurrencyProvider->executeGui(new CRUITtsFinishedEvent(_ttsCallback));
}

bool CRUIQtTextToSpeech::setRate(int rate) {
    if (rate < 0)
        rate = 0;
    else if (rate > 100)
        rate = 100;
    _rate = rate;
    _speechManager->setRate(rate);
    return true;
}

int CRUIQtTextToSpeech::getRate() {
    return _rate;
}

bool CRUIQtTextToSpeech::setCurrentVoice(lString8 id) {
    if (id.empty() && _defaultVoice)
        id = _defaultVoice->getId();
    if (_currentVoice->getId() == id)
        return true;
    stop();
    for (int i = 0; i < _voices.length(); i++) {
        if (_voices[i]->getId() == id) {
            QtSpeech::VoiceName n;
            n.id = QString::fromUtf8(_voices[i]->getId().c_str());
            n.name = QString::fromUtf8(_voices[i]->getName().c_str());
            n.lang = QString::fromUtf8(_voices[i]->getLang().c_str());
            if (_speechManager)
                delete _speechManager;
            _speechManager = new QtSpeech(n, this);
            _speechManager->setRate(_rate);
            _currentVoice = _voices[i];
            return true;
        }
    }
    return false;
}

bool CRUIQtTextToSpeech::canChangeCurrentVoice() {
    return true;
}

bool CRUIQtTextToSpeech::tell(lString16 text) {
    //CRLog::trace("CRUIQtTextToSpeech::tell %s", UnicodeToUtf8(text).c_str());
    if (!_speechManager)
        return false;
    _isSpeaking = true;
    lString8 txt = UnicodeToUtf8(text);
    QString s = QString::fromUtf8(txt.c_str());
    _speechManager->tell(s, this, SLOT(sentenceFinished()));
    return true;
}

CRUIQtTextToSpeech::~CRUIQtTextToSpeech() {
    if (_speechManager)
        delete _speechManager;
}



//! [1]
OpenGLWindow::OpenGLWindow(QWindow *parent)
    : QWindow(parent)
    , m_initialized(false)
    , m_update_pending(false)
    , m_animating(false)
    , m_coverpageManagerPaused(false)
    , m_context(0)
    , m_device(0)
{
    setSurfaceType(QWindow::OpenGLSurface);
    int dx = 800; // 480;
    int dy = 500; // 800;
    resize(QSize(dx, dy));

    //_qtgl = this;

    deviceInfo.setScreenDimensions(dx, dy, DESKTOP_DPI);
    crconfig.setupResourcesForScreenSize();

    _textToSpeech = new CRUIQtTextToSpeech();

    _widget = new CRUIMainWidget(this, this);
    //_widget->setScreenUpdater(this);
    //_widget->setPlatform(this);
    _eventManager = new CRUIEventManager();
    _eventAdapter = new CRUIEventAdapter(_eventManager);
    _eventManager->setRootWidget(_widget);
    _downloadManager = new CRUIHttpTaskManagerQt(_eventManager);
    CRLog::info("Pausing coverpage manager on start");
    CRPauseCoverpageManager();
    m_coverpageManagerPaused = true;
    _fullscreen = false;
}
//! [1]

void OpenGLWindow::restorePositionAndShow() {
    CRPropRef settings = _widget->getSettings();
    int state = settings->getIntDef(PROP_APP_WINDOW_STATE, WINDOW_STATE_NORMAL);
    _fullscreen = settings->getBoolDef(PROP_APP_FULLSCREEN, false);
    if (_fullscreen)
        state = WINDOW_STATE_FULLSCREEN;
    if (state == WINDOW_STATE_MINIMIZED)
        state = WINDOW_STATE_NORMAL;
    int x = settings->getIntDef(PROP_APP_WINDOW_X, -1);
    int y = settings->getIntDef(PROP_APP_WINDOW_Y, -1);
    int width = settings->getIntDef(PROP_APP_WINDOW_WIDTH, -1);
    int height = settings->getIntDef(PROP_APP_WINDOW_HEIGHT, -1);
    if (width > 0 && height > 0 && state == WINDOW_STATE_NORMAL)
        resize(width, height);
    if (x >= 0 && y >= 0 && state == WINDOW_STATE_NORMAL)
        setPosition(x, y);
    if (state == WINDOW_STATE_MAXIMIZED)
        showMaximized();
    else if (state == WINDOW_STATE_FULLSCREEN)
        showFullScreen();
    else
        show();
}

void OpenGLWindow::saveWindowStateAndPosition() {
    if (!_widget || !m_initialized)
        return;
    CRPropRef settings = _widget->getSettings();
    int oldstate = settings->getIntDef(PROP_APP_WINDOW_STATE, WINDOW_STATE_NORMAL);
    int oldx = settings->getIntDef(PROP_APP_WINDOW_X, -1);
    int oldy = settings->getIntDef(PROP_APP_WINDOW_Y, -1);
    int oldwidth = settings->getIntDef(PROP_APP_WINDOW_WIDTH, -1);
    int oldheight = settings->getIntDef(PROP_APP_WINDOW_HEIGHT, -1);
    int newstate = oldstate;
    int newx = oldx;
    int newy = oldy;
    int newwidth = oldwidth;
    int newheight = oldheight;
    switch(visibility()) {
    default:
    case Windowed:
        newstate = WINDOW_STATE_NORMAL;
        newx = x();
        newy = y();
        newwidth = width();
        newheight = height();
        break;
    case Minimized:
        newstate = WINDOW_STATE_MINIMIZED;
        break;
    case Maximized:
        newstate = WINDOW_STATE_MAXIMIZED;
        break;
    case FullScreen:
        newstate = WINDOW_STATE_FULLSCREEN;
        break;
    }
    if (newstate == WINDOW_STATE_MINIMIZED)
        return; // don't save minimized state
    bool fullscreen = settings->getBoolDef(PROP_APP_FULLSCREEN, false);
    if (fullscreen)
        newstate = WINDOW_STATE_FULLSCREEN;

    CRLog::trace("saveWindowStateAndPosition()");

    bool changed = false;
    if (newstate == WINDOW_STATE_NORMAL) {
        if (oldx != newx || oldy != newy || oldwidth != newwidth || oldheight != newheight) {
            settings->setInt(PROP_APP_WINDOW_X, newx);
            settings->setInt(PROP_APP_WINDOW_Y, newy);
            settings->setInt(PROP_APP_WINDOW_WIDTH, newwidth);
            settings->setInt(PROP_APP_WINDOW_HEIGHT, newheight);
            changed = true;
        }
    }
    if (newstate != oldstate) {
        settings->setInt(PROP_APP_WINDOW_STATE, newstate);
        changed = true;
    }
    if (changed)
        _widget->saveSettings();
}

void OpenGLWindow::onMessageReceived(const QString & msg) {
    lString8 fn((const char *)msg.toUtf8().constData());
    CRLog::info("onMessageReceived: %s", fn.c_str());
    if (!LVFileExists(fn)) {
        CRLog::warn("File %s does not exist", fn.c_str());
        return;
    }
    if (_widget)
        _widget->openBookFromFile(fn);
}

void OpenGLWindow::setFileToOpenOnStart(lString8 filename) {
    _widget->setFileToOpenOnStart(filename);
}


OpenGLWindow::~OpenGLWindow()
{
    if (_textToSpeech) {
        delete _textToSpeech;
        _textToSpeech = NULL;
    }
    delete _widget;
    delete _eventAdapter;
    delete _eventManager;
    delete _downloadManager;
//    delete m_device;
    //_qtgl = NULL;
}

CRUITextToSpeech * OpenGLWindow::getTextToSpeech() {
    return _textToSpeech;
}

//! [2]
void OpenGLWindow::render(QPainter *painter)
{
    CR_UNUSED(painter);
}

void OpenGLWindow::initialize()
{
}

void adaptThemeForScreenSize() {
    crconfig.setupResourcesForScreenSize();
}

void OpenGLWindow::wheelEvent(QWheelEvent * event) {
    _eventAdapter->dispatchWheelEvent(event);
    event->accept();
}

void OpenGLWindow::mousePressEvent(QMouseEvent * event) {
    _eventAdapter->dispatchTouchEvent(event);
    event->accept();
    //renderIfChanged();
}

void OpenGLWindow::mouseReleaseEvent(QMouseEvent * event) {
    _eventAdapter->dispatchTouchEvent(event);
    event->accept();
    //renderIfChanged();
}

void OpenGLWindow::mouseMoveEvent(QMouseEvent * event) {
    _eventAdapter->dispatchTouchEvent(event);
    event->accept();
    //renderIfChanged();
}

void OpenGLWindow::keyPressEvent(QKeyEvent * event) {
    _eventAdapter->dispatchKeyEvent(event);
    event->accept();
    //renderIfChanged();
}

void OpenGLWindow::keyReleaseEvent(QKeyEvent * event) {
    _eventAdapter->dispatchKeyEvent(event);
    event->accept();
    //renderIfChanged();
}

void OpenGLWindow::setScreenUpdateMode(bool updateNow, int animationFps) {
    setAnimating(animationFps > 0);
    if (!animationFps && updateNow)
        renderIfChanged();
}

void OpenGLWindow::renderIfChanged()
{
    bool needLayout, needDraw, animating;
    CRUICheckUpdateOptions(_widget, needLayout, needDraw, animating);
    if (animating) {
        setAnimating(true);
        //CRLog::trace("needLayout=%s needDraw=%s animating=true", needLayout ? "true" : "false", needDraw ? "true" : "false");
    } else {
        setAnimating(false);
        if (needLayout || needDraw) {
            //CRLog::trace("needLayout=%s needDraw=%s animating=false", needLayout ? "true" : "false", needDraw ? "true" : "false");
            renderLater();
        }
    }
}

void OpenGLWindow::render()
{
    if (!m_device)
        m_device = new QOpenGLPaintDevice;
    m_initialized = true;
    //CRLog::trace("Render is called");
    glClearColor(1,1,1,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    m_device->setSize(size());

    QSize sz = size();
    if (deviceInfo.isSizeChanged(sz.width(), sz.height())) {
//        int dpi = 72;
//        int minsz = sz.width() < sz.height() ? sz.width() : sz.height();
//        if (minsz >= 320)
//            dpi = 120;
//        if (minsz >= 480)
//            dpi = 160;
//        if (minsz >= 640)
//            dpi = 200;
//        if (minsz >= 800)
//            dpi = 250;
//        if (minsz >= 900)
//            dpi = 300;
//        dpi = 100;
        deviceInfo.setScreenDimensions(sz.width(), sz.height(), DESKTOP_DPI); // dpi
        adaptThemeForScreenSize();
        //CRLog::trace("Layout is needed");
        _widget->onThemeChanged();
    }
    GLDrawBuf buf(sz.width(), sz.height(), 32, false);
    //CRLog::trace("Calling buf.beforeDrawing");
    buf.beforeDrawing();
#if 0
    //TiledGLDrawBuf tiled2(sz.width(), sz.height(), 32, 256, 256);
    TiledGLDrawBuf tiled(sz.width(), sz.height(), 32, 256, 256);
    //GLDrawBuf tiled(sz.width(), sz.height(), 32, true);

    //LVDrawBuf * p = &buf;
    LVDrawBuf * p = &tiled;
    if (p != &buf)
        p->beforeDrawing();
#if 0
//    buf.FillRect(0, 0, sz.width(), sz.height(), 0x80F080);
//    buf.FillRect(2, 2, sz.width()-2, sz.height()-2, 0x8080FF);
    //{
    //TiledGLDrawBuf tiled(sz.width(), sz.height(), 32, 1024, 1024);
    //GLDrawBuf tiled(sz.width(), sz.height(), 32, true);

      p->GradientRect(0, 0, sz.width(), sz.height(), 0xFF0000, 0x00FF00, 0x0000FF, 0xFF00FF);

    LVImageSourceRef icon1 = resourceResolver->getImageSource("add_file");
    LVImageSourceRef icon2 = resourceResolver->getImageSource("add_folder");
//    p->FillRect(10, 10, sz.width() - 10, sz.height() - 10, 0xFF0000);
//    p->FillRect(12, 12, sz.width() - 12, sz.height() - 12, 0x00FF00);
      p->FillRect(14, 14, 16, 16, 0x0000FF);
      p->FillRect(sz.width() / 4, sz.height() * 1 / 4, sz.width() * 3 / 4, sz.height() * 3 / 4,0x80FF80);
      LVFontRef font = currentTheme->getFont();
      font->DrawTextString(p, 240 + 256, 245, L"Test", 4, '?');

//        GLDrawBuf img0(100, 100, 32, true);
//        img0.beforeDrawing();
//        img0.FillRect(10, 10, 90, 90, 0x0000FF);
//        img0.afterDrawing();
//        img0.DrawTo(&tiled, 0, 0, 0, NULL);

    p->Draw(icon1, 20, 20, icon1->GetWidth(), icon1->GetHeight(), false);
    p->Draw(icon1, 240, 20, icon1->GetWidth(), icon1->GetHeight(), false);

    p->Draw(icon2, 240, 240, icon1->GetWidth() * 2, icon1->GetHeight() * 2, false);

    //buf.Draw(icon2, 240 + 256, 240, 64, 64, false);
#else
    bool needLayout, needDraw, animating;
    //CRLog::trace("Checking if draw is required");
    CRUICheckUpdateOptions(_widget, needLayout, needDraw, animating);
    _widget->invalidate();
    if (needLayout) {
        //CRLog::trace("need layout");
        _widget->measure(sz.width(), sz.height());
        _widget->layout(0, 0, sz.width(), sz.height());
        //CRLog::trace("done layout");
    }
    needDraw = true;
    if (needDraw) {
        //CRLog::trace("need draw");
        _widget->draw(p);
        //CRLog::trace("done draw");
    }
#endif
    if (p != &buf)
        p->afterDrawing();
//    tiled2.beforeDrawing();
////    for (int x = 0; x < buf.GetWidth(); x += 64)
////        tiled2.DrawFragment(&tiled, x, 0, 64, buf.GetHeight(), x, 0, 64, buf.GetHeight(), 0);
//    tiled2.DrawFragment(&tiled, 0, 0, buf.GetWidth(), buf.GetHeight(), 0, 0, buf.GetWidth(), buf.GetHeight(), 0);
//    tiled2.afterDrawing();
    if (p != &buf)
        buf.DrawFragment(p, 0, 0, buf.GetWidth(), buf.GetHeight(), 0, 0, buf.GetWidth(), buf.GetHeight(), 0);
    //tiled2.DrawTo(&buf, 0, 0, 0, NULL);
    //tiled.DrawTo(&buf, 0, 0, 0, NULL);
//}
    buf.FillRect(256, 0, 257, 512, 0x80FF8080);
    buf.FillRect(512, 0, 513, 512, 0x80FF8080);
    buf.FillRect(0, 256, 1024, 257, 0x80FF8080);
#else
    bool needLayout, needDraw, animating;
    //CRLog::trace("Checking if draw is required");
    CRUICheckUpdateOptions(_widget, needLayout, needDraw, animating);
    _widget->invalidate();
    if (needLayout) {
        //CRLog::trace("need layout");
        _widget->measure(sz.width(), sz.height());
        _widget->layout(0, 0, sz.width(), sz.height());
        //CRLog::trace("done layout");
    }
    needDraw = true;
    if (needDraw) {
        //CRLog::trace("need draw");
        _widget->draw(&buf);
        //CRLog::trace("done draw");
    }
#endif
    //CRLog::trace("Calling buf.afterDrawing");
    buf.afterDrawing();
    //CRLog::trace("Finished buf.afterDrawing");

//    QPainter painter(m_device);
//    render(&painter);
}
//! [2]

//! [3]
void OpenGLWindow::renderLater()
{
    if (!m_update_pending) {
        m_update_pending = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}

bool OpenGLWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest:
        m_update_pending = false;
        renderNow();
        return true;
    default:
        return QWindow::event(event);
    }
}

void OpenGLWindow::exposeEvent(QExposeEvent *event)
{
    Q_UNUSED(event);

    if (isExposed())
        renderNow();
}
//! [3]

void OpenGLWindow::resizeEvent(QResizeEvent * event) {
    QWindow::resizeEvent(event);
    CRLog::trace("OpenGLWindow::resizeEvent");
    saveWindowStateAndPosition();
}

void OpenGLWindow::moveEvent(QMoveEvent * event) {
    QWindow::moveEvent(event);
    CRLog::trace("OpenGLWindow::moveEvent");
    saveWindowStateAndPosition();
}

void OpenGLWindow::showEvent(QShowEvent * event) {
    QWindow::showEvent(event);
    //saveWindowStateAndPosition();
}

//! [4]
void OpenGLWindow::renderNow()
{
    if (!isExposed())
        return;

    CRLog::trace("OpenGLWindow::renderNow()");

    bool needsInitialize = false;

    if (!m_context) {
        m_context = new QOpenGLContext(this);
        m_context->setFormat(requestedFormat());
        m_context->create();

        needsInitialize = true;
    }

    m_context->makeCurrent(this);

    if (needsInitialize) {
        initializeOpenGLFunctions();
        initialize();
    }

    render();

    m_context->swapBuffers(this);

    if (m_animating)
        renderLater();
    if (m_coverpageManagerPaused) {
        CRLog::info("Resuming coverpage manager after draw");
        CRResumeCoverpageManager();
        m_coverpageManagerPaused = false;
    }
}
//! [4]

//! [5]
void OpenGLWindow::setAnimating(bool animating)
{
    m_animating = animating;

    if (animating)
        renderLater();
}
//! [5]

/// minimize app or show Home Screen
void OpenGLWindow::minimizeApp() {

}

void OpenGLWindow::exitApp() {
    QApplication::exit();
}

/// override to open URL in external browser; returns false if failed or feature not supported by platform
bool OpenGLWindow::openLinkInExternalBrowser(lString8 url) {
    CRLog::trace("openLinkInExternalBrowser(%s)", url.c_str());
    QString link = QString::fromUtf8(url.c_str());
    QUrl linkurl(link);
    return QDesktopServices::openUrl(linkurl);
}

/// override to open file in external application; returns false if failed or feature not supported by platform
bool OpenGLWindow::openFileInExternalApp(lString8 filename, lString8 mimeType) {
    CR_UNUSED(mimeType);
    filename = lString8("file:///") + filename;
    CRLog::trace("openFileInExternalApp(%s)", filename.c_str());
    QString link = QString::fromUtf8(filename.c_str());
    QUrl linkurl(link);
    return QDesktopServices::openUrl(linkurl);
}

// copy text to clipboard
void OpenGLWindow::copyToClipboard(lString16 text) {
    QClipboard *clipboard = QApplication::clipboard();
    QString txt(UnicodeToUtf8(text).c_str());
    clipboard->setText(txt);
}


bool OpenGLWindow::isFullscreen() {
    return _fullscreen;
}

void OpenGLWindow::setFullscreen(bool fullscreen) {
    if (_fullscreen == fullscreen)
        return;
    _fullscreen = fullscreen;
    CRLog::trace("OpenGLWindow::setFullscreen(%s)", fullscreen ? "true" : "false");
    //CRLog::trace("OpenGLWindow::setFullscreen - hiding");
    //hide();
    CRPauseCoverpageManager();
    m_coverpageManagerPaused = true;
    crconfig.clearGraphicsCaches();
//    if (m_context)
//        delete m_context;
//    m_context = NULL;
    if (_fullscreen) {
        CRLog::trace("OpenGLWindow::setFullscreen - showing fullscreen");
        setWindowState(Qt::WindowFullScreen); // windowState() ^ (Qt::WindowStats)Qt::WindowFullScreen );
    } else {
        CRLog::trace("OpenGLWindow::setFullscreen - showing windowed");
        setWindowState(Qt::WindowNoState);
        //setWindowState( windowState() ^ (Qt::WindowState)Qt::WindowFullScreen );
    }
}



/// returns 0 if not supported, task ID if download task is started
int OpenGLWindow::openUrl(lString8 url, lString8 method, lString8 login, lString8 password, lString8 saveAs) {
    CRLog::info("openUrl(%s %s) -> %s", url.c_str(), method.c_str(), saveAs.c_str());
    return _downloadManager->openUrl(url, method, login, password, saveAs);
}

/// cancel specified download task
void OpenGLWindow::cancelDownload(int downloadTaskId) {
    _downloadManager->cancelDownload(downloadTaskId);
}

CRUIHttpTaskQt::~CRUIHttpTaskQt() {
    CRLog::trace("CRUIHttpTaskQt::~CRUIHttpTaskQt()");
}

/// override if you want do main work inside task instead of inside CRUIHttpTaskManagerBase::executeTask
void CRUIHttpTaskQt::doDownload() {
    url.setUrl(QString::fromUtf8(_url.c_str()));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(QString("CoolReader/3.3 (Qt)")));
    reply = qnam->get(request);
    connect(reply, SIGNAL(finished()),
            this, SLOT(httpFinished()));
    connect(reply, SIGNAL(readyRead()),
            this, SLOT(httpReadyRead()));
//    connect(reply, SIGNAL(error()),
//            this, SLOT(httpError()));
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
            this, SLOT(updateDataReadProgress(qint64,qint64)));
}

void CRUIHttpTaskQt::httpError(QNetworkReply::NetworkError code) {
    QNetworkReply::NetworkError error = code;
    _result = error;
    _resultMessage = reply->errorString().toUtf8().constData();
    CRLog::warn("httpError(result=%d resultMessage=%s url='%s')", _result, _result ? _resultMessage.c_str() : "", _url.c_str());
}

lString8 extractHostUrl(lString8 url) {
    int i = 0;
    if (url.startsWith("http://"))
        i = 7;
    else if (url.startsWith("https://"))
        i = 8;
    for (; i < url.length() && url[i] != '?' && url[i] != '/'; i++) {
    }
    return url.substr(0, i) + "/";
}

lString8 extractPathUrl(lString8 url) {
    int i = 0;
    if (url.startsWith("http://"))
        i = 7;
    else if (url.startsWith("https://"))
        i = 8;
    for (; i < url.length() && url[i] != '?'; i++) {
    }
    url = url.substr(0, i);
    if (!url.endsWith("/"))
        url += '/';
    return url;
}

lString8 makeRedirectUrl(lString8 from, lString8 to) {
    if (to.startsWith("http://") || to.startsWith("https://"))
        return to;
    if (to.startsWith("./"))
        to = to.substr(1); // ebooks libres et graduits
    if (to.startsWith("/")) {
        lString8 serverurl = extractHostUrl(from);
        return serverurl + to.substr(1);
    }
    if (to.startsWith("./"))
        to = to.substr(2);
    from = extractPathUrl(from);
    return from + to;
}

void CRUIHttpTaskQt::httpFinished() {
    //CRLog::trace("CRUIHttpTaskQt::httpFinished()");
     QNetworkReply::NetworkError error = reply->error();
    _result = error;
    _resultMessage = reply->errorString().toUtf8().constData();

    QVariant possibleRedirectUrl =
             reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    QUrl redirectUrl = possibleRedirectUrl.toUrl();
    if (!redirectUrl.isEmpty()) {
        lString8 redir(redirectUrl.toString().toUtf8().constData());
        CRLog::warn("Redirection to %s", redir.c_str());
        if (redirectCount < 3 && canRedirect(redir)) {
            redirectCount++;
            _url = makeRedirectUrl(_url, redir);
            CRLog::warn("New URL: %s", _url.c_str());
            reply->deleteLater();
            doDownload();
            return;
        } else {
            _result = 1;
            _resultMessage = "Too many redirections";
        }
    }

    QVariant contentMimeType = reply->header(QNetworkRequest::ContentTypeHeader);
    QString contentTypeString;
    if (contentMimeType.isValid())
        contentTypeString = contentMimeType.toString();
    _mimeType = contentTypeString.toUtf8().constData();
//    if (!_result) {
//        QByteArray data = reply->readAll();
//        if (data.length()) {
//            CRLog::trace("readAll() in httpFinished returned %d bytes", data.length());
//            dataReceived((const lUInt8 *)data.constData(), data.length());
//        }
//    }
    if (!_stream.isNull())
        _stream->SetPos(0);
    CRLog::debug("httpFinished(result=%d resultMessage=%s mimeType=%s url='%s')", _result, _result ? _resultMessage.c_str() : "", _mimeType.c_str(), _url.c_str());
    _taskManager->onTaskFinished(this);
    reply->deleteLater();
    deleteLater();
}

void CRUIHttpTaskQt::httpReadyRead() {
    if (!_size) {
    //reply->a
        QVariant contentLength = reply->header(QNetworkRequest::ContentLengthHeader);
        bool ok = false;
        int len = contentLength.toInt(&ok);
        if (ok)
            _size = len;
    }
    //CRLog::trace("httpReadyRead(total size = %d)", _size);
    QByteArray data = reply->readAll();
    dataReceived((const lUInt8 *)data.constData(), data.length());
}

void CRUIHttpTaskQt::updateDataReadProgress(qint64 bytesRead, qint64 totalBytes) {
    CR_UNUSED2(bytesRead, totalBytes);
    // progress
    _size = (int)totalBytes;
    _sizeDownloaded = (int)bytesRead;
    if (_size > 0 && _result == 0)
        _taskManager->onTaskProgress(this);
}

CRUIHttpTaskManagerQt::CRUIHttpTaskManagerQt(CRUIEventManager * eventManager) : CRUIHttpTaskManagerBase(eventManager, DOWNLOAD_THREADS) {
    connect(&qnam, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
            this, SLOT(slotAuthenticationRequired(QNetworkReply*,QAuthenticator*)));
}

//    void enableDownloadButton();
void CRUIHttpTaskManagerQt::slotAuthenticationRequired(QNetworkReply* reply,QAuthenticator * authenticator) {
    CRLog::trace("slotAuthenticationRequired()");
    for (LVHashTable<lUInt32, CRUIHttpTaskBase*>::iterator i = _activeTasks.forwardIterator(); ;) {
        LVHashTable<lUInt32, CRUIHttpTaskBase*>::pair * item = i.next();
        if (!item)
            break;
        CRUIHttpTaskQt * task = (CRUIHttpTaskQt *)item->value;
        if (task->reply == reply) {
            if (!task->_login.empty()) {
                authenticator->setUser(QString::fromUtf8(task->_login.c_str()));
                authenticator->setPassword(QString::fromUtf8(task->_password.c_str()));
            }
            return;
        }
    }
}

#ifndef QT_NO_OPENSSL
void CRUIHttpTaskQt::sslErrors(QNetworkReply*,const QList<QSslError> &errors) {
    CRLog::trace("sslErrors()");
    QString errorString;
    foreach (const QSslError &error, errors) {
        if (!errorString.isEmpty())
            errorString += ", ";
        errorString += error.errorString();
    }
    CRLog::error("SSL Errors, ignoring: %s", errorString.toUtf8().constData());
    reply->ignoreSslErrors();
}

#endif

