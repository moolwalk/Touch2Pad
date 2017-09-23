/*++
 
Copyright (C) Microsoft Corporation, All Rights Reserved.

Module Name:

    Device.cpp

Abstract:

    This module contains the implementation of the UMDF sample driver's
    device callback object.

    This sample demonstrates how to register for PnP event notification
    for an interface class, and how to handle arrival events.

Environment:

   Windows User-Mode Driver Framework (WUDF)

--*/

#include "internal.h"
#if defined(EVENT_TRACING)
#include "device.tmh"
#endif

HRESULT
CMyDevice::CreateInstance(
    _In_ IWDFDriver *FxDriver,
    _In_ IWDFDeviceInitialize * FxDeviceInit,
    _Out_ PCMyDevice *Device
    )
/*++
 
  Routine Description:

    This method creates and initializs an instance of the driver's
    device callback object.

  Arguments:

    FxDeviceInit - the settings for the device.

    Device - a location to store the referenced pointer to the device object.

  Return Value:

    Status

--*/
{
    PCMyDevice device;
    HRESULT hr;

#if DBG
	// Give the chance of 10 seconds to attach Visual Studio as the source-level debugger.
	Sleep(10000);
#endif

    //
    // Allocate a new instance of the device class.
    //

    device = new CMyDevice();

    if (NULL == device)
    {
        return E_OUTOFMEMORY;
    }

    //
    // Initialize the instance.
    //

    hr = device->Initialize(FxDriver, FxDeviceInit);

    if (SUCCEEDED(hr)) 
    {
        *Device = device;
    } 
    else 
    {
        device->Release();
    }

    return hr;
}

HRESULT
CMyDevice::Initialize(
    _In_ IWDFDriver           * FxDriver,
    _In_ IWDFDeviceInitialize * FxDeviceInit
    )
/*++
 
  Routine Description:

    This method initializes the device callback object and creates the
    partner device object.

    Then it registers for toaster device notifications.


  Arguments:

    FxDeviceInit - the settings for this device.

  Return Value:

    status.

--*/
{
    CComPtr<IWDFDevice> fxDevice;
    HRESULT hr = S_OK;

    //
    // Save a weak reference to the Fx driver object. We'll need it to create
    // CMyRemoteTarget objects.
    //
    
    m_FxDriver = FxDriver;

    //
    // Configure things like the locking model before we go to create our 
    // partner device.
    //

    //
    // We don't need device level locking since we do not keep any state
    // across the requests
    //
    FxDeviceInit->SetLockingConstraint(None);

    //
    // Mark ourselves as a filter
    //
    FxDeviceInit->SetFilter();

    //
    // Hidclass is the power policy owner so relinquish power policy ownership
    //
    FxDeviceInit->SetPowerPolicyOwnership(FALSE);

	//
	// Create a new FX device object and assign the new callback object to 
	// handle any device level events that occur.
	//

	//
	// QueryIUnknown references the IUnknown interface that it returns
	// (which is the same as referencing the device).  We pass that to 
	// CreateDevice, which takes its own reference if everything works.
	//

    if (SUCCEEDED(hr))
	{
        IUnknown *unknown = this->QueryIUnknown();

        //
        // Create a new FX device object and assign the new callback object to 
        // handle any device level events that occur.
        //
        hr = FxDriver->CreateDevice(FxDeviceInit, unknown, &fxDevice);

        unknown->Release();
    }

    //
    // If that succeeded then set our FxDevice member variable.
    //

    CComPtr<IWDFDevice2> fxDevice2;
    if (SUCCEEDED(hr))
    {
        //
        // Q.I. for the latest version of this interface.
        //
        hr = fxDevice->QueryInterface(IID_PPV_ARGS(&fxDevice2));
    }

    if (SUCCEEDED(hr))
    {
        //
        // Store a weak reference to the IWDFDevice2 interface. Since this object
        // is partnered with the framework object they have the same lifespan - 
        // there is no need for an additional reference.
        //

        m_FxDevice = fxDevice2;
    }

    return hr;
}

HRESULT
CMyDevice::Configure(
    VOID
    )
/*++
 
  Routine Description:

    This method is called after the device callback object has been initialized 
    and returned to the driver.  It would setup the device's queues and their 
    corresponding callback objects.

  Arguments:

    FxDevice - the framework device object for which we're handling events.

  Return Value:

    status

--*/
{
    PCMyQueue defaultQueue = NULL;
    PCMyManualQueue manualQueue = NULL;
    HRESULT hr;

    m_hHidUpperDetect = CreateEvent(NULL, FALSE, FALSE, NULL);

    //
    // create a default queue
    //
    hr = CMyQueue::CreateInstance(this, &defaultQueue);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = defaultQueue->Configure();
    defaultQueue->Release();

    if (FAILED(hr))
    {
        return hr;
    }

    //
    // create a manual queue for holding read requests
    //
    hr = CMyManualQueue::CreateInstance(this, &manualQueue);
    if (FAILED(hr))
    {
        return hr;
    }

    m_ManualQueue = manualQueue;
    manualQueue->Release();

    return hr;
}

HRESULT
CMyDevice::QueryInterface(
    _In_ REFIID InterfaceId,
    _Out_ PVOID *Object
    )
/*++
 
  Routine Description:

    This method is called to get a pointer to one of the object's callback
    interfaces.  

  Arguments:

    InterfaceId - the interface being requested

    Object - a location to store the interface pointer if successful

  Return Value:

    S_OK or E_NOINTERFACE

--*/
{
    HRESULT hr;

	hr = CUnknown::QueryInterface(InterfaceId, Object);
    
    return hr;
}