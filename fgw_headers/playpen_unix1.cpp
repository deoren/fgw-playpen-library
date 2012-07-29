// This is a X Window/Posix implementation of the playpen interface.
//
// Author: Jean-Marc Bourguet
//
// This file shares part of the implementation with the MS Windows one by
// copying the code.  A better way would be to separate platform-specific
// and platform-independent code into different translation units.
// However, this would complicate the build steps and considering the code
// is intended to be used (if not understood) by novices, this has not been
// done.

// Implemented interface

#include "keyboard.h"
#include "mouse.h"
#include "playpen.h"

// C++ standard headers

#include <assert.h>
#include <map>
#include <stdexcept>
#include <stdlib.h>
#include <streambuf>

// Posix headers

#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

// X Window headers

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

   namespace
   {
   
      using namespace studentgraphics;
   
      int const key_unknown = 0xFE;
    
    // ======================================================================
    // Platform-independant utility classes
    // ======================================================================
    
    // **********************************************************************
    // Inherit privately from this class to disable copy constructor and
    // assignment operator
    
       class CopyDisabler
      {
      public:
         CopyDisabler();
        
      private:
         CopyDisabler(CopyDisabler const&);
         CopyDisabler& operator=(CopyDisabler const&);
      };
    
       inline
       CopyDisabler::CopyDisabler()
      {
      }
    
    // **********************************************************************
    // The mapping between hue and RGB value
    
       struct HueRGB256
      {
         HueRGB rgbs[colours];
        
         HueRGB256();
      };
   
       HueRGB256::HueRGB256()
      {
         memset(rgbs, 0, sizeof(HueRGB) * colours);
      
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
        
         rgbs[colours-1].r = rgbs[colours-1].g = rgbs[colours-1].b = 0xFF;
      }
   
    // **********************************************************************
    // The content of the graphic window
    
       struct Pixels
      {
         hue p[Ypixels][Xpixels];
        
         explicit Pixels(hue fillHue);
      
         void Clear(hue fillHue);
      };
   
       inline
       Pixels::Pixels(hue fillHue)
      {
         Clear(fillHue);
      }
    
       inline
       void Pixels::Clear(hue fillHue)
      {
         memset(p[0], fillHue, Ypixels*Xpixels);
      }
   
   
    // ======================================================================
    // Platform-specific utility classes
    // ======================================================================
    
    // **********************************************************************
    // Wrapper around a mutex
    
       class CriticalSection: private CopyDisabler
      {
      public:
         CriticalSection();
         ~CriticalSection();
      
         void Enter();
         void Leave();
      
      private:
         pthread_mutex_t cs_;
      };
   
       CriticalSection::CriticalSection()
      {
         static pthread_mutex_t init = PTHREAD_MUTEX_INITIALIZER;
         cs_ = init;
      }
   
       inline
       CriticalSection::~CriticalSection()
      {
      }
   
       inline
       void CriticalSection::Enter()
      {
         pthread_mutex_lock(&cs_);
      }
   
       inline
       void CriticalSection::Leave()
      {
         pthread_mutex_unlock(&cs_);
      }
   
    // **********************************************************************
    // Constructor lock, destructor unlock so that we are sure to free the
    // lock when leaving the scope.
    
       class CSLocker: private CopyDisabler
      {
      public:
         CSLocker(CriticalSection& cs);
         ~CSLocker();
      
      private:
         CriticalSection& cs_;
      };
   
       inline
       CSLocker::CSLocker(CriticalSection& cs)
        : cs_(cs)
      {
         cs_.Enter();
      }
   
       inline
       CSLocker::~CSLocker()
      {
         cs_.Leave();
      }
    
    // **********************************************************************
    // Wrapper around thread
   
    extern "C" {
      typedef void* (*ThreadFn)(void*);
    }
    
       class Thread: private CopyDisabler
      {
      public:
      
         Thread();
         ~Thread();
      
         void Run(ThreadFn threadFn, void* context); 
         void Join();
        
      private:
         pthread_t thread_;
         bool      running_;
      }; 
   
       inline
       Thread::Thread()
      {
      }
        
       Thread::~Thread()
      {
         assert(!running_);
      }
    
       void Thread::Run(ThreadFn threadFn, void* context)
      {
         pthread_create(&thread_, 0, threadFn, context);
         running_ = true;
      }
   
       void Thread::Join()
      {
         if (running_) {
            void* status;
            pthread_join(thread_, &status);
            running_ = false;
         }
      }
   
    // ======================================================================
    // Main platform-specific class
    // ======================================================================
    
    // **********************************************************************
    // This is the class which does all the hard work.  On creation, a X
    // window is created and a thread is lauched which will handle the
    // events received by the window.  These events will update accordingly
    // the member variables of the instance.  Public members provides an
    // access to the state.
    //
    // There are two locks: one (sharedStateLock_) is used for all the
    // members which are shared by the main and the worker threads.  The
    // other (xLock_) is used to protect the X functions as they are not
    // reentrant.  If a thread must acquire both, it must be in the order
    // sharedStateLock_ then xLock_ to prevent dead lock.
    //
    // The two threads communicate throw the shared members and throw a
    // pipe.  Currently the only message which pass in the pipe is a
    // notification for the worker thread to terminate when the pipe is
    // closed.
   
    extern "C" {
      static void* WorkerThreadForwarder(void*);
    }
    
       class SingletonWindowImpl: private CopyDisabler
      {
      public:
         SingletonWindowImpl(Pixels const& pixels, HueRGB256 const& hueRGBs);
         ~SingletonWindowImpl();
      
         void Display(Pixels const& pixels);        
         void UpdatePalette(Pixels const& pixels, HueRGB256 const& hueRGBs);
        
         mouse::location GetMouseLocation() const;
         bool IsMouseButtonDown() const;
        
         int KeyPressed();
      
      private:
        
         friend void* WorkerThreadForwarder(void*);
        
         void         InitializeX();
         void         InitializeKeySymMap();
         void         FinalizeX();
        
      
         void*        WorkerThread();
      
         bool         GetEvent(XEvent& event);
         void         HandleKeyPress(XEvent& event);
         void         InitializePalette(HueRGB256 const&);
         void         FinalizePalette();
        
        // used read only when both threads exist
         static SingletonWindowImpl* instance_;
         Thread                  thread_;
         int                     pipeout_;
         int                     pipein_;
        
        // protected by sharedStateLock_
         mutable CriticalSection sharedStateLock_;
         mouse::location         mouseLocation_;
         bool                    mouseButtonDown_;
         int                     key_;
         bool                    quit_;
         unsigned long           palette_[colours];
         std::map<KeySym, int>   keySymToKey_;
      
        // protected by xLock_
         mutable CriticalSection xLock_;
         ::Display*              display_;
         int                     screen_;
         Pixmap                  pixmap_;
         Colormap                colormap_;
         GC                      gc_;
         Window                  window_;
         XComposeStatus          compose_;
        
      };
   
    extern "C" 
       void* WorkerThreadForwarder(void* arg)
      {
         return reinterpret_cast<SingletonWindowImpl*>(arg)->WorkerThread();
      }
   
      SingletonWindowImpl* SingletonWindowImpl::instance_;
    
       SingletonWindowImpl::SingletonWindowImpl
        (Pixels const& pixels, HueRGB256 const& palette)
      {
         mouseLocation_.x(-1);
         mouseLocation_.y(-1);
      
         InitializeKeySymMap();
         InitializeX();
         InitializePalette(palette);
      
         int thePipes[2];
         pipe(thePipes);
         pipeout_ = thePipes[1];
         pipein_ = thePipes[0];
      
         thread_.Run(WorkerThreadForwarder, this);
        
         Display(pixels);
      }
   
       SingletonWindowImpl::~SingletonWindowImpl()
      {
         sharedStateLock_.Enter();
         quit_ = true;
         close(pipeout_);
         sharedStateLock_.Leave();
         thread_.Join();
      
         FinalizePalette();
         FinalizeX();
      }
   
       void SingletonWindowImpl::InitializeKeySymMap()
      {
         keySymToKey_[XK_BackSpace] = key_backspace;
         keySymToKey_[XK_Tab] = key_tab;
         keySymToKey_[XK_Return] = key_enter;
         keySymToKey_[XK_Pause] = key_pause;
         keySymToKey_[XK_Escape] = key_escape;
         keySymToKey_[XK_space] = key_space;
         keySymToKey_[XK_Page_Up] = key_page_up;
         keySymToKey_[XK_Page_Down] = key_page_down;
         keySymToKey_[XK_End] = key_end;
         keySymToKey_[XK_Home] = key_home;
         keySymToKey_[XK_Left] = key_left_arrow;
         keySymToKey_[XK_Up] = key_up_arrow;
         keySymToKey_[XK_Right] = key_right_arrow;
         keySymToKey_[XK_Down] = key_down_arrow;
      // keySymToKey_[] = key_print_screen;
         keySymToKey_[XK_Insert] = key_insert;
         keySymToKey_[XK_Delete] = key_delete;
         keySymToKey_[XK_Help] = key_help;
         keySymToKey_[XK_0] = key_0;
         keySymToKey_[XK_1] = key_1;
         keySymToKey_[XK_2] = key_2;
         keySymToKey_[XK_3] = key_3;
         keySymToKey_[XK_4] = key_4;
         keySymToKey_[XK_5] = key_5;
         keySymToKey_[XK_6] = key_6;
         keySymToKey_[XK_7] = key_7;
         keySymToKey_[XK_8] = key_8;
         keySymToKey_[XK_9] = key_9;
         keySymToKey_[XK_A] = key_a;
         keySymToKey_[XK_B] = key_b;
         keySymToKey_[XK_C] = key_c;
         keySymToKey_[XK_D] = key_d;
         keySymToKey_[XK_E] = key_e;
         keySymToKey_[XK_F] = key_f;
         keySymToKey_[XK_G] = key_g;
         keySymToKey_[XK_H] = key_h;
         keySymToKey_[XK_I] = key_i;
         keySymToKey_[XK_J] = key_j;
         keySymToKey_[XK_K] = key_k;
         keySymToKey_[XK_L] = key_l;
         keySymToKey_[XK_M] = key_m;
         keySymToKey_[XK_N] = key_n;
         keySymToKey_[XK_O] = key_o;
         keySymToKey_[XK_P] = key_p;
         keySymToKey_[XK_Q] = key_q;
         keySymToKey_[XK_R] = key_r;
         keySymToKey_[XK_S] = key_s;
         keySymToKey_[XK_T] = studentgraphics::key_t;
         keySymToKey_[XK_U] = key_u;
         keySymToKey_[XK_V] = key_v;
         keySymToKey_[XK_W] = key_w;
         keySymToKey_[XK_X] = key_x;
         keySymToKey_[XK_Y] = key_y;
         keySymToKey_[XK_Z] = key_z;
         keySymToKey_[XK_a] = key_a;
         keySymToKey_[XK_b] = key_b;
         keySymToKey_[XK_c] = key_c;
         keySymToKey_[XK_d] = key_d;
         keySymToKey_[XK_e] = key_e;
         keySymToKey_[XK_f] = key_f;
         keySymToKey_[XK_g] = key_g;
         keySymToKey_[XK_h] = key_h;
         keySymToKey_[XK_i] = key_i;
         keySymToKey_[XK_j] = key_j;
         keySymToKey_[XK_k] = key_k;
         keySymToKey_[XK_l] = key_l;
         keySymToKey_[XK_m] = key_m;
         keySymToKey_[XK_n] = key_n;
         keySymToKey_[XK_o] = key_o;
         keySymToKey_[XK_p] = key_p;
         keySymToKey_[XK_q] = key_q;
         keySymToKey_[XK_r] = key_r;
         keySymToKey_[XK_s] = key_s;
         keySymToKey_[XK_t] = studentgraphics::key_t;
         keySymToKey_[XK_u] = key_u;
         keySymToKey_[XK_v] = key_v;
         keySymToKey_[XK_w] = key_w;
         keySymToKey_[XK_x] = key_x;
         keySymToKey_[XK_y] = key_y;
         keySymToKey_[XK_z] = key_z;
         keySymToKey_[XK_KP_0] = key_numpad_0;
         keySymToKey_[XK_KP_1] = key_numpad_1;
         keySymToKey_[XK_KP_2] = key_numpad_2;
         keySymToKey_[XK_KP_3] = key_numpad_3;
         keySymToKey_[XK_KP_4] = key_numpad_4;
         keySymToKey_[XK_KP_5] = key_numpad_5;
         keySymToKey_[XK_KP_6] = key_numpad_6;
         keySymToKey_[XK_KP_7] = key_numpad_7;
         keySymToKey_[XK_KP_8] = key_numpad_8;
         keySymToKey_[XK_KP_9] = key_numpad_9;
         keySymToKey_[XK_KP_Multiply] = key_multiply;
         keySymToKey_[XK_KP_Add] = key_add;
         keySymToKey_[XK_KP_Subtract] = key_subtract;
         keySymToKey_[XK_KP_Decimal] = key_decimal_point;
         keySymToKey_[XK_KP_Divide] = key_divide;
         keySymToKey_[XK_F1] = key_f1;
         keySymToKey_[XK_F2] = key_f2;
         keySymToKey_[XK_F3] = key_f3;
         keySymToKey_[XK_F4] = key_f4;
         keySymToKey_[XK_F5] = key_f5;
         keySymToKey_[XK_F6] = key_f6;
         keySymToKey_[XK_F7] = key_f7;
         keySymToKey_[XK_F8] = key_f8;
         keySymToKey_[XK_F9] = key_f9;
         keySymToKey_[XK_F10] = key_f10;
         keySymToKey_[XK_F11] = key_f11;
         keySymToKey_[XK_F12] = key_f12;
      }
    
       void SingletonWindowImpl::InitializeX()
      {
         display_ = XOpenDisplay("");
         if (display_ == 0) {
            throw playpen::exception
                (playpen::exception::error, "Unable to open display");
         }
      
         screen_ = DefaultScreen(display_);
      
         if (DefaultDepth(display_, screen_) < 8) {
            XCloseDisplay(display_);
            throw playpen::exception
                (playpen::exception::error, "Not enough colours available");
         } 
         else if (DefaultDepth(display_, screen_) == 8) {
            // use a private colormap, this will result in flashing when
            // moving the mouse, but what else is possible?
            colormap_ =
                XCreateColormap(display_,
                                RootWindow(display_, screen_),
                                DefaultVisual(display_, screen_),
                                AllocNone);
         } 
         else {
            // use the default colormap
            colormap_ = DefaultColormap(display_, screen_);
         }
      
         window_ = XCreateSimpleWindow
                    (display_,
                     DefaultRootWindow(display_),
                     50, 100,
                     Xpixels, Ypixels,
                     10,
                     WhitePixel(display_, screen_),
                     BlackPixel(display_, screen_));
        
         XSetWindowColormap(display_, window_, colormap_);
        
         pixmap_ = XCreatePixmap
                     (display_, window_,
                      Xpixels, Ypixels,
                      DefaultDepth(display_, screen_));
         XGCValues GCValues;
         gc_ = XCreateGC(display_, window_, 0, &GCValues);
      
         XSelectInput(display_, window_,
                     ExposureMask | KeyPressMask
                     | ButtonPressMask | ButtonReleaseMask | PointerMotionMask
                     | EnterWindowMask | LeaveWindowMask
                     | StructureNotifyMask);
      
         XMapWindow(display_, window_);
      }
   
       void SingletonWindowImpl::InitializePalette(HueRGB256 const& palette)
      {
         XColor color;
         color.flags = DoRed|DoGreen|DoBlue;
         for (unsigned int i = 0; i < colours; ++i) {
            color.red = palette.rgbs[i].r * 0x101;
            color.green = palette.rgbs[i].g * 0x101;
            color.blue = palette.rgbs[i].b * 0x101;
            XAllocColor(display_, colormap_, &color);
            palette_[i] = color.pixel;
         }
      }
   
       void SingletonWindowImpl::FinalizePalette()
      {
         XFreeColors(display_, colormap_, palette_, colours, 0);
      }
    
       void SingletonWindowImpl::FinalizeX()
      {
         XCloseDisplay(display_);
      }
   
       void SingletonWindowImpl::Display(Pixels const& pixels)
      {
         XGCValues newValues;
         CSLocker locker(xLock_);
      
         newValues.foreground = palette_[pixels.p[0][0]];
         XChangeGC (display_, gc_, GCForeground, &newValues);
        
         for (int x = 0; x < Xpixels; ++x) {
            int starty = 0;
            if (newValues.foreground != palette_[pixels.p[0][x]]) {
               newValues.foreground = palette_[pixels.p[0][x]];
               XChangeGC (display_, gc_, GCForeground, &newValues);
            }
            for (int y = 1; y < Ypixels; ++y) {
               if (newValues.foreground != palette_[pixels.p[y][x]]) {
                  if (y == starty + 1) {
                     XDrawPoint(display_, pixmap_, gc_, x, starty);
                  } 
                  else {
                     XDrawLine(display_, pixmap_, gc_, x, starty, x, y-1);
                  }
                  newValues.foreground = palette_[pixels.p[y][x]];
                  starty = y;
                  XChangeGC (display_, gc_, GCForeground, &newValues);
               }
            }
            if (Ypixels == starty + 1) {
               XDrawPoint(display_, pixmap_, gc_, x, starty);
            } 
            else {
               XDrawLine(display_, pixmap_, gc_, x, starty, x, Ypixels-1);
            }
         }
        
         XCopyArea
            (display_, pixmap_, window_, gc_, 0, 0, Xpixels, Ypixels, 0, 0);
         XFlush(display_);
      }
   
       void SingletonWindowImpl::UpdatePalette
        (Pixels const & pixels, HueRGB256 const & palette)
      {
         {
            CSLocker locker(xLock_);
            FinalizePalette();
            InitializePalette(palette);
         }
         Display(pixels);
      }
                        
       mouse::location SingletonWindowImpl::GetMouseLocation() const
      {
         CSLocker lock(sharedStateLock_);
         return mouseLocation_;
      }
   
       bool SingletonWindowImpl::IsMouseButtonDown() const
      {
         CSLocker lock(sharedStateLock_);
         return mouseButtonDown_;
      }
   
       int SingletonWindowImpl::KeyPressed()
      {
         CSLocker lock(sharedStateLock_);
         int result = key_;
         key_ = 0;
         return result;
      }
   
       void SingletonWindowImpl::HandleKeyPress(XEvent& event)
      {
         CSLocker lock(xLock_);
         KeySym keysym;
         XLookupString(&event.xkey, 0, 0, &keysym, &compose_);
         if (keySymToKey_.find(keysym) != keySymToKey_.end()) {
            key_ = keySymToKey_[keysym];
         } 
         else {
            key_ = key_unknown;
         }
         if ((event.xkey.state & ShiftMask) != 0)
            key_ |= modifier_shift;
         if ((event.xkey.state & LockMask) != 0)
            key_ |= modifier_caps_lock;
         if ((event.xkey.state & ControlMask) != 0)
            key_ |= modifier_control;
         if ((event.xkey.state & Mod1Mask) != 0)
            key_ |= modifier_alt;
         if ((event.xkey.state & Mod2Mask) != 0)
            key_ |= modifier_num_lock;
         if ((event.xkey.state & Mod4Mask) != 0)
            key_ |= modifier_alt;
      }
    
       bool SingletonWindowImpl::GetEvent(XEvent& event)
      {
         CSLocker lock(xLock_);
         return XCheckMaskEvent(display_, -1, &event);
      }
    
       void* SingletonWindowImpl::WorkerThread()
      {
         XEvent event;
         bool quit = false;
         while (!quit) {
            while (GetEvent(event)) {
               CSLocker lock(sharedStateLock_);
               switch(event.type) {
                  case Expose:
                     if (event.xexpose.count == 0) {
                        CSLocker lock(xLock_);
                        XCopyArea
                            (display_, pixmap_, window_, gc_,
                             0, 0, Xpixels, Ypixels, 0, 0);
                        XFlush(display_);
                     }
                     break;
                  case MotionNotify:
                     mouseLocation_.x(event.xmotion.x);
                     mouseLocation_.y(event.xmotion.y);
                     break;
                  case ButtonPress:
                     mouseButtonDown_ = true;
                     break;
                  case ButtonRelease:
                     mouseButtonDown_ =
                        (event.xbutton.state
                         & (Button1Mask | Button2Mask | Button3Mask
                            | Button4Mask | Button5Mask)
                         != 0);
                     break;
                  case EnterNotify:
                     break;
                  case LeaveNotify:
                     mouseLocation_.x(-1);
                     mouseLocation_.y(-1);
                     break;
                  case KeyPress:
                     HandleKeyPress(event);
                     break;
                  default:
                     break;
               }
            }
            
            // wait with select until something is available to prevent
            // near deadlock (the main thread waiting for xLock_ in display
            // while the worker thread is blocked waiting an event) and to
            // allow to check for input in the command pipe
            
            int descriptorsReady;
            fd_set readDescriptors;
            do {
               FD_ZERO(&readDescriptors);
               FD_SET(XConnectionNumber(display_), &readDescriptors);
               FD_SET(pipein_, &readDescriptors);
               descriptorsReady = select(std::max(XConnectionNumber(display_),
                                                   pipein_)+1,
                                          &readDescriptors, 0, 0, 0);
            } while (descriptorsReady == 0);
            
            {
               CSLocker lock(sharedStateLock_);
               quit = quit_;
            }
         }
         return 0;
      }
   
    // ======================================================================
    // Platform-specific functions handling non blocking input on standard
    // input
    // ======================================================================
    // The MS Windows version return interpreted keyboard events, this is
    // not possible on Unix, so we try to reconstruct some information from
    // the character read.  This explain some strangeness: CTRL-A is
    // described a 0x141 but CTRL-H as 0x8 (key_backspace); CTRL-J is
    // mapped to 0xd (key_return) and CTRL-M is mapped to 0x10d.  The only
    // modifiers generated are modifier_control, modifier_shift and
    // modifier_alt.
    //
    // We don't use terminfo or termcap database to get the character
    // sequences for function keys but use the VT100 one (which should
    // cover the situation of beginners quite well: terminal emulators --
    // xterm, gnome-term,... -- are emulating VT100 with extentions).  The
    // use of termcap/terminfo would also have allowed to make a
    // distinction between the keypad and the other characters.
    //
    // Another problem is that the specification allows intermixed input on
    // cin and with this API.  So we are switching the terminal between
    // cooked and raw mode with most time passed in cooked mode, this mean
    // that interrupt characters (notably CTRL-C, CTRL-Z) will often but
    // not always be handled by the driver (so generating signals) and not
    // passed to the program.  That's the raison for which ECHO is not
    // disabled in raw mode, it seemed preferable to always have echo than
    // sometimes yes, sometimes no.
   
    // **********************************************************************
    // Put a terminal in raw mode in the constructor and put it back in
    // normal mode in the destructor
    
       class RawMode: private CopyDisabler
      {
      public:
         RawMode(int fd);
         ~RawMode();
      
      private:
         int fd_;
         struct termios savedAttributes_;
        
      };
    
       RawMode::RawMode(int fd)
        : fd_(fd)
      {    
         struct termios tattr;
      
         if (!isatty (fd_)) {
            return;
         }
      
         tcgetattr (fd_, &savedAttributes_);
      
         tcgetattr (fd_, &tattr);
         tattr.c_lflag &= ~ICANON; // ~(ICANON|ECHO) to disable echo
         tattr.c_cc[VMIN] = 1;
         tattr.c_cc[VTIME] = 0;
         tcsetattr (fd_, TCSADRAIN, &tattr);
      }
   
       RawMode::~RawMode()
      {    
         if (!isatty (fd_)) {
            return;
         }
      
         tcsetattr (fd_, TCSADRAIN, &savedAttributes_);
      }
    
    
    // **********************************************************************
   
       bool ConsoleCharAvailable()
      {
         fd_set readDescriptors;
         struct timeval timeOut;
         timeOut.tv_sec = 0;
         timeOut.tv_usec = 0;
         FD_ZERO(&readDescriptors);
         FD_SET(STDIN_FILENO, &readDescriptors);
      
         return select(STDIN_FILENO+1, &readDescriptors, 0, 0, &timeOut) != 0;
      }
   
    // **********************************************************************
   
       int ConsoleGetChar()
      {
         char c;
         if (read(STDIN_FILENO, &c, 1) == 0) {
            return -1;
         } 
         else {
            return static_cast<unsigned char>(c);
         }
      }
   
    // **********************************************************************
   
       int EscSequence()
      {
         static std::map<std::string, int> funcMap;
         if (funcMap.empty()) {
            funcMap["2"] = key_insert;
            funcMap["3"] = key_delete;
            funcMap["5"] = key_page_up;
            funcMap["6"] = key_page_down;
            funcMap["11"] = key_f1;
            funcMap["12"] = key_f2;
            funcMap["13"] = key_f3;
            funcMap["14"] = key_f4;
            funcMap["15"] = key_f5;
            funcMap["17"] = key_f6;
            funcMap["18"] = key_f7;
            funcMap["19"] = key_f8;
            funcMap["20"] = key_f9;
            funcMap["21"] = key_f10;
            funcMap["23"] = key_f11;
            funcMap["24"] = key_f12;            
         }
         int result;
         int c;
         if (ConsoleCharAvailable()) {
            c = ConsoleGetChar();
            if (c == '[' && ConsoleCharAvailable()) {
               c = ConsoleGetChar();
               switch (c) {
                  case 'A': result = key_up_arrow; 
                     break;
                  case 'B': result = key_down_arrow; 
                     break;
                  case 'C': result = key_right_arrow; 
                     break;
                  case 'D': result = key_left_arrow; 
                     break;
                  case 'Z': result = modifier_shift | key_tab; 
                     break;
                  case '0': case '1': case '2': case '3': case '4':
                  case '5': case '6': case '7': case '8': case '9':
                     {
                        std::string value;
                        while (c != '~') {
                           value += c;
                           c = ConsoleGetChar();
                        }
                        if (funcMap.find(value) != funcMap.end()) {
                           result = funcMap[value];
                        } 
                        else {
                           result = key_unknown;
                        }
                        break;
                     }
                  default:
                     result = key_unknown;
               }
            } 
            else if (c == 'O' && ConsoleCharAvailable()) {
               c = ConsoleGetChar();
               switch (c) {
                  case 'A' : result = key_up_arrow; 
                     break;
                  case 'B' : result = key_down_arrow; 
                     break;
                  case 'C' : result = key_right_arrow; 
                     break;
                  case 'D' : result = key_left_arrow; 
                     break;
                  case 'P' : result = key_f1; 
                     break;
                  case 'Q' : result = key_f2; 
                     break;
                  case 'R' : result = key_f3; 
                     break;
                  case 'S' : result = key_f4; 
                     break;
                  case 'j' : result = key_multiply; 
                     break;
                  case 'k' : result = key_add; 
                     break;
                  case 'm' : result = key_subtract; 
                     break;
                  case 'n' : result = key_decimal_point; 
                     break;
                  case 'o' : result = key_divide; 
                     break;
                  case 'p' : result = key_numpad_0; 
                     break;
                  case 'q' : result = key_numpad_1; 
                     break;
                  case 'r' : result = key_numpad_2; 
                     break;
                  case 's' : result = key_numpad_3; 
                     break;
                  case 't' : result = key_numpad_4; 
                     break;
                  case 'u' : result = key_numpad_5; 
                     break;
                  case 'v' : result = key_numpad_6; 
                     break;
                  case 'w' : result = key_numpad_7; 
                     break;
                  case 'x' : result = key_numpad_8; 
                     break;
                  case 'y' : result = key_numpad_9; 
                     break;
                  default:   result = key_unknown; 
                     break;
               }
            } 
            else if (c >= 'A' && c <= 'Z') {
               result = modifier_alt | modifier_shift | c;
            } 
            else if (c >= 'a' && c <= 'z') {
               result = modifier_alt | (c - 'a' + 'A');
            } 
            else {
               result = modifier_alt | key_unknown;
            }
         } 
         else {
            result = key_escape;
         }
         return result;
      }
    
    // **********************************************************************
   
       int ConsoleKeyPressed()
      {
         RawMode modeChanger(STDIN_FILENO);
      
         if (!ConsoleCharAvailable()) {
            return 0;
         } 
         else {
            int c = ConsoleGetChar();
            int result;
            if (c < 0) {
               result = 0;
            } 
            else {
               switch (c) {
                  case '\b': case '\t':
                     result = c;
                     break;
                  case '\n':
                     result = key_enter;
                     break;
                  case 0x1B:
                     result = EscSequence();
                     break;
                  case 0x00: case 0x01: case 0x02: case 0x03:
                  case 0x04: case 0x05: case 0x06: case 0x07:
                                                 case 0x0B:
                  case 0x0C: case 0x0D: case 0x0E: case 0x0F: 
                  case 0x10: case 0x11: case 0x12: case 0x13:
                  case 0x14: case 0x15: case 0x16: case 0x17:
                  case 0x18: case 0x19: case 0x1A:           
                  case 0x1C: case 0x1D: case 0x1E: case 0x1F:
                     c += '@';
                     if (c < 'A' || c > 'Z')
                        result = modifier_control | key_unknown;
                     else
                        result = modifier_control | c;
                     break;
                  case 'A': case 'B': case 'C': case 'D': case 'E':
                  case 'F': case 'G': case 'H': case 'I': case 'J':
                  case 'K': case 'L': case 'M': case 'N': case 'O':
                  case 'P': case 'Q': case 'R': case 'S': case 'T':
                  case 'U': case 'V': case 'W': case 'X': case 'Y':
                  case 'Z':
                     result =  modifier_shift | c;
                     break;
                  case 'a': case 'b': case 'c': case 'd': case 'e':
                  case 'f': case 'g': case 'h': case 'i': case 'j':
                  case 'k': case 'l': case 'm': case 'n': case 'o':
                  case 'p': case 'q': case 'r': case 's': case 't':
                  case 'u': case 'v': case 'w': case 'x': case 'y':
                  case 'z':
                     result = (c - 'a' + 'A');
                     break;
                  case '0': case '1': case '2': case '3': case '4':
                  case '5': case '6': case '7': case '8': case '9':
                     result = c;
                     break;
                  case ' ':  result = key_space; 
                     break;
                  case '*':  result = key_multiply; 
                     break;
                  case '+':  result = key_add; 
                     break;
                  case '-':  result = key_subtract; 
                     break;
                  case '.':  result = key_decimal_point; 
                     break;
                  case '/':  result = key_divide; 
                     break;
                  case 0x7F: result = key_delete; 
                     break;
                  default:   result = key_unknown; 
                     break;
               }
            }
            return result;
         }
      }
   }

// ======================================================================
// Platform-specific implementation of Wait
// ======================================================================

// **********************************************************************

    void studentgraphics::Wait(unsigned ms)
   {
      struct timeval timeOut;
      timeOut.tv_sec = ms/1000;
      timeOut.tv_usec = (ms % 1000) * 1000;
      select(0, 0, 0, 0, &timeOut);
   }

// ======================================================================
// Plaform-specific code ends here. From now on it's platform-independent
// code until the end of the file.
// ======================================================================

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
            static SingletonWindow* GetWindow(hue background);
            void                    ReleaseWindow();
            
            // Drawing functions.
            void    Plot(int x, int y, hue, plotmode);
             void    Display() { impl_.Display(pixels_); }
            void    Clear();
            void    Clear(hue);
         
            // Palette handling.
             void    UpdatePalette() { impl_.UpdatePalette(pixels_, hueRGBs_); }
            void    SetPaletteEntry(hue, HueRGB const &);
            HueRGB  GetPaletteEntry(hue);
         
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
            Pixels              pixels_;
            HueRGB256           hueRGBs_;
            SingletonWindowImpl impl_;
            hue                 background_;
         
            static unsigned         refCount_;
            static SingletonWindow* instance_;
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
               case direct:  pixels_.p[y][x] = c;                        
                  break;
               case filter:  pixels_.p[y][x] = hue(c & pixels_.p[y][x]); 
                  break;
               case additive:pixels_.p[y][x] = hue(c | pixels_.p[y][x]); 
                  break;
               case disjoint:pixels_.p[y][x] = hue(c ^ pixels_.p[y][x]); 
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
    
       playpen::playpen(hue background)
        : pmode(direct),xorg(Xpixels/2), yorg(Ypixels/2) {
         graphicswindow = detail::SingletonWindow::GetWindow(background);
         if (!graphicswindow) {
            throw playpen::exception(playpen::exception::fatal, 
                    "Could not get window handle in playpen constructor.");
         }
         rgbpalette();    
      }
       playpen::playpen(playpen const & pp)
        : pmode(pp.pmode), xorg(pp.xorg), yorg(pp.yorg) {
         graphicswindow = detail::SingletonWindow::GetWindow(black);
         if (!graphicswindow) {
            throw playpen::exception(playpen::exception::fatal, 
                    "Could not get window handle in playpen constructor.");
         }
      }
           
   
       playpen::~playpen() {
         graphicswindow->ReleaseWindow();    
      }
   
    // Allows the plotmode state to be changed. Might consider making that
    // part of the state of a playpen rather than of the SingletonWindow
    // instance
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
   
    // 12/12/02 function to return hue of pixel allowing for origin and
    // scale. FGW
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
         static unsigned char colourvalues[]
            = {0, 36, 73, 110, 147, 183, 219, 255};
         for(int r=0; r<8; ++r){
            for(int b=0; b<8; ++b){
               int huentry=r*32+b;
              // GSL: Changed bitand to & for MSVC6 compatibility.
               int lowblue = (b & 1);
              // make the blue lowbit match the green lowbit
               setpalettentry
                  (huentry,
                   HueRGB(colourvalues[r],
                          colourvalues[lowblue],
                          colourvalues[b]));
               setpalettentry
                  (huentry+8,
                   HueRGB(colourvalues[r],
                          colourvalues[lowblue + 2],
                          colourvalues[b]));
               setpalettentry
                  (huentry+16,
                   HueRGB(colourvalues[r],
                          colourvalues[lowblue + 4],
                          colourvalues[b]));
               setpalettentry
                  (huentry+24,
                   HueRGB(colourvalues[r],
                          colourvalues[lowblue + 6],
                          colourvalues[b]));
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
   
    // **********************************************************************
   
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
   
    // **********************************************************************
    
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
         int     result = window_->KeyPressed();
         if (result == 0)
            result = ConsoleKeyPressed();
         return result;
      }
   
   }
