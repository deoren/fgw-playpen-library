// newplaypen.cpp last updated to include keyboard fixes 12/06/03
// 
// History: see end of file
// WARNING this must be statically linked and must NOT be part of a DLL

// Using stdlib.h rather than cstdlib because MSVC6 doesn't define
// cstdlib correctly (some functions not in std that should be).
#include <stdlib.h>		// For memset.
#include <cassert>
#include <stdexcept>	// For std::bad_alloc.
#include "playpen.h"
#include "mouse.h"
#include "keyboard.h"	//Inserted 12/06/03

// Platform specific headers. Use STRICT to configure windows.h for
// maximum type safety. To avoid linker errors, any other translation 
// units that include windows.h should also use STRICT.  
//#define STRICT
#include <windows.h>
#include <process.h>	// For _beginthreadex.
#include <conio.h>	// inserted 12/06/03

   namespace {
   
      using namespace studentgraphics;
   
      int const BitsPerPixel = 8; // 8 bits in 256 colours.
   
       class CopyDisabler {
      public:
          CopyDisabler(){}
      private:
         CopyDisabler(CopyDisabler const &);
         CopyDisabler & operator=(CopyDisabler const &);
      };
   
       struct HueRGB256 {
         HueRGB rgbs[colours];
         HueRGB256();
      };
   
       HueRGB256::HueRGB256() {
      // Start with whole palette set to black.
         memset(rgbs, 0, sizeof(HueRGB)*colours);
      
      // Add 216 colour "safety palette". Although not identical to 
      // the palette created by the Windows CreateHalftonePalette() 
      // function, it fulfils a similar role, and avoids the platform 
      // dependence.
         unsigned char h = 0;
         for (int r = 0x00; r < 0x100; r += 0x33) {
            for (int g = 0x00; g < 0x100; g += 0x33) {
               for (int b = 0x00; b < 0x100; b += 0x33) {
                  rgbs[h].r = r;
                  rgbs[h].g = g;
                  rgbs[h].b = b;
                  ++h;
               }
            }
         }
           // Set final palette colour to white.
         rgbs[colours-1].r = rgbs[colours-1].g = rgbs[colours-1].b = 0xFF;	
      }// HueRGB256 ctor
   
       struct Pixels {
         hue p[Ypixels][Xpixels];
          explicit Pixels(hue fillHue) { Clear(fillHue); }
          void Clear(hue fillHue) {
            memset(p[0], fillHue, Ypixels*Xpixels);
         }
      };
   
   // Platform-specific code starts here.
   
       class CriticalSection : private CopyDisabler {
      public:
         CriticalSection();
          ~CriticalSection()	{DeleteCriticalSection(&cs_);}
         void Enter();
          void Leave()		{LeaveCriticalSection(&cs_);}
      
      private:
         CRITICAL_SECTION cs_;
      };
   
       CriticalSection::CriticalSection() {
      // InitializeCriticalSection throws an OS exception in low memory
      // situations. Not all compilers will trap OS exceptions with a
      // catch(...) block, but for those that do this translates the
      // exception to something more meaningful.
         try {
            InitializeCriticalSection(&cs_);
         } 
             catch(...) {
               throw playpen::exception(playpen::exception::fatal,
                  			"Couldn't initialize critical section.");
            }
      }
   
       void CriticalSection::Enter() {
      // EnterCriticalSection can throw an OS exception in low memory
      // situations.
         try {
            EnterCriticalSection(&cs_);		
         } 
             catch(...) {
               throw playpen::exception(playpen::exception::fatal,
                  			"Couldn't enter critical section.");			
            }
      }
   
       class CSLocker : private CopyDisabler {
      public:
          CSLocker(CriticalSection& cs) : cs_(cs)	{ cs_.Enter();}
          ~CSLocker()								{ cs_.Leave();}
      
      private:
         CriticalSection&	cs_;
      };
   
   // RAII wrapper around BeginPaint/EndPaint.
       class Painter : private CopyDisabler {
      public:
         explicit Painter(HWND hWnd);
          ~Painter() { EndPaint(hWnd_, &ps_); }
          HDC				GetHDC() const { 
            return hDC_; }
          PAINTSTRUCT&	GetPAINTSTRUCT() { 
            return ps_; }
      		
      private:
         HWND		hWnd_;
         HDC			hDC_;
         PAINTSTRUCT	ps_;
      };// class Painter
   
       Painter::Painter(HWND hWnd) : hWnd_(hWnd), hDC_(BeginPaint(hWnd, &ps_)) {
         assert(hWnd);
         if (0 == hDC_) {
            throw playpen::exception(playpen::exception::error, 
               				 "Couldn't begin painting.");
         }		
      }
   
   // RAII wrapper around GetDC/ReleaseDC API calls.
       class DCLocker : private CopyDisabler {
      public:
         explicit DCLocker(HWND hWnd);
          ~DCLocker() { ReleaseDC(hWnd_, hDC_); }
          HDC GetHDC() const { 
            return hDC_; }
      
      private:
         HWND	hWnd_;
         HDC		hDC_;
      };// class DCLocker
   
       DCLocker::DCLocker(HWND hWnd) : hWnd_(hWnd), hDC_(::GetDC(hWnd)) {
         if(0 == hDC_) {
            throw playpen::exception(playpen::exception::fatal,
               				 "Couldn't get device context.");
         }
      }
   
   // RAII wrapper around thread synchronization event handles.
       class Event : private CopyDisabler {
      public:
         Event();
          ~Event() { CloseHandle(hEvent_); }
         void WaitFor(unsigned timeout);
          void Set() { SetEvent(hEvent_); }
      
      private:
         HANDLE	hEvent_;
      };// class Event
   
       Event::Event() : hEvent_(CreateEvent(0, FALSE, FALSE, 0)) {
         if(0 == hEvent_) {
            throw playpen::exception(playpen::exception::fatal,
               				 "Couldn't create synchronization event.");
         }
      }
   
       void Event::WaitFor(unsigned timeout) {
         DWORD result = WaitForSingleObject(hEvent_, timeout);
         switch (result) {
            case WAIT_OBJECT_0:	// Event was signalled normally.
               break;
            case WAIT_TIMEOUT:
               throw playpen::exception(playpen::exception::fatal, 
                  			 "Timed out waiting for event.");
            default:
               throw playpen::exception(playpen::exception::fatal, 
                  			 "Failure waiting for event.");
         }
      }
   
       class Thread : private CopyDisabler {
      public:
         typedef unsigned (__stdcall *ThreadFn)(void*);
      
          Thread() : hThread_(INVALID_HANDLE_VALUE) {}
         ~Thread();
      
      // Purpose:
      //	Start a new thread running.
      // Parameters:
      //	[in] threadFn -	A pointer to the (free standing or static
      //					member) function which will be called by the
      //					new thread and whose normal termination marks
      //					the end of the thread.
      //	[in] context -	Pointer to the context. This will be passed
      //					to the function pointed to be threadFn as its
      //					only parameter.
      // Notes:
      // 1. If the function pointed to by threadFn propagates an exception
      //	back to its caller, the behaviour is undefined.
      // 2. OK, not a particularly OOP interface. To be able to pass a
      //	functor as in boost::thread would be nice, but the amount of code
      //	required seemed like overkill for Playpen (I tried it). This class
      //	is a compromise between the completely unencapsulated Windows 
      //	thread management API functions and the full OOP deal.
      // 3. A common mistake is to pass a pointer to a local variable of
      //	the creating thread as context. Unless the creating thread joins 
      //	the new thread before the local variable goes out of scope, the
      //	local variable could be invalidated before it is used by the new
      //	thread.
         void Run(ThreadFn threadFn, void* context); 
      
      // Purpose:
      //	Wait for the new thread to finish.
      // Parameters:
      //	[in] timeout -	The time, in milliseconds, to wait for the new
      //					thread to finish. If this time is exceeded and the
      //					new thread has still not finished, a 
      //					Playpen::exception is thrown.
      // Exceptions:
      //	Playpen::exceptions are thrown if the Join fails for whatever 
      //	reason.
         void Join(unsigned timeout);
      
      private:
         HANDLE				hThread_;
      };// class Thread
   
       Thread::~Thread() {
      // Shouldn't destroy a running thread.
         assert(INVALID_HANDLE_VALUE == hThread_);
      }
   
       void Thread::Run(Thread::ThreadFn threadFn, void* context) {
      // Create a thread. The _beginthreadex function is used in preference 
      // to the CreateWindow function because it deals correctly with 
      // initialization and clean-up of the runtime library on the new
      // thread. Even though a thread may not appear to use many run-time 
      // library functions, they are usually there: exception handling 
      // and dynamic allocation, for instance. Using beginthreadex_ has 
      // the additional advantage that it causes a BUILD ERROR if not 
      // linked with a multi-threaded runtime library, thus "drawing 
      // attention" to a likely build configuration error.
         unsigned thid;
         hThread_ = (HANDLE) _beginthreadex(0, 0, threadFn, 
            						   context, 0, &thid);
         if (!hThread_) {
            throw playpen::exception(playpen::exception::error,
               				"Could not begin thread.");
         }
      }// Thread::Run
   
       void Thread::Join(unsigned timeout) {
         DWORD result = WaitForSingleObject(hThread_, timeout);
         switch (result) {
            case WAIT_OBJECT_0:	// Thread finished normally.
               break;
            case WAIT_TIMEOUT:
               throw playpen::exception(playpen::exception::error,
                  				"Timed out waiting for thread.");
            default:
               throw playpen::exception(playpen::exception::error,
                  				"Failure waiting for thread.");				
         }
         CloseHandle(hThread_);
         hThread_ = INVALID_HANDLE_VALUE;
      }// Thread::Join
   
      int const LogPaletteVersion     = 0x0300; // Has to be this value.
   
   // RAII wrapper around HPALETTE.
       class Palette : private CopyDisabler {
      public:
         explicit Palette(HueRGB256 const & rgbs);
         ~Palette();
         void SetEntry(hue h, HueRGB const & rgb);
         void SetEntries(HueRGB256 const & rgbs);
         HueRGB GetEntry(hue h) const;
          HPALETTE GetHPALETTE() const { 
            return hPalette_; }
      
      private:
      // A 256 colour specific version of the LOGPALETTE structure.
          struct LogPalette256 {
            WORD         palVersion; 
            WORD         palNumEntries; 
            PALETTEENTRY palPalEntry[colours]; 
         };
      
         HPALETTE		hPalette_;
      };// class Palette
   
       Palette::Palette(HueRGB256 const & rgbs) {
         LogPalette256 logPalette;
      
      // Set to all black.
         FillMemory(&logPalette, sizeof(logPalette), 0);
         logPalette.palVersion		= LogPaletteVersion;
         logPalette.palNumEntries	= colours;
      
         hPalette_ = CreatePalette((LOGPALETTE*)&logPalette);
         if (0 == hPalette_) {
            throw playpen::exception(playpen::exception::fatal,
               				 "Couldn't create palette.");
         }
      
         SetEntries(rgbs);
      }// Palette ctor
   
       Palette::~Palette() {
         DeleteObject((HGDIOBJ)hPalette_);
      }
   
       void Palette::SetEntry(hue h, HueRGB const & rgb) {
         PALETTEENTRY entry;
         entry.peRed		= rgb.r;
         entry.peGreen	= rgb.g;
         entry.peBlue	= rgb.b;
         entry.peFlags	= 0;
         if (0 == SetPaletteEntries(hPalette_, h, 1, &entry)) {
            throw playpen::exception(playpen::exception::error,
               				"Couldn't set palette entry.");
         }
      }
   
       void Palette::SetEntries(HueRGB256 const & rgbs) {
         LogPalette256 logPalette;
         for (unsigned int h = 0; h < colours; ++h) {
            logPalette.palPalEntry[h].peRed		= rgbs.rgbs[h].r;
            logPalette.palPalEntry[h].peGreen	= rgbs.rgbs[h].g;
            logPalette.palPalEntry[h].peBlue	= rgbs.rgbs[h].b;
            logPalette.palPalEntry[h].peFlags	= 0;
         } 
         if (0 == SetPaletteEntries(hPalette_, 0, colours,
         					   logPalette.palPalEntry)) {
            throw playpen::exception(playpen::exception::error,
               				"Couldn't set palette entries.");
         }
      }
   
       HueRGB Palette::GetEntry(hue h) const {
         PALETTEENTRY pe;
         if (0 == GetPaletteEntries(hPalette_, h, 1, &pe)) {
            throw playpen::exception(playpen::exception::error,
               				"Couldn't get palette entry.");
         }
         return HueRGB(pe.peRed, pe.peGreen, pe.peBlue);
      }
   
   // RAII wrapper around palette selection.
       class PaletteSelection : private CopyDisabler {
      public:
      // Purpose:
      //	Create an active palette selection i.e. a selection of a
      //	a palette into a device context.
      // Parameters:
      //	[in] palette -	The palette to select into the device context. 
      //					The palette object must stay alive until this 
      //					object is destroyed.
      //	[in] hDC -	Handle to the device context into which the palette
      //				will be selected. This HDC must stay valid until the
      //				object destruction.
      //	[in] forceRemap -	Forces the logical palette, as represented by
      //						the Palette object, to be remapped to the
      //						physical palette, internal to GDI. Mapping 
      //						happens the first time a Palette object is
      //						selected anyway, but subsequent selections do 
      //						not remap unless forceRemap is true or if the
      //						physical palette has changed. 
         PaletteSelection(Palette const & palette, HDC hDC, 
         			 bool forceRemap = false);
      
         ~PaletteSelection(); 
          bool MappingChanged() const { 
            return mappingChanged_; }
      
      private:
         HPALETTE		hOldPalette_;
         HDC				hRealizingDC_;
         bool			mappingChanged_;
      };// class PaletteSelection
   
       PaletteSelection::PaletteSelection(Palette const & palette, HDC hDC, 
       							   bool forceRemap) :
       hOldPalette_(0), hRealizingDC_(hDC), mappingChanged_(false) { 
         assert(hDC);
         hOldPalette_ = SelectPalette(hDC, palette.GetHPALETTE(), FALSE);
         if (0 == hOldPalette_) {
            throw playpen::exception(playpen::exception::error,
               		"Couldn't select palette into device context.");
         }
         if (forceRemap) {
            UnrealizeObject((HGDIOBJ)palette.GetHPALETTE());
         }
         UINT changedEntryCount = RealizePalette(hDC);
         if (GDI_ERROR == changedEntryCount) {
            SelectPalette(hDC, hOldPalette_, FALSE);
            throw playpen::exception(playpen::exception::error,
               				 "Couldn't realize palette.");
         }
         mappingChanged_ = changedEntryCount ? true : false;
      }// PaletteSelection ctor
   
       PaletteSelection::~PaletteSelection() {
         SelectPalette(hRealizingDC_, hOldPalette_, FALSE);
      }
   
   // a 256 colour specific of the BITMAPINFO structure
       struct BitmapInfo256 {
         BITMAPINFOHEADER bmiHeader;
      // These entries hold indices into the logical palette and
      // are initialized to identity, hopefully avoiding some internal
      // GDI palette manipulation.
         short			 bmiColors[colours]; // we are using 256 colours!
      };
   
   // RAII wrapper for memory device contexts. A memory device context is
   // a temporary holder for a device dependent bitmap, through which the
   // bitmap can be copied to any other compatible device context.
       class MemoryDC : private CopyDisabler {
      public:
      // Parameters:
      //	[in] sourceDC -	Handle to the device context whose format should
      //					be used for the new memory device context.
         explicit MemoryDC(HDC sourceDC);
         ~MemoryDC();
          HDC GetHDC() const { 
            return hMemDC_; }
      
      private:
         HDC hMemDC_;
      };// class MemoryDC
   
       MemoryDC::MemoryDC(HDC sourceDC) : 
       hMemDC_(CreateCompatibleDC(sourceDC)) {
         assert(sourceDC);
         if (0 == hMemDC_) {
            throw playpen::exception(playpen::exception::error,
               				"Couldn't create memory DC.");
         }
      }
   
       MemoryDC::~MemoryDC() {
         DeleteDC(hMemDC_);
      }
   
   // RAII class for holding a device-dependent bitmap. A device-dependent 
   // bitmap is one whose pixel format is the same as a particular device 
   // context. Thus it will convert the device-independent 256 colour
   // paletted bitmap it is supplied with to a device-dependent bitmap in
   // whatever format the system is currently using.
       class Bitmap : private CopyDisabler {
      public:
         Bitmap();
      
      // Parameters:
      //	As for Reset().
         Bitmap(HDC hModelDC, unsigned width, unsigned height, 
         void const * bits = 0);
      
         ~Bitmap();
          HBITMAP GetHBITMAP() const { 
            return hBitmap_; }
      
      // Purpose:
      //	Reset the bitmap.
      // Parameters:
      //	[in] hModelDC -	A handle to the device context whose format
      //					should be used by the newly reset bitmap.
      //	[in] width -	The width of the newly reset bitmap in pixels.
      //					Must be a multiple of 4.
      //	[in] height -	The height of the newly reset bitmap in pixels.
      //	[in, opt] bits -	If present and non-zero, this is a pointer
      //						to the bits that should be copied to the
      //						newly reset bitmap. Regardless of the 
      //						format of hModelDC, these bits are always
      //						in 256 colour, 1 byte per pixel format. If
      //						the parameter is omitted or a NULL pointer
      //						passed, the pixel values are not set.
         void Reset(HDC hModelDC, unsigned width, unsigned height,
         void const * bits = 0);	// Non-zero bits inits pixel vals.
         void SetBits(HDC hModelDC, void const * bits);
      
      private:
         BitmapInfo256	bitmapInfo_;
         HBITMAP			hBitmap_;
      };// class Bitmap
   
       Bitmap::Bitmap() : hBitmap_(0) {
         DCLocker dcLocker(0);	// Get screen DC.
         Reset(dcLocker.GetHDC(), 1, 1, 0);
      }
   
       Bitmap::Bitmap(HDC hModelDC, unsigned width, unsigned height, 
       		   void const * bits) : hBitmap_(0) {
         Reset(hModelDC, width, height, bits);
      }
   
       Bitmap::~Bitmap() {
         DeleteObject((HGDIOBJ)hBitmap_);
      }
   
       void Bitmap::Reset(HDC hModelDC, unsigned width, unsigned height,
       void const * bits) {
         assert(hModelDC);
         assert(width);
         assert(height);	
      	
      // Initialise the bitmap header information.
         BitmapInfo256 bitmapInfo;
         bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
         bitmapInfo.bmiHeader.biWidth         = width;
      // biHeight is -ve to indicate origin is top left.
         bitmapInfo.bmiHeader.biHeight        = -static_cast<int>(height);
         bitmapInfo.bmiHeader.biPlanes        = 1;
         bitmapInfo.bmiHeader.biBitCount      = BitsPerPixel;
         bitmapInfo.bmiHeader.biCompression   = BI_RGB;	// No compression.
         bitmapInfo.bmiHeader.biSizeImage     = 0;		// Valid - see docs.
         bitmapInfo.bmiHeader.biXPelsPerMeter = 3000;	// Arbitary.
         bitmapInfo.bmiHeader.biYPelsPerMeter = 3000;	// Arbitary.
         bitmapInfo.bmiHeader.biClrUsed       = 0;		// Valid - see docs.
         bitmapInfo.bmiHeader.biClrImportant  = 0;		// As above.
      
      // Set up identity palette mapping.
         for (unsigned int c = 0 ; c < colours ; c++){
            bitmapInfo.bmiColors[c] = c;
         }
      
      // Despite its name (and some of the documentation), this function 
      // actually creates a device-dependent bitmap.
         HBITMAP hBitmap = CreateDIBitmap(
            hModelDC,		// Use pixel format of this device contxt.
            (BITMAPINFOHEADER const *) &bitmapInfo, 
            bits ? CBM_INIT : 0, // Initialize pixels if pixel data supplied.
            bits, 
            (BITMAPINFO const *) &bitmapInfo, 
            DIB_PAL_COLORS);	// Palette is mapping, not RGB values.
         if (0 == hBitmap) {
            throw playpen::exception(playpen::exception::error,
               				"Can't create bitmap.");
         }
      
      // Exception safe no-throw section to modify object state.
         bitmapInfo_ = bitmapInfo;
         if (hBitmap_) {
            DeleteObject((HGDIOBJ)hBitmap_);
         }		
         hBitmap_ = hBitmap;
      }// Bitmap::Reset
   
       void Bitmap::SetBits(HDC hModelDC, void const * bits) {
         assert(hModelDC);
         assert(bits);
      
         int result = SetDIBits(
            hModelDC,	// Use pixel format of this device context. 
            hBitmap_,
            0,
            -bitmapInfo_.bmiHeader.biHeight,	// biHeight is -ve, remember.
            bits,
            (BITMAPINFO const *) &bitmapInfo_,
            DIB_PAL_COLORS);
         if (0 == result) {
            throw playpen::exception(playpen::exception::error,
               				"Can't set bitmap bits.");
         }
      }// Bitmap::SetBits
   
   
   // Device dependent bitmap "selection" wrapper. Selection is the
   // temporary association of a graphical object with a device context.
       class BitmapSelection : private CopyDisabler {
      public:
         BitmapSelection(Bitmap const & bitmap, MemoryDC const & memDC);
          ~BitmapSelection() { SelectObject(hDC_, (HGDIOBJ) hOldBitmap_); }
      
      private:
         HBITMAP hOldBitmap_;
         HDC		hDC_;
      };// class BitmapSelection
   
       BitmapSelection::BitmapSelection(Bitmap const & bitmap, MemoryDC const & memDC) :
       hOldBitmap_((HBITMAP) SelectObject(memDC.GetHDC(), 
       									(HGDIOBJ) bitmap.GetHBITMAP())),
       hDC_(memDC.GetHDC()) {
         if (0 == hOldBitmap_) {
            throw playpen::exception(playpen::exception::error,
               				"Can't select bitmap into DC.");
         }
      }
   // INSERTED 12/06/03 from Garry's version
   // RAII wrapper for a Windows Window Class, which is nothing to do with
   // a C++ class. It is more like a blueprint for a collection of windows.
       class WindowClass {
      public:
         explicit WindowClass(const WNDCLASS& wndClass);
         ~WindowClass();
      
      private:
         ATOM atom_;
      };// class WindowClass
   
   /*explicit*/ 
       WindowClass::WindowClass(const WNDCLASS& wndClass) :
       atom_(RegisterClass(&wndClass)) {
      
         if (!atom_) {
            throw playpen::exception(playpen::exception::fatal, 
               				"RegisterClass API function failed.");
         }
      }
   
       WindowClass::~WindowClass() {
         UnregisterClass(MAKEINTATOM(atom_), GetModuleHandle(0));
      }
   
       HANDLE GetStdIn() {
         HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
         if (INVALID_HANDLE_VALUE == hStdIn) {
            throw playpen::exception(playpen::exception::error,
               "GetStdHandle API call failed");
         }
         return hStdIn;
      }
   
       bool UnreadInputEvents(HANDLE hStdIn) {
         DWORD eventCount = 0;
      
         if (!GetNumberOfConsoleInputEvents(hStdIn, &eventCount)) {
            throw playpen::exception(playpen::exception::error,
               "GetNumberOfConsoleInputEvents API call failed");
         }
      
         return (0 != eventCount);
      }
   
       INPUT_RECORD ReadInputEvent(HANDLE hStdIn) {
         DWORD			recordsRead = 0;
         INPUT_RECORD	result;
      		
         if (!ReadConsoleInput(hStdIn, &result, 1, &recordsRead)) {
            throw playpen::exception(playpen::exception::error,
               "ReadConsoleInput API call failed");
         }
      
         if (!recordsRead) {
            throw playpen::exception(playpen::exception::error,
               "ReadInputEvent read 0 records");
         }
      
         return result;
      }// ReadInputEvent
   
       bool IsModifierKey(WPARAM virtualKeyCode) {
         switch (virtualKeyCode) {
            case VK_SHIFT:		// FALLTHRU
            case VK_CONTROL:	// FALLTHRU
            case VK_MENU:		// FALLTHRU
            case VK_CAPITAL:	// FALLTHRU
            case VK_NUMLOCK:
               return true;
         
            default:
               return false;
         }
      }// IsModifierKey
   
       int SetCharacterBits(WPARAM virtualKeyCode, int keys) {
         int result = keys;
      
         assert(virtualKeyCode);
      
         result &= modifier_bits;
         result |= virtualKeyCode;	
      
         return result;
      }
   
       int GetCharacterBits(int keys) {
         return keys & character_bits;
      }
   
       int GetModifierBits(int keys) {
         return keys & modifier_bits;
      }
   
       int OnKeyDownEvent(WPARAM virtualKeyCode, DWORD controlKeys, int keys) {
         int result = keys;
      
         if (!IsModifierKey(virtualKeyCode)) {
            int curModifierBits		= GetModifierBits(result);
            unsigned int curCharacterBits	= GetCharacterBits(result);
            int modifierBits		= 0;
         
            if (controlKeys & CAPSLOCK_ON) {
               modifierBits |= modifier_caps_lock;
            }
            if (controlKeys & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
               modifierBits |= modifier_alt;
            }
            if (controlKeys & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
               modifierBits |= modifier_control;
            }
            if (controlKeys & NUMLOCK_ON) {
               modifierBits |= modifier_num_lock;
            }
            if (controlKeys & SHIFT_PRESSED) {
               modifierBits |= modifier_shift;
            }
         
            if (curCharacterBits) {
               if (virtualKeyCode != curCharacterBits ||
               modifierBits != curModifierBits) {
               // New key press is not the same as previous, so cannot
               // be sensibly ignored.
                  result = SetCharacterBits(key_multiple, result);
               }
            } 
            else {
               result = SetCharacterBits(virtualKeyCode, result);
            }
         
            result |= modifierBits;
         }
      
         return result;
      }// OnKeyDownEvent
   // END OF INSERT
   
   // constants
      int const InitTimeout  = 10000;		// 10s for system to initialise.
      int const DestroyTimeout = 10000;	// Same for thread/window destruction.
   
   // The platform-specific SingletonWindowImpl class. As long as you
   // maintain the semantics of the public interface, you can implement
   // this on non-Windows platforms and keep all other classes the same.
       class SingletonWindowImpl : private CopyDisabler {
      public:
      // All public functions are only called by public thread, not
      // worker thread.
      
      // Parameters are not cached. To update display after changes call
      // Display() and/or UpdatePalette().
         SingletonWindowImpl(Pixels const & pixels, HueRGB256 const & hueRGBs);
      
         ~SingletonWindowImpl();
      
      // Purpose:
      //	Update the display to reflect a change in the pixels array.
         void Display(Pixels const & pixels);
      
      // Purpose:
      //	Update the display to reflect a change in the colour palette.
         void UpdatePalette(Pixels const & pixels, HueRGB256 const & hueRGBs);
      
      // GSL: Added for mouse support.
         mouse::location GetMouseLocation() const;
         bool IsMouseButtonDown() const;
      // INSERT
      // GSL: Added for GUI window keyboard support.
         int KeyPressed();
      // END INSERT
      private:
      // Called by public thread only. Requires sharedStateLock_ held.
         void CheckForPendingException();
      
      // Called by worker thread only.
         static LRESULT CALLBACK WindowProcForwarder(HWND,UINT,WPARAM,LPARAM);
         static unsigned __stdcall WorkerThreadForwarder(void*);
         LRESULT WindowProc(HWND,UINT,WPARAM,LPARAM);
         void WorkerThread();
         void DoPaint();
         void ReRealizePalette();
         void UpdateMouseButtons(WPARAM w);
         void UpdateMouseLocation(WPARAM w, int x, int y);
         void UpdateKeyPressed(WPARAM w); // inserted 12/06/03
      
      // Called by either thread. Requires sharedStateLock_ is already held.
         void Draw(HDC hDC, RECT const * rect = 0);
      
         Thread					thread_;
         int						key_;  // inserted 12/06/03
         HWND					hWnd_;
         Bitmap					bitmap_;
         Event					readyEvent_;
         mutable CriticalSection	sharedStateLock_;
         Palette					palette_;
         playpen::exception		pendingException_;
         bool					hasPendingException_;
         bool					mouseButtonDown_;
         mouse::location			mouseLocation_;
      
      // Used by WindowProcForwarder.
         static SingletonWindowImpl* instance_;
      };// class SingletonWindowImpl
   
   /*static*/ SingletonWindowImpl* SingletonWindowImpl::instance_;
   
       SingletonWindowImpl::SingletonWindowImpl(
       Pixels const & pixels, HueRGB256 const & hueRGBs) :
       key_(0),		// inserted 12/06/03
       hWnd_(0),
       palette_(hueRGBs),
       hasPendingException_(false),
       mouseButtonDown_(false) {
      
         mouseLocation_.x(-1); mouseLocation_.y(-1);	  // Outside window.
      
      // Initialize bitmap.
         {
            DCLocker dcLocker(0);	// Get screen DC.
            PaletteSelection ps(palette_, dcLocker.GetHDC());
            bitmap_.Reset(dcLocker.GetHDC(), Xpixels, Ypixels, pixels.p[0]);
         }
      
      // Start a worker thread to create the play pen window and run its
      // message loop.
         thread_.Run(WorkerThreadForwarder, this);
      
         readyEvent_.WaitFor(InitTimeout);
      }// SingletonWindowImpl ctor
   
       SingletonWindowImpl::~SingletonWindowImpl() {
      // Send a WM_CLOSE message, which in turn sends a WM_DESTROY message,
      // which in turn *posts* (i.e. asynchronous) a WM_QUIT message, then
      // wait for the worker thread to completely finish. 
         SendMessage(hWnd_, WM_CLOSE, 0, 0);
         thread_.Join(DestroyTimeout);
      }
   
       void SingletonWindowImpl::Display(Pixels const & pixels){
         CSLocker lock(sharedStateLock_);
         CheckForPendingException();
         DCLocker dcLocker(hWnd_);
         PaletteSelection ps(palette_, dcLocker.GetHDC());
         bitmap_.SetBits(dcLocker.GetHDC(), pixels.p[0]);
         Draw(dcLocker.GetHDC());
      }
   
       void SingletonWindowImpl::UpdatePalette(Pixels const & pixels, 
       									HueRGB256 const & hueRGBs) {
         CSLocker lock(sharedStateLock_);
         CheckForPendingException();
         palette_.SetEntries(hueRGBs);
         DCLocker dcLocker(hWnd_);
         PaletteSelection ps(palette_, dcLocker.GetHDC(), true);
      // Under Windows, changing the logical palette does not necessarily
      // change the physical palette (could be running in true colour
      // mode for a start), so we need to recreate the device-depenedent
      // bitmap and draw the window at this point.
         bitmap_.SetBits(dcLocker.GetHDC(), pixels.p[0]);
         Draw(dcLocker.GetHDC(), 0);
      }
   					
       mouse::location SingletonWindowImpl::GetMouseLocation() const {
         CSLocker lock(sharedStateLock_);
         return mouseLocation_;
      }
   
       bool SingletonWindowImpl::IsMouseButtonDown() const {
         CSLocker lock(sharedStateLock_);
         return mouseButtonDown_;
      }
   
   
   //INSERT 12/06/03
       int SingletonWindowImpl::KeyPressed() {
         CSLocker lock(sharedStateLock_);
         int result = key_;
         key_ = 0;
         return result;
      }
   //END INSERT
   /*static*/ 
       unsigned __stdcall SingletonWindowImpl::WorkerThreadForwarder(
       void* untypedImpl) {
      
         SingletonWindowImpl* impl = static_cast<SingletonWindowImpl*>(untypedImpl);
         impl->WorkerThread();
         return 0;
      }
   
       void SingletonWindowImpl::WorkerThread(){
         try {
         // Create a window class. 
            char const * const WindowClassName = "Playpen Window Class";
            WNDCLASS wc;
            FillMemory(&wc, sizeof(wc), 0);
            wc.lpfnWndProc		= WindowProcForwarder;
         // GetModuleHandle(0) retrieves HINSTANCE of this executable.
            wc.hInstance		= GetModuleHandle(0);
            wc.hIcon			= LoadIcon(NULL, IDI_APPLICATION);
            wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
            wc.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
            wc.lpszClassName	= WindowClassName;
            wc.style			= CS_HREDRAW | CS_VREDRAW;
            WindowClass windowClass(wc);
         
         // Create a window with size
         // Y:	desired client size (Ypixels) + caption + 2 borders
         // X:	desired client width (Xpixels) + 2 borders
            int requiredHeight = GetSystemMetrics(SM_CYCAPTION) + 
               			 GetSystemMetrics(SM_CYFIXEDFRAME)*2 + Ypixels;
            int requiredWidth  = GetSystemMetrics(SM_CXFIXEDFRAME)*2 + Xpixels;
            instance_ = this;
            hWnd_ = CreateWindow(WindowClassName, "Playpen", WS_OVERLAPPED, 
               CW_USEDEFAULT, CW_USEDEFAULT, requiredWidth, requiredHeight, 
               0, 0, GetModuleHandle(0), 0);
         
            if (hWnd_ == 0) {
               throw playpen::exception(playpen::exception::fatal, 
                  				"Can't initialise Window.");
            }
         
         // Start us off.
            UpdateWindow(hWnd_);
            ShowWindow(hWnd_, SW_SHOW);
            // Signal to main thread that window creation was successful.
            readyEvent_.Set(); // inserted 03/08/2005 as part of race condition fix
         
         // Run the Windows message loop. This only terminates when the
         // last playpen is destroyed and is the reason we need a worker 
         // thread: to isolate the event driven-ness of Windows from the 
         // 'simple' user.
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0)){
               TranslateMessage(&msg);
               DispatchMessage(&msg);
            }			
         } 
             catch(playpen::exception const & ex) {
            // Create a pending exception for the main thread to pick up
            // the next time it checks.
               CSLocker lock(sharedStateLock_);
               pendingException_ = ex;
               hasPendingException_ = true;
            }
      }// SingletonWindowImpl::WorkerThread
   
   /*static*/ 
       LRESULT CALLBACK SingletonWindowImpl::WindowProcForwarder(
       HWND h, UINT m, WPARAM w, LPARAM l) {
         return SingletonWindowImpl::instance_->WindowProc(h, m, w, l);
      }
   
       LRESULT SingletonWindowImpl::WindowProc(HWND h, UINT m, WPARAM w, LPARAM l) {
         LRESULT rtn = 0;
         try {
            switch (m){
               //case WM_CREATE: deleteded as part of bug fix 03/08/2005// Sent as part of CreateWindow() call.
               case WM_PAINT: // Part of the window needs repainting.
                  DoPaint(); 
                  break;
               case WM_QUERYNEWPALETTE:
               // Our window is becoming active and the system is giving
               // us a chance to use our preferred palette.
                  ReRealizePalette();
                  rtn = TRUE;
                  break;
               case WM_PALETTECHANGED:
                  if ((HWND) w != hWnd_) {
                  // Another window has become active and changed the 
                  // palette. We have a chance to rematch our palette
                  // as best we can to the new system palette.
                     ReRealizePalette();
                  }
                  break;
               case WM_DESTROY: 
                  PostQuitMessage(0);
                  break;
               case WM_LBUTTONDOWN:	// FALLTHRU
               case WM_LBUTTONUP:		// FALLTHRU
               case WM_MBUTTONDOWN:	// FALLTHRU
               case WM_MBUTTONUP:		// FALLTHRU
               case WM_RBUTTONDOWN:	// FALLTHRU
               case WM_RBUTTONUP:
                  UpdateMouseButtons(w);
                  rtn = DefWindowProc(h, m, w, l);
                  break;
               case WM_MOUSEMOVE:
                  UpdateMouseLocation(w, LOWORD(l), HIWORD(l));
                  rtn = DefWindowProc(h, m, w, l);
                  break;
               case WM_CAPTURECHANGED:
               // Another window stole the mouse capture off us!
                  UpdateMouseLocation(0, -1, -1);
                  rtn = DefWindowProc(h, m, w, l);
                  break;
               case WM_SYSKEYDOWN:		// FALLTHRU
               case WM_KEYDOWN:
                  UpdateKeyPressed(w);
                  rtn = DefWindowProc(h, m, w, l);
                  break;
               default:
                  rtn = DefWindowProc(h, m, w, l);
                  break;
            }
         } 
             catch(playpen::exception const & ex) {
               CSLocker lock(sharedStateLock_);
               pendingException_		= ex;
               hasPendingException_	= true;
            }
         return rtn;
      }// SingletonWindowImpl::WindowProc
   
       void SingletonWindowImpl::CheckForPendingException() {
         if (hasPendingException_) {
            hasPendingException_ = false;
            throw pendingException_;
         }
      }
   
       void SingletonWindowImpl::UpdateMouseButtons(WPARAM w) {
         CSLocker lock(sharedStateLock_);
         if (0 == (w & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON))) {
            mouseButtonDown_ = false;
         } 
         else {
            mouseButtonDown_ = true;
         }
      }
   
       void SingletonWindowImpl::UpdateMouseLocation(WPARAM w, int x, int y) {
         CSLocker	lock(sharedStateLock_);
      
         POINT	clientPt	= {x, y};
         RECT	clientRect	= {0, 0, Xpixels, Ypixels};
         POINT   screenPt	= {x, y};
      
         if (!ClientToScreen(hWnd_, &screenPt)) {
            throw playpen::exception(playpen::exception::error,
               "ClientToScreen failed in "
               "SingletonWindowImpl::UpdateMouseLocation");
         }
      
         if (PtInRect(&clientRect, clientPt) && 
         hWnd_ == WindowFromPoint(screenPt)) {
         // Mouse is inside window client area.
            if (mouseLocation_.x() == -1 && mouseLocation_.y() == -1) {
            // Mouse is not currently captured. Capture it so that we will
            // keep receiving mouse move messages even when the mouse
            // moves outside the window. (When that happens we immediately
            // release the capture, but the point is we want to know when
            // it happens.)
               SetCapture(hWnd_);
            
            // Ensure that we recognise a pressed mouse button if it was
            // pressed outside the window yet held so that it is still
            // down when entering the window.
               if (0 != (w & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON))) {
                  mouseButtonDown_ = true;
               }
            }
         
            mouseLocation_.x(x);
            mouseLocation_.y(y);
         } 
         else {
         // Mouse is outside window client area.
            if (mouseLocation_.x() != -1 && mouseLocation_.y() != -1) {
            // Mouse is currently captured. Be a good citizen by releasing
            // it for other windows to receive mouse information, and 
            // reset our mouse information.
               ReleaseCapture();
               mouseLocation_.x(-1);
               mouseLocation_.y(-1);
               mouseButtonDown_ = false;
            }
         }
      }// SingletonWindowImpl::UpdateMouseLocation
   
   //INSERT 12/06/03
       void SingletonWindowImpl::UpdateKeyPressed(WPARAM w) {
         enum { KeyDown = 0x8000, KeyToggled = 0x0001 };
      
      // Translate control keys to the console control key codes.
         DWORD controlKeys = 0;
         if (KeyToggled & GetKeyState(VK_CAPITAL)) {
            controlKeys |= CAPSLOCK_ON;
         }
         if (KeyDown & GetKeyState(VK_MENU)) {
         // At least one of the Alt keys is pressed.
            controlKeys |= LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED;
         }
         if (KeyDown & GetKeyState(VK_CONTROL)) {
         // At least one of the Ctrl keys is pressed.
            controlKeys |= LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED;
         }
         if (KeyDown & GetKeyState(VK_SHIFT)) {
            controlKeys |= SHIFT_PRESSED;
         }
         if (KeyToggled & GetKeyState(VK_NUMLOCK)) {
            controlKeys |= NUMLOCK_ON;
         }
      
         CSLocker lock(sharedStateLock_);
         key_ = OnKeyDownEvent(w, controlKeys, key_);		
      }// SingletonWindowImpl::UpdateKeyPressed
   //END INSERT
   
       void SingletonWindowImpl::ReRealizePalette() {
         CSLocker lock(sharedStateLock_);
         DCLocker dcLocker(hWnd_);
         PaletteSelection ps(palette_, dcLocker.GetHDC());
         if (ps.MappingChanged()) {
         // Cause WM_PAINT message to be received eventually.
            InvalidateRect(hWnd_, 0, FALSE);
         }
      }
   
       void SingletonWindowImpl::DoPaint(){
         CSLocker lock(sharedStateLock_);
         Painter painter(hWnd_);
         Draw(painter.GetHDC(), &painter.GetPAINTSTRUCT().rcPaint);
      }
   
   // Draw the bitmap to the screen. If we don't need a full draw, the rect
   // will be non-null, and define the area that needs drawing.
       void SingletonWindowImpl::Draw(HDC hDC, RECT const * rect){
         PaletteSelection psWindow(palette_, hDC);
         MemoryDC memDC(hDC);
         BitmapSelection bitmapSelection(bitmap_, memDC);
         if (rect){
            BitBlt(hDC, rect->left, rect->top, 
               rect->right - rect->left, 
               rect->bottom - rect->top,
               memDC.GetHDC(), rect->left, rect->top, SRCCOPY);
         }
         else
         {
            BitBlt(hDC, 0, 0, Xpixels, Ypixels, memDC.GetHDC(), 
               0, 0, SRCCOPY);
         }
      }// SingletonWindowImpl::Draw
   			
   }// anonymous namespace

// Platform-specific implementation of Wait.
    void studentgraphics::Wait(unsigned ms) {
      ::Sleep(ms);
   }


// Plaform-specific code ends here. From now on it's platform
// independent code until the end of the file.


   namespace studentgraphics {
   
      namespace detail {
      
      ///////////////////////////////////////////////////////////////////
      // SingletonWindow.
      //
      // The middle layer of the system, representing the platform-
      // independent output window. playpen is a wrapper round this 
      // allowing you to construct as many Playpens as you like, and 
      // only have one output window.
          class SingletonWindow : private CopyDisabler {
         public:
         // These two control lifetime of singleton using reference count.
            static SingletonWindow*	GetWindow(hue background);
            void					ReleaseWindow();
         
         // Drawing functions.
            void	Plot(int x, int y, hue, plotmode);
             void	Display() { impl_.Display(pixels_); }
            void 	Clear();
            void	Clear(hue);
         
         // Palette handling.
             void    UpdatePalette() { impl_.UpdatePalette(pixels_, hueRGBs_); }
            void	SetPaletteEntry(hue, HueRGB const &);
            HueRGB 	GetPaletteEntry(hue);
         
         // Serialization.
            ostream& Save(ostream&);
            istream& Restore(istream &);
         
         // GSL: Added for MiniPNG support.
            hue GetPixel(int x, int y) const;
         
         // GSL: Added for mouse support.
             mouse::location GetMouseLocation() const 
            { 
               return impl_.GetMouseLocation(); }
             bool IsMouseButtonDown() const 
            { 
               return impl_.IsMouseButtonDown(); }
         // INSERT 12/06/03
         // GSL: Added for GUI keyboard support.
             int KeyPressed()
            { 
               return impl_.KeyPressed(); }
         
         private:
         // Public interface to construction/destruction is GetWindow/
         // ReleaseWindow.
            SingletonWindow(hue);
         
         // Construction order of impl_ relative to other members is 
         // important. DO NOT CHANGE.
            Pixels				pixels_;
            HueRGB256			hueRGBs_;
            SingletonWindowImpl	impl_;
            hue 				background_;
         
            static unsigned			refCount_;
            static SingletonWindow*	instance_;
         };// class SingletonWindow
      
      /*static*/ unsigned SingletonWindow::refCount_ = 0;
      /*static*/ SingletonWindow* SingletonWindow::instance_ = 0;
      
          void SingletonWindow::Clear(){ pixels_.Clear(background_);}
          void SingletonWindow::Clear(hue h){ pixels_.Clear(h);}
      
          SingletonWindow* SingletonWindow::GetWindow(hue background) {
            if (0 == refCount_) {
               instance_ = new SingletonWindow(background);
               if (!instance_) { // Support MSVC6 non-standard new behaviour.
                  throw std::bad_alloc();
               }
            }	 
            ++refCount_;
            return instance_;
         }
      
          void SingletonWindow::ReleaseWindow() {
            if (0 == --refCount_) {
               delete instance_;
               instance_ = 0;
            }
         }
      
          SingletonWindow::SingletonWindow(hue background) :
          pixels_(background),
          impl_(pixels_, hueRGBs_),
          background_(background) {}
      
      // Simply set the appropriate location in the array.
          void SingletonWindow::Plot(int x, int y, hue c, plotmode pm) {
            if (x < 0 || x >= Xpixels) 
               return; // if out of bounds, ignore
            if (y < 0 || y >= Ypixels) 
               return; // i.e. it is not an error
         // the above is cleanest here as it allows easy scaling at top level
            switch (pm) {
               case direct:	pixels_.p[y][x] = c;	
                  break;
               case filter:	pixels_.p[y][x] = hue(c & pixels_.p[y][x]);	 
                  break;
               case additive:	pixels_.p[y][x] = hue(c | pixels_.p[y][x]);	
                  break;
               case disjoint:	pixels_.p[y][x] = hue(c ^ pixels_.p[y][x]);   
                  break;
            }
         }     
      
      // GSL: Added for MiniPNG support.
          hue SingletonWindow::GetPixel(int x, int y) const {
            if (x < 0 || x >= Xpixels || y < 0 || y >= Ypixels) {
               throw playpen::exception(playpen::exception::error,
                  "Co-ordinates out of range in SingletonWindow::GetPixel.");
            }
            return pixels_.p[y][x];
         }
      
          void SingletonWindow::SetPaletteEntry(hue h, HueRGB const & rgb) {
            hueRGBs_.rgbs[h] = rgb;
         }
      
          HueRGB SingletonWindow::GetPaletteEntry(hue h){
            return hueRGBs_.rgbs[h];	
         }
      
          ostream& SingletonWindow::Save(ostream & out) {
            out.write((char*)&background_, sizeof(background_));
            out.write((char*)&hueRGBs_, sizeof(hueRGBs_)); 
            out.write((char*)pixels_.p[0], sizeof(hue)*Xpixels*Ypixels);
            return out;
         }
          istream& SingletonWindow::Restore(istream & inp){
            inp.read((char*)&background_, sizeof(background_));
            inp.read((char*)&hueRGBs_, sizeof(hueRGBs_));
            inp.read((char*)pixels_.p[0], sizeof(hue)*Xpixels*Ypixels);
            UpdatePalette();
            Display();
            return inp;
         }
      
      }// namespace detail
   
   ///////////////////////////////////////////////////////////////////
   //
   // playpen code.
   // The one SingletonWindow shared by all playpen objects.
      /*static*/ detail::SingletonWindow *  playpen::graphicswindow = 0;
   
       playpen::playpen(hue background) : pmode(direct),xorg(Xpixels/2), yorg(Ypixels/2) {
         graphicswindow = detail::SingletonWindow::GetWindow(background);
         if (!graphicswindow) {
            throw playpen::exception(playpen::exception::fatal, 
               "Could not get window handle in playpen constructor.");
         }
         rgbpalette();     	 
      }
       playpen::playpen(playpen const & pp): pmode(pp.pmode), xorg(pp.xorg), yorg(pp.yorg) {
         graphicswindow = detail::SingletonWindow::GetWindow(black);
         if (!graphicswindow) {
            throw playpen::exception(playpen::exception::fatal, 
               "Could not get window handle in playpen constructor.");
         }
      }
      	   
   
       playpen::~playpen() {
         graphicswindow->ReleaseWindow();	
      }
   
   // Allows the plotmode state to be changed. Might consider making that part of
   // the state of a playpen rather than of the SingletonWindow instance	  
       plotmode playpen::setplotmode(plotmode pm){
         plotmode was(pmode);
         pmode = pm;
         return was;
      }
   
   // Save to and recover from platform-independent graphics image file.
       ostream & playpen::save(ostream & out)const {
         out.write((char*)this, sizeof(this));
         return graphicswindow->Save(out);
      }
       istream & playpen::restore(istream & inp) {
         inp.read((char*)this, sizeof(this));
         return graphicswindow->Restore(inp);
      }
   
       playpen const & playpen::display() const {
         graphicswindow->Display();	
         return *this;
      }
   
       playpen const & playpen::updatepalette() const {
         graphicswindow->UpdatePalette();	
         return *this;
      }
   
       playpen& playpen::plot(int x, int y, hue c){
         x = x*pixsize.size();
         y = y*pixsize.size();	 	 	 
         for(int i=0; i != pixsize.size(); ++i){	
            for(int j=0; j != pixsize.size(); ++j){	
               graphicswindow->Plot(x+xorg+i,yorg-y-j, c, pmode);
            }
         }	   	   
         return *this;
      }
   
   // 12/12/02 function to return hue of pixel allowing for origin and scale. FGW
       hue playpen::get_hue(int x, int y)const{
         try {
            x *= pixsize.size();
            y *= pixsize.size();			
            return graphicswindow->GetPixel(x+xorg,yorg-y);
         }
             catch(...){
               return black;}	
      }	 	 
   
       playpen& playpen::setpalettentry(hue c, HueRGB const & target) {
         graphicswindow->SetPaletteEntry(c, target);
         return *this;
      }
   
       HueRGB playpen::getpalettentry(hue c) const {
         return graphicswindow->GetPaletteEntry(c);
      }
   
       playpen & playpen::rgbpalette(){
         static unsigned char colourvalues[] = {0, 36, 73, 110, 147, 183, 219, 255};
         for(int r=0; r<8; ++r){
            for(int b=0; b<8; ++b){
               int huentry=r*32+b;
            // GSL: Changed bitand to & for MSVC6 compatibility.
               int lowblue = (b & 1);
            // make the blue lowbit match the green lowbit
               setpalettentry(huentry, HueRGB(colourvalues[r], colourvalues[lowblue], colourvalues[b]));
               setpalettentry(huentry+8, HueRGB(colourvalues[r], colourvalues[lowblue + 2], colourvalues[b]));
               setpalettentry(huentry+16, HueRGB(colourvalues[r], colourvalues[lowblue + 4], colourvalues[b]));
               setpalettentry(huentry+24, HueRGB(colourvalues[r], colourvalues[lowblue + 6], colourvalues[b]));
            }
         }
         updatepalette();
         return *this;
      }
       playpen & playpen::clear(hue h){
         graphicswindow->Clear(h);
         return *this;
      }
   
   // GSL: Added these two for minipng support.	    		  
       hue playpen::getrawpixel(int x, int y) const {
         return graphicswindow->GetPixel(x, y);
      }
   
       void playpen::setrawpixel(int x, int y, hue h) {
         graphicswindow->Plot(x, y, h, direct);
      }
   
   // mouse class.
   
       mouse::mouse() :
       window_(detail::SingletonWindow::GetWindow(black)) {
         if (!window_) {
            throw playpen::exception(playpen::exception::fatal, 
               "Could not get window in mouse constructor.");
         }
      }
   
       mouse::~mouse() {
         window_->ReleaseWindow();
      }	
   
       mouse::location mouse::cursor_at() const {
         return window_->GetMouseLocation();
      }
   
       bool mouse::button_pressed() const {
         return window_->IsMouseButtonDown();
      }
   
   // INSERT 12/06/03
   // keyboard class
   
       keyboard::keyboard() :
       window_(detail::SingletonWindow::GetWindow(black)) {
         if (!window_) {
            throw playpen::exception(playpen::exception::fatal, 
               "Could not get window in keyboard constructor.");
         }
      }
   
       keyboard::~keyboard() {
         window_->ReleaseWindow();
      }	
   
       int keyboard::key_pressed() const {
         int		result = window_->KeyPressed();
         HANDLE	hStdIn = GetStdIn();
      
         while (UnreadInputEvents(hStdIn))
         {
            INPUT_RECORD inputRecord = ReadInputEvent(hStdIn);
         
            if (KEY_EVENT == inputRecord.EventType && 
            inputRecord.Event.KeyEvent.bKeyDown) {
            
               result = OnKeyDownEvent(
                  inputRecord.Event.KeyEvent.wVirtualKeyCode, 
                  inputRecord.Event.KeyEvent.dwControlKeyState,
                  result);
            }
         } 
         return result;
      }// keyboard::key_pressed
   
   }// namespace studentgraphics


// - Originally created for article in ACCU magazine.
// - Modified by Francis Glassborow.
// - Modified Oct 2002 by Garry Lancaster (GSL).
// - Modified Dec 2002 by GSL to add MiniPNG support.
// - Modified Jan 2003 by GSL to add mouse support.
// - Merged with FGW master Jan 24
// bug in mouse_location written round, gethue changed to get_hue 29/05/03
//
// Notes:
// 1. You MUST not put this in a library and then link it to multiple DLLs
// 	or DLL/EXE. It's intended for static linking only.
// 2. Normally we would separate platform-specific and platform-independent
//	code into different translation units. However, this would complicate
//	the build steps and considering the code is intended to be used (if not
//	understood) by novices, this has not been done. Apart from the platform-
//	specific includes, all the platform-specific code has been kept together 
//	in a single clearly marked continuous section of the file to aid porting.
// 3. You must link this file with a multi-threaded runtime library. This
//	is because SingletonWindowImpl creates a worker thread (at least for the
//	Windows implementation). This does not mean the public interfaces 
//	support multi-threaded access: they do not.

// race condition bug corrected 03/08/2005

