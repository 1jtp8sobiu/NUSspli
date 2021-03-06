/***************************************************************************
 * This file is part of NUSspli.                                           *
 * Copyright (c) 2019-2020 Pokes303                                        *
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

#include <input.h>
#include <main.h>
#include <renderer.h>
#include <status.h>
#include <swkbd_wrapper.h>
#include <utils.h>
#include <menu/utils.h>

#include <stdio.h>
#include <uchar.h>

#include <vpad/input.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memfrmheap.h>
#include <coreinit/memory.h>
#include <coreinit/thread.h>

//WIP. This need a better implementation

VPADStatus vpad;

Swkbd_CreateArg createArg;

int globalMaxlength;
bool globalLimit;

bool okButtonEnabled;

bool isSpecial(char c)
{
	return isNumber(c) || isLowercase(c) || isUppercase(c) || c == ' ';
}

bool isUrl(char c)
{
	return isNumber(c) || isLowercase(c) || isUppercase(c) || c == '.' || c == '/' || c == ':' || c == '%' || c == '-' || c == '_'; //TODO
}

typedef bool (*checkingFunction)(char);

void SWKBD_Render(KeyboardChecks check)
{
	if (globalMaxlength != -1) {
		char *inputFormString = Swkbd_GetInputFormString();
		if(inputFormString != NULL)
		{
			size_t len = strlen(inputFormString);
			if(len != 0 && check != CHECK_NONE && check != CHECK_NUMERICAL)
			{
				checkingFunction cf;
				switch(check)
				{
					case CHECK_HEXADECIMAL:
						cf = &isHexa;
						break;
					case CHECK_NOSPECIAL:
						cf = &isSpecial;
						break;
					case CHECK_URL:
						cf = &isUrl;
						break;
					default:
						// DEAD CODE
						debugPrintf("0xDEADC0DE: %d", check);
						MEMFreeToDefaultHeap(inputFormString);
						return;
				}
				
				len = 0;
				while(true)
				{
					if(!cf(inputFormString[len]))
					{
						if(inputFormString[len] != '\0')
						{
							inputFormString[len] = '\0';
							Swkbd_SetInputFormString(inputFormString);
						}
						break;
					}
					len++;
				}
			}
			
			MEMFreeToDefaultHeap(inputFormString);
			okButtonEnabled = globalLimit ? len == globalMaxlength : len <= globalMaxlength;
		}
		else
			okButtonEnabled = false;
		Swkbd_SetEnableOkButton(okButtonEnabled);
	}
	
	Swkbd_ControllerInfo controllerInfo;
	controllerInfo.vpad = &vpad;
	controllerInfo.kpad[0] = NULL;
	controllerInfo.kpad[1] = NULL;
	controllerInfo.kpad[2] = NULL;
	controllerInfo.kpad[3] = NULL;
	Swkbd_Calc(controllerInfo); //TODO: Do this in a new thread?

	if (Swkbd_IsNeedCalcSubThreadFont())
	{
		Swkbd_CalcSubThreadFont(); //TODO: Do this in a new thread?
		debugPrintf("SWKBD nn::swkbd::IsNeedCalcSubThreadFont()");
	}

	drawKeyboard();
}

bool SWKBD_Show(KeyboardType type, int maxlength, bool limit, const char *okStr) {
	debugPrintf("SWKBD_Show()");
	if(!Swkbd_IsHidden())
	{
		debugPrintf("...while visible!!!");
		return false;
	}
	
	char16_t *okStrL;
	if(okStr == NULL)
		okStrL = NULL;
	else
	{
		size_t strLen = strlen(okStr) + 1;
		okStrL = MEMAllocFromDefaultHeap(strLen);
		if(okStrL != NULL)
			for(size_t i = 0; i < strLen; i++)
				okStrL[i] = okStr[i];
	}
	
	// Show the keyboard
	Swkbd_AppearArg appearArg;
	
	OSBlockSet(&appearArg.keyboardArg.configArg, 0, sizeof(Swkbd_ConfigArg));
	OSBlockSet(&appearArg.keyboardArg.receiverArg, 0, sizeof(Swkbd_ReceiverArg));
	
	appearArg.keyboardArg.configArg.languageType = Swkbd_LanguageType__English;
	appearArg.keyboardArg.configArg.unk_0x04 = 4;
	appearArg.keyboardArg.configArg.unk_0x08 = 2;
	appearArg.keyboardArg.configArg.unk_0x0C = 2;
	appearArg.keyboardArg.configArg.unk_0x10 = 2;
	appearArg.keyboardArg.configArg.unk_0x14 = -1;
	appearArg.keyboardArg.configArg.str = okStrL;
	appearArg.keyboardArg.configArg.framerate = FRAMERATE_30FPS;
	appearArg.keyboardArg.configArg.unk_0xA4 = -1;
	
	appearArg.keyboardArg.receiverArg.unk_0x14 = 2;
	
	appearArg.inputFormArg.unk_0x00 = type;
	appearArg.inputFormArg.unk_0x04 = -1;
	appearArg.inputFormArg.unk_0x08 = 0;
	appearArg.inputFormArg.unk_0x0C = 0;
	appearArg.inputFormArg.unk_0x14 = 0;
	appearArg.inputFormArg.unk_0x18 = 0x00008000; // Monospace seperator after 16 chars (for 32 char keys)
	appearArg.inputFormArg.unk_0x1C = true;
	appearArg.inputFormArg.unk_0x1D = true;
	appearArg.inputFormArg.unk_0x1E = true;
	globalMaxlength = appearArg.inputFormArg.maxTextLength = maxlength;
	
	bool kbdVisible = Swkbd_AppearInputForm(appearArg);
	debugPrintf("Swkbd_AppearInputForm(): %s", kbdVisible ? "true" : "false");
	
	if(okStrL != NULL)
		MEMFreeToDefaultHeap(okStrL);
	
	if(!kbdVisible)
		return false;
	
	globalLimit = limit;
	okButtonEnabled = limit || maxlength == -1;
	Swkbd_SetEnableOkButton(okButtonEnabled);
	
	debugPrintf("nn::swkbd::AppearInputForm success");
	return true;
}

void SWKBD_Hide()
{
	debugPrintf("SWKBD_Hide()");
	if(Swkbd_IsHidden())
	{
		debugPrintf("...while invisible!!!");
		return;
	}
	
	Swkbd_DisappearInputForm();
	
	// DisappearInputForm() wants to render a fade out animation
	while(!Swkbd_IsHidden())
		SWKBD_Render(CHECK_NONE);
}

bool SWKBD_Init()
{
	debugPrintf("SWKBD_Init()");
	createArg.fsClient = MEMAllocFromDefaultHeap(sizeof(FSClient));
	createArg.workMemory = MEMAllocFromDefaultHeap(Swkbd_GetWorkMemorySize(0));
	if(createArg.fsClient == NULL || createArg.workMemory == NULL)
	{
		debugPrintf("SWKBD: Can't allocate memory!");
		return false;
	}
	
	FSAddClient(createArg.fsClient, 0);
	
	createArg.regionType = Swkbd_RegionType__Europe;
	createArg.unk_0x08 = 0;
	
	return Swkbd_Create(createArg);
}

void SWKBD_Shutdown()
{
	if(createArg.workMemory != NULL)
	{
		Swkbd_Destroy();
		MEMFreeToDefaultHeap(createArg.workMemory);
		createArg.workMemory = NULL;
	}
	
	if(createArg.fsClient != NULL)
	{
		FSDelClient(createArg.fsClient, 0);
		MEMFreeToDefaultHeap(createArg.fsClient);
		createArg.fsClient = NULL;
	}
}

void readInput()
{
	VPADReadError vError;
	bool run = true;
	while(run)
	{
		VPADRead(VPAD_CHAN_0, &vpad, 1, &vError);
		
		if(!run)
			break;
		if(app == 2)
			continue;
		
		switch(vError)
		{
			case VPAD_READ_NO_SAMPLES:
				OSSleepTicks(10);
				break;
			case VPAD_READ_INVALID_CONTROLLER:
				OSBlockSet(&vpad, 0, sizeof(VPADStatus));
				addErrorOverlay("Error reading the WiiU Gamepad!");
				run = false;
				break;
			default:
				removeErrorOverlay();
				run = false;
		}
	}
}

bool showKeyboard(KeyboardType type, char *output, KeyboardChecks check, int maxlength, bool limit, const char *input, const char *okStr) {
	debugPrintf("Initialising SWKBD");
	if(!SWKBD_Show(type, maxlength, limit, okStr))
	{
		drawErrorFrame("Error showing SWKBD:\nnn::swkbd::AppearInputForm failed", B_RETURN);
		
		while(true)
		{
			showFrame();							
			if(vpad.trigger == VPAD_BUTTON_B)
				return false;
		}
	}
	debugPrintf("SWKBD initialised successfully");
	
	if(input != NULL)
		Swkbd_SetInputFormString(input);
	
	bool dummy;
	while(true)
	{
		VPADGetTPCalibratedPoint(VPAD_CHAN_0, &vpad.tpFiltered1, &vpad.tpNormal);
		vpad.tpFiltered2 = vpad.tpNormal = vpad.tpFiltered1;
		SWKBD_Render(check);
//		sleepTillFrameEnd();
		
		if(okButtonEnabled && (Swkbd_IsDecideOkButton(&dummy) || vpad.trigger == VPAD_BUTTON_A))
		{
			debugPrintf("SWKBD Ok button pressed");
			char *outputStr = Swkbd_GetInputFormString();
			strcpy(output, outputStr);
			MEMFreeToDefaultHeap(outputStr);
			SWKBD_Hide();
			return true;
		}
		
		bool close = vpad.trigger == VPAD_BUTTON_B;
		if(close)
		{
			char *inputFormString = Swkbd_GetInputFormString();
			if(inputFormString != NULL)
			{
				size_t ifslen = strlen(inputFormString);
				close = ifslen == 0;
				if(!close)
				{
					inputFormString[ifslen] = '\0';
					Swkbd_SetInputFormString(inputFormString);
				}
				MEMFreeToDefaultHeap(inputFormString);
			}
		}
		
		if(close || Swkbd_IsDecideCancelButton(&dummy)) {
			debugPrintf("SWKBD Cancel button pressed");
			SWKBD_Hide();
			return false;
		}
	}
}
