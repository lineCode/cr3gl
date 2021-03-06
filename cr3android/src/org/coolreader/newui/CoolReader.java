package org.coolreader.newui;

import java.io.File;
import java.io.FileInputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;

import org.coolreader.Dictionaries;
import org.coolreader.Dictionaries.DictInstallationInfo;
import org.coolreader.Dictionaries.DictionaryException;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.PowerManager;
import android.text.ClipboardManager;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.view.inputmethod.InputMethodManager;

@SuppressWarnings("deprecation")
public class CoolReader extends Activity {
	
	public CRView crview;
	private View mDecorView;
	public static final String TAG = "cr3";
	public static final Logger log = L.create(TAG);

	private ClipboardManager clipboardManager;
	private InputMethodManager inputMethodManager;
	BroadcastReceiver intentReceiver;
	private Dictionaries dicts;
	
	private CRTTS tts;
	
	private DownloadManager downloadManager;
	
	public DownloadManager getDownloadManager() {
		return downloadManager;
	}

	public final void copyToClipboard(String s) {
		if (clipboardManager != null)
			clipboardManager.setText(s);
	}
	
	public final static int SCREEN_ORIENTATION_SYSTEM = 0;
	public final static int SCREEN_ORIENTATION_SENSOR = 1;
	public final static int SCREEN_ORIENTATION_0 = 2;
	public final static int SCREEN_ORIENTATION_90 = 3;
	public final static int SCREEN_ORIENTATION_180 = 4;
	public final static int SCREEN_ORIENTATION_270 = 5;
	public final static int SCREEN_ORIENTATION_PORTRAIT = 6;
	public final static int SCREEN_ORIENTATION_LANDSCAPE = 7;
	
	private int _screenOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
	// support pre API LEVEL 9
	final static public int ActivityInfo_SCREEN_ORIENTATION_SENSOR_PORTRAIT = 7;
	final static public int ActivityInfo_SCREEN_ORIENTATION_SENSOR_LANDSCAPE = 6;
	final static public int ActivityInfo_SCREEN_ORIENTATION_REVERSE_PORTRAIT = 9;
	final static public int ActivityInfo_SCREEN_ORIENTATION_REVERSE_LANDSCAPE = 8;
	final static public int ActivityInfo_SCREEN_ORIENTATION_FULL_SENSOR = 10;
	
	private static int toAndroidOrientation(int orient) {
		boolean level9 = DeviceInfo.getSDKLevel() >= 9;
		switch(orient) {
		default:
		case SCREEN_ORIENTATION_SYSTEM:
			return ActivityInfo.SCREEN_ORIENTATION_USER;
		case SCREEN_ORIENTATION_SENSOR:
			return level9 ? ActivityInfo_SCREEN_ORIENTATION_FULL_SENSOR : ActivityInfo.SCREEN_ORIENTATION_SENSOR;
		case SCREEN_ORIENTATION_0:
			return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
		case SCREEN_ORIENTATION_90:
			return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
		case SCREEN_ORIENTATION_180:
			return level9 ? ActivityInfo_SCREEN_ORIENTATION_REVERSE_PORTRAIT : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
		case SCREEN_ORIENTATION_270:
			return level9 ? ActivityInfo_SCREEN_ORIENTATION_REVERSE_LANDSCAPE : ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
		case SCREEN_ORIENTATION_PORTRAIT:
			return level9 ? ActivityInfo_SCREEN_ORIENTATION_SENSOR_PORTRAIT : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
		case SCREEN_ORIENTATION_LANDSCAPE:
			return level9 ? ActivityInfo_SCREEN_ORIENTATION_SENSOR_LANDSCAPE : ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
		}
	}
	
	public final void setScreenOrientation(int v) {
		int orient = toAndroidOrientation(v);
		if (_screenOrientation == orient)
			return;
		log.d("setScreenOrientation " + v + " (android:" + orient + ")");
		_screenOrientation = orient;
		setRequestedOrientation(_screenOrientation);
		applyScreenOrientation(getWindow());
	}

	public void applyScreenOrientation( Window wnd )
	{
		if ( wnd!=null ) {
			WindowManager.LayoutParams attrs = wnd.getAttributes();
			attrs.screenOrientation = _screenOrientation;
			wnd.setAttributes(attrs);
			if (DeviceInfo.EINK_SCREEN){
				//TODO:
				//EinkScreen.ResetController(mReaderView);
			}
			
		}
	}

    private boolean mIsStarted = false;
    private boolean mPaused = false;
	
	public boolean isStarted() { return mIsStarted; }
	
	public boolean isWakeLockEnabled() {
		return screenBacklightDuration > 0;
	}

	/**
	 * @param backlightDurationMinutes 0 = system default, 1 == 3 minutes, 2..5 == 2..5 minutes
	 */
	public void setScreenBacklightDuration(int backlightDurationMinutes)
	{
		if (backlightDurationMinutes == 1)
			backlightDurationMinutes = 3;
		if (screenBacklightDuration != backlightDurationMinutes * 60 * 1000) {
			screenBacklightDuration = backlightDurationMinutes * 60 * 1000;
			if (screenBacklightDuration == 0)
				backlightControl.release();
			else
				backlightControl.onUserActivity();
		}
	}

    private final static int MIN_BACKLIGHT_LEVEL_PERCENT = DeviceInfo.MIN_SCREEN_BRIGHTNESS_PERCENT;
    
    protected void setDimmingAlpha(int alpha) {
    	// TODO
    }
    
    protected boolean allowLowBrightness() {
    	// override to force higher brightness in non-reading mode (to avoid black screen on some devices when brightness level set to small value)
    	// TODO
    	return true;
    }
    
    private final static int MIN_BRIGHTNESS_IN_BROWSER = 12;
    public void onUserActivity()
    {
    	if (backlightControl != null)
      	    backlightControl.onUserActivity();
    	// Hack
    	//if ( backlightControl.isHeld() )
    	crview.post(new Runnable() {
			@Override
			public void run() {
				try {
		        	float b;
		        	int dimmingAlpha = 255;
		        	// screenBacklightBrightness is 0..100
		        	if (screenBacklightBrightness >= 0) {
		        		int percent = screenBacklightBrightness;
		        		if (!allowLowBrightness() && percent < MIN_BRIGHTNESS_IN_BROWSER)
		        			percent = MIN_BRIGHTNESS_IN_BROWSER;
	        			float minb = MIN_BACKLIGHT_LEVEL_PERCENT / 100.0f; 
		        		if ( percent >= 10 ) {
		        			// real brightness control, no colors dimming
		        			b = (percent - 10) / (100.0f - 10.0f); // 0..1
		        			b = minb + b * (1-minb); // minb..1
				        	if (b < minb) // BRIGHTNESS_OVERRIDE_OFF
				        		b = minb;
				        	else if (b > 1.0f)
				        		b = 1.0f; //BRIGHTNESS_OVERRIDE_FULL
		        		} else {
			        		// minimal brightness with colors dimming
			        		b = minb;
			        		dimmingAlpha = 255 - (11-percent) * 180 / 10; 
		        		}
		        	} else {
		        		// system
		        		b = -1.0f; //BRIGHTNESS_OVERRIDE_NONE
		        	}
		        	setDimmingAlpha(dimmingAlpha);
			    	//log.v("Brightness: " + b + ", dim: " + dimmingAlpha);
			    	updateBacklightBrightness(b);
			    	updateButtonsBrightness(keyBacklightOff ? 0.0f : -1.0f);
				} catch ( Exception e ) {
					// ignore
				}
			}
    	});
    }
	
	private Runnable backlightTimerTask = null;
	private static long lastUserActivityTime;
	public static final int DEF_SCREEN_BACKLIGHT_TIMER_INTERVAL = 3 * 60 * 1000;
	private int screenBacklightDuration = DEF_SCREEN_BACKLIGHT_TIMER_INTERVAL;
	private class ScreenBacklightControl {
		PowerManager.WakeLock wl = null;

		public ScreenBacklightControl() {
		}

		long lastUpdateTimeStamp;
		
		public void onUserActivity() {
			lastUserActivityTime = Utils.timeStamp();
			if (Utils.timeInterval(lastUpdateTimeStamp) < 5000)
				return;
			lastUpdateTimeStamp = android.os.SystemClock.uptimeMillis();
			if (!isWakeLockEnabled())
				return;
			if (wl == null) {
				PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
				wl = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK
				/* | PowerManager.ON_AFTER_RELEASE */, "cr3");
				log.d("ScreenBacklightControl: WakeLock created");
			}
			if (!isStarted()) {
				log.d("ScreenBacklightControl: user activity while not started");
				release();
				return;
			}

			if (!isHeld()) {
				log.d("ScreenBacklightControl: acquiring WakeLock");
				wl.acquire();
			}

			if (backlightTimerTask == null) {
				log.v("ScreenBacklightControl: timer task started");
				backlightTimerTask = new BacklightTimerTask();
				crview.postDelayed(backlightTimerTask,
						screenBacklightDuration / 10);
			}
		}

		public boolean isHeld() {
			return wl != null && wl.isHeld();
		}

		public void release() {
			if (wl != null && wl.isHeld()) {
				log.d("ScreenBacklightControl: wl.release()");
				wl.release();
			}
			backlightTimerTask = null;
			lastUpdateTimeStamp = 0;
		}

		private class BacklightTimerTask implements Runnable {

			@Override
			public void run() {
				if (backlightTimerTask == null)
					return;
				long interval = Utils.timeInterval(lastUserActivityTime);
//				log.v("ScreenBacklightControl: timer task, lastActivityMillis = "
//						+ interval);
				int nextTimerInterval = screenBacklightDuration / 20;
				boolean dim = false;
				if (interval > screenBacklightDuration * 8 / 10) {
					nextTimerInterval = nextTimerInterval / 8;
					dim = true;
				}
				if (interval > screenBacklightDuration) {
					log.v("ScreenBacklightControl: interval is expired");
					release();
				} else {
					crview.postDelayed(backlightTimerTask, nextTimerInterval);
					if (dim) {
						updateBacklightBrightness(-0.9f); // reduce by 9%
					}
				}
			}

		};

	}

    private void turnOffKeyBacklight() {
    	if (!isStarted())
    		return;
		if (DeviceInfo.getSDKLevel() >= DeviceInfo.HONEYCOMB) {
			setKeyBacklight(0);
		}
    	// repeat again in short interval
    	if (!setKeyBacklightUsingHack(0)) {
    		//log.w("Cannot control key backlight directly");
    		return;
    	}
    	// repeat again in short interval
    	Runnable task = new Runnable() {
			@Override
			public void run() {
		    	if (!isStarted())
		    		return;
		    	if (!setKeyBacklightUsingHack(0)) {
		    		//log.w("Cannot control key backlight directly (delayed)");
		    	}
			}
		};
		crview.postDelayed(task, 1);
		//BackgroundThread.instance().postGUI(task, 10);
    }
    
    private void updateBacklightBrightness(float b) {
        Window wnd = getWindow();
        if (wnd != null) {
	    	LayoutParams attrs =  wnd.getAttributes();
	    	boolean changed = false;
	    	if (b < 0 && b > -0.99999f) {
	    		//log.d("dimming screen by " + (int)((1 + b)*100) + "%");
	    		b = -b * attrs.screenBrightness;
	    		if (b < 0.15)
	    			return;
	    	}
	    	float delta = attrs.screenBrightness - b;
	    	if (delta < 0)
	    		delta = -delta;
	    	if (delta > 0.01) {
	    		attrs.screenBrightness = b;
	    		changed = true;
	    	}
	    	if ( changed ) {
	    		log.d("Window attribute changed: " + attrs);
	    		wnd.setAttributes(attrs);
	    	}
        }
    }

    
    private boolean setKeyBacklightUsingHack(int value) {
    	// TODO
    	return false;
    }
    
	private int currentKeyBacklightLevel = 1;
	public int getKeyBacklight() {
		return currentKeyBacklightLevel;
	}
	public boolean setKeyBacklight(int value) {
		currentKeyBacklightLevel = value;
		// Try ICS way
		if (DeviceInfo.getSDKLevel() >= DeviceInfo.HONEYCOMB) {
			setSystemUiVisibility();
		}
		// thread safe
		return setKeyBacklightUsingHack(value);
	}
    
    private boolean keyBacklightOff = true;
    public boolean isKeyBacklightDisabled() {
    	return keyBacklightOff;
    }
    
    public void setKeyBacklightDisabled(boolean disabled) {
    	keyBacklightOff = disabled;
    	onUserActivity();
    }
    
    public void setScreenBacklightLevel( int percent )
    {
    	if ( percent<-1 )
    		percent = -1;
    	else if ( percent>100 )
    		percent = -1;
    	screenBacklightBrightness = percent;
    	onUserActivity();
    }
    
    private int screenBacklightBrightness = -1; // use default
    
    private static boolean brightnessHackError = false;
    private void updateButtonsBrightness(float buttonBrightness) {
        Window wnd = getWindow();
        if (wnd != null) {
	    	LayoutParams attrs =  wnd.getAttributes();
	    	boolean changed = false;
	    	// hack to set buttonBrightness field
	    	//float buttonBrightness = keyBacklightOff ? 0.0f : -1.0f;
	    	if (!brightnessHackError)
	    	try {
	        	Field bb = attrs.getClass().getField("buttonBrightness");
	        	if (bb != null) {
	        		Float oldValue = (Float)bb.get(attrs);
	        		if (oldValue == null || oldValue.floatValue() != buttonBrightness) {
	        			bb.set(attrs, buttonBrightness);
		        		changed = true;
	        		}
	        	}
	    	} catch ( Exception e ) {
	    		log.e("WindowManager.LayoutParams.buttonBrightness field is not found, cannot turn buttons backlight off");
	    		brightnessHackError = true;
	    	}
	    	//attrs.buttonBrightness = 0;
	    	if (changed) {
	    		log.d("Window attribute changed: " + attrs);
	    		wnd.setAttributes(attrs);
	    	}
	    	if (keyBacklightOff)
	    		turnOffKeyBacklight();
        }
    }
	
	public void releaseBacklightControl()
	{
		backlightControl.release();
	}

	ScreenBacklightControl backlightControl = new ScreenBacklightControl();
	
	public final void showVirtualKeyboard() {
		log.d("showVirtualKeyboard() - java hasFocus = " + crview.hasFocus());
		//crview.req
		//getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN);
		//inputMethodManager.toggleSoftInput(InputMethodManager.SHOW_FORCED,0);
		//inputMethodManager.showSoftInput(crview, 0, new ResultReceiver(handler));
		//inputMethodManager.toggleSoftInput(InputMethodManager.SHOW_FORCED,0);
		inputMethodManager.showSoftInput(crview, 0); //InputMethodManager.SHOW_FORCED);
	}

	public final void hideVirtualKeyboard() {
		log.d("hideVirtualKeyboard() - java");
/*		inputMethodManager.hideSoftInputFromWindow(getCurrentFocus().getApplicationWindowToken(), 0);
		inputMethodManager.toggleSoftInput(InputMethodManager.HIDE_IMPLICIT_ONLY,0);
		getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
*/	
		//inputMethodManager.hideSoftInput(crview, 0); //InputMethodManager.SHOW_FORCED);
		inputMethodManager.hideSoftInputFromWindow(getCurrentFocus().getApplicationWindowToken(), 0);
	}
	
	static class OldApiHelper {
		@TargetApi(Build.VERSION_CODES.FROYO)
		static private String getExtFilesDir(Activity activity) {
			return activity.getExternalFilesDir(null) != null ? activity.getExternalFilesDir(null).getAbsolutePath() : null;
		}
	}
	
	private CRConfig createConfig() {
		CRConfig cfg = new CRConfig();
		DisplayMetrics metrics = getResources().getDisplayMetrics();
		cfg.screenDPI = (int)(metrics.density * 160);
		cfg.screenX = metrics.widthPixels;
		cfg.screenY = metrics.heightPixels;
		cfg.einkMode = DeviceInfo.EINK_SCREEN;
		cfg.einkModeSettingsSupported = DeviceInfo.EINK_SCREEN_UPDATE_MODES_SUPPORTED;
		String externalStorageDir = Environment.getExternalStorageDirectory().getAbsolutePath();
		
		initMountRoots();
		
		String dataDir = null;
		PackageManager m = getPackageManager();
		String s = getPackageName();
	    String externalFilesDir = null;
	    if (DeviceInfo.getSDKLevel() < 8) {
	    	log.i("Due to old sdk version, getExternalFilesDir is not available, using getFilesDir");
	    	externalFilesDir = externalStorageDir + "/.cr3";
	    } else
	    	externalFilesDir = OldApiHelper.getExtFilesDir(this);
		try {
		    PackageInfo p = m.getPackageInfo(s, 0);
		    
		    dataDir = p.applicationInfo.dataDir;
		    log.i("DataDir = " + dataDir);
		    log.i("ExternalFilesDir = " + dataDir);
		} catch (NameNotFoundException e) {
		    Log.w(TAG, "Error Package name not found ", e);
		}
		
		cfg.assetManager = getAssets();


		// internal storage
		cfg.internalStorageDir = externalStorageDir;
		String normalizedInternalStorage = pathCorrector.normalizeIfPossible(externalStorageDir);
		// sd card
		int bestSdLocationRate = 0;
		for (File f : mountedRootsList) {
			String path = f.getAbsolutePath();
			String normalizedPath = pathCorrector.normalizeIfPossible(path);
			if (!path.equals(externalStorageDir) 
					&& !path.equals(normalizedInternalStorage)
					&& !normalizedPath.equals(externalStorageDir)
					&& !normalizedPath.equals(normalizedInternalStorage)) {
				int rate = 1;
				if (path.contains("external_sd") || path.contains("external_SD"))
					rate = 10;
				else if (path.contains("extSdCard"))
					rate = 9;
				else if (path.contains("sdcard1"))
					rate = 8;
				else if (path.contains("emmc"))
					rate = 7;
				else if (path.contains("sdcard/sd"))
					rate = 7;
				else if (path.contains("ExternalSD"))
					rate = 7;
				else if (path.contains("sdcard-ext"))
					rate = 9;
				else if (path.contains("MicroSD"))
					rate = 7;
				else if (path.contains("external1"))
					rate = 7;
				else if (path.contains("sdcard0"))
					rate = 7;
				else if (path.contains("sdcard"))
					rate = 4;
				else if (path.contains("external"))
					rate = 5;
				if (cfg.sdcardDir == null || rate > bestSdLocationRate) {
					bestSdLocationRate = rate;
					cfg.sdcardDir = path;
				}
			}
		}
		
		cfg.coverCacheDir = externalFilesDir + "/coverpages";
		cfg.cssDir = "@css";
		cfg.docCacheDir = externalFilesDir + "/doccache";
		cfg.dbFile = externalFilesDir + "/cr3new.sqlite";
		cfg.iniFile = externalFilesDir + "/cr3new.ini";
		cfg.defaultDownloadsDir = standardDownloadsDir;
		
		
		cfg.hyphDir = "@hyph";
		cfg.i18nDir = "@i18n";
		cfg.resourceDir = "@"; // TODO
		cfg.themesDir = "@themes";
		cfg.manualsDir = "@manuals";
		cfg.manualFile = externalFilesDir + "/cr3_manual.fb2";
		
		Locale locale = getResources().getConfiguration().locale;
		String langName = locale.getLanguage();
		if (langName.length() > 2)
			langName = langName.substring(0, 2);
		cfg.systemLanguage = langName; //"en"; // TODO
		cfg.uiFontFace = getSDKLevel() >= ICE_CREAM_SANDWICH ? "Roboto" : "Droid Sans";
		cfg.fallbackFontFace = "Droid Sans Fallback";
		cfg.fontFiles = findFonts();
		
		cfg.coverDirMaxFiles = 500;
		cfg.coverDirMaxItems = 5000;
		cfg.coverDirMaxSize = 1024 * 1024 * 16;
		cfg.coverRenderCacheMaxBytes = 1024 * 1024 * 16;
		cfg.coverRenderCacheMaxItems = 128;
		
		return cfg;
	}
	
	public final static int ICE_CREAM_SANDWICH = 14;
	public final static int HONEYCOMB = 11;

	private static int sdkInt = 0;
	public static int getSDKLevel() {
		if (sdkInt > 0)
			return sdkInt;
		// hack for Android 1.5
		sdkInt = android.os.Build.VERSION.SDK_INT;
//		Field fld;
//		try {
//			Class<?> cl = android.os.Build.VERSION.class;
//			fld = cl.getField("SDK_INT");
//			sdkInt = fld.getInt(cl);
//			Log.i("cr3", "API LEVEL " + sdkInt + " detected");
//		} catch (SecurityException e) {
//			// ignore
//		} catch (NoSuchFieldException e) {
//			// ignore
//		} catch (IllegalArgumentException e) {
//			// ignore
//		} catch (IllegalAccessException e) {
//			// ignore
//		}
		return sdkInt;
	}
	
	@TargetApi(Build.VERSION_CODES.KITKAT)
	private void setFullscreenFlagsApi19() {
		int flag = 0;
		flag |= View.SYSTEM_UI_FLAG_LAYOUT_STABLE
				| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
				| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
				| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
				| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
				| View.SYSTEM_UI_FLAG_FULLSCREEN;

        mDecorView.setSystemUiVisibility(flag);
	}
	
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
	super.onWindowFocusChanged(hasFocus);
	if (hasFocus && (DeviceInfo.getSDKLevel() >= 19)) {
		if (fullscreen)
			setFullscreenFlagsApi19();
        }
    }
	
    public CRTTS getTTS() {
    	return tts;
    }
    
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		log.i("CoolReader.onCreate() is called");
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		dicts = new Dictionaries(this);
		tts = new CRTTS(this);
		mDecorView = getWindow().getDecorView();
		super.onCreate(savedInstanceState);
		crview = new CRView(this);
		crview.init(createConfig());
		tts.setTextToSpeechCallback(crview);
		setContentView(crview);
		clipboardManager = (ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE);
		inputMethodManager = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
		downloadManager = new DownloadManager();
		downloadManager.setCallback(crview);
		downloadManager.start();
		
    	// Battery state listener
		intentReceiver = new BroadcastReceiver() {

			@Override
			public void onReceive(final Context context, final Intent intent) {
				if (crview != null) {
					crview.post(new Runnable() {
						@Override
						public void run() {
							int level = intent.getIntExtra("level", 0);
							int scale = intent.getIntExtra("scale", 100);
							//int status = intent.getIntExtra("status", 0);
							level = level * 100 / scale;
							//if (status == 2) // BATTERY_STATUS_CHARGING
							//	level = -1;
							//else if (status == 5) // BATTERY_STATUS_FULL
							//	level = 100;
							if (level > 100)
								level = 100;
							L.d("intentReceiver.onReceive level=" + level);
							if (crview != null)
								crview.setBatteryLevel(level);
							else
								initialBatteryLevel = level;
						}
					});
				}
			}
			
		};
		registerReceiver(intentReceiver, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        
		setVolumeControlStream(AudioManager.STREAM_MUSIC);		
	}
	int initialBatteryLevel = 100;

	boolean justCreated = true;
	@Override
	protected void onStart() {
		log.i("CoolReader.onStart()");
		super.onStart();
		
		mIsStarted = true;
		mPaused = false;
		onUserActivity();
		
		// Donations support code
//		if (billingSupported)
//			ResponseHandler.register(mPurchaseObserver);
		
//		PhoneStateReceiver.setPhoneActivityHandler(new Runnable() {
//			@Override
//			public void run() {
//				if (mReaderView != null) {
//					mReaderView.stopTTS();
//					mReaderView.save();
//				}
//			}
//		});
		
		
//		if ( isBookOpened() ) {
//			showOpenedBook();
//			return;
//		}
//		
//		if (!isFirstStart)
//			return;
//		isFirstStart = false;
		
		if (justCreated) {
			justCreated = false;
			if (!processIntent(getIntent())) {
				//showLastLocation();
			}
		}
		
		
		log.i("CoolReader.onStart() exiting");
	}
	
	@Override
	protected void onStop() {
		mIsStarted = false;
		super.onStop();
	}
	
    @Override
    protected void onPause() {
		log.i("CoolReader.onPause() is called");
		mIsStarted = false;
		mPaused = true;
        super.onPause();
        crview.onPause();
    }

    @Override
    protected void onResume() {
		log.i("CoolReader.onResume() is called");
        crview.onResume();
        super.onResume();
    }
    
	@Override
	protected void onDestroy() {
		log.i("CoolReader.onDestroy() is called");
		downloadManager.stop();
		crview.uninit();
		if ( intentReceiver!=null ) {
			unregisterReceiver(intentReceiver);
			intentReceiver = null;
		}
		super.onDestroy();
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		//getMenuInflater().inflate(R.menu.cool_reader, menu);
		return true;
	}

	private boolean fullscreen = false; 
	public boolean isFullScreen() {
		return fullscreen;
	}
	
	public void setFullscreen(boolean fullscreen) {
		if (this.fullscreen != fullscreen) {
			this.fullscreen = fullscreen;
			Window wnd = getWindow();
			if ( fullscreen ) {
				//mActivity.getWindow().requestFeature(Window.)
				wnd.setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, 
				        WindowManager.LayoutParams.FLAG_FULLSCREEN );
			} else {
				wnd.setFlags(0, 
				        WindowManager.LayoutParams.FLAG_FULLSCREEN );
			}
			setSystemUiVisibility();
		}
	}
	
	final int sdkLevel = android.os.Build.VERSION.SDK_INT;
	
	public boolean setSystemUiVisibility() {
		if (sdkLevel >= HONEYCOMB) {
			int flags = 0;
//			if (getKeyBacklight() == 0)
//				flags |= SYSTEM_UI_FLAG_LOW_PROFILE;
//			if (isFullscreen() && wantHideNavbarInFullscreen() && isSmartphone())
//				flags |= SYSTEM_UI_FLAG_HIDE_NAVIGATION;
			setSystemUiVisibility(flags);
//			if (isFullscreen() && DeviceInfo.getSDKLevel() >= DeviceInfo.ICE_CREAM_SANDWICH)
//				simulateTouch();
			return true;
		}
		return false;
	}

	private int lastSystemUiVisibility = -1;
	private final static int SYSTEM_UI_FLAG_LOW_PROFILE = 1;
	//private boolean systemUiVisibilityListenerIsSet = false;
	@TargetApi(11)
	@SuppressLint("NewApi")
	private boolean setSystemUiVisibility(int value) {
		if (sdkLevel >= HONEYCOMB) {
			if (DeviceInfo.getSDKLevel() < 19) {
	//			if (!systemUiVisibilityListenerIsSet && contentView != null) {
	//				contentView.setOnSystemUiVisibilityChangeListener(new OnSystemUiVisibilityChangeListener() {
	//					@Override
	//					public void onSystemUiVisibilityChange(int visibility) {
	//						lastSystemUiVisibility = visibility;
	//					}
	//				});
	//			}
				boolean a4 = sdkLevel >= ICE_CREAM_SANDWICH;
				if (!a4)
					value &= SYSTEM_UI_FLAG_LOW_PROFILE;
				if (value == lastSystemUiVisibility && value != SYSTEM_UI_FLAG_LOW_PROFILE)// && a4)
					return false;
				lastSystemUiVisibility = value;
	
				View view;
				//if (a4)
					view = getWindow().getDecorView(); // getReaderView();
				//else
				//	view = mActivity.getContentView(); // getReaderView();
				
				if (view == null)
					return false;
				Method m;
				try {
					m = view.getClass().getMethod("setSystemUiVisibility", int.class);
					m.invoke(view, value);
					return true;
				} catch (SecurityException e) {
					// ignore
				} catch (NoSuchMethodException e) {
					// ignore
				} catch (IllegalArgumentException e) {
					// ignore
				} catch (IllegalAccessException e) {
					// ignore
				} catch (InvocationTargetException e) {
					// ignore
				}
			}
		}
		return false;
	}
//	
//	private int currentKeyBacklightLevel = 1;
//	public int getKeyBacklight() {
//		return currentKeyBacklightLevel;
//	}
//	public boolean setKeyBacklight(int value) {
//		currentKeyBacklightLevel = value;
//		// Try ICS way
//		if (DeviceInfo.getSDKLevel() >= DeviceInfo.HONEYCOMB) {
//			setSystemUiVisibility();
//		}
//		// thread safe
//		return Engine.getInstance(this).setKeyBacklight(value);
//	}
//	
//
//
//    private boolean keyBacklightOff = true;
//    public boolean isKeyBacklightDisabled() {
//    	return keyBacklightOff;
//    }
//    
//    public void setKeyBacklightDisabled(boolean disabled) {
//    	keyBacklightOff = disabled;
//    	onUserActivity();
//    }
//    
//    public void setScreenBacklightLevel( int percent )
//    {
//    	if ( percent<-1 )
//    		percent = -1;
//    	else if ( percent>100 )
//    		percent = -1;
//    	screenBacklightBrightness = percent;
//    	onUserActivity();
//    }
//    
//    private int screenBacklightBrightness = -1; // use default
//    //private boolean brightnessHackError = false;
//    private boolean brightnessHackError = DeviceInfo.SAMSUNG_BUTTONS_HIGHLIGHT_PATCH;
//
//    private void turnOffKeyBacklight() {
//    	if (!isStarted())
//    		return;
//		if (DeviceInfo.getSDKLevel() >= DeviceInfo.HONEYCOMB) {
//			setKeyBacklight(0);
//		}
//    	// repeat again in short interval
//    	if (!Engine.getInstance(this).setKeyBacklight(0)) {
//    		//log.w("Cannot control key backlight directly");
//    		return;
//    	}
//    	// repeat again in short interval
//    	Runnable task = new Runnable() {
//			@Override
//			public void run() {
//		    	if (!isStarted())
//		    		return;
//		    	if (!Engine.getInstance(BaseActivity.this).setKeyBacklight(0)) {
//		    		//log.w("Cannot control key backlight directly (delayed)");
//		    	}
//			}
//		};
//		BackgroundThread.instance().postGUI(task, 1);
//		//BackgroundThread.instance().postGUI(task, 10);
//    }
//    
//    private void updateBacklightBrightness(float b) {
//        Window wnd = getWindow();
//        if (wnd != null) {
//	    	LayoutParams attrs =  wnd.getAttributes();
//	    	boolean changed = false;
//	    	if (b < 0 && b > -0.99999f) {
//	    		//log.d("dimming screen by " + (int)((1 + b)*100) + "%");
//	    		b = -b * attrs.screenBrightness;
//	    		if (b < 0.15)
//	    			return;
//	    	}
//	    	float delta = attrs.screenBrightness - b;
//	    	if (delta < 0)
//	    		delta = -delta;
//	    	if (delta > 0.01) {
//	    		attrs.screenBrightness = b;
//	    		changed = true;
//	    	}
//	    	if ( changed ) {
//	    		log.d("Window attribute changed: " + attrs);
//	    		wnd.setAttributes(attrs);
//	    	}
//        }
//    }
//
//    private void updateButtonsBrightness(float buttonBrightness) {
//        Window wnd = getWindow();
//        if (wnd != null) {
//	    	LayoutParams attrs =  wnd.getAttributes();
//	    	boolean changed = false;
//	    	// hack to set buttonBrightness field
//	    	//float buttonBrightness = keyBacklightOff ? 0.0f : -1.0f;
//	    	if (!brightnessHackError)
//	    	try {
//	        	Field bb = attrs.getClass().getField("buttonBrightness");
//	        	if (bb != null) {
//	        		Float oldValue = (Float)bb.get(attrs);
//	        		if (oldValue == null || oldValue.floatValue() != buttonBrightness) {
//	        			bb.set(attrs, buttonBrightness);
//		        		changed = true;
//	        		}
//	        	}
//	    	} catch ( Exception e ) {
//	    		log.e("WindowManager.LayoutParams.buttonBrightness field is not found, cannot turn buttons backlight off");
//	    		brightnessHackError = true;
//	    	}
//	    	//attrs.buttonBrightness = 0;
//	    	if (changed) {
//	    		log.d("Window attribute changed: " + attrs);
//	    		wnd.setAttributes(attrs);
//	    	}
//	    	if (keyBacklightOff)
//	    		turnOffKeyBacklight();
//        }
//    }
//
//    private final static int MIN_BACKLIGHT_LEVEL_PERCENT = DeviceInfo.MIN_SCREEN_BRIGHTNESS_PERCENT;
 	
	@Override
	protected void onNewIntent(Intent intent) {
		log.i("onNewIntent : " + intent);
//		if ( mDestroyed ) {
//			log.e("engine is already destroyed");
//			return;
//		}
		processIntent(intent);
//		String fileToOpen = null;
//		if ( Intent.ACTION_VIEW.equals(intent.getAction()) ) {
//			Uri uri = intent.getData();
//			if ( uri!=null ) {
//				fileToOpen = extractFileName(uri);
//			}
//			intent.setData(null);
//		}
//		log.v("onNewIntent, fileToOpen=" + fileToOpen);
//		if ( fileToOpen!=null ) {
//			// load document
//			final String fn = fileToOpen;
//			BackgroundThread.instance().postGUI(new Runnable() {
//				@Override
//				public void run() {
//					loadDocument(fn, new Runnable() {
//						public void run() {
//							log.v("onNewIntent, loadDocument error handler called");
//							showToast("Error occured while loading " + fn);
//							Services.getEngine().hideProgress();
//						}
//					});
//				}
//			}, 100);
//		}
	}

	public static final String OPEN_FILE_PARAM = "FILE_TO_OPEN";
	private boolean processIntent(Intent intent) {
		log.d("intent=" + intent);
		if (intent == null)
			return false;
		String fileToOpen = null;
		if (Intent.ACTION_VIEW.equals(intent.getAction())) {
			Uri uri = intent.getData();
			intent.setData(null);
			if (uri != null) {
				fileToOpen = uri.getPath();
//				if (fileToOpen.startsWith("file://"))
//					fileToOpen = fileToOpen.substring("file://".length());
			}
		}
		if (fileToOpen == null && intent.getExtras() != null) {
			log.d("extras=" + intent.getExtras());
			fileToOpen = intent.getExtras().getString(OPEN_FILE_PARAM);
		}
		if (fileToOpen != null) {
			// patch for opening of books from ReLaunch (under Nook Simple Touch) 
			while (fileToOpen.indexOf("%2F") >= 0) {
				fileToOpen = fileToOpen.replace("%2F", "/");
			}
			log.d("FILE_TO_OPEN = " + fileToOpen);
			if (crview != null)
				crview.loadBook(fileToOpen);
			return true;
		} else {
			log.d("No file to open");
			return false;
		}
	}


	
	
	
	//=================================================================================================
	
	
	/**
	 * Returns array of writable data directories on external storage
	 * 
	 * @param subdir
	 * @param createIfNotExists
	 * @return
	 */
	public static File[] getDataDirectories(String subdir,
			boolean createIfNotExists, boolean writableOnly) {
		File[] roots = getStorageDirectories(writableOnly);
		ArrayList<File> res = new ArrayList<File>(roots.length);
		for (File dir : roots) {
			File dataDir = getSubdir(dir, ".cr3", createIfNotExists,
					writableOnly);
			if (subdir != null)
				dataDir = getSubdir(dataDir, subdir, createIfNotExists,
						writableOnly);
			if (dataDir != null)
				res.add(dataDir);
		}
		return res.toArray(new File[] {});
	}

	/**
	 * Get or create writable subdirectory for specified base directory
	 * 
	 * @param dir
	 *            is base directory
	 * @param subdir
	 *            is subdirectory name, null to use base directory
	 * @param createIfNotExists
	 *            is true to force directory creation
	 * @return writable directory, null if not exist or not writable
	 */
	public static File getSubdir(File dir, String subdir,
			boolean createIfNotExists, boolean writableOnly) {
		if (dir == null)
			return null;
		File dataDir = dir;
		if (subdir != null) {
			dataDir = new File(dataDir, subdir);
			if (!dataDir.isDirectory() && createIfNotExists)
				dataDir.mkdir();
		}
		if (dataDir.isDirectory() && (!writableOnly || dataDir.canWrite()))
			return dataDir;
		return null;
	}


	/**
	 * Get storage root directories.
	 * 
	 * @return array of r/w storage roots
	 */
	public static File[] getStorageDirectories(boolean writableOnly) {
		Collection<File> res = new HashSet<File>(2);
		for (File dir : mountedRootsList) {
			if (dir.isDirectory() && (!writableOnly || dir.canWrite())) {
				String[] items = dir.list();
				if (items != null && items.length > 0)
					res.add(dir);
			}
		}
		return res.toArray(new File[res.size()]);
	}
	
	private static String[] findFonts() {
		ArrayList<File> dirs = new ArrayList<File>();
		File[] dataDirs = getDataDirectories("fonts", false, false);
		for (File dir : dataDirs)
			dirs.add(dir);
		File[] rootDirs = getStorageDirectories(false);
		for (File dir : rootDirs)
			dirs.add(new File(dir, "fonts"));
		dirs.add(new File(Environment.getRootDirectory(), "fonts"));
		ArrayList<String> fontPaths = new ArrayList<String>();
		for (File fontDir : dirs) {
			if (fontDir.isDirectory()) {
				log.v("Scanning directory " + fontDir.getAbsolutePath()
						+ " for font files");
				// get font names
				String[] fileList = fontDir.list(new FilenameFilter() {
					public boolean accept(File dir, String filename) {
						String lc = filename.toLowerCase();
						return (lc.endsWith(".ttf") || lc.endsWith(".otf")
								|| lc.endsWith(".pfb") || lc.endsWith(".pfa"))
//								&& !filename.endsWith("Fallback.ttf")
								;
					}
				});
				// append path
				for (int i = 0; i < fileList.length; i++) {
					String pathName = new File(fontDir, fileList[i])
							.getAbsolutePath();
					fontPaths.add(pathName);
					log.v("found font: " + pathName);
				}
			}
		}
		Collections.sort(fontPaths);
		return fontPaths.toArray(new String[] {});
	}

	
	private static File[] mountedRootsList;
	private static Map<String, String> mountedRootsMap;
	private static MountPathCorrector pathCorrector;
	
	private static boolean addMountRoot(Map<String, String> list, String path, String name) {
		if (path.endsWith("/"))
			path = path.substring(0, path.length() - 1);
		if (list.containsKey(path))
			return false;
		for (String key : list.keySet()) {
			String link = CRView.isLink(key);
			if (link != null&& link.length() > 0)
				key = link;
			String p = CRView.isLink(path);
			if (p == null || p.length() == 0)
				p = path;
			if (p.equals(key)) { // path.startsWith(key + "/")
				log.w("Skipping duplicate path " + path + " == " + key);
				return false; // duplicate subpath
			}
		}
		try {
			File dir = new File(path);
			if (dir.isDirectory()) {
				String[] d = dir.list();
				if (d == null || d.length == 0) {
					File books = new File(dir, "Books");
					if (!books.mkdir())
						return false;
					log.i("Created Books directory at FS root: " + path + " " + name);
					d = dir.list();
				}
				if ((d != null && d.length > 0)) {
					log.i("Adding FS root: " + path + " " + name);
					list.put(path, name);
					return true;
				}
//				} else {
//					log.i("Skipping mount point " + path + " : no files or directories found here, and writing is disabled");
//				}
			}
		} catch (Exception e) {
			// ignore
		}
		return false;
	}
	
	public static String loadFileUtf8(File file) {
		try {
			InputStream is = new FileInputStream(file);
			return loadResourceUtf8(is);
		} catch (Exception e) {
			log.w("cannot load resource from file " + file);
			return null;
		}
	}

	public static String loadResourceUtf8(InputStream is) {
		try {
			int available = is.available();
			if (available <= 0)
				return null;
			byte buf[] = new byte[available];
			if (is.read(buf) != available)
				throw new IOException("Resource not read fully");
			is.close();
			String utf8 = new String(buf, 0, available, "UTF8");
			return utf8;
		} catch (Exception e) {
			log.e("cannot load resource");
			return null;
		}
	}

	public static byte[] loadResourceBytes(File f) {
		if (f == null || !f.isFile() || !f.exists())
			return null;
		FileInputStream is = null;
		try {
			is = new FileInputStream(f);
			byte[] res = loadResourceBytes(is);
			return res;
		} catch (IOException e) {
			log.e("Cannot open file " + f);
		}
		return null;
	}

	public static byte[] loadResourceBytes(InputStream is) {
		try {
			int available = is.available();
			if (available <= 0)
				return null;
			byte buf[] = new byte[available];
			if (is.read(buf) != available)
				throw new IOException("Resource not read fully");
			is.close();
			return buf;
		} catch (Exception e) {
			log.e("cannot load resource");
			return null;
		}
	}

	@SuppressLint("SdCardPath")
	private static void initMountRoots() {
		Map<String, String> map = new LinkedHashMap<String, String>();

		// standard external directory
		String sdpath = Environment.getExternalStorageDirectory().getAbsolutePath();
		// dirty fix
		if ("/nand".equals(sdpath) && new File("/sdcard").isDirectory())
			sdpath = "/sdcard";
		// main storage
		addMountRoot(map, sdpath, "SD CARD");

		// retrieve list of mount points from system
		String[] fstabLocations = new String[] {
			"/system/etc/vold.conf",
			"/system/etc/vold.fstab",
			"/etc/vold.conf",
			"/etc/vold.fstab",
		};
		String s = null;
		String fstabFileName = null;
		for (String fstabFile : fstabLocations) {
			fstabFileName = fstabFile;
			s = loadFileUtf8(new File(fstabFile));
			if (s != null)
				log.i("found fstab file " + fstabFile);
		}
		if (s == null)
			log.w("fstab file not found");
		if ( s!= null) {
			String[] rows = s.split("\n");
			int rulesFound = 0;
			for (String row : rows) {
				if (row != null && row.startsWith("dev_mount")) {
					log.d("mount rule: " + row);
					rulesFound++;
					String[] cols = Utils.splitByWhitespace(row);
					if (cols.length >= 5) {
						String name = Utils.ntrim(cols[1]);
						String point = Utils.ntrim(cols[2]);
						String mode = Utils.ntrim(cols[3]);
						String dev = Utils.ntrim(cols[4]);
						if (Utils.empty(name) || Utils.empty(point) || Utils.empty(mode) || Utils.empty(dev))
							continue;
						String label = null;
						boolean hasusb = dev.indexOf("usb") >= 0;
						boolean hasmmc = dev.indexOf("mmc") >= 0;
						log.i("*** mount point '" + name + "' *** " + point + "  (" + dev + ")");
						if ("auto".equals(mode)) {
							// assume AUTO is for externally automount devices
							if (hasusb)
								label = "USB Storage";
							else if (hasmmc)
								label = "External SD";
							else
								label = "External Storage";
						} else {
							if (hasmmc)
								label = "Internal SD";
							else
								label = "Internal Storage";
						}
						if (!point.equals(sdpath)) {
							// external SD
							addMountRoot(map, point, label + " (" + point + ")");
						}
					}
				}
			}
			if (rulesFound == 0)
				log.w("mount point rules not found in " + fstabFileName);
		}

		// TODO: probably, hardcoded list is not necessary after /etc/vold parsing 
		String[] standardMountPoints = new String[] {
			"/system/media/sdcard", // internal SD card on Nook
			"/media",
			"/nand",
			"/PocketBook701", // internal SD card on PocketBook 701 IQ
			"/mnt/extsd",
			"/mnt/external1",
			"/mnt/external_sd",
			"/mnt/udisk",
			"/mnt/sdcard2",
			"/mnt/ext.sd",
			"/ext.sd",
			"/extsd",
			"/storage/sdcard",
			"/storage/sdcard0",
			"/storage/sdcard1",
			"/storage/sdcard2",
			"/mnt/extSdCard",
			"/sdcard",
			"/sdcard2",
			"/mnt/udisk",
			"/sdcard-ext",
			"/sd-ext",
			"/mnt/external1",
			"/mnt/external2",
			"/mnt/sdcard1",
			"/mnt/sdcard2",
			"/mnt/usb_storage",
			"/mnt/external_sd",
			"/storage/external_SD/",
			"/emmc",
			"/external",
			"/Removable/SD",
			"/Removable/MicroSD",
			"/Removable/USBDisk1",
			"/storage/sdcard1",
			"/mnt/sdcard/extStorages/SdCard",
			"/storage/extSdCard",
		};
		ArrayList<String> knownMountPoints = new ArrayList<String>();
		for (String mp : standardMountPoints)
			knownMountPoints.add(mp);

		
		String storageList = System.getenv("SECONDARY_STORAGE");
		if (storageList != null) {
			for (String p : storageList.split(":")) {
				if (p.startsWith("/"))
					knownMountPoints.add(p);
			}
		}
		
		for (String point : knownMountPoints) {
			String link = CRView.isLink(point);
			if (link != null) {
				String link2 = CRView.isLink(link);
				if (link2 != null)
					link = link2;
				String link3 = CRView.isLink(link);
				if (link3 != null)
					link = link3;
				log.d("standard mount point path is link: " + point + " > " + link);
				addMountRoot(map, link, link);
			} else {
				addMountRoot(map, point, point);
			}
		}
		
		// auto detection
		//autoAddRoots(map, "/", SYSTEM_ROOT_PATHS);
		//autoAddRoots(map, "/mnt", new String[] {});
		
		mountedRootsMap = map;
		Collection<File> list = new ArrayList<File>();
		log.i("Mount ROOTS:");
		for (String f : map.keySet()) {
			File path = new File(f);
			list.add(path);
			String label = map.get(f);
			log.i("*** " + f + " '" + label + "' isDirectory=" + path.isDirectory() + " canRead=" + path.canRead() + " canWrite=" + path.canRead() + " isLink=" + CRView.isLink(f));
		}
		mountedRootsList = list.toArray(new File[] {});
		pathCorrector = new MountPathCorrector(mountedRootsList);

		for (String point : knownMountPoints) {
			String link = CRView.isLink(point);
			if (link != null)
				pathCorrector.addRootLink(point, link);
			String link2 = CRView.isLink(link);
			if (link2 != null) {
				link = link2;
				pathCorrector.addRootLink(point, link);
			}
			String link3 = CRView.isLink(link);
			if (link3 != null) {
				link = link3;
				pathCorrector.addRootLink(point, link);
			}
		}
		
		Log.i("cr3", "Root list: " + list + ", root links: " + pathCorrector);
//		testPathNormalization("/sdcard/books/test.fb2");
//		testPathNormalization("/mnt/sdcard/downloads/test.fb2");
//		testPathNormalization("/mnt/sd/dir/test.fb2");
		String extDownloadDir = null;
		if (DeviceInfo.getSDKLevel() >= 8) {
			extDownloadDir = getExternalDownloadsDirApi8();
		}
		extDownloadDir = pathCorrector.normalizeIfPossible(extDownloadDir);
		Log.i("cr3", "extDownloadDir from API: " + extDownloadDir);
		if (extDownloadDir != null && (new File(extDownloadDir)).isDirectory() && (new File(extDownloadDir)).canWrite())
			standardDownloadsDir = extDownloadDir;
		if (standardDownloadsDir == null) {
			for(File f: mountedRootsList) {
				if (!f.isDirectory())
					continue;
				File subdir1 = new File(f, "Download");
				if (subdir1.isDirectory() && subdir1.canWrite()) {
					standardDownloadsDir = subdir1.getAbsolutePath();
					break;
				}
				File subdir2 = new File(f, "Downloads");
				if (subdir2.isDirectory() && subdir2.canWrite()) {
					standardDownloadsDir = subdir2.getAbsolutePath();
					break;
				}
			}
			
		}
	}
	
	@TargetApi(Build.VERSION_CODES.FROYO)
	private static String getExternalDownloadsDirApi8() {
		File f = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
		if (f != null)
			return f.getAbsolutePath();
		return null;
	}
	
	public static String standardDownloadsDir = null;

	public DictInstallationInfo[] getAvailableDictionaries() {
		return dicts.getAvailableDictionaries();
	}
	
	public void lookupInDictionary(final String dictionaryId, final String word) {
		dicts.setDict(dictionaryId);
		try {
			dicts.findInDictionary(word);
		} catch (DictionaryException e) {
			L.e(e.getMessage());
			if (crview != null)
				crview.onDictionaryError(1);
		}
	}
	
}
