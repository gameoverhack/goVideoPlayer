#include "goVideoPlayer.h"
#include "ofUtils.h"

#ifdef TARGET_LINUX
#include <gst/video/video.h>
#endif

//--------------------------------------------------------------
#ifdef  OF_VIDEO_PLAYER_QUICKTIME
//--------------------------------------------------------------

bool  	createMovieFromPath(char * path, Movie &movie);
bool 	createMovieFromPath(char * path, Movie &movie)
{



    Boolean 	isdir			= false;
    OSErr 		result 			= 0;
    FSSpec 		theFSSpec;

    short 		actualResId 	= DoTheRightThing;

#ifdef TARGET_WIN32
    result = NativePathNameToFSSpec (path, &theFSSpec, 0);
    if (result != noErr)
    {
        ofLog(OF_LOG_ERROR,"NativePathNameToFSSpec failed %d", result);
        ofLog(OF_LOG_ERROR,"Error loading movie");
        return false;
    }

#endif



#ifdef TARGET_OSX
    FSRef 		fsref;
    result = FSPathMakeRef((const UInt8*)path, &fsref, &isdir);
    if (result)
    {
        ofLog(OF_LOG_ERROR,"FSPathMakeRef failed %d", result);
        ofLog(OF_LOG_ERROR,"Error loading movie");
        return false;
    }
    result = FSGetCatalogInfo(&fsref, kFSCatInfoNone, NULL, NULL, &theFSSpec, NULL);
    if (result)
    {
        ofLog(OF_LOG_ERROR,"FSGetCatalogInfo failed %d", result);
        ofLog(OF_LOG_ERROR,"Error loading movie");
        return false;
    }
#endif


    short movieResFile;
    result = OpenMovieFile (&theFSSpec, &movieResFile, fsRdPerm);
    if (result == noErr)
    {

        short   movieResID = 0;
        result = NewMovieFromFile(&movie, movieResFile, &movieResID, (unsigned char *) 0, newMovieActive, (Boolean *) 0);
        if (result == noErr)
        {
            CloseMovieFile (movieResFile);
        }
        else
        {
            ofLog(OF_LOG_ERROR,"NewMovieFromFile failed %d", result);
            return false;
        }
    }
    else
    {
        ofLog(OF_LOG_ERROR,"OpenMovieFile failed %d", result);
        return false;
    }

    return true;
}

//--------------------------------------------------------------
bool createMovieFromURL(string urlIn,  Movie &movie)
{
    char * url = (char *)urlIn.c_str();
    Handle urlDataRef;

    OSErr err;

    urlDataRef = NewHandle(strlen(url) + 1);
    if ( ( err = MemError()) != noErr)
    {
        ofLog(OF_LOG_ERROR,"createMovieFromURL: error creating url handle");
        return false;
    }

    BlockMoveData(url, *urlDataRef, strlen(url) + 1);

    err = NewMovieFromDataRef(&movie, newMovieActive,nil, urlDataRef, URLDataHandlerSubType);
    DisposeHandle(urlDataRef);

    if(err != noErr)
    {
        ofLog(OF_LOG_ERROR,"createMovieFromURL: error loading url");
        return false;
    }
    else
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------
OSErr 	DrawCompleteProc(Movie theMovie, long refCon);
OSErr 	DrawCompleteProc(Movie theMovie, long refCon)
{

    goVideoPlayer * ofvp = (goVideoPlayer *)refCon;

	//cout << ofvp->getCurrentFileName() << endl;

#if defined(TARGET_OSX) && defined(__BIG_ENDIAN__)
    convertPixels(ofvp->offscreenGWorldPixels, ofvp->pixels, ofvp->width, ofvp->height, ofvp->pixelType);
#endif

    ofvp->bHavePixelsChanged = true;
    if (ofvp->bUseTexture == true)
    {
        ofvp->tex.loadData(ofvp->pixels, ofvp->width, ofvp->height, ofvp->pixelType);
    }

    return noErr;

}

//--------------------------------------------------------------
#endif
//--------------------------------------------------------------

//---------------------------------------------------------------------------
goVideoPlayer::goVideoPlayer ()
{

    bLoaded 					= false;
    width 						= 0;
    height						= 0;
    speed 						= 1;
    bUseTexture					= true;
    bStarted					= false;
    pixels						= NULL;
    nFrames						= 0;
    bPaused						= false;
    pixelType                   = GO_TV_RGB;


    //--------------------------------------------------------------
#ifndef  TARGET_LINUX  // !linux = quicktime...
    //--------------------------------------------------------------
    moviePtr	 				= NULL;
    allocated 					= false;
    offscreenGWorld				= NULL;
    //--------------------------------------------------------------
#endif
    //--------------------------------------------------------------

}

//---------------------------------------------------------------------------
string goVideoPlayer::getCurrentFileName() {
	return currentFileName;
}
//---------------------------------------------------------------------------
unsigned char * goVideoPlayer::getPixels()
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------
    return pixels;
    //--------------------------------------
#else
    //--------------------------------------
    return gstUtils.getPixels();
    //--------------------------------------
#endif
    //--------------------------------------
}

//------------------------------------
//for getting a reference to the texture
ofTexture & goVideoPlayer::getTextureReference()
{
    if(!tex.bAllocated() )
    {
        ofLog(OF_LOG_WARNING, "goVideoPlayer - getTextureReference - texture is not allocated");
    }
    return tex;
}


//---------------------------------------------------------------------------
bool goVideoPlayer::isFrameNew()
{
    return bIsFrameNew;


}

//--------------------------------------------------------------------
void goVideoPlayer::update()
{
    idleMovie();
}

//---------------------------------------------------------------------------
void goVideoPlayer::idleMovie()
{

    if (bLoaded == true)
    {

        //--------------------------------------------------------------
#ifndef TARGET_LINUX  // !linux = quicktime...

        //--------------------------------------------------------------
#if defined(TARGET_WIN32) || defined(QT_USE_MOVIETASK)
        MoviesTask(moviePtr,100); // does changing this time really work? maybe not?
#endif

        //--------------------------------------------------------------
#else // linux.
        //--------------------------------------------------------------

        gstUtils.update();
        if(gstUtils.isFrameNew())
        {
            bHavePixelsChanged = true;
            width = gstUtils.getWidth();
            height = gstUtils.getHeight();
            tex.loadData(gstUtils.getPixels(), width, height, pixelType);
        }

        //--------------------------------------------------------------
#endif
        //--------------------------------------------------------------
    }

    // ---------------------------------------------------
    // 		on all platforms,
    // 		do "new"ness ever time we idle...
    // 		before "isFrameNew" was clearning,
    // 		people had issues with that...
    // 		and it was badly named so now, newness happens
    // 		per-idle not per isNew call
    // ---------------------------------------------------

    if (bLoaded == true)
    {

        bIsFrameNew = bHavePixelsChanged;
        if (bHavePixelsChanged == true)
        {
            bHavePixelsChanged = false;
        }
    }

}

//---------------------------------------------------------------------------
void goVideoPlayer::closeMovie()
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    if (bLoaded == true)
    {

		//cout << "Closing: " << currentFileName << endl;
        DisposeMovie (moviePtr);
        DisposeMovieDrawingCompleteUPP(myDrawCompleteProc);

        moviePtr		= NULL;
		currentFileName = "";
		nFrames			= 0;
        //if(allocated)	delete[] pixels;
        //if(allocated)	delete[] offscreenGWorldPixels;
        // tex.clear();
        //allocated = false;
        // if ((offscreenGWorld)) DisposeGWorld((offscreenGWorld));

    }

    //--------------------------------------
#else
    //--------------------------------------

    gstUtils.close();

    //--------------------------------------
#endif
    //--------------------------------------

    bLoaded = false;

}

//---------------------------------------------------------------------------
void goVideoPlayer::close()
{
    closeMovie();
}

//---------------------------------------------------------------------------
goVideoPlayer::~goVideoPlayer()
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------
	//cout << "Deleteing: " << currentFileName << endl;
    closeMovie();
    if(allocated)	{
        delete [] pixels;
        pixels = NULL; // added by gameover - make sure those pixels are truly deleted
        delete [] offscreenGWorldPixels;
        offscreenGWorldPixels = NULL; // added by gameover - make sure those pixels are truly deleted
    }
    if ((offscreenGWorld)) DisposeGWorld((offscreenGWorld));

    //closeQuicktime();
    //--------------------------------------
#else
    //--------------------------------------

    closeMovie();

    //--------------------------------------
#endif
    //--------------------------------------

    tex.clear();

}




//--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
//--------------------------------------

void goVideoPlayer::createImgMemAndGWorld()
{
    Rect movieRect;
    movieRect.top 			= 0;
    movieRect.left 			= 0;
    movieRect.bottom 		= height;
    movieRect.right 		= width;
    offscreenGWorldPixels 	= new unsigned char[4 * width * height + 32];

    if (pixelType == GO_TV_RGBA) {
        pixels					= new unsigned char[width*height*4];

#if defined(TARGET_OSX) && defined(__BIG_ENDIAN__)
        QTNewGWorldFromPtr (&(offscreenGWorld), k32ARGBPixelFormat, &(movieRect), NULL, NULL, 0, (offscreenGWorldPixels), 4 * width);
#else
        QTNewGWorldFromPtr (&(offscreenGWorld), k32RGBAPixelFormat, &(movieRect), NULL, NULL, 0, (pixels), 4 * width);
#endif
    } else {
        pixels					= new unsigned char[width*height*3];

        #if defined(TARGET_OSX) && defined(__BIG_ENDIAN__)
            QTNewGWorldFromPtr (&(offscreenGWorld), k32ARGBPixelFormat, &(movieRect), NULL, NULL, 0, (offscreenGWorldPixels), 4 * width);
        #else
            QTNewGWorldFromPtr (&(offscreenGWorld), k24RGBPixelFormat, &(movieRect), NULL, NULL, 0, (pixels), 3 * width);
        #endif
    }


    LockPixels(GetGWorldPixMap(offscreenGWorld));
    SetGWorld (offscreenGWorld, NULL);
    SetMovieGWorld (moviePtr, offscreenGWorld, nil);
    //------------------------------------ texture stuff:
    if (bUseTexture)
    {
        // create the texture, set the pixels to black and
        // upload them to the texture (so at least we see nothing black the callback)
        if (pixelType == GO_TV_RGBA) {
            tex.allocate(width,height,pixelType);
            memset(pixels, 0, width*height*4);
            tex.loadData(pixels, width, height, pixelType);
            //allocated = true;     // if it's done here then it only gets allocated if
                                    //we're using a texture and therefore the pixels
                                    // don't get deleted when you close the movie
        } else {
            tex.allocate(width,height,pixelType);
            memset(pixels, 0, width*height*3);
            tex.loadData(pixels, width, height, pixelType);
            //allocated = true;     // if it's done here then it only gets allocated if
                                    //we're using a texture and therefore the pixels
                                    // don't get deleted when you close the movie

        }
    }

    allocated = true; // moved by gameover from above
}

//--------------------------------------
#endif
//--------------------------------------




//---------------------------------------------------------------------------
bool goVideoPlayer::loadMovie(string name, bool loadedInThread)
{

	currentFileName = name;

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    initializeQuicktime();			// init quicktime

    closeMovie();					// if we have a movie open, close it

    bLoaded 				= false;	// try to load now


    if( name.substr(0, 7) == "http://" || name.substr(0,7) == "rtsp://" )
    {
        if(! createMovieFromURL(name, moviePtr) ) return false;
    }
    else
    {
        name 					= ofToDataPath(name);

        if( !createMovieFromPath((char *)name.c_str(), moviePtr) ) return false;
    }

    bool bDoWeAlreadyHaveAGworld = false;
    if (width != 0 && height != 0)
    {
        bDoWeAlreadyHaveAGworld = true;
    }
    Rect 				movieRect;
    GetMovieBox(moviePtr, &(movieRect));

    if (bDoWeAlreadyHaveAGworld)
    {
        // is the gworld the same size, then lets *not* de-allocate and reallocate:
        if (width == movieRect.right &&
                height == movieRect.bottom)
        {
            SetMovieGWorld (moviePtr, offscreenGWorld, nil);
        }
        else
        {
            width 	= movieRect.right;
            height 	= movieRect.bottom;
            delete [] pixels;
            delete [] offscreenGWorldPixels;
            if ((offscreenGWorld)) DisposeGWorld((offscreenGWorld));
            createImgMemAndGWorld();
        }
    }
    else
    {
        width	= movieRect.right;
        height 	= movieRect.bottom;
        createImgMemAndGWorld();
    }

    if (moviePtr == NULL)
    {
        return false;
    }

    //----------------- callback method
	if (!loadedInThread) {
		myDrawCompleteProc = NewMovieDrawingCompleteUPP (DrawCompleteProc);
		SetMovieDrawingCompleteProc (moviePtr, movieDrawingCallWhenChanged,  myDrawCompleteProc, (long)this);
		nFrames = 0;
		getTotalNumFrames(); // hmmm...putting this here so things load uber quick in threads but might break things if you check duration before playing?
	}

    // ------------- get some pixels in there ------
    GoToBeginningOfMovie(moviePtr);
    SetMovieActiveSegment(moviePtr, -1,-1);
    MoviesTask(moviePtr,0);

#if defined(TARGET_OSX) && defined(__BIG_ENDIAN__)
    convertPixels(offscreenGWorldPixels, pixels, width, height, pixelType);
#endif

    if (bUseTexture == true)
    {
        tex.loadData(pixels, width, height, pixelType);
    }

    bStarted 				= false;
    bLoaded 				= true;
    bPlaying 				= false;
    bHavePixelsChanged 		= false;
    speed 					= 1;

    /*
    	for (int trackIndex = 1; trackIndex <= GetMovieTrackCount(moviePtr); trackIndex++) {
    		Track       track = GetMovieIndTrack(moviePtr, trackIndex);
    		OSType      dwType;
    		GetMediaHandlerDescription(GetTrackMedia(track),
    								   &dwType, NULL, NULL);

    		if(dwType == VideoMediaType) {
    			cout << trackIndex << " video" << endl;
    		}
    		if(dwType == SoundMediaType) {
    			cout << trackIndex << " sound" << endl;
    		}
    		if(dwType == TextMediaType) {
    			cout << trackIndex << " text" << endl;
    		}
    		if(dwType == MovieMediaType) {
    			cout << trackIndex << " movie" << endl;
    		}
    		if(dwType == TimeCodeMediaType) {
    			cout << trackIndex << " timecode" << endl;
    		}

    		cout << trackIndex << dwType << endl;
    	}
    */
    return true;

    //--------------------------------------
#else
    //--------------------------------------


    if(gstUtils.loadMovie(name))
    {
        if(bUseTexture)
        {
            tex.allocate(gstUtils.getWidth(),gstUtils.getHeight(),pixelType,false);
            tex.loadData(gstUtils.getPixels(), gstUtils.getWidth(), gstUtils.getHeight(), pixelType);
        }
        bLoaded = true;
        allocated = true;
        ofLog(OF_LOG_VERBOSE,"goVideoPlayer: movie loaded");
        return true;
    }
    else
    {
        ofLog(OF_LOG_ERROR,"goVideoPlayer couldn't load movie");
        return false;
    }


    //--------------------------------------
#endif
    //--------------------------------------

}

//--------------------------------------------------------
void goVideoPlayer::start()
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    if (bLoaded == true && bStarted == false)
    {
        SetMovieActive(moviePtr, true);

        //------------------ set the movie rate to default
        //------------------ and preroll, so the first frames come correct

        TimeValue timeNow 	= 	GetMovieTime(moviePtr, 0);
        Fixed playRate 		=	GetMoviePreferredRate(moviePtr); 		//Not being used!

        PrerollMovie(moviePtr, timeNow, X2Fix(speed));
        SetMovieRate(moviePtr,  X2Fix(speed));
        setLoopState(OF_LOOP_NORMAL);

        // get some pixels in there right away:
        MoviesTask(moviePtr,0);
#if defined(TARGET_OSX) && defined(__BIG_ENDIAN__)
        convertPixels(offscreenGWorldPixels, pixels, width, height);
#endif
        bHavePixelsChanged = true;
        if (bUseTexture == true)
        {
            tex.loadData(pixels, width, height, pixelType);
        }

        bStarted = true;
        bPlaying = true;
    }

    //--------------------------------------
#endif
    //--------------------------------------
}

//--------------------------------------------------------
void goVideoPlayer::play()
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    if (!bStarted)
    {
        start();
    }
    else
    {
        // ------------ lower level "startMovie"
        // ------------ use desired speed & time (-1,1,etc) to Preroll...
        bPlaying = true;
        TimeValue timeNow;
        timeNow = GetMovieTime(moviePtr, nil);
        PrerollMovie(moviePtr, timeNow, X2Fix(speed));
        SetMovieRate(moviePtr,  X2Fix(speed));
        MoviesTask(moviePtr,0);
    }

    //--------------------------------------
#else
    //--------------------------------------
    gstUtils.play();
    //--------------------------------------
#endif
    //--------------------------------------



}

bool goVideoPlayer::isPlaying()
{
    return bPlaying && bStarted;
}

//--------------------------------------------------------
void goVideoPlayer::stop()
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    StopMovie (moviePtr);
    SetMovieActive (moviePtr, false);
    bStarted = false;

    //--------------------------------------
#else
    //--------------------------------------

    gstUtils.setPaused(true);

    //--------------------------------------
#endif
    //--------------------------------------
}

//--------------------------------------------------------
void goVideoPlayer::setPixelType(goPixelType _pixelType) {
    pixelType = _pixelType;
//    if(width != 0 && height != 0) {
//        delete [] pixels;
//        delete [] offscreenGWorldPixels;
//        if ((offscreenGWorld)) DisposeGWorld((offscreenGWorld));
//        createImgMemAndGWorld();
//    }
}

//--------------------------------------------------------
goPixelType goVideoPlayer::getPixelType() {
    return pixelType;
}

//--------------------------------------------------------
void goVideoPlayer::setPan(float pan)
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    //SetMovieAudioBalance(moviePtr, pan, 0);
    //--------------------------------------
#else
    //--------------------------------------

    // what is equivalent?

    //--------------------------------------
#endif
    //--------------------------------------

}

//--------------------------------------------------------
void goVideoPlayer::setVolume(int volume)
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    SetMovieVolume(moviePtr, volume);
    //--------------------------------------
#else
    //--------------------------------------

    gstUtils.setVolume(volume);

    //--------------------------------------
#endif
    //--------------------------------------

}


//--------------------------------------------------------
void goVideoPlayer::setLoopState(int state)
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    TimeBase myTimeBase;
    long myFlags = 0L;
    myTimeBase = GetMovieTimeBase(moviePtr);
    myFlags = GetTimeBaseFlags(myTimeBase);
    switch (state)
    {
    case OF_LOOP_NORMAL:
        myFlags |= loopTimeBase;
        myFlags &= ~palindromeLoopTimeBase;
        SetMoviePlayHints(moviePtr, hintsLoop, hintsLoop);
        SetMoviePlayHints(moviePtr, 0L, hintsPalindrome);
        break;
    case OF_LOOP_PALINDROME:
        myFlags |= loopTimeBase;
        myFlags |= palindromeLoopTimeBase;
        SetMoviePlayHints(moviePtr, hintsLoop, hintsLoop);
        SetMoviePlayHints(moviePtr, hintsPalindrome, hintsPalindrome);
        break;
    case OF_LOOP_NONE:
    default:
        myFlags &= ~loopTimeBase;
        myFlags &= ~palindromeLoopTimeBase;
        SetMoviePlayHints(moviePtr, 0L, hintsLoop |
                          hintsPalindrome);
        break;
    }
    SetTimeBaseFlags(myTimeBase, myFlags);

    //--------------------------------------
#else
    //--------------------------------------

    gstUtils.setLoopState(state);

    //--------------------------------------
#endif
    //--------------------------------------

}


//---------------------------------------------------------------------------
void goVideoPlayer::setPosition(float pct)
{


    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    TimeRecord tr;
    tr.base 		= GetMovieTimeBase(moviePtr);
    long total 		= GetMovieDuration(moviePtr );
    long newPos 	= (long)((float)total * pct);
    SetMovieTimeValue(moviePtr, newPos);
    MoviesTask(moviePtr,0);

    //--------------------------------------
#else
    //--------------------------------------

    gstUtils.setPosition(pct);

    //--------------------------------------
#endif
    //--------------------------------------


}

//---------------------------------------------------------------------------
void goVideoPlayer::setFrame(int frame)
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    // frame 0 = first frame...

    // this is the simple way...
    //float durationPerFrame = getDuration() / getTotalNumFrames();

    // seems that freezing, doing this and unfreezing seems to work alot
    // better then just SetMovieTimeValue() ;

    if (!bPaused) SetMovieRate(moviePtr, X2Fix(0));

    // this is better with mpeg, etc:
    double frameRate = 0;
    double movieTimeScale = 0;
    MovieGetStaticFrameRate(moviePtr, &frameRate);
    movieTimeScale = GetMovieTimeScale(moviePtr);

    if (frameRate > 0)
    {
        double frameDuration = 1 / frameRate;
        TimeValue t = (TimeValue)ceil(frame * frameDuration * movieTimeScale); // gameover -> ceil got rid of choking on certain time values but is this accurate??
        SetMovieTimeValue(moviePtr, t);
        MoviesTask(moviePtr,0);
    }

    if (!bPaused) SetMovieRate(moviePtr, X2Fix(speed));

    //--------------------------------------
#else
    //--------------------------------------

    gstUtils.setFrame(frame);


    //--------------------------------------
#endif
    //--------------------------------------

}


//---------------------------------------------------------------------------
float goVideoPlayer::getDuration()
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    return (float) (GetMovieDuration (moviePtr) / (double) GetMovieTimeScale (moviePtr));

    //--------------------------------------
#else
    //--------------------------------------

    return gstUtils.getDuration();

    //--------------------------------------
#endif
    //--------------------------------------

}

//---------------------------------------------------------------------------
float goVideoPlayer::getPosition()
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    long total 		= GetMovieDuration(moviePtr);
    long current 	= GetMovieTime(moviePtr, nil);
    float pct 		= ((float)current/(float)total);
    return pct;

    //--------------------------------------
#else
    //--------------------------------------

    return gstUtils.getPosition();

    //--------------------------------------
#endif
    //--------------------------------------


}

//---------------------------------------------------------------------------
int goVideoPlayer::getCurrentFrame()
{
    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------
    int frame = 0;

    // zach I think this may fail on variable length frames...
    float pos = getPosition();


    float  framePosInFloat = ((float)getTotalNumFrames() * pos);
    int    framePosInInt = (int)framePosInFloat;
    float  floatRemainder = (framePosInFloat - framePosInInt);

    if (floatRemainder > 0.5f) framePosInInt = framePosInInt + 1;
    //frame = (int)ceil((getTotalNumFrames() * getPosition()));
    frame = framePosInInt;
	//cout << "pos " << pos << " frame " << frame << " framefloat: " << framePosInFloat << " frameInt " << framePosInInt << " remanider " << floatRemainder << endl;


    return frame;
    //--------------------------------------
#else
    //--------------------------------------

    return gstUtils.getCurrentFrame();

    //--------------------------------------
#endif
    //--------------------------------------

}


//---------------------------------------------------------------------------
bool goVideoPlayer::getIsMovieDone()
{

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------
    bool bIsMovieDone = (bool)IsMovieDone(moviePtr);
    return bIsMovieDone;
    //--------------------------------------
#else
    //--------------------------------------
    return gstUtils.getIsMovieDone();
    //--------------------------------------
#endif
    //--------------------------------------

}

//---------------------------------------------------------------------------
void goVideoPlayer::firstFrame()
{
    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------
    setFrame(0);
    //--------------------------------------
#else
    //--------------------------------------
    gstUtils.firstFrame();
    //--------------------------------------
#endif
    //--------------------------------------

}

//---------------------------------------------------------------------------
void goVideoPlayer::nextFrame()
{
    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------
    setFrame(getCurrentFrame() + 1);
    //--------------------------------------
#else
    //--------------------------------------
    gstUtils.nextFrame();
    //--------------------------------------
#endif
    //--------------------------------------
}

//---------------------------------------------------------------------------
void goVideoPlayer::previousFrame()
{
    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------
    setFrame(getCurrentFrame() - 1);
    //--------------------------------------
#else
    //--------------------------------------
    gstUtils.previousFrame();
    //--------------------------------------
#endif
    //--------------------------------------
}



//---------------------------------------------------------------------------
void goVideoPlayer::setSpeed(float _speed)
{

    speed 				= _speed;

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    if (bPlaying == true)
    {
        //setMovieRate actually plays, so let's call it only when we are playing
        SetMovieRate(moviePtr, X2Fix(speed));
    }

    //--------------------------------------
#else
    //--------------------------------------

    gstUtils.setSpeed(_speed);

    //--------------------------------------
#endif
    //--------------------------------------
}

//---------------------------------------------------------------------------
float goVideoPlayer::getSpeed()
{
    return speed;
}

//---------------------------------------------------------------------------
void goVideoPlayer::setPaused(bool _bPause)
{

    bPaused = _bPause;

    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    // there might be a more "quicktime-ish" way (or smarter way)
    // to do this for now, to pause, just set the movie's speed to zero,
    // on un-pause, set the movie's speed to "speed"
    // (and hope that speed != 0...)
    if (bPlaying == true)
    {
        if (bPaused == true) 	SetMovieRate(moviePtr, X2Fix(0));
        else 					SetMovieRate(moviePtr, X2Fix(speed));
    }
    //--------------------------------------
#else
    //--------------------------------------

    gstUtils.setPaused(_bPause);

    //--------------------------------------
#endif
    //--------------------------------------

}

void goVideoPlayer::togglePaused() {
	setPaused(!bPaused);
}

//------------------------------------
void goVideoPlayer::forceTextureUpload()  	// added by gameover
{

	myDrawCompleteProc = NewMovieDrawingCompleteUPP (DrawCompleteProc);
    SetMovieDrawingCompleteProc (moviePtr, movieDrawingCallWhenChanged,  myDrawCompleteProc, (long)this);

    tex.allocate(width,height,pixelType);

#if defined(TARGET_OSX) && defined(__BIG_ENDIAN__)
    convertPixels(offscreenGWorldPixels, pixels, width, height, pixelType);
#endif

    tex.loadData(pixels, width, height, pixelType);
}

//------------------------------------
void goVideoPlayer::setUseTexture(bool bUse)
{

    bUseTexture = bUse;
}

//we could cap these values - but it might be more useful
//to be able to set anchor points outside the image

//----------------------------------------------------------
void goVideoPlayer::setAnchorPercent(float xPct, float yPct)
{
    if (bUseTexture)tex.setAnchorPercent(xPct, yPct);
}

//----------------------------------------------------------
void goVideoPlayer::setAnchorPoint(int x, int y)
{
    if (bUseTexture)tex.setAnchorPoint(x, y);
}

//----------------------------------------------------------
void goVideoPlayer::resetAnchor()
{
    if (bUseTexture)tex.resetAnchor();
}

//------------------------------------
void goVideoPlayer::draw(float _x, float _y, float _w, float _h)
{
    if (bUseTexture)
    {
        tex.draw(_x, _y, _w, _h);
    }
}

//------------------------------------
void goVideoPlayer::draw(float _x, float _y)
{
    draw(_x, _y, (float)width, (float)height);
}

//------------------------------------
int goVideoPlayer::getTotalNumFrames()
{
    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------

    if(nFrames == 0)
    {
        // ------------- get the total # of frames:
        nFrames				= 0;
        TimeValue			curMovieTime;
        curMovieTime		= 0;
        TimeValue			duration;

        //OSType whichMediaType	= VIDEO_TYPE; // mingw chokes on this
        OSType whichMediaType	= FOUR_CHAR_CODE('vide');

        short flags				= nextTimeMediaSample + nextTimeEdgeOK;

        while( curMovieTime >= 0 )
        {
            nFrames++;
            GetMovieNextInterestingTime(moviePtr,flags,1,&whichMediaType,curMovieTime,0,&curMovieTime,&duration);
            flags = nextTimeMediaSample;
        }
        nFrames--; // there's an extra time step at the end of themovie
    }
    return nFrames;
    //--------------------------------------
#else
    //--------------------------------------
    return gstUtils.getTotalNumFrames();
    //--------------------------------------
#endif
    //--------------------------------------

}

//----------------------------------------------------------
float goVideoPlayer::getHeight()
{
    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------
    return (float)height;
    //--------------------------------------
#else
    //--------------------------------------
    return gstUtils.getHeight();
    //--------------------------------------
#endif
    //--------------------------------------
}

//----------------------------------------------------------
float goVideoPlayer::getWidth()
{
    //--------------------------------------
#ifdef OF_VIDEO_PLAYER_QUICKTIME
    //--------------------------------------
    return (float)width;
    //--------------------------------------
#else
    //--------------------------------------
    return gstUtils.getWidth();
    //--------------------------------------
#endif
    //--------------------------------------

}
