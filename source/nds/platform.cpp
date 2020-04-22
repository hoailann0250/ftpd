// ftpd is a server implementation based on the following:
// - RFC  959 (https://tools.ietf.org/html/rfc959)
// - RFC 3659 (https://tools.ietf.org/html/rfc3659)
// - suggested implementation details from https://cr.yp.to/ftp/filesystem.html
//
// Copyright (C) 2020 Michael Theall
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "platform.h"

#include "log.h"

#include <dswifi9.h>
#include <fat.h>

#include <netinet/in.h>

#include <cstring>

#ifndef CLASSIC
#error "NDS must be built in classic mode"
#endif

PrintConsole g_statusConsole;
PrintConsole g_logConsole;
PrintConsole g_sessionConsole;

namespace
{
struct in_addr s_addr = {0};
}

bool platform::networkVisible ()
{
	switch (Wifi_AssocStatus ())
	{
	case ASSOCSTATUS_DISCONNECTED:
	case ASSOCSTATUS_CANNOTCONNECT:
		s_addr.s_addr = 0;
		Wifi_AutoConnect ();
		break;

	case ASSOCSTATUS_SEARCHING:
	case ASSOCSTATUS_AUTHENTICATING:
	case ASSOCSTATUS_ASSOCIATING:
	case ASSOCSTATUS_ACQUIRINGDHCP:
		s_addr.s_addr = 0;
		break;

	case ASSOCSTATUS_ASSOCIATED:
		if (!s_addr.s_addr)
			s_addr = Wifi_GetIPInfo (nullptr, nullptr, nullptr, nullptr);
		return true;
	}

	return false;
}

bool platform::init ()
{
	sassert (fatInitDefault (), "Failed to initialize fat");

	videoSetMode (MODE_0_2D);
	videoSetModeSub (MODE_0_2D);

	vramSetBankA (VRAM_A_MAIN_BG);
	vramSetBankC (VRAM_C_SUB_BG);

	consoleInit (&g_statusConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 4, 0, true, true);
	g_logConsole = g_statusConsole;
	consoleInit (&g_sessionConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 4, 0, false, true);

	consoleSetWindow (&g_statusConsole, 0, 0, 32, 1);
	consoleSetWindow (&g_logConsole, 0, 1, 32, 23);
	consoleSetWindow (&g_sessionConsole, 0, 0, 32, 24);

	consoleDebugInit (DebugDevice_NOCASH);
	std::setvbuf (stderr, nullptr, _IONBF, 0);

	Wifi_InitDefault (INIT_ONLY);
	Wifi_AutoConnect ();

	return true;
}

bool platform::loop ()
{
	scanKeys ();

	if (keysDown () & KEY_START)
		return false;

	return true;
}

void platform::render ()
{
	swiWaitForVBlank ();
	consoleSelect (&g_statusConsole);
	std::printf ("\n%s %s%s",
	    STATUS_STRING,
	    s_addr.s_addr ? inet_ntoa (s_addr) : "Waiting on WiFi",
	    s_addr.s_addr ? ":5000" : "");
	std::fflush (stdout);
	drawLog ();
}

void platform::exit ()
{
	info ("Press any key to exit\n");
	render ();

	do
	{
		swiWaitForVBlank ();
		scanKeys ();
	} while (!keysDown ());
}