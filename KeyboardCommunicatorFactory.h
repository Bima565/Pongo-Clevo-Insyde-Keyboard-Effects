// Created by DeviceIoControl

#pragma once

#include "stdafx.h"
#include "DeviceIds.h"
#include "IKeyboardCommunicator.h"

#include "WmiKBCommunicator.h"
#include "InsydeKBCommunicator.h"
#include "FakeKeyboardCommunicator.h"

#define KB_COMM_INDEX_FAKE 0
#define KB_COMM_INDEX_WMI 1
#define KB_COMM_INDEX_INSYDE 2

class KeyboardCommunicatorFactory 
{
public:
	KeyboardCommunicatorFactory() 
	{
	}

	IKeyboardCommunicatorPtr Create(uint32_t deviceId)
	{
		switch (deviceId)
		{
		case DEVICE_ID_FAKE:
			return std::make_shared<FakeKeyboardCommunicator>();

		case DEVICE_ID_P650RS_G:
			return std::make_shared<WmiKBCommunicator>();

		case DEVICE_ID_NP50SXX:
			return std::make_shared<InsydeKBCommunicator>();
		}

		return nullptr;
	}
	
	~KeyboardCommunicatorFactory() = default;
};
