
#define WIN32_LEAN_AND_MEAN
#define WINVER       0x0400 /* Windows 9x, Windows NT and later */
#define _WIN32_WINNT 0x0400 /* Windows NT and later */

#include <windows.h>
#include <winuser.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "os.h"

#define TRACE 1 ? ((void)0) : printf

#ifndef SPI_GETSCREENSAVEACTIVE
#define SPI_GETSCREENSAVEACTIVE   0x0010
#endif

#ifndef SPI_GETSCREENSAVERRUNNING
#define SPI_GETSCREENSAVERRUNNING 0x0072
#endif

#ifndef DESKTOP_ENUMERATE
#define DESKTOP_ENUMERATE 0x0040
#endif

#ifndef DESKTOP_SWITCHDESKTOP
#define DESKTOP_SWITCHDESKTOP 0x0100
#endif

static HMODULE g_hUser32DLL   = NULL;
static int     g_bInitialize  = 1;
static DWORD   g_dwWinVersion = 0;
static BOOL    (__stdcall * g_fnSystemParametersInfoA)(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) = NULL;
static HDESK   (__stdcall * g_fnOpenDesktopA)(char* lpszDesktop, DWORD dwFlags, BOOL fInherit, ACCESS_MASK dwDesiredAccess) = NULL;
static BOOL    (__stdcall * g_fnSwitchDesktop)(HDESK hDesktop) = NULL;
static BOOL    (__stdcall * g_fnCloseDesktop)(HDESK hDesktop) = NULL;
static int g_iDetectResult[2] = {0x10000, 0x10000};

/* free resources at exit */
static void DoneDetectLockedWorkstation()
{
    if (g_hUser32DLL)
    {
        HMODULE hUser32DLL = g_hUser32DLL;
        g_hUser32DLL = NULL;
        FreeLibrary (hUser32DLL);
    }
}

/*
 * this function tests, if a screen saver is configured and enabled
 * (it does not test the running state)
 */
static int IsScreenSaverActive()
{
    BOOL bActive  =FALSE;
    if (!g_fnSystemParametersInfoA)
        return -1;
    if (!(*g_fnSystemParametersInfoA)(SPI_GETSCREENSAVEACTIVE, 0, &bActive, 0))
        return -1;
    return bActive ? 1 : 0;
}

/*
 * this function tests, if the screen saver is running
 */
static int IsScreenSaverRunning()
{
    BOOL bActive = FALSE;
    if ((g_dwWinVersion & 0xFF) >= 5)
    {
        if (!g_fnSystemParametersInfoA)
            return -1;
        /*
         * Windows 2000 and higher
         * Will determine if screen saver is running whether or not the "Password Protected" box is checked
         */
        if (!(*g_fnSystemParametersInfoA)(SPI_GETSCREENSAVERRUNNING, 0, &bActive, 0))
            return -1;
    }
    else
    {
        HDESK hDesktop;

        if (!g_fnOpenDesktopA || !g_fnCloseDesktop)
            return -1;
        /*
         * Windows NT
         * Note, will only be TRUE if the "Password Protected" box is checked.
         */
        hDesktop = (*g_fnOpenDesktopA)("screen-saver", 0, FALSE, DESKTOP_ENUMERATE);
        if (hDesktop)
        {
            bActive = TRUE;
            (*g_fnCloseDesktop)(hDesktop);
        }
    }
    return bActive ? 1 : 0;
}

/*
 * this function tests, if the interactive workstation is locked (on Windows NT and later)
 */
static int IsWorkstationLocked()
{
    BOOL bLocked = FALSE;
    HDESK hDesktop;

    if (!g_fnOpenDesktopA || !g_fnSwitchDesktop || !g_fnCloseDesktop) return -1;

    hDesktop = (*g_fnOpenDesktopA)("Default", 0, FALSE, DESKTOP_SWITCHDESKTOP);
    if (hDesktop)
    {
        if (!(*g_fnSwitchDesktop)(hDesktop))
        {
            if (GetLastError())
            {
                (*g_fnCloseDesktop)(hDesktop);
                return -1;
            }
            bLocked = TRUE;
        }
        (*g_fnCloseDesktop)(hDesktop);
    }
    return bLocked ? 1 : 0;
}

/*
 * helper function for "os_DetectLockedWorkstation()", initializes globals on first call
 */
static int DetectInfo()
{
    int iDetectResult = g_iDetectResult[0];
    int i;

    if (g_bInitialize)
    {
        g_bInitialize = 0;

        g_dwWinVersion = GetVersion();
        TRACE("g_dwWinVersion = 0x%08lx\n", g_dwWinVersion);

        if ((g_dwWinVersion & 0xFF) < 4 ||
            ((g_dwWinVersion & 0x80000000) && (g_dwWinVersion & 0xFF) == 4 && ((g_dwWinVersion >> 8) & 0xFF) < 10))
        {
            /* Windows 95 and before not supported */
            TRACE("DetectLockedWorkstation failed\n");
            return -1;
        }
        /* Windows 98, Windows NT 4.0 and later full supported */

        g_hUser32DLL = LoadLibraryA ("user32.dll");
        TRACE("g_hUser32DLL = 0x%08lx\n", (unsigned long)g_hUser32DLL);
        if (!g_hUser32DLL)
        {
            TRACE("DetectLockedWorkstation failed\n");
            return -1;
        }

        atexit (&DoneDetectLockedWorkstation);
        g_fnSystemParametersInfoA = (BOOL (__stdcall *)(UINT, UINT, PVOID, UINT)) GetProcAddress (g_hUser32DLL, "SystemParametersInfoA");
        g_fnOpenDesktopA = (HDESK (__stdcall *)(char *, DWORD, BOOL, ACCESS_MASK)) GetProcAddress (g_hUser32DLL, "OpenDesktopA");
        g_fnSwitchDesktop = (BOOL (__stdcall *)(HDESK)) GetProcAddress (g_hUser32DLL, "SwitchDesktop");
        g_fnCloseDesktop = (BOOL (__stdcall *)(HDESK)) GetProcAddress (g_hUser32DLL, "CloseDesktop");

        TRACE("g_fnSystemParametersInfoA = 0x%08lx\n", (unsigned long)g_fnSystemParametersInfoA);
        TRACE("g_fnOpenDesktopA = 0x%08lx\n", (unsigned long)g_fnOpenDesktopA);
        TRACE("g_fnSwitchDesktop = 0x%08lx\n", (unsigned long)g_fnSwitchDesktop);
        TRACE("g_fnCloseDesktop = 0x%08lx\n", (unsigned long)g_fnCloseDesktop);

        if (!g_hUser32DLL || !g_fnSystemParametersInfoA ||
            (!(g_dwWinVersion & 0x80000000) && (!g_fnOpenDesktopA || !g_fnSwitchDesktop || !g_fnCloseDesktop)))
        {
            TRACE("DetectLockedWorkstation failed\n");
            DoneDetectLockedWorkstation ();
            return -1;
        }
    }
    if (!g_hUser32DLL) return -1;

    i = IsWorkstationLocked();    /* test locked workstation */
    if (i >= 0)
    {
        if (i > 0)
            iDetectResult |=  2;
        else
            iDetectResult &= ~2;
    }

    i = IsScreenSaverActive();    /* test, if the user has a screen saver configured */
    if (i > 0)
    {
        i = IsScreenSaverRunning(); /* is the screen saver running? */
        if (i >= 0)
        {
            if (i > 0)
                iDetectResult |=  1;
            else
                iDetectResult &= ~1;
        }
        else
        {
            /*
             * cannot detect, if the screen saver is running
             * copy state "locked workstation" state
             */

            if (iDetectResult & 2)
                iDetectResult |=  1;
            else
                iDetectResult &= ~1;
        }
    }
    else
    {
        /*
         * cannot detect, if the screen saver is running
         * or the user has no screen saver configured
         * copy state "locked workstation" state
         */

        if (iDetectResult & 2)
            iDetectResult |=  1;
        else
            iDetectResult &= ~1;
    }

    return (iDetectResult & 0xFF);
}


/*
 * should work on Windows 98/ME
 * works on Windows NT, if screen saver is password protected
 * works on Windows 2000 and later
 *
 * detects a running screen saver
 * detects a locked workstation on Windows NT
 *
 * result codes:
 *   <0     error; cannot detect anything; wrong OS version
 *   >=0 see following, set/active, if bit is set (==1)
 *       bit  0:  screen saver active
 *       bit  1:  NT workstation locked
 *       bit 16:  init mode active (please ignore this)
 */
int os_DetectLockedWorkstation()
{
    int iResult = DetectInfo();
    if (iResult < 0) return -1;

    if (g_iDetectResult[0] == iResult)
    {
        /*
         * a) g_iDetectResult[1]==iResult   no change
         * b) g_iDetectResult[1]!=iResult   final result
         */
        g_iDetectResult[1] = iResult;
        return iResult;
    }

    if (g_iDetectResult[0] != iResult && g_iDetectResult[1] == iResult)
    {
        /* short fluctuation: interpreted as "no change" */
        g_iDetectResult[0] = iResult;
        return iResult;
    }

    /*
     * a) first change
     * b) three different results
     * the first result is returned, the change will be effective the next run
     * if the result is the same
     */
    g_iDetectResult[0] = iResult;
    return g_iDetectResult[1];
}


static const char *os_packagesubdir (const char *sub)
{
    const char *key = "Software\\mICQ";
    char *result = NULL;
    HKEY reg_key = NULL;
    DWORD type;
    DWORD nbytes = 0;
    
    if (RegOpenKeyEx (HKEY_CURRENT_USER, key, 0, 
                      KEY_QUERY_VALUE, &reg_key) == ERROR_SUCCESS)
    {
        if (!RegQueryValueEx (reg_key, "InstallationDirectory", 0,
                              &type, NULL, &nbytes) == ERROR_SUCCESS
            || type != REG_SZ)
        {
             RegCloseKey (reg_key);
             reg_key = NULL;
        }
    }
    if (!reg_key && RegOpenKeyEx (HKEY_LOCAL_MACHINE, key, 0,
                                  KEY_QUERY_VALUE, &reg_key) == ERROR_SUCCESS)
    {
        if (!RegQueryValueEx (reg_key, "InstallationDirectory", 0,
                              &type, NULL, &nbytes) == ERROR_SUCCESS
            || type != REG_SZ)
        {
            RegCloseKey (reg_key);
            reg_key = NULL;
        }
    }
    if (!reg_key)
    {
        char *p;
        
        result = malloc (MAX_PATH + 1 + strlen (sub));
        if (!GetModuleFileName (NULL, result, MAX_PATH))
        {
            free (result);
            return NULL;
        }

        if ((p = strrchr (result, '\\')) != NULL)
            *p = '\0';

        p = strrchr (result, '\\');
        if (p && !strcasecmp (p + 1, "bin"))
            *p = '\0';
        nbytes = strlen (result);
        result[nbytes++] = '\\';
    }
    else
    {
        result = malloc (nbytes + 1 + strlen (sub));
        if (!result)
            return NULL;
        
        RegQueryValueEx (reg_key, "InstallationDirectory", 0,
                         &type, result, &nbytes);
        RegCloseKey (reg_key);
    }
    strcpy (result + nbytes, sub);
    return result;
}

const char *os_packagedatadir (void)
{
    static const char *datadir = NULL;
    if (!datadir)
        datadir = os_packagesubdir ("share");
    return datadir ? datadir : "C:\\mICQ";
}

const char *os_packagehomedir (void)
{
    static const char *homedir = NULL;
    if (!homedir)
        homedir = os_packagesubdir ("etc");
    return homedir;
}
