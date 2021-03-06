/***************************************************************************
 * This file is part of NUSspli.                                           *
 * Copyright (c) 2020 V10lator <v10lator@myway.de>                         *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.             *
 ***************************************************************************/

#include <wut-fixups.h>

#include <file.h>
#include <usb.h>
#include <utils.h>

#include <stdbool.h>
#include <stdio.h>

#include <coreinit/ios.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <iosuhax.h>
#include <iosuhax_devoptab.h>

int fsaHandle = -1;

static void iosuCallback(IOSError err, void *dummy)
{
	debugPrintf("IOSUHAX: Closing callback!");
	// An IOSError can't be a positive integer but IOS_ERROR_OK == 0,
	// so make sure we don't set fsaHandle to 0
	fsaHandle = err == 0 ? -1 : err;
}

void IOSUHAXHookClose()
{
	//close down wupserver, return control to mcp
	IOSUHAX_Close();
	//wait for mcp to return
	while(fsaHandle >= 0)
		OSSleepTicks(1024 << 10); //TODO: What's a good value here?
}

bool mountUSB()
{
	if(fsaHandle >= 0)
		return true;
	
	//take over mcp thread
	IOSError err = IOS_IoctlAsync(mcpHandle, 0x62, NULL, 0, NULL, 0, iosuCallback, NULL);
	if(err != IOS_ERROR_OK)
	{
		debugPrintf("IOSUHAX: Error sending IOCTL: %d", err);
		return false;
	}
	
	//let wupserver start up
	OSSleepTicks(OSMillisecondsToTicks(50)); // TODO: Extensively test this in release builds
	if(IOSUHAX_Open("/dev/mcp") < 0)
	{
		debugPrintf("IOSUHAX: Error opening!");
		return false;
	}
	
	fsaHandle = IOSUHAX_FSA_Open();
	if(fsaHandle < 0)
	{
		IOSUHAXHookClose();
		debugPrintf("IOSUHAX: Error opening FSA!");
		return false;
	}
	
	int ret = mount_fs("usb", fsaHandle, NULL, "/vol/storage_usb01");
	if(ret != 0 || !dirExists("usb:/"))
    {
        IOSUHAX_FSA_Close(fsaHandle);
		IOSUHAXHookClose();
		debugPrintf("IOSUHAX: error mounting USB drive: %#010x", ret);
		return false;
    }
	
	debugPrintf("IOSUHAX: USB drive mounted!");
	return true;
}

void unmountUSB()
{
	if(fsaHandle < 0)
		return;
	
	unmount_fs("usb");
	IOSUHAX_FSA_Close(fsaHandle);
	IOSUHAXHookClose();
	
	debugPrintf("IOSUHAX: USB drive unmounted!");
}
