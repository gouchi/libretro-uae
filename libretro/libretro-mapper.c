#include "libretro.h"
#include "libretro-glue.h"
#include "keyboard.h"
#include "libretro-keymap.h"
#include "graph.h"
#include "vkbd.h"
#include "libretro-mapper.h"

#include "uae_types.h"
#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "inputdevice.h"

#include "gui.h"
#include "xwin.h"
#include "disk.h"

#ifdef __CELLOS_LV2__
#include "sys/sys_time.h"
#include "sys/timer.h"
#include <sys/time.h>
#include <time.h>
#define usleep  sys_timer_usleep

void gettimeofday (struct timeval *tv, void *blah)
{
   int64_t time = sys_time_get_system_time();

   tv->tv_sec  = time / 1000000;
   tv->tv_usec = time - (tv->tv_sec * 1000000);  // implicit rounding will take care of this for us
}

#else
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#endif

unsigned short int bmp[1024*1024];
unsigned short int savebmp[1024*1024];

int NPAGE=-1, KCOL=1, BKGCOLOR=0, MAXPAS=8;
int SHIFTON=-1,TABON=-1,MOUSEMODE=-1,SHOWKEY=-1,PAS=4,STATUSON=-1,LEDON=-1;

short signed int SNDBUF[1024*2];
int snd_sampler = 44100 / 50;
char RPATH[512];
char DSKNAME[512];

int gmx=320,gmy=240; //gui mouse

//int al[2];//left analog1
int ar[2];//right analog1
unsigned long MXjoy[4]={0}; // joyports
int touch=-1; // gui mouse btn
int fmousex,fmousey; // emu mouse
int pauseg=0; //enter_gui
int slowdown=0;

static int jbt[24]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static int kbt[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//   RETRO          B     Y     SLT   STA   UP    DWN   LEFT  RGT   A     X     L     R     L2    R2    L3    R3
//   INDEX          0     1     2     3     4     5     6     7     8     9     10    11    12    13    14    15
static unsigned long vbt[16]={0x020,0x200,0x080,0x100,0x001,0x002,0x004,0x008,0x010,0x040,0x400,0x800};

extern void enter_options(void);
extern void reset_drawing(void);
extern void retro_key_up(int);
extern void retro_key_down(int);
extern void retro_mouse(int, int);
extern void retro_mouse_but0(int);
extern void retro_mouse_but1(int);
extern void retro_joy(unsigned int, unsigned long);
extern unsigned uae_devices[4];
extern int mapper_keys[30];
extern int video_config;

#define EMU_VKBD 1
#define EMU_STATUSBAR 2
#define EMU_MOUSE_TOGGLE 3
#define EMU_MOUSE_SPEED 4
#define EMU_GUI 5

void emu_function(int function) {
   //printf("EMU: %d\n", function);
   switch (function)
   {
      case EMU_VKBD:
         SHOWKEY=-SHOWKEY;
         Screen_SetFullUpdate();
         break;
      case EMU_STATUSBAR:
         STATUSON=-STATUSON;
         LEDON=-LEDON;
         Screen_SetFullUpdate();
         break;
      case EMU_MOUSE_TOGGLE:
         MOUSEMODE=-MOUSEMODE;
         break;
      case EMU_MOUSE_SPEED:
         PAS=PAS+2;
         if(PAS>MAXPAS)PAS=2;
         break;
      case EMU_GUI:
         pauseg=1;
         break;
   }
}


long frame=0;
unsigned long  Ktime=0 , LastFPSTime=0;

int BOXDEC= 32+2;
int STAT_BASEY;
int FONT_WIDTH;
int FONT_HEIGHT;
int BOX_Y;
int BOX_WIDTH;
int BOX_HEIGHT;

extern char key_state[512];
extern char key_state2[512];

extern char * filebrowser(const char *path_and_name);

static retro_input_state_t input_state_cb;
static retro_input_poll_t input_poll_cb;

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

#define MDEBUG
#ifdef MDEBUG
#define mprintf printf
#else
#define mprintf(...) 
#endif

void enter_gui(void)
{	
   enter_options();
}

#ifdef WIIU
#include <features_cpu.h>
#endif

/* in milliseconds */
long GetTicks(void)
{
#ifdef _ANDROID_
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   return (now.tv_sec*1000000 + now.tv_nsec/1000)/1000;
#elif defined(WIIU)
return (cpu_features_get_time_usec())/1000;
#else
   struct timeval tv;
   gettimeofday (&tv, NULL);
   return (tv.tv_sec*1000000 + tv.tv_usec)/1000;
#endif

} 

void gui_poll_events(void)
{
   //NO SURE FIND BETTER WAY TO COME BACK IN MAIN THREAD IN HATARI GUI

   Ktime = GetTicks();

   if(Ktime - LastFPSTime >= 1000/50)
   {
      slowdown=0;
      frame++; 
      LastFPSTime = Ktime;		
      co_switch(mainThread);
      //retro_run();
   }
}

void Print_Status(void)
{
   if(video_config & 0x04) // PUAE_VIDEO_HIRES
   {
      STAT_BASEY=9;
      FONT_WIDTH=2;
      FONT_HEIGHT=2;
      BOX_Y=STAT_BASEY-3;
      BOX_WIDTH=CROP_WIDTH-320;
      BOX_HEIGHT=STAT_YSZ+1;
   }
   else
   {
      STAT_BASEY=0;
      FONT_WIDTH=1;
      FONT_HEIGHT=1;
      BOX_Y=STAT_BASEY;
      BOX_WIDTH=CROP_WIDTH;
      BOX_HEIGHT=10;
   }

   DrawFBoxBmp(bmp,0,BOX_Y,BOX_WIDTH,BOX_HEIGHT,RGB565(0,0,0));

   Draw_text(bmp,STAT_DECX,STAT_BASEY,0xffff,0x0000,FONT_WIDTH,FONT_HEIGHT,20,((MOUSEMODE==-1) ? "Joy  " : "Mouse"));
   Draw_text(bmp,STAT_DECX+80,STAT_BASEY,0xffff,0x0000,FONT_WIDTH,FONT_HEIGHT,20,"MSpd%d",PAS);
   Draw_text(bmp,STAT_DECX+175,STAT_BASEY,0xffff,0x0000,FONT_WIDTH,FONT_HEIGHT,40,(SHIFTON>0?"CapsLock":""));
}

void Screen_SetFullUpdate(void)
{
   reset_drawing();
}

void Process_key(int disable_physical_cursor_keys)
{
   int i;
   for(i=0;i<320;i++)
   {
      key_state[i]=input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0,i)?0x80:0;

      if(keyboard_translation[i]==AK_CAPSLOCK)
      {

         if( key_state[i] && key_state2[i]==0 )
         {
            retro_key_down(keyboard_translation[i]);
            retro_key_up(keyboard_translation[i]);
            SHIFTON=-SHIFTON;
            Screen_SetFullUpdate();
            key_state2[i]=1;
         }
         else if (!key_state[i] && key_state2[i]==1)
            key_state2[i]=0;

      }
      else if(keyboard_translation[i]==AK_TAB)
      {
         if( key_state[i] && key_state2[i]==0 )
         {
            TABON=1;
            retro_key_down(keyboard_translation[i]);
            key_state2[i]=1;
         }
         else if (!key_state[i] && key_state2[i]==1)
         {
            TABON=-1;
            retro_key_up(keyboard_translation[i]);
            key_state2[i]=0;
         }

      }
      else
      {
         if(disable_physical_cursor_keys && (i == RETROK_DOWN || i == RETROK_UP || i == RETROK_LEFT || i == RETROK_RIGHT))
            continue;

         if(key_state[i] && keyboard_translation[i]!=-1 && key_state2[i] == 0)
         {
            if(SHIFTON==1)
               retro_key_down(keyboard_translation[RETROK_LSHIFT]);

            retro_key_down(keyboard_translation[i]);
            key_state2[i]=1;
         }
         else if ( !key_state[i] && keyboard_translation[i]!=-1 && key_state2[i]==1 )
         {
            retro_key_up(keyboard_translation[i]);
            key_state2[i]=0;

            if(SHIFTON==1)
               retro_key_up(keyboard_translation[RETROK_LSHIFT]);

         }

      }
   }
}

void update_input(int disable_physical_cursor_keys)
{
   int i, mk;

   static int oldi=-1;
   static int vkx=0,vky=0;

   int LX, LY, RX, RY;
   int threshold=20000;

   if(oldi!=-1)
   {
      retro_key_up(oldi);
      oldi=-1;
   }

   input_poll_cb();

   /* Keyboard hotkeys */
   int imax = 5;
   for(i = 0; i < imax; i++) {
      mk = i + 24;
      /* Key down */
      if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[mk]) && kbt[i]==0 && mapper_keys[mk]!=0)
      {
         //printf("KEYDN: %d\n", mk);
         kbt[i]=1;
         switch(mk) {
            case 24:
               emu_function(EMU_VKBD);
               break;
            case 25:
               emu_function(EMU_STATUSBAR);
               break;
            case 26:
               emu_function(EMU_MOUSE_TOGGLE);
               break;
            case 27:
               emu_function(EMU_MOUSE_SPEED);
               break;
            case 28:
               emu_function(EMU_GUI);
               break;
         }
      }
      /* Key up */
      else if (kbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[mk]) && mapper_keys[mk]!=0)
      {
         //printf("KEYUP: %d\n", mk);
         kbt[i]=0;
         switch(mk) {
            case 26:
               /* simulate button press to activate mouse properly */
               if(MOUSEMODE==1) {
                  retro_mouse_but0(1);
                  retro_mouse_but0(0);
               }
               break;
         }
      }
   }

   

   /* The check for kbt[i] here prevents the hotkey from generating key events */
   /* SHOWKEY check is now in Process_key to allow certain keys while SHOWKEY */
   int processkey=1;
   for(i = 0; i < (sizeof(kbt)/sizeof(kbt[0])); i++) {
      if(kbt[i] == 1)
      {
         processkey=0;
         break;
      }
   }

   if (processkey) 
      Process_key(disable_physical_cursor_keys);

   /* RetroPad hotkeys */
   if (uae_devices[0] == RETRO_DEVICE_JOYPAD) {

        LX = input_state_cb(0, RETRO_DEVICE_ANALOG, 0, 0);
        LY = input_state_cb(0, RETRO_DEVICE_ANALOG, 0, 1);
        RX = input_state_cb(0, RETRO_DEVICE_ANALOG, 1, 0);
        RY = input_state_cb(0, RETRO_DEVICE_ANALOG, 1, 1);

        /* shortcut for joy mode only */
        for(i = 0; i < 24; i++)
        {
            int just_pressed = 0;
            int just_released = 0;
            if((i<4 || i>8) && i < 16) /* remappable retropad buttons (all apart from DPAD and A) */
            {
                if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && jbt[i]==0/* && i!=turbo_fire_button*/)
                    just_pressed = 1;
                else if (jbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
                    just_released = 1;
            }
            else if (i >= 16) /* remappable retropad joystick directions */
            {
                switch (i)
                {
                    case 16: /* LR */
                        if (LX > threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (LX < threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 17: /* LL */
                        if (LX < -threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (LX > -threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 18: /* LD */
                        if (LY > threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (LY < threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 19: /* LU */
                        if (LY < -threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (LY > -threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 20: /* RR */
                        if (RX > threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (RX < threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 21: /* RL */
                        if (RX < -threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (RX > -threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 22: /* RD */
                        if (RY > threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (RY < threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    case 23: /* RU */
                        if (RY < -threshold && jbt[i] == 0)
                            just_pressed = 1;
                        else if (RY > -threshold && jbt[i] == 1)
                            just_released = 1;
                        break;
                    default:
                        break;
                }
            }

            if (just_pressed)
            {
                jbt[i] = 1;
                if(mapper_keys[i] == 0) /* unmapped, e.g. set to "---" in core options */
                    continue;

                if(mapper_keys[i] == mapper_keys[24]) /* Virtual keyboard */
                    emu_function(EMU_VKBD);
                else if(mapper_keys[i] == mapper_keys[25]) /* Statusbar */
                    emu_function(EMU_STATUSBAR);
                else if(mapper_keys[i] == mapper_keys[26]) /* Toggle mouse control */
                    emu_function(EMU_MOUSE_TOGGLE);
                else if(mapper_keys[i] == mapper_keys[27]) /* Alter mouse speed */
                    emu_function(EMU_MOUSE_SPEED);
                else if(mapper_keys[i] == mapper_keys[28]) /* Enter GUI */
                    emu_function(EMU_GUI);
                else
                    retro_key_down(keyboard_translation[mapper_keys[i]]);
            }
            else if (just_released)
            {
                jbt[i] = 0;
                if(mapper_keys[i] == 0) /* unmapped, e.g. set to "---" in core options */
                    continue;

                if(mapper_keys[i] == mapper_keys[24])
                    ; /* nop */
                else if(mapper_keys[i] == mapper_keys[25])
                    ; /* nop */
                else if(mapper_keys[i] == mapper_keys[26])
                    ; /* nop */
                else if(mapper_keys[i] == mapper_keys[27])
                    ; /* nop */
                else if(mapper_keys[i] == mapper_keys[28])
                    ; /* nop */
                else
                    retro_key_up(keyboard_translation[mapper_keys[i]]);
            }
        } /* for i */
    } /* if uae_devices[0]==joypad */
 
   /* Virtual keyboard */
   if(SHOWKEY==1)
   {
      static int vkflag[5]={0,0,0,0,0};		

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) && vkflag[0]==0 )
         vkflag[0]=1;
      else if (vkflag[0]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) )
      {
         vkflag[0]=0;
         vky -= 1; 
      }

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) && vkflag[1]==0 )
         vkflag[1]=1;
      else if (vkflag[1]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) )
      {
         vkflag[1]=0;
         vky += 1; 
      }

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) && vkflag[2]==0 )
         vkflag[2]=1;
      else if (vkflag[2]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) )
      {
         vkflag[2]=0;
         vkx -= 1;
      }

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) && vkflag[3]==0 )
         vkflag[3]=1;
      else if (vkflag[3]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) )
      {
         vkflag[3]=0;
         vkx += 1;
      }

      if(vkx < 0)
         vkx=9;
      if(vkx > 9)
         vkx=0;
      if(vky < 0)
         vky=4;
      if(vky > 4)
         vky=0;

      virtual_kbd(bmp,vkx,vky);

      i=8;
      if(input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i)  && vkflag[4]==0) 	
         vkflag[4]=1;
      else if( !input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i)  && vkflag[4]==1)
      {

         vkflag[4]=0;
         i=check_vkey2(vkx,vky);

         if(i==-2)
         {
            NPAGE=-NPAGE;oldi=-1;
            //Clear interface zone					
            Screen_SetFullUpdate();
         }
         else if(i==-1)
            oldi=-1;
         else if(i==-3)
         {//KDB bgcolor
            Screen_SetFullUpdate();
            KCOL=-KCOL;
            oldi=-1;
         }
         else if(i==-4)
         {//VKbd show/hide 			
            oldi=-1;
            Screen_SetFullUpdate();
            SHOWKEY=-SHOWKEY;
         }
         else
         {
            if(i==AK_CAPSLOCK)
            {
               retro_key_down(i);
               retro_key_up(i);
               SHIFTON=-SHIFTON;
               Screen_SetFullUpdate();
               oldi=-1;
            }
            else
            {
               oldi=i;
               retro_key_down(i);
            }
         }				
      }
   }
}

int input_gui(void)
{
   int SAVPAS=PAS;	
   int ret=0;

   input_poll_cb();

   int mouse_l;
   int mouse_r;
   int16_t mouse_x,mouse_y;
   mouse_x=mouse_y=0;

   //mouse/joy toggle
   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, 2) && kbt[2]==0 )
      kbt[2]=1;
   else if ( kbt[2]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, 2) )
   {
      kbt[2]=0;
      MOUSEMODE=-MOUSEMODE;
   }
   
   if(slowdown>0)return 0;
   
   if(MOUSEMODE==1)
   {
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))mouse_x += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))mouse_x -= PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))mouse_y += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))mouse_y -= PAS;
      mouse_l=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
      mouse_r=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);

      PAS=SAVPAS;
   }
   else
   {
      mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
      mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
      mouse_l = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
      mouse_r = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
   }
   
   slowdown=1;
   static int mmbL=0,mmbR=0;

   if(mmbL==0 && mouse_l)
   {
      mmbL=1;		
      touch=1;

   }
   else if(mmbL==1 && !mouse_l)
   {
      mmbL=0;
      touch=-1;
   }

   if(mmbR==0 && mouse_r)
      mmbR=1;		
   else if(mmbR==1 && !mouse_r)
      mmbR=0;

   gmx+=mouse_x;
   gmy+=mouse_y;
   if(gmx<0)
      gmx=0;
   if(gmx>retrow-1)
      gmx=retrow-1;
   if(gmy<0)
      gmy=0;
   if(gmy>retroh-1)
      gmy=retroh-1;
   
   return 0;
}

int update_input_gui(void)
{
   int ret=0;	
   static int dskflag[7]={0,0,0,0,0,0,0};// UP -1 DW 1 A 2 B 3 LFT -10 RGT 10 X 4	

   input_poll_cb();	

   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) && dskflag[0]==0 )
   {
      dskflag[0]=1;
      ret= -1; 
   }
   else /*if (dskflag[0]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) ){
          dskflag[0]=0;
          ret= -1; 
          }*/dskflag[0]=0;

      if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) && dskflag[1]==0 ){
         dskflag[1]=1;
         ret= 1;
      } 
      else/* if (dskflag[1]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) ){
             dskflag[1]=0;
             ret= 1; 
             }*/dskflag[1]=0;

         if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) && dskflag[4]==0 )
            dskflag[4]=1;
         else if (dskflag[4]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) )
         {
            dskflag[4]=0;
            ret= -10; 
         }

   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) && dskflag[5]==0 )
      dskflag[5]=1;
   else if (dskflag[5]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) )
   {
      dskflag[5]=0;
      ret= 10; 
   }

   if ( ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) || \
            input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_LCTRL) ) && dskflag[2]==0 ){
      dskflag[2]=1;

   }
   else if (dskflag[2]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A)\
         && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_LCTRL) )
   {
      dskflag[2]=0;
      ret= 2;
   }

   if ( ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) || \
            input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_LALT) ) && dskflag[3]==0 ){
      dskflag[3]=1;
   }
   else if (dskflag[3]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) &&\
         !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_LALT) ){

      dskflag[3]=0;
      ret= 3;
   }

   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) && dskflag[6]==0 )
      dskflag[6]=1;
   else if (dskflag[6]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) )
   {
      dskflag[6]=0;
      ret= 4; 
   }

   return ret;

}



void retro_poll_event()
{
   /* if user plays with cursor keys, then prevent up/down/left/right from generating keyboard key presses */
   if (
      (uae_devices[0] == RETRO_DEVICE_JOYPAD) && TABON==-1 &&
      (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) ||
       input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) ||
       input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) ||
       input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
   )
      update_input(1); /* Process all inputs but disable cursor keys */
   else
      update_input(0); /* Process all inputs */

   //if(SHOWKEY==-1) /* retro joypad take control over keyboard joy */
   /* override keydown, but allow keyup, to prevent key sticking during keyboard use, if held down on opening keyboard */
   /* keyup allowing most likely not needed on actual keyboard presses even though they get stuck also */
   if (TABON==-1)
   {
      static int mbL=0,mbR=0;
      int16_t mouse_x;
      int16_t mouse_y;
      int mouse_l;
      int mouse_r;
      int i;

      int retro_port;
      for (retro_port = 0; retro_port <= 3; retro_port++)
      {
         switch(uae_devices[retro_port])
         {
            case RETRO_DEVICE_JOYPAD:
               // Joystick control
               if(MOUSEMODE==-1) {
                  MXjoy[retro_port]=0;
                  if(SHOWKEY==-1)
                     for (i=0;i<9;i++)
                        if(i==0 || (i>3 && i<9))
                           // Need to fight around presses on the same axis
                           if(i>3 && i<8)
                           {
                              if(i==RETRO_DEVICE_ID_JOYPAD_UP)
                              {
                                    if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) )
                                       MXjoy[retro_port] |= vbt[i];
                              }
                              else if(i==RETRO_DEVICE_ID_JOYPAD_DOWN)
                              {
                                    if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) )
                                       MXjoy[retro_port] |= vbt[i];
                              }

                              if(i==RETRO_DEVICE_ID_JOYPAD_LEFT)
                              {
                                    if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) )
                                       MXjoy[retro_port] |= vbt[i];
                              }
                              else if(i==RETRO_DEVICE_ID_JOYPAD_RIGHT)
                              {
                                    if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) )
                                       MXjoy[retro_port] |= vbt[i];
                              }
                           }
                           else
                           {
                              if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) )
                                 MXjoy[retro_port] |= vbt[i];
                           }

                  retro_joy(retro_port, MXjoy[retro_port]);
               }

               // Mouse control only for the first RetroPad
               if(retro_port==0 && SHOWKEY==-1)
               {
                  if(MOUSEMODE==1) {
                     // Joypad buttons
                     mouse_l = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
                     mouse_r = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
                  } else {
                     mouse_l = 0;
                     mouse_r = 0;
                  }

                  if(!mouse_l && !mouse_r) {
                     // Mouse buttons
                     mouse_l = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
                     mouse_r = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
                  }

                  fmousex=fmousey=0;

                  if(MOUSEMODE==1) {
                     // D-pad
                     if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
                        fmousex += PAS;
                     if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
                        fmousex -= PAS;
                     if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
                        fmousey += PAS;
                     if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
                        fmousey -= PAS;
                  }

                  if(!fmousex && !fmousey) {
                     // Right Analog
                     ar[0] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X));
                     ar[1] = (input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y));

                     if(ar[0]<=-4096)
                        fmousex-=(-ar[0])/2048;
                     if(ar[0]>= 4096)
                        fmousex+=( ar[0])/2048;
                     if(ar[1]<=-4096)
                        fmousey-=(-ar[1])/2048;
                     if(ar[1]>= 4096)
                        fmousey+=( ar[1])/2048;
                  }

                  if(!fmousex && !fmousey) {
                     // Real mouse
                     mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
                     mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

                     if(mouse_x || mouse_y) {
                        fmousex = mouse_x;
                        fmousey = mouse_y;
                     }
                  }

                  if(mbL==0 && mouse_l)
                  {
                     mbL=1;
                     retro_mouse_but0(1);
                  }
                  else if(mbL==1 && !mouse_l)
                  {
                     retro_mouse_but0(0);
                     mbL=0;
                  }

                  if(mbR==0 && mouse_r)
                  {
                     mbR=1;
                     retro_mouse_but1(1);
                  }
                  else if(mbR==1 && !mouse_r)
                  {
                     retro_mouse_but1(0);
                     mbR=0;
                  }

                  if(fmousex || fmousey)
                     retro_mouse(fmousex, fmousey);
               }
               break;

            case RETRO_DEVICE_UAE_CD32PAD:
               MXjoy[retro_port]=0;
                  for (i=0;i<12;i++)
                     // Need to fight around presses on the same axis
                     if(i>3 && i<8)
                     {
                        if(i==RETRO_DEVICE_ID_JOYPAD_UP)
                        {
                              if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) )
                                 MXjoy[retro_port] |= vbt[i];
                        }
                        else if(i==RETRO_DEVICE_ID_JOYPAD_DOWN)
                        {
                              if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) )
                                 MXjoy[retro_port] |= vbt[i];
                        }

                        if(i==RETRO_DEVICE_ID_JOYPAD_LEFT)
                        {
                              if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) )
                                 MXjoy[retro_port] |= vbt[i];
                        }
                        else if(i==RETRO_DEVICE_ID_JOYPAD_RIGHT)
                        {
                              if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) )
                                 MXjoy[retro_port] |= vbt[i];
                        }
                     }
                     else
                     {
                        if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) )
                        {
                           /* Button switcheroo for actual physical placement */
                           if(i==RETRO_DEVICE_ID_JOYPAD_A)
                              MXjoy[retro_port] |= vbt[RETRO_DEVICE_ID_JOYPAD_B];
                           else if(i==RETRO_DEVICE_ID_JOYPAD_B)
                              MXjoy[retro_port] |= vbt[RETRO_DEVICE_ID_JOYPAD_A];
                           else if(i==RETRO_DEVICE_ID_JOYPAD_X)
                              MXjoy[retro_port] |= vbt[RETRO_DEVICE_ID_JOYPAD_Y];
                           else if(i==RETRO_DEVICE_ID_JOYPAD_Y)
                              MXjoy[retro_port] |= vbt[RETRO_DEVICE_ID_JOYPAD_X];
                           else
                              MXjoy[retro_port] |= vbt[i];
                        }
                     }

               retro_joy(retro_port, MXjoy[retro_port]);
               break;

            case RETRO_DEVICE_UAE_JOYSTICK:
               MXjoy[retro_port]=0;
               if(SHOWKEY==-1)
                  for (i=0;i<9;i++)
                     if(i==0 || (i>3 && i<9))
                        // Need to fight around presses on the same axis
                        if(i>3 && i<8)
                        {
                           if(i==RETRO_DEVICE_ID_JOYPAD_UP)
                           {
                                 if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) )
                                    MXjoy[retro_port] |= vbt[i];
                           }
                           else if(i==RETRO_DEVICE_ID_JOYPAD_DOWN)
                           {
                                 if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) )
                                    MXjoy[retro_port] |= vbt[i];
                           }

                           if(i==RETRO_DEVICE_ID_JOYPAD_LEFT)
                           {
                                 if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) )
                                    MXjoy[retro_port] |= vbt[i];
                           }
                           else if(i==RETRO_DEVICE_ID_JOYPAD_RIGHT)
                           {
                                 if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) )
                                    MXjoy[retro_port] |= vbt[i];
                           }
                        }
                        else
                        {
                           if( input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, i) )
                              MXjoy[retro_port] |= vbt[i];
                        }

               retro_joy(retro_port, MXjoy[retro_port]);
               break;
         }
      }
   }
}
