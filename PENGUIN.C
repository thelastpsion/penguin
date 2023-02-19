/*******************************************************************************
***
*** Filename         : penguin.c
*** Purpose          : Penguin - SIBO-LINUX Loader and Accessory
*** Author           : Matt J. Gumbley
*** Created          : 02/06/98
*** Last updated     : 02/06/98
***
********************************************************************************
***
*** Modification Record
***
*******************************************************************************/

#include "..\..\psistack\source\library\utils\sysincl.h"
#include "..\..\psistack\source\include\psistack\sys\global.h"
#include "..\..\psistack\source\library\utils\machine.h"
#include "..\..\psistack\source\library\utils\version.h"

extern char hexdig(int num);
/*#define DEBUG */

#define SOH		0x1	/* ^A */
#define ETX		0x3	/* ^C */
#define EOT		0x4	/* ^D */
#define ENQ		0x5	/* ^E */
#define ACK		0x6	/* ^F */
#define DLE		0x10	/* ^P */
#define XON		0x11	/* ^Q */
#define XOFF	0x13		/* ^S */
#define NAK	 	0x15	/* ^U */
#define CAN		0x18	/* ^X */
#define FALLBACK 16
#define CPMEOF  032		/* ^Z */
#define OK       0
#define TIMEOUT -1		/* -1 is returned by readbyte() upon timeout */
#define ERROR   -2
#define WCEOT   -3
#define RETRYMAX 10
#define Maxtime 10
#define SECSIZ  1025
#define version 100

#define STATE_WAITINITIALNAK 1
#define STATE_SENDSECTOR 2
#define STATE_WAITRESPONSE 3
#define STATE_COMPLETE 4
#define STATE_WAITCOMPLETEACK 5
#define MAXRETRY 10

GLDEF_D UBYTE state = 0;
GLDEF_D UWORD segment = 0;
GLDEF_D UWORD offset = 0;
GLDEF_D UWORD sector = 0;
GLDEF_D UWORD lastsector = 0;
GLDEF_D UBYTE attempts = 0;

GLREF_D UWORD DatLocked;            /* non-zero when application "busy" */
GLREF_D TEXT *DatUsedPathNamePtr;   /* full path name of current file */
GLREF_D UWORD _UseFullScreen, _SmallFontDialog;
GLREF_D VOID *winHandle;
GLDEF_D VOID *serHandle = NULL;
GLDEF_D WORD serStatus;
GLDEF_D WORD serLength;
#define SERBUFSIZE 300
GLDEF_D UBYTE serBuffer[SERBUFSIZE];
GLDEF_D INT useCRC = 0;


#define MENU_SENDROM  0
#define MENU_TESTBOOT 1
#define MENU_BOOT     2
#define MENU_EXIT     3

GLREF_D UWORD *_MenuPositions;
GLREF_D UWORD *_GreyLines;
LOCAL_D UWORD lines[16];

/* the commands in the menu bar */
LOCAL_D TEXT *cmds[]=
/* abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 
    b      i          t   x                             
*/
{
  /* Penguin */
  "iSend ROM Image to PC",
  "tTest Boot Process",
  "bBoot SIBO-Linux",
  "xExit",
  NULL
};

/* the menus in the menu bar */
LOCAL_D H_MENU_DATA mdata[]=
{
  "Penguin", 4,
  NULL
};

LOCAL_D TEXT *szYes = "Yes";
LOCAL_D TEXT *szNo = "No";
LOCAL_D TEXT *szIgnore = "Ignore";
LOCAL_D TEXT *szUnimplemented = "Not implemented yet";
LOCAL_D TEXT *szNone = "None";
LOCAL_D TEXT *szConfirm = "Confirm";
LOCAL_D TEXT *szNeedReload = "Reload to use new settings";
LOCAL_D TEXT *szCRLF = "\r\n";
LOCAL_D TEXT ifaceTitle[64];

GLDEF_D TEXT ** _cmds=(&cmds[0]);           /* Hwif code expects this */
GLDEF_D H_MENU_DATA * _mdata=(&mdata[0]);   /* Hwif code expects this */

LOCAL_D UINT MainWid;       /* ID of main window */

/* Keyboard stuff */
LOCAL_D WORD keystat;
LOCAL_D WMSG_KEY key;
LOCAL_D WORD keyactive=FALSE;

/* Variables for the timer stuff */
GLDEF_D WORD  timer_status;
GLDEF_D ULONG timer_ticks = 0L;
LOCAL_D VOID *timer_handle;
LOCAL_D ULONG timer_interval = 5; /* Every half a second */

/* Size of status window; whether Status Window is showing. See WSERV
   manual, p83 */
GLDEF_D UWORD StatWinState;

LOCAL_D P_SCR_SET_FONT Font;        /* Font used by the console */
LOCAL_D UWORD ScreenWidth;          /* width of main display area */


/* Forward declarations */
GLDEF_C INT timer_open(VOID);
GLDEF_C VOID timer_queue(VOID);
GLDEF_C VOID timer_cancel(VOID);
GLDEF_C VOID timer_process(VOID);
GLDEF_C VOID asy_sethandler(VOID);
GLDEF_C INT asy_open(VOID);
GLDEF_C VOID asy_close(VOID);
GLDEF_C VOID asy_writeln(UBYTE *str);
GLDEF_C INT asy_write(UBYTE *Data, WORD len);
GLDEF_C INT asy_writechar(UBYTE Data);
GLDEF_C INT asy_queue(VOID);
GLDEF_C VOID asy_process(VOID);
GLDEF_C VOID log_write(TEXT *buf, WORD len);
GLDEF_C VOID log_writes(TEXT *s);
GLDEF_C VOID log_writesn(TEXT *s);
GLDEF_C VOID CDECL log_printf(TEXT *fmt,UINT arg,...);
GLDEF_C VOID HomeCursor(VOID);
GLDEF_C VOID ClearWholeWindow(VOID);
GLDEF_C VOID SetStyle(INT style);
GLDEF_C INT Confirm(TEXT *pb);
GLDEF_C VOID ExitApplication(VOID);
GLDEF_C void Synchronise();
GLDEF_C void NewAddr();
GLDEF_C void FindAddr();
GLDEF_C void EditAddr();
GLDEF_C void RemoveAddr();
GLDEF_C VOID ManageCommand(INT index);
GLDEF_C VOID ProcessSystemCommand(VOID);
GLDEF_C VOID TryExecuteCommand(INT keycode);
GLDEF_C INT ResizeWidth(UWORD width);
GLDEF_C VOID CycleStatusWindow(VOID);
GLDEF_C VOID SwitchMode(INT newmode) ;
GLDEF_C VOID key_process(VOID);
GLDEF_C VOID MainLoop(VOID);
GLDEF_C VOID CreateGC(VOID);
GLDEF_C VOID SetupConsoleFont(VOID);
GLDEF_C VOID SpecificInit(VOID);
GLDEF_C VOID main(VOID);

/*GLDEF_C BYTE INTAC(UWORD seg, UWORD off)
{
unsigned char far * p = (unsigned char far *) (seg << 4) + off;
  return *p;
}*/

GLREF_C BYTE ACCESS(WORD, WORD);

void SendROM()
{
  state = STATE_WAITINITIALNAK;
  segment = 0x0000;
  offset = 0x0000;
  sector = 0;
  /* The ROM is 1MB in size. */
  lastsector = 0x2000;
  p_iosignal();
}


UWORD computeCRC(UBYTE *buf, WORD len)
{
int i;
long CRC = 0;
  while (len--) {
    CRC |= (*buf++) & 0xff;
    for (i=0; i<8; i++) {
      CRC <<= 1;
      if (CRC & 0x1000000L)
        CRC ^= 0x102100L;
    }
  }
  return (UWORD) (CRC >> 8);
}

void StateMachine()
{
UBYTE buffer[128];
int i,cksum;
UWORD CRC;
  switch (state) {
    case 0:
      log_writesn("nothing to do");
      break;
    case STATE_WAITINITIALNAK:
      log_writesn("Waiting for the remote XMODEM...");
      break;
    case STATE_SENDSECTOR:
#ifdef DEBUG
      log_printf("%s SEC 0x%04x SEG 0x%04x\r\n",(UINT)(useCRC ? "CRC" : "CKS"), sector,segment);
#endif
      for (cksum=i=0; i<128; i++) {
        buffer[i] = ACCESS(segment, offset);
        cksum += buffer[i];
        if (++offset == 0x10) {
          offset = 0;
          segment++;
        }
      }
      asy_writechar(SOH);
      asy_writechar((UBYTE)(sector & 0xff));
      asy_writechar((UBYTE)(-(sector & 0xff) - 1));
      asy_write(buffer, 128);
      if (useCRC) {
        CRC = computeCRC(buffer,130);
        asy_writechar((CRC >> 8) & 0xff);
        asy_writechar(CRC & 0xff);
      }
      else {
        asy_writechar(cksum & 0xff);
      }
      state = STATE_WAITRESPONSE;
      break;
    case STATE_COMPLETE:
log_writesn("Finished. Sending EOT. Waiting for ACK.");
      asy_writechar(EOT);
      state = STATE_WAITCOMPLETEACK;
      break;
  }
}


/*******************************************************************************
***
*** Function         : timer_open
*** Preconditions    : 
*** Postconditions   : 
***
*******************************************************************************/

GLDEF_C INT timer_open(VOID)
{
  return p_open(&timer_handle, "TIM:", -1);
}


/*******************************************************************************
***
*** Function         : timer_queue
*** Preconditions    : 
*** Postconditions   : 
***
*******************************************************************************/

GLDEF_C VOID timer_queue(VOID)
{
  p_ioa4(timer_handle, P_FRELATIVE, &timer_status, &timer_interval);
}


/*******************************************************************************
***
*** Function         : timer_cancel
*** Preconditions    : 
*** Postconditions   : 
***
*******************************************************************************/

GLDEF_C VOID timer_cancel(VOID)
{
  if (timer_interval > 0) {
    p_iow2(timer_handle, P_FCANCEL);
    p_waitstat(&timer_status);
  }
}


/*******************************************************************************
***
*** Function         : timer_process
*** Preconditions    : A timer event has occurred
*** Postconditions   : Various timers within the system are updated.
***
*******************************************************************************/

GLDEF_C VOID timer_process(VOID)
{
  timer_ticks ++;
  if (serHandle) {
  }
}

void abandon(UBYTE *str)
{
  log_printf("Abandoning upload: %s\r\n", (UINT)str);
  state = 0;
}

/*******************************************************************************
***
*** Function         : asy_open opens the specified port with the specified 
***                    characteristics, or, if the port is already open, 
***                    changes the port characteristics.
*** Preconditions    : Port is a string like "TTY:A"
***                    Baud is P_BAUD_50 to P_BAUD_115000
***                    Parity is IGNORE, NOPAR, EVENPAR or ODDPAR
***                    Databits is P_DATA_7 or P_DATA_8
***                    Stopbits is P_TWOSTOP or 0 (=> One stop bit)
***                    Handshake is IGNORE, XONXOFF, RTSCTS
*** Postconditions   : asy_open is TRUE, and the open worked.
***                    asy_open is FALSE, and the open failed.
***
*******************************************************************************/

GLDEF_C INT asy_open(VOID)
{
P_SRCHAR p;
TEXT errmsg[100];
INT retval;

  p.rbaud = p.tbaud = P_BAUD_19200;
  p.flags = 0;
  p.xon = 0x11;
  p.xoff = 0x13;
  p.tmask = 0L;
  p.parity = 0; 
  p.flags &= ~P_IGNORE_PARITY;
  p.frame = P_DATA_8;
  p.hand = P_FAIL_DSR | P_OBEY_DSR;
 
  /* Open the port (need to have option to select TTY:A or TTY:I for the
     new GS18 with IrDA) */
  if ((retval=p_open(&serHandle, "TTY:A", -1)) != 0) {
    p_scpy(errmsg, "Port open:");
    switch (retval) {
      case E_GEN_NOMEMORY: p_scat(errmsg, "no memory"); break;
      case E_GEN_INUSE   : p_scat(errmsg, "in use"); break;
      case E_FILE_DEVICE : p_scat(errmsg, "illegal device"); break;
      case E_FILE_LOCKED : p_scat(errmsg, "is PsiWin link active?"); break;
      default            : p_scat(errmsg, "Mail Matt: asy_open bug"); break;
    }
    wInfoMsg(errmsg);
    log_writesn(errmsg);
    serHandle = NULL;
    return FALSE;
  }

  /* Now change the parameters */
  if ((retval=p_iow(serHandle, P_FSET, &p)) != 0) {
    p_scpy(errmsg, "Port setup:");
    switch (retval) {
      case E_GEN_ARG   : p_scat(errmsg, "bad settings"); break;
      case E_GEN_NSUP  : p_scat(errmsg, "not supported"); break;
      case E_FILE_LINE : p_scat(errmsg, "no DSR/DTR"); break;
      default          : p_scat(errmsg, "huh?"); break;
    }
    wInfoMsg(errmsg);
    log_writesn(errmsg);
    asy_close();
    return FALSE;
  }


  /* Queue a read, abandoning any previously queued read */
  if ((retval=asy_queue()) != 0) {
    p_scpy(errmsg, "Port read:");
    switch (retval) {
      case E_FILE_PARITY   : p_scat(errmsg, "parity err"); break;
      case E_FILE_FRAME  : p_scat(errmsg, "framing err"); break;
      case E_FILE_OVERRUN: p_scat(errmsg, "overrun err"); break;
      case E_FILE_LINE : p_scat(errmsg, "line fail"); break;
      case E_GEN_OVER : p_scat(errmsg, "input discard"); break;
      default          : p_scat(errmsg, "huh?"); break;
    }
    log_writesn(errmsg);
    wInfoMsg(errmsg);
    asy_close();
    return FALSE;
  }

  return TRUE;
}



/*******************************************************************************
***
*** Function         : asy_close()
*** Precondition     : n is a network interface with an open device
*** Postcondition    : The edevice is closed.
***
*******************************************************************************/

GLDEF_C VOID asy_close(VOID)
{
  if (serHandle == NULL)
    return;
  p_iow2(serHandle, P_FCANCEL);
  p_waitstat(&serStatus);
  p_close(serHandle);
  serHandle = NULL;
}


GLDEF_C VOID asy_writeln(UBYTE *str)
{
  asy_write((UBYTE *)str, (WORD)p_slen((TEXT *)str));
  asy_write((UBYTE *)szCRLF, 2);
}


/*******************************************************************************
***
*** Function         : asy_write
*** Precondition     : fd is open.
*** Postcondition    : asy_write is positive and the data has been sent. The
***                    return value indicates how many bytes have been sent.
***                    asy_write is FALSE and the data has not been sent.
***
*******************************************************************************/

GLDEF_C INT asy_write(UBYTE *Data, WORD len)
{
register int retval;
UBYTE ctrl[3];

  if (serHandle == NULL) {
    wInfoMsg("asy_write: Port not open");
    return FALSE;
  }

#ifdef DATADUMP
  printf( "asy_write: dump:\n");
  hexdump(Data, len);
#endif

  /* If we're using hardware handshaking, is CTS active? Checking here will
     help prevent the line blocking... Just pretend we've sent the data,
     but warn the user. */
  ctrl[1]=0x00;
  if (p_iow(serHandle, P_FCTRL, ctrl)==0 && (!ctrl[0] & P_SRCTRL_CTS)) {
    wInfoMsg("CTS down");
    return len;
  }

  /* OK, this should be done asynchronously in a "quality system". */
  retval=p_write(serHandle, Data, len);
  switch (retval) {
    case E_FILE_PARITY:
    case E_FILE_FRAME:
    case E_FILE_OVERRUN:
      log_writesn("asy_write: frame error - check baud rate etc.");
      break;
    case E_FILE_LINE:
      log_writesn("asy_write: line was dropped");
      break;
    case 0: /* All OK */
      break;
    default:
      log_printf("asy_write: write failed err %d\r\n", retval);
  }
  return (retval==0);
}


/*******************************************************************************
***
*** Function         : asy_writechar
*** Precondition     : fd is open, Data is a character
*** Postcondition    : asy_writechar is positive and the data has been sent. 
***                    asy_writechar is FALSE and the data has not been sent.
***
*******************************************************************************/

GLDEF_C INT asy_writechar(byte Data)
{
byte Buffer = Data;
  return asy_write(&Buffer, 1);
}


/*******************************************************************************
***
*** Function         : asy_queue
*** Preconditions    : fd is open
*** Postconditions   : A read is queued.
***
*******************************************************************************/

GLDEF_C INT asy_queue(VOID)
{
TEXT errmsg[100];
INT retval;

  if (serHandle == NULL) {
    return E_FILE_LINE;
  }
  serLength = 1;
  retval= p_ioa5(serHandle, P_FREAD, &serStatus, &serBuffer, &serLength);
  if (retval != 0) {
    p_scpy(errmsg, "Port queue:");
    switch (retval) {
      case E_FILE_PARITY   : p_scat(errmsg, "parity err"); break;
      case E_FILE_FRAME  : p_scat(errmsg, "framing err"); break;
      case E_FILE_OVERRUN: p_scat(errmsg, "overrun err"); break;
      case E_FILE_LINE : p_scat(errmsg, "line fail"); break;
      case E_GEN_OVER : p_scat(errmsg, "input discard"); break;
      default          : p_scat(errmsg, "huh?"); break;
    }
    wInfoMsg(errmsg);
    asy_close();
  }
  return retval;
}


/*******************************************************************************
***
*** Function         : log_hexdump
*** Preconditions    : buf points to an area of memory and len is the length of
***                    this area
*** Postconditions   : An offset | hexadecimal | printable ASCII dump of the
***                    designated memory area is output
***
*******************************************************************************/


void log_hexdump(UBYTE *buf, UWORD buflen)
{ 
char LINE[80];
UWORD offset=0;
WORD left=(buflen & 0x7fff);
UWORD i,upto16,x;
unsigned char b;
  while (left>0) {
    for (i=0; i<78; i++)
      LINE[i]=' ';
    LINE[9]=LINE[59]='|';
    LINE[77]='\r';
    LINE[78]='\n';
    LINE[79]='\0';
    p_atos(LINE,"    %04X",offset); 
    LINE[8]=' ';
    upto16 = (left>16) ? 16 : (int)left;
    for (x=0; x<upto16; x++) {
      b = buf[offset+x];
      LINE[11+(3*x)]=hexdig((b&0xf0)>>4);
      LINE[12+(3*x)]=hexdig(b&0x0f);
      LINE[61+x]=p_isprint((char)b) ? ((char)b) : '.';
    }
    log_write((TEXT *)LINE,79);
    offset+=16;
    left-=16;
  }
}

/*******************************************************************************
***
*** Function         : asy_process(netif)
*** Preconditions    : at least one octet of serial data is available.
*** Postconditions   : Any further outstanding serial data is read into the
***                    serial input buffer.
***
*******************************************************************************/

GLDEF_C VOID asy_process(VOID)
{
  if (serLength) { /* Append further data to buffer */
    p_iow(serHandle, P_FTEST, (UWORD *)&serLength);

    /* Limit the amount that can be processed due to small buffer */
    if (serLength > (SERBUFSIZE-1)) {
      /* If you get one of these in your log, please mail me.. */
      log_printf("Mail Matt: Set SERBUFSIZE>%d\r\n", serLength);
      serLength = (SERBUFSIZE-1);
    }

    if (serLength > 0) /* If there was some more data.. */
      /* Read as much as we can into the serial buffer */
      p_read(serHandle, &serBuffer[1], serLength);

    /* Don't forget that first byte */
    serLength++;
  }
  /* log_hexdump((UBYTE *)serBuffer, (UWORD)serLength);*/

  /* Process serLength bytes in serBuffer */
  switch (state) {
    case STATE_WAITINITIALNAK:
      switch (serBuffer[0]) {
        case NAK: 
          /* OK, let's go. SendROM or SendRAM initialised the memory range 
             state, so start sending. */
          log_writesn("OK, Starting XMODEM send...");
          attempts = 0;
          state=STATE_SENDSECTOR;
          p_iosignal();
          break;
        case 'C': 
          useCRC = 1;
          log_writesn("OK, Starting XMODEM-CRC send...");
          attempts = 0;
          state=STATE_SENDSECTOR;
          p_iosignal();
          break;
        default:
          abandon((UBYTE *)"Eh? You should Download XMODEM on your PC");
          break;
      }
      break;
    case STATE_WAITCOMPLETEACK:
      if (serBuffer[0] == ACK) {
        log_writesn("OK, received final ACK. Finished.");
        state = 0;
      }
      else {
         log_writesn("Uh? Got this instead of the final ACK:");
         log_hexdump(serBuffer, serLength);
      }
      break;
    case STATE_WAITRESPONSE:
      switch (serBuffer[0]) {
        case ACK: 
          attempts = 0;
#ifdef DEBUG
          log_writesn("ACK");
#endif
          if (++sector > lastsector) {
            log_printf("FIN ------ SEG 0x%04x\r\n", segment);
            state = STATE_COMPLETE;
          }
          else {
            state = STATE_SENDSECTOR;
          }
          p_iosignal();
          break;
        case NAK:
          log_writesn("NAK: Retrying...");
          if (++attempts == MAXRETRY) {
            abandon((UBYTE *)"Giving up");
          }
          else {
            state = STATE_SENDSECTOR;
            p_iosignal();
          }
          break;
        case CAN:
          abandon((UBYTE *)"You cancelled from the remote side");
          break;
        case 'C':
          abandon((UBYTE *)"You shouldn't be sending C's after the first ACK/NAK..");
          break;
      }
      break;
  }
}




GLDEF_C VOID log_write(TEXT *buf, WORD len)
{
  p_write(winHandle, buf, len);
  p_iow(winHandle, P_FFLUSH);
}


GLDEF_C VOID log_writes(TEXT *s)
{
int len = p_slen(s);
  log_write(s,len);
}


GLDEF_C VOID log_writesn(TEXT *s)
{
static char szCRLF[]={0x0a,0x0d};
int len = p_slen(s);
  log_write(s, len);
  log_write(szCRLF, 2);
}


GLDEF_C VOID CDECL log_printf(TEXT *fmt,UINT arg,...)
{
UINT len;
UBYTE buf[256]; 
  len = p_atob((TEXT *)&buf[0], fmt, &arg);
  log_write((TEXT *)buf, len);
}


/*******************************************************************************
***
*** Function         : HomeCursor()
*** Preconditions    : 
*** Postconditions   : As it says...
***
*******************************************************************************/

GLDEF_C VOID HomeCursor(VOID)
{
static P_POINT home = {0,0};
static UWORD func = P_SCR_POSA;
  p_iow(winHandle, P_FSET, &func, &home);
}


/*******************************************************************************
***
*** Function         : ClearWholeWindow
*** Preconditions    : 
*** Postconditions   : Clear the entire screen and home cursor
***
*******************************************************************************/

GLDEF_C VOID ClearWholeWindow(VOID)
{
P_RECT whole;

  whole.tl.x=whole.tl.y=0;
  whole.br.x=machine_fullscreenwidth;
  whole.br.y=machine_fullscreenheight;
  gClrRect(&whole,G_TRMODE_CLR);
  HomeCursor();
}


/*******************************************************************************
***
*** Function         : SetStyle
*** Preconditions    : 
*** Postconditions   : Set the style of the current graphics context
***
*******************************************************************************/

GLDEF_C VOID SetStyle(INT style)
{
G_GC gc;

  gc.style=style;
  gSetGC(0,G_GC_MASK_STYLE,&gc);
}



/*******************************************************************************
***
*** Function         : Confirm
*** Preconditions    : 
*** Postconditions   : Present a "No/Yes" dialog
***
*******************************************************************************/

GLDEF_C INT Confirm(TEXT *pb)
{
  if (uOpenDialog(pb))    /* title text supplied by caller */
    return(-1);
  if (uAddButtonList("No",-'n',"Yes",'y',NULL))
    return(-1);
  return(uRunDialog()!='y');   /* return TRUE iff user answered Yes */
}



/*******************************************************************************
***
*** Function         : ExitApplication
*** Preconditions    : User wants to exit
*** Postconditions   : 
***
*******************************************************************************/

GLDEF_C VOID ExitApplication(VOID)
{
  if (!Confirm("Exit Penguin?"))
    p_exit(0);
}


/*******************************************************************************
***
*** Function         : ManageCommand
*** Preconditions    : The user selected a menu item
*** Postconditions   : Switch on command index to appropriate function
***
*******************************************************************************/

GLDEF_C VOID ManageCommand(INT index)
{
  switch (index) {
    case MENU_SENDROM:
      SendROM();
      break;

    case MENU_TESTBOOT:
    case MENU_BOOT:
    case MENU_EXIT:
      ExitApplication();
    break;
  }
}



/*******************************************************************************
***
*** Function         : ProcessSystemCommand
*** Preconditions    : 
*** Postconditions   : Respond to message from System Screen
***
*******************************************************************************/

GLDEF_C VOID ProcessSystemCommand(VOID)
{ 
TEXT buf[P_FNAMESIZE+2];

  wGetCommand((UBYTE *)&buf[0]);
  if (buf[0]=='X')
    ExitApplication();
}


/*******************************************************************************
***
*** Function         : TryExecuteCommand
*** Preconditions    : 
*** Postconditions   : Try to find command to execute matching accelerator 
***                    passed
***
*******************************************************************************/

GLDEF_C VOID TryExecuteCommand(INT keycode)
{
  keycode=uLocateCommand(keycode);        /* look up accelerator table */
  if (keycode>=0)                 /* found - keycode converted to index */
    ManageCommand(keycode);
}


/*******************************************************************************
***
*** Function         : ResizeWidth
*** Preconditions    : 
*** Postconditions   : Try to change the width of the main display region 
***
*******************************************************************************/

GLDEF_C INT ResizeWidth(UWORD width)
{
INT ret;
W_WINDATA wd;

  wd.extent.tl.x=wd.extent.tl.y=0;
  wd.extent.width=width;
  wd.extent.height=machine_fullscreenheight;
  wSetWindow(MainWid,W_WIN_EXTENT,&wd);
  ret=uErrorValue(wCheckPoint());     /* check whether successful */
  if (!ret)
    ScreenWidth=width;
  return(ret);
}


/*******************************************************************************
***
*** Function         : CycleStatusWindow
*** Preconditions    : 
*** Postconditions   : Attempt to flip through the different states of the 
***                    status window 
***
*******************************************************************************/

GLDEF_C VOID CycleStatusWindow(VOID)
{
P_EXTENT pextent;
UWORD NewStatWinState;

  switch (StatWinState) {
    case W_STATUS_WINDOW_BIG:
      /* big status window is currently showing -> go to off */
      if (ResizeWidth(machine_fullscreenwidth))
        return;
      StatWinState = W_STATUS_WINDOW_OFF;
      wStatusWindow(StatWinState);
      break;
    case W_STATUS_WINDOW_OFF:
      /* Status window not currently showing -> go to narrow */
      /* How wide will the status window be? shrink the main window
         by that much. */
      wInquireStatusWindow(W_STATUS_WINDOW_SMALL, &pextent);
      if (ResizeWidth(machine_fullscreenwidth - pextent.width))
        return;
      StatWinState = W_STATUS_WINDOW_SMALL;
      wStatusWindow(StatWinState);
      break;
    case W_STATUS_WINDOW_SMALL:
      /* Small status window currently showing -> go to off or big
         depending on whether we're running on a Siena or 3A/C */
      /* How wide will the status window be? shrink the main window
         by that much. */
      NewStatWinState = ((machine_type==MACH_SIENA ||
                          machine_type==MACH_EM_SIENA) ? W_STATUS_WINDOW_OFF :
                                                         W_STATUS_WINDOW_BIG);
      wInquireStatusWindow(NewStatWinState, &pextent);
      if (ResizeWidth(machine_fullscreenwidth - pextent.width))
        return;
      StatWinState = NewStatWinState;
      wStatusWindow(StatWinState);
      break;
  }

  /*ClearWholeWindow(); */
  /* Redraw here? */
}



/*******************************************************************************
***
*** Function         : key_process
*** Preconditions    : A keypress is passed
*** Postconditions   : It's handled.
***
*******************************************************************************/

GLDEF_C VOID key_process(VOID)
{
INT ret, code;
  if (key.keycode & W_EVENT_KEY) {
    switch (key.keycode) {
      case CONS_EVENT_FOREGROUND:
        break;
      case CONS_EVENT_COMMAND:
        ProcessSystemCommand();
        break;
    }
  }
  else if (key.keycode & W_SPECIAL_KEY) {
    /* Psion key pressed */
    code = key.keycode & (~W_SPECIAL_KEY);
    if (key.modifiers & W_SHIFT_MODIFIER)
      code = p_toupper(code);
    TryExecuteCommand(code);
  }
  else {
    switch (key.keycode) {
      case W_KEY_MENU:
        if (key.modifiers & W_CTRL_MODIFIER) {
          CycleStatusWindow();
        }
        else {           /* display menu bar */
          ret=uPresentMenus();
          if (ret>0)
            TryExecuteCommand(ret);
        }
        break;
      default:
        break;
    }
  }
}



/*******************************************************************************
***
*** Function         : MainLoop
*** Preconditions    : The system has been initialised.
*** Postconditions   : This is the main loop - control never passes out.
***
*******************************************************************************/

GLDEF_C VOID MainLoop(VOID)
{
  FOREVER {
    if (keyactive)
      wFlush();
    else {
      uGetKeyA(&keystat,&key);
      keyactive=TRUE;
    }

    p_iowait();

    /* Do the main work */
    if (serHandle) { /* If port is open */
      switch (serStatus) {

        case E_FILE_PENDING:
          /* Request still outstanding - do nothing */
          break;

        case E_FILE_LINE:
          /* DTR or DCD was dropped */
          asy_close();
          wInfoMsg("Line was dropped");
          ExitApplication();
          break;

        case E_FILE_PARITY:
          wInfoMsg("Parity error");
          asy_queue();
          break;

        case E_FILE_FRAME:
          wInfoMsg("Frame error: check baud rate");
          asy_queue();
          break;

        case E_FILE_OVERRUN:
          wInfoMsg("Overrun error");
          asy_queue();
          break;

        case 0:
          /* Handle serial data */
          p_tickle();

          /* Collect any further outstanding serial data */
          asy_process();

          /* And queue another read */
          asy_queue();
          break;

        default: /* Some other asynchronous response code */
          log_printf("main_loop: Unknown P_FREAD completion code: %d\r\n", 
                     serStatus);
          asy_queue();
          break;
      }
    }

    /* Timer? */
    /*if (timer_status != E_FILE_PENDING) {
      timer_process();
      timer_queue();
    }*/

    /* This should be called userinterface_process, I suppose... */
    /* Keyboard? */
    if (keystat != E_FILE_PENDING) {
      key_process();
      keyactive = FALSE;
    }

    /* Do something with the state machine? */
    if (state != 0)
      StateMachine();
  }
}

/*******************************************************************************
***
*** Function         : CreateGC
*** Preconditions    : 
*** Postconditions   : Attempt to create default graphics context on main 
***                    window
***
*******************************************************************************/

GLDEF_C VOID CreateGC(VOID)
{
INT hgc;        /* graphics context handle (if successful) */
  hgc=gCreateGC0(MainWid);
  if (hgc<0)
    p_exit(hgc);
}



/*******************************************************************************
***
*** Function         : SetupConsoleFont()
*** Preconditions    : Font is set to a font, machine_fullscreencols,
***                    machine_fullscreenrows is the full of the screen.
*** Postconditions   : The console font is changed, and the console resized.
***
*******************************************************************************/

GLDEF_C VOID SetupConsoleFont(VOID)
{
UWORD func;
P_RECT rect;
  /* Change the console font. Can't do this on MCx00's */
  if (machine_type != MACH_MC200 && machine_type != MACH_MC400) {
    /* Set the font */
    func = P_SCR_FONT;
    p_iow(winHandle, P_FSET, &func, &Font);

    /* Set the console size and position */
    func = P_SCR_WSET;
    rect.tl.x=0;
    rect.tl.y=0;
    rect.br.x=machine_fullscreencols;
    rect.br.y=machine_fullscreenrows;
    if (p_iow(winHandle, P_FSET, &func, &rect)!=0)
      wInfoMsg("Couldn't resize console");
  }
}


/*******************************************************************************
***
*** Function         : SpecificInit
*** Preconditions    : HWIF has been initialised
*** Postconditions   : The rest of the system is initialised.
***
*******************************************************************************/

GLDEF_C VOID SpecificInit(VOID)
{
UWORD pflag,func;
LOCAL_D INT gc;
P_EXTENT pextent;
int serOpen;

  p_unmarka();
  _GreyLines = &lines[0];
  p_bfil(&lines[0], sizeof(lines), 0);
  uAddGreyUline('Z',&lines[0]);

  gc = gCreateGC0(uFindMainWid());
  if (_UseFullScreen) {
    /* Running on a Series 3a or Workabout */
    wFree(gc);
    uEnableGrey();
    gc = gCreateGC0(uFindMainWid());
  }

  /* Which type of machine are we running on? */
  machine_check();
  if (machine_type == MACH_UNKNOWN) {
    p_puts("Unknown machine!");
    p_exit(0);
  }

  /* Set initial configuration and limits based on machine type */
  Font.style = G_STY_MONO;
  switch (machine_type) {
    case MACH_SIENA:
      StatWinState = W_STATUS_WINDOW_SMALL;
      Font.id = WS_FONT_BASE+8;
      break;
    case MACH_EM_SIENA:
      StatWinState = W_STATUS_WINDOW_SMALL;
      Font.id = WS_FONT_BASE+8;
      break;
    case MACH_S3a:
      StatWinState = W_STATUS_WINDOW_BIG;
      Font.id = WS_FONT_BASE+8;
      break;
    case MACH_EM_S3a:
      StatWinState = W_STATUS_WINDOW_BIG;
      Font.id = WS_FONT_BASE+8;
      break;
    case MACH_S3c:
      StatWinState = W_STATUS_WINDOW_BIG;
      Font.id = WS_FONT_BASE+8;
      break;
    case MACH_S3:
      StatWinState = W_STATUS_WINDOW_SMALL;
      Font.id = WS_FONT_BASE;
      break;
    case MACH_MC400:
      StatWinState = W_STATUS_WINDOW_BIG;
      Font.id = WS_FONT_BASE+3;
      break;
    case MACH_MC200:
      StatWinState = W_STATUS_WINDOW_BIG;
      Font.id = WS_FONT_BASE+3;
      break;
    case MACH_HC:
      StatWinState = W_STATUS_WINDOW_SMALL;
      Font.id = WS_FONT_BASE+2;
      break;
    case MACH_WORKABOUT:
      StatWinState = W_STATUS_WINDOW_SMALL;
      Font.id = WS_FONT_BASE+8;
      _SmallFontDialog = TRUE;
      _UseFullScreen = TRUE;
      break;
    case MACH_EM_WORKABOUT:
      StatWinState = W_STATUS_WINDOW_SMALL;
      Font.id = WS_FONT_BASE+8;
      _SmallFontDialog = TRUE;
      break;
  }
  SetupConsoleFont();
  uEscape(FALSE);             /* disable Psion-Esc */
  MainWid=uFindMainWid();     /* find id of main display window */
  CreateGC();                 /* create default graphics context */
  /* analyse command line */
  hCrackCommandLine();
  /*DisplayStart();*/

  p_print("Penguin - SIBO-Linux Loader v%d.%02d\r\n", 
          version / 100, version % 100);
  p_puts("\270 1998 MJ Gumbley, et al.");
  p_puts("http://www.gumbley.demon.co.uk/");
  p_puts("RTFM please!");
  p_puts("This is ALPHA software.");
  p_puts("Serial link: 19200,8,N,1.");
  p_print("Machine: %s\r\n",machine_name);

  /* Turn on the cursor */
  pflag = TRUE;
  func = P_SCR_CURSOR;
  p_iow(winHandle, P_FSET, &func, &pflag);

  wInquireStatusWindow(StatWinState, &pextent);
  if (ResizeWidth(machine_fullscreenwidth - pextent.width))
    ExitApplication();
  wStatusWindow(StatWinState);

  /* Repeat open of serial port, while asy_open is false, and user wants 
     to retry */
  do {
    serOpen = asy_open();
    if (!serOpen) {
      if (!Confirm("PC not connected - Retry?"))
        ExitApplication();
    }
  }
  while (!serOpen);
  asy_write((UBYTE *)"Hello from Penguin!\r\n", (WORD)21);
}


/*******************************************************************************
***
*** Function         : main
*** Preconditions    : Zilch-o
*** Postconditions   : Happy user.
***
*******************************************************************************/

GLDEF_C VOID main(VOID)
{
  _UseFullScreen=TRUE;
  uCommonInit();                /* initialisation common to all Hwif programs */
  SpecificInit();                          /* initialisation specific to GS18 */
  MainLoop();                            /* the central event processing loop */
}

