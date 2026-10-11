// Created by DeviceIoControl
//
// Entry point for keyboard colour effects. Expanded logging and newer
// animations (strobe/music) rely on this file being up to date.

#include "stdafx.h"
#include <fstream>

#include "Test.h"
#include "Keyboard.h"
#include "Animations.h"
#include "ServiceMain.h"
#include "CommandLine.h"

#pragma warning(disable: 4995)

namespace
{
	// Minimal timestamped logger so we can inspect batch-menu selections.
	void Log(const std::wstring& line)
	{
		SYSTEMTIME st{};
		GetLocalTime(&st);

		wchar_t ts[32]{};
		swprintf_s(ts, L"%04u-%02u-%02u %02u:%02u:%02u",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

		std::wofstream log(L"run.log", std::ios::app);
		if (log.is_open())
		{
			log << ts << L" | " << line << L"\n";
		}
	}
}

int wmain(int argc, const wchar_t* argv[])
{
	SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);

	//
	// SERVICE_TABLE_ENTRYW svcTableEntry{};
	// svcTableEntry.lpServiceName = g_strServiceName.data();
	// svcTableEntry.lpServiceProc = ServiceMain;
	// 
	// StartServiceCtrlDispatcherW(&svcTableEntry);
	//
	Keyboard keyboard;
	const auto cmdLines = CommandLine::GetCommandLines(argc, argv);

	std::unique_ptr<IAnimation> pAnimation{};

	// Start global keypress listener (once) for brightness boost effects.
	KeypressBoost::Start(300, 0.25, 1.0);

	// Log the arguments we received (helps diagnose batch-menu issues).
	std::wstring argLine{};
	for (int i = 1; i < argc; ++i)
	{
		if (i > 1)
		{
			argLine += L" ";
		}
		argLine += argv[i];
	}
	Log(L"Args: " + (argLine.empty() ? L"<none>" : argLine));

	if (!CommandLine::Contains(L"theme", cmdLines)) 
	{
		Log(L"Missing 'theme' argument");
		std::cout << "Invalid command line. Command is: KeyboardColour.exe theme [themeName]\n";
		return ERROR_INVALID_PARAMETER;
	}
	
	if (CommandLine::Contains(L"breathe", cmdLines)) 
	{
		Log(L"Selected animation: breathe");
		pAnimation = std::make_unique<BreatheAnimation>();
	}
	else if (CommandLine::Contains(L"colourshift", cmdLines))
	{
		Log(L"Selected animation: colourshift");
		pAnimation = std::make_unique<ColourShiftAnimation>();
	}
	else if (CommandLine::Contains(L"newcolourbreathe", cmdLines)) 
	{
		Log(L"Selected animation: newcolourbreathe");
		pAnimation = std::make_unique<BreatheAnimationNewColours>();
	}
	else if (CommandLine::Contains(L"colourtransform", cmdLines)) 
	{
		Log(L"Selected animation: colourtransform");
		pAnimation = std::make_unique<ColourTransformAnimation>();
	}
	else if (CommandLine::Contains(L"blink", cmdLines)) 
	{
		Log(L"Selected animation: blink");
		pAnimation = std::make_unique<PulsatingBlinkAnimation>();
	}
	else if (CommandLine::Contains(L"strobe", cmdLines))
	{
		Log(L"Selected animation: strobe");
		pAnimation = std::make_unique<StrobeTransformAnimation>();
	}
	else if (CommandLine::Contains(L"musicadaptive", cmdLines))
	{
		Log(L"Selected animation: musicadaptive");
		pAnimation = std::make_unique<MusicReactiveColourTransformAnimation>();
	}
	else if (CommandLine::Contains(L"fireflicker", cmdLines) ||
	         CommandLine::Contains(L"colourtransformboost", cmdLines) ||
	         CommandLine::Contains(L"colortransformboost", cmdLines) ||
	         CommandLine::Contains(L"transformboost", cmdLines))
	{
		Log(L"Selected animation: fireflicker (boosted colour transform)");
		pAnimation = std::make_unique<ColourTransformBoostAnimation>();
	}
	else if (CommandLine::Contains(L"rgbzones", cmdLines))
	{
		Log(L"Selected animation: rgbzones");
		pAnimation = std::make_unique<RgbZoneTestAnimation>();
	}
	else if (CommandLine::Contains(L"screenreactive", cmdLines))
	{
		Log(L"Selected animation: screenreactive");
		pAnimation = std::make_unique<ScreenReactiveAnimation>();
	}

	if (pAnimation)
	{
		keyboard.PlayAnimation(*pAnimation, true);
		Log(L"Animation play invoked");
	}
	else 
	{
		Log(L"No animation matched arguments");
		std::cout << "No animation was provided. Exiting...\n";
	}
	
	return 0;
}
