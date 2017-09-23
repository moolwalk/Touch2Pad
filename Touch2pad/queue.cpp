/*++
 
Copyright (C) Microsoft Corporation, All Rights Reserved.

Module Name:

    Queue.cpp

Abstract:

    This module contains the implementation of the driver's
    queue callback object.

Environment:

   Windows User-Mode Driver Framework (WUDF)

--*/

#include "internal.h"
#include "queue.h"
#include "Gesture.h"


#if defined(EVENT_TRACING)
#include "queue.tmh"
#endif


//
// This is the default report descriptor for the Hid device provided
// by the mini driver in response to IOCTL_HID_GET_REPORT_DESCRIPTOR.
// 
// AC: Aplication Control
// AL: Application Launch
UCHAR G_DefaultReportDescriptor[] = {
    0x05, 0x01,                         // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                         // USAGE (Mouse)
    0xa1, 0x01,                         // COLLECTION (Application)
    0x85, REPORTID_MOUSE,               //   REPORT_ID (Mouse)
    0x09, 0x01,                         //   USAGE (Pointer)
    0xa1, 0x00,                         //   COLLECTION (Physical)
    0x05, 0x09,                         //     USAGE_PAGE (Button)
    0x19, 0x01,                         //     USAGE_MINIMUM (Button 1)
    0x29, 0x02,                         //     USAGE_MAXIMUM (Button 2)
    0x15, 0x00,                         //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                         //     LOGICAL_MAXIMUM (1)
    0x75, 0x01,                         //     REPORT_SIZE (1)
    0x95, 0x02,                         //     REPORT_COUNT (2)
    0x81, 0x02,                         //       INPUT (Data,Var,Abs)
    0x95, 0x06,                         //     REPORT_COUNT (6)
    0x81, 0x03,                         //       INPUT (Cnst,Var,Abs)
    0x05, 0x01,                         //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                         //     USAGE (X)
    0x09, 0x31,                         //     USAGE (Y)
    0x75, 0x10,                         //     REPORT_SIZE (16)
    0x95, 0x02,                         //     REPORT_COUNT (2)
	0x16, 0x00, 0xFC,                   //     LOGICAL_MINIMUM (-1024)
    0x26, 0xff, 0x03,                    //    LOGICAL_MAXIMUM (1023)
    0x81, 0x06,                         //       INPUT (Data,Var,Rel)
	0x09, 0x38,                         //     USAGE (Wheel)
	0x15, 0x81,                         //     LOGICAL_MINIMUM (-127)
	0x25, 0x7f,                         //     LOGICAL_MINIMUM (127)
	0x75, 0x08,                         //     REPORT_SIZE (8)
	0x95, 0x01,                         //     REPORT_COUNT (1)
	0x81, 0x06,                         //       INPUT (Data,Var,Rel)
    0xc0,                               //   END_COLLECTION
    0xc0,                               // END_COLLECTION
};

HID_DESCRIPTOR G_DefaultHidDescriptor =
{
	sizeof(HID_DESCRIPTOR),             //bLength
	HID_HID_DESCRIPTOR_TYPE,            //bDescriptorType
	HID_REVISION,                       //bcdHID
	0,                                  //bCountry - not localized
	1,                                  //bNumDescriptors
	{                                   //DescriptorList[0]
		HID_REPORT_DESCRIPTOR_TYPE,     //bReportType
		sizeof(G_DefaultReportDescriptor)       //wReportLength
	}
};

HRESULT
CMyQueue::CreateInstance(
    _In_ PCMyDevice Device,
    _Out_ CMyQueue **Queue
    )
/*++
 
  Routine Description:

    This method creates and initializs an instance of the driver's 
    queue callback object.

  Arguments:

    FxDeviceInit - the settings for the device.

    Queue - a location to store the referenced pointer to the queue object.

  Return Value:

    Status

--*/
{
    CMyQueue *queue;

    HRESULT hr = S_OK;

    //
    // Allocate a new instance of the device class.
    //

    queue = new CMyQueue(Device);

    if (NULL == queue)
    {
        hr = E_OUTOFMEMORY;
    }

    //
    // Initialize the instance.
    //

    if (SUCCEEDED(hr)) 
    {
        hr = queue->Initialize(Device->GetFxDevice());
    }

    if (SUCCEEDED(hr)) 
    {
        queue->AddRef();
        *Queue = queue;
    }

    if (NULL != queue)
    {
        queue->Release();
    }

    return hr;
}

HRESULT
CMyQueue::QueryInterface(
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

    if(IsEqualIID(InterfaceId, __uuidof(IQueueCallbackDeviceIoControl))) 
    {
        hr = S_OK;
        AddRef();
        *Object = static_cast<IQueueCallbackDeviceIoControl *>(this);
    } 
    else 
    {
        hr = CUnknown::QueryInterface(InterfaceId, Object);
    }

    return hr;
}

HRESULT
CMyQueue::Initialize(
    _In_ IWDFDevice *FxDevice
    )
/*++
 
  Routine Description:

    This method initializes the queue callback object.  Any operations which
    need to be performed before the caller can use the callback object, but 
    which couldn't be done in the constructor becuase they could fail would
    be placed here.

  Arguments:

    FxDevice - the device which this Queue is for.

  Return Value:

    status.

--*/
{
    IWDFIoQueue     *fxQueue;
    HRESULT hr;

    //
    // Create the framework queue
    //
    IUnknown *unknown = QueryIUnknown();
    hr = FxDevice->CreateIoQueue(
                        unknown,
                        TRUE,                        // bDefaultQueue
                        WdfIoQueueDispatchParallel, 
                        FALSE,                       // bPowerManaged
                        TRUE,                        // bAllowZeroLengthRequests
                        &fxQueue 
                        );
    if (FAILED(hr))
    {
        Trace(
            TRACE_LEVEL_ERROR, 
            "%!FUNC!: Could not create default I/O queue, %!hresult!",
            hr
            );
    }

    if (SUCCEEDED(hr)) 
    {
        m_FxQueue = fxQueue;

        //
        // m_FxQueue is kept as a Weak reference to framework Queue object to avoid 
        // circular reference. This object's lifetime is contained within 
        // framework Queue object's lifetime
        //
        
        fxQueue->Release();
    }

    unknown->Release();

    return hr;
}

VOID
STDMETHODCALLTYPE
CMyQueue::OnDeviceIoControl(
    _In_ IWDFIoQueue *FxQueue,
    _In_ IWDFIoRequest *FxRequest,
    _In_ ULONG ControlCode,
    _In_ SIZE_T InputBufferSizeInBytes,
    _In_ SIZE_T OutputBufferSizeInBytes
    )
/*++

Routine Description:

    DeviceIoControl dispatch routine

Aruments:
    
    FxQueue - Framework Queue instance
    FxRequest - Framework Request  instance
    ControlCode - IO Control Code
    InputBufferSizeInBytes - Lenth of input buffer
    OutputBufferSizeInBytes - Lenth of output buffer

    Always succeeds DeviceIoIoctl
Return Value:

    VOID

--*/
{
    UNREFERENCED_PARAMETER(FxQueue);
    UNREFERENCED_PARAMETER(InputBufferSizeInBytes);
    UNREFERENCED_PARAMETER(OutputBufferSizeInBytes);
    
    HRESULT hr = S_OK;
    BOOLEAN completeRequest = TRUE;
    IWDFIoRequest2 *fxRequest2;

    hr = FxRequest->QueryInterface(IID_PPV_ARGS(&fxRequest2));
    if (FAILED(hr)){
        FxRequest->Complete(hr);
        return;
    }
    fxRequest2->Release();

    switch (ControlCode)
    {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:   // METHOD_NEITHER
        //
        // Retrieves the device's HID descriptor.
        //
        hr = GetHidDescriptor(fxRequest2);
        break;
    
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:    // METHOD_NEITHER
        //
        //Retrieves a device's attributes in a HID_DEVICE_ATTRIBUTES structure.
        //
        hr = GetDeviceAttributes(fxRequest2);
        break;

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:    // METHOD_NEITHER
        //
        //Obtains the report descriptor for the HID device.
        //
        hr = GetReportDescriptor(fxRequest2);
        break;

    case IOCTL_HID_READ_REPORT:    // METHOD_NEITHER
        //
        // Returns a report from the device into a class driver-supplied 
        // buffer. 
        //
        hr = ReadReport(fxRequest2, &completeRequest);
        break;

    case IOCTL_UMDF_HID_GET_FEATURE:       // METHOD_NEITHER

        hr = GetFeature(fxRequest2);
        break;

    case IOCTL_UMDF_HID_GET_INPUT_REPORT:  // METHOD_NEITHER

        hr = GetInputReport(fxRequest2);
        break;

    case IOCTL_UMDF_HID_SET_FEATURE:       // METHOD_NEITHER

        hr = SetFeature(fxRequest2);
        break;

    case IOCTL_UMDF_HID_SET_OUTPUT_REPORT: // METHOD_NEITHER

        hr = SetOutputReport(fxRequest2);
        break;

    case IOCTL_HID_WRITE_REPORT:           // METHOD_NEITHER
        //
        // Transmits a class driver-supplied report to the device.
        //
        hr = WriteReport(fxRequest2);
        break;

    case IOCTL_HID_GET_STRING:                      // METHOD_NEITHER     
    case IOCTL_HID_GET_INDEXED_STRING:              // METHOD_OUT_DIRECT
        //
        // Transmits a class driver-supplied report to the device.
        //
        hr = GetString(fxRequest2);
        break;

    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:  // METHOD_NEITHER
        //
        // This has the USBSS Idle notification callback. If the lower driver
        // can handle it (e.g. USB stack can handle it) then pass it down
        // otherwise complete it here as not inplemented. For a virtual
        // device, idling is not needed.
        //
        // Not implemented. fall through...
        //
    case IOCTL_HID_ACTIVATE_DEVICE:                 // METHOD_NEITHER
    case IOCTL_HID_DEACTIVATE_DEVICE:               // METHOD_NEITHER
    case IOCTL_GET_PHYSICAL_DESCRIPTOR:             // METHOD_OUT_DIRECT 
        //
        // Not implemented. fall through...
        //
    default:
        hr = HRESULT_FROM_NT(STATUS_NOT_IMPLEMENTED);
        break;
    }

    //
    // Complete the request. Information value has already been set by request
    // handlers.
    //
    if (completeRequest) {
        fxRequest2->Complete(hr);
    }

    return;
}

HRESULT
CMyQueue::GetHidDescriptor(
    _In_ IWDFIoRequest2 *FxRequest
    )
/*++

Routine Description:

    This routines returns HID descriptor.

Arguments:

    Request - Handle to request object

Return Value:

    HRESULT

--*/
{
    HRESULT             hr = S_OK;
    size_t              bytesToCopy = 0;
    IWDFMemory *memory = NULL;

    //
    // Retrieve buffer
    //
    hr = FxRequest->RetrieveOutputMemory(&memory);
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, "RetrieveOutputMemory failed %!hresult!\n", hr);
        goto exit;        
    }

    //
    // Copy hard-coded HID Descriptor to request memory.
    //
    bytesToCopy = G_DefaultHidDescriptor.bLength;
    if (bytesToCopy == 0) 
    {
        hr = HRESULT_FROM_NT(STATUS_DATA_ERROR);
        Trace(TRACE_LEVEL_ERROR, "%!FUNC! desriptor length is zero %!hresult!\n", 
            hr);
        goto exit;        
    }

    hr = memory->CopyFromBuffer(0, 
                                (PVOID) &G_DefaultHidDescriptor,
                                bytesToCopy);
    
    if (FAILED(hr)) 
    {
        Trace(TRACE_LEVEL_ERROR, "%!FUNC! Buffer copy failed %!hresult!\n", hr);
        goto exit;;
    }

    //
    // Report how many bytes were copied
    //
    FxRequest->SetInformation(bytesToCopy);

exit:
    SAFE_RELEASE(memory);

    return hr;
}

HRESULT
CMyQueue::GetReportDescriptor(
    _In_ IWDFIoRequest2 *FxRequest
    )
/*++

Routine Description

    Handles IOCTL_UMDF_HID_GET_REPORT_DESCRIPTOR

Arguments:

Return Value:

    NT status value

--*/
{
    HRESULT     hr = S_OK;
    size_t      bytesToCopy = 0;
    IWDFMemory *memory = NULL;

    hr = FxRequest->RetrieveOutputMemory(&memory);
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, "RetrieveOutputMemory failed %!hresult!\n", hr);
        goto exit;        
    }

    bytesToCopy = memory->GetSize();
    if (bytesToCopy != sizeof(G_DefaultReportDescriptor)) {
        hr = E_INVALIDARG;
        Trace(TRACE_LEVEL_ERROR, "Incorrect buffer size received %!hresult!\n", hr);
        goto exit;        
    }

    hr = memory->CopyFromBuffer(0, 
                                (PVOID) &G_DefaultReportDescriptor,
                                sizeof(G_DefaultReportDescriptor));
    if (FAILED(hr)) 
    {
        Trace(TRACE_LEVEL_ERROR, "%!FUNC! Buffer copy failed %!hresult!\n", hr);
        goto exit;;
    }

    //
    // Report how many bytes were copied
    //
    FxRequest->SetInformation(sizeof(G_DefaultReportDescriptor));

exit:
    
    SAFE_RELEASE(memory);

    return hr;
}

HRESULT
CMyQueue::GetDeviceAttributes(
    _In_ IWDFIoRequest2 *FxRequest
    )
/*++

Routine Description:

    Finds the HID descriptor and copies it into the buffer provided by the 
    Request.

Arguments:

    Device - Handle to WDF Device Object

    Request - Handle to request object

Return Value:

    NT status code.

--*/
{
    HRESULT             hr = S_OK;
    IWDFMemory *memory = NULL;
    PVOID buffer;
    SIZE_T bufferSizeCb;

    hr = FxRequest->RetrieveOutputMemory(&memory);
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, "RetrieveOutputMemory failed %!hresult!\n", hr);
        return hr;        
    }

    buffer = memory->GetDataBuffer(&bufferSizeCb);
    memory->Release();

    if (bufferSizeCb != sizeof (HID_DEVICE_ATTRIBUTES)) 
    {
        hr = HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! Insufficient HID attributes buffer size %!hresult!\n", hr);
        return hr;
    }

    CopyMemory(buffer, &m_Device->m_Attributes, sizeof (HID_DEVICE_ATTRIBUTES));
    
    //
    // Report how many bytes were copied
    //
    FxRequest->SetInformation(sizeof (HID_DEVICE_ATTRIBUTES));

    return hr;
}

HRESULT
CMyQueue::ReadReport(
    _In_ IWDFIoRequest2 *FxRequest,
    _Out_ BOOLEAN* CompleteRequest
    )
{
    HRESULT hr;
    PTP_TIMER timer = NULL;

    //
    // start the timer if not already started
    //
    timer = m_Device->m_ManualQueue->GetTimer();
    if (IsThreadpoolTimerSet(timer) == FALSE) 
    {     
        FILETIME dueTime;
        Trace(TRACE_LEVEL_INFORMATION, "*** Setting the timer ***\n");

        *reinterpret_cast<PLONGLONG>(&dueTime) = 
            -static_cast<LONGLONG>(MILLI_SECOND_TO_NANO100(1000));
        
        SetThreadpoolTimer(timer,
                           &dueTime,
                           5000,  // ms periodic
                           0      // optional delay in ms
                           );
    }

    //
    // forward the request to manual queue
    //
    hr = FxRequest->ForwardToIoQueue(m_Device->m_ManualQueue->GetQueue());
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! ForwardToIoQueue returned failure %!hresult!\n", hr);
        *CompleteRequest = TRUE;
    }
    else {
        *CompleteRequest = FALSE;
    }

    return hr;
}

HRESULT
CMyQueue::GetFeature(
    _In_ IWDFIoRequest2 *FxRequest
    )
/*++

Routine Description:

    Handles IOCTL_UMDF_HID_GET_FEATURE for all the collection.


Arguments:

    Request - Pointer to Request Packet.

Return Value:

    NT status code.

--*/
{
    HRESULT hr;
    IWDFMemory *memory = NULL;
    SIZE_T inBufferCb, outBufferCb;
    ULONG reportSize;
    PVOID inBuffer;
    UCHAR reportId;
    PUCHAR reportBuffer = NULL;
    PMY_DEVICE_ATTRIBUTES myAttributes;

    // *************************************************************************
    // IOCTL_UMDF_HID_GET_FEATURE
    // Input report ID:
    //     UMDF driver retrieves report ID associated with this collection 
    //     by retrieving the buffer containing report ID by calling 
    //     IWDFRequest::GetInputMemory() or RetrieveInputMemory. 
    // Output report buffer and length: 
    //     UMDF driver calls IWDFRequest::GetOutputMemory() to get memory 
    //     buffer and its length. The driver fills the buffer with feature data. 
    //
    // *************************************************************************

    Trace(TRACE_LEVEL_INFORMATION, "GetFeature\n");

    //
    // Get input report ID
    //
    hr = FxRequest->RetrieveInputMemory(&memory);
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, "RetrieveINputMemory failed %!hresult!\n", hr);
        return hr;        
    }

    inBuffer = memory->GetDataBuffer(&inBufferCb);
    SAFE_RELEASE(memory);
    
    if (inBufferCb < sizeof(reportId)) 
    {
        hr = HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! Insufficient buffer size %!hresult!\n", hr);
        return hr;
    }

    reportId = *(PUCHAR)inBuffer;
    if (reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        //
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        hr = HRESULT_FROM_NT(STATUS_INVALID_PARAMETER);
        Trace(TRACE_LEVEL_INFORMATION, 
            "Unexpected request, %!hresult!\n", hr);
        return hr;
    }

    //
    // Get output buffer
    //
    hr = FxRequest->RetrieveOutputMemory(&memory);
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, "RetrieveOutputMemory failed %!hresult!\n", hr);
        return hr;        
    }
    reportBuffer = (PUCHAR) memory->GetDataBuffer(&outBufferCb);
    SAFE_RELEASE(memory);
    
    //
    // Since output buffer is for write only (no read allowed by UMDF in output
    // buffer), any read from output buffer would be reading garbage), so don't 
    // let app embed custom control code in output buffer. The minidriver can 
    // support multiple features using separate report ID instead of using 
    // custom control code. Since this is targeted at report ID 1, we know it
    // is a request for getting attributes.
    //
    reportSize = sizeof(MY_DEVICE_ATTRIBUTES) + 1;  // +1 for report ID
    if (outBufferCb < reportSize)
    {
        hr = HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! Insufficient report buffer size %!hresult!\n", hr);
        return hr;
    }

    //
    // Since this device has one report ID, hidclass would pass on the report
    // ID in the buffer (it wouldn't if report descriptor did not have any report
    // ID). However, since UMDF allows only writes to an output buffer, we can't 
    // "read" the report ID from "output" buffer. There is no need to read the 
    // report ID since we get it other way as shown above, however this is 
    // something to keep in mind.
    //
    reportBuffer++;   // skip the report ID
    myAttributes = (PMY_DEVICE_ATTRIBUTES) reportBuffer;
    myAttributes->ProductID = m_Device->m_Attributes.ProductID;
    myAttributes->VendorID = m_Device->m_Attributes.VendorID;
    myAttributes->VersionNumber = m_Device->m_Attributes.VersionNumber;

    //
    // Report how many bytes were copied
    //
    FxRequest->SetInformation(reportSize);
    hr = S_OK;
    
    return hr;
}

HRESULT
CMyQueue::SetFeature(
    _In_ IWDFIoRequest2 *FxRequest
    )
/*++

Routine Description:

    Handles IOCTL_UMDF_HID_SET_FEATURE for all the collection.
    For control collection (custom defined collection) it handles
    the user-defined control codes for sideband communication

Arguments:

    Request - Pointer to Request Packet.

Return Value:

    NT status code.

--*/
{
    HRESULT hr;
    IWDFMemory *memory = NULL;
    SIZE_T inBufferCb, reportSize;
    UCHAR reportId; 
    SIZE_T pOutBufferSize;
    PHIDMINI_CONTROL_INFO controlInfo = NULL;
    
    // *************************************************************************
    // IOCTL_UMDF_HID_SET_FEATURE
    // Input report ID:
    //     UMDF driver retrieves report ID associated with this collection 
    //     through pOutBufferSize parameter passed to a call to 
    //     IWDFRequest::GetDeviceIoControlParameters DDI.
    // Input report buffer and length: 
    //     UMDF driver calls IWDFRequest::GetInputMemory() to get memory 
    //     buffer and its length. The buffer contains feature report data that
    //     driver transfers to the device. 
    //
    // *************************************************************************

    Trace(TRACE_LEVEL_INFORMATION, "SetFeature\n");

    //
    // Get report ID if needed.
    //
    FxRequest->GetDeviceIoControlParameters(NULL, NULL, &pOutBufferSize);
    reportId = (UCHAR) pOutBufferSize;

    if (reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        //
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        hr = HRESULT_FROM_NT(STATUS_INVALID_PARAMETER);
        Trace(TRACE_LEVEL_INFORMATION, 
            "Unknown request, %!hresult!\n", hr);
        return hr;
    }

    //
    // Get input report buffer. 
    //
    hr = FxRequest->RetrieveInputMemory(&memory);
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, "RetrieveInputMemory failed %!hresult!\n", hr);
        return hr;        
    }
    controlInfo = (PHIDMINI_CONTROL_INFO)memory->GetDataBuffer(&inBufferCb);
    SAFE_RELEASE(memory);

    //
    // before touching control code make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_CONTROL_INFO);
    Trace(TRACE_LEVEL_INFORMATION, "Expected report size: %I64d\n", reportSize);
    if (inBufferCb < reportSize) 
    {
        hr = HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! Unexpected buffer size %I64d, expected %I64d %!hresult!\n", 
            inBufferCb, reportSize, hr);
        return hr;
    }

    switch(controlInfo->ControlCode) 
    {
    case HIDMINI_CONTROL_CODE_SET_ATTRIBUTES:
        //
        // Store the device attributes in device extension
        //
        m_Device->m_Attributes.ProductID = controlInfo->u.Attributes.ProductID;
        m_Device->m_Attributes.VendorID = controlInfo->u.Attributes.VendorID;
        m_Device->m_Attributes.VersionNumber = controlInfo->u.Attributes.VersionNumber;

        //
        // set status and information
        //
        FxRequest->SetInformation(reportSize);
        hr = S_OK;

        break;

    case HIDMINI_CONTROL_CODE_DUMMY1:
        Trace(TRACE_LEVEL_INFORMATION,
            "Control Code HIDMINI_CONTROL_CODE_DUMMY1\n");
        hr = HRESULT_FROM_NT(STATUS_NOT_IMPLEMENTED);
        break;

    case HIDMINI_CONTROL_CODE_DUMMY2:
        Trace(TRACE_LEVEL_INFORMATION, 
            "Control Code HIDMINI_CONTROL_CODE_DUMMY2\n");
        hr = HRESULT_FROM_NT(STATUS_NOT_IMPLEMENTED);
        break;

    default:
        Trace(TRACE_LEVEL_INFORMATION, "Unknown control Code\n");
        hr = HRESULT_FROM_NT(STATUS_NOT_IMPLEMENTED);
        break;
    }

    return hr;
}

HRESULT
CMyQueue::GetInputReport(
    _In_ IWDFIoRequest2 *FxRequest
    )
/*++

Routine Description:

    Handles IOCTL_UMDF_HID_GET_INPUT_REPORT for all the collection.

Arguments:

    Request - Pointer to Request Packet.

Return Value:

    NT status code.

--*/
{
    HRESULT hr;
    IWDFMemory *memory = NULL;
    SIZE_T inBufferCb, outBufferCb;
    ULONG reportSize;
    PVOID inBuffer;
    UCHAR reportId;
    PHIDMINI_INPUT_REPORT reportBuffer = NULL;

    // *************************************************************************
    // IOCTL_UMDF_HID_GET_INPUT_REPORT
    // Input report ID:
    //     UMDF driver retrieves report ID associated with this collection 
    //     by retrieving the buffer containing report ID by calling 
    //     IWDFRequest::RetrieveInputMemory(). 
    // Output report buffer and length: 
    //     UMDF driver calls IWDFRequest::RetrieveOutputMemory() to get memory 
    //     buffer and its length. The driver fills the buffer with feature data. 
    //
    // *************************************************************************

    Trace(TRACE_LEVEL_INFORMATION, "GetInputReport\n");

    //
    // Get input report ID
    //
    hr = FxRequest->RetrieveInputMemory(&memory);
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, "RetrieveInputMemory failed %!hresult!\n", hr);
        return hr;        
    }

    inBuffer = memory->GetDataBuffer(&inBufferCb);
    SAFE_RELEASE(memory);
    
    if (inBufferCb < sizeof(reportId)) 
    {
        hr = HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! Insufficient buffer size %!hresult!\n", hr);
        return hr;
    }

    reportId = *(PUCHAR)inBuffer;
#if TBD
    if (reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        //
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        hr = HRESULT_FROM_NT(STATUS_INVALID_PARAMETER);
        Trace(TRACE_LEVEL_INFORMATION, 
            "Unexpected request, %!hresult!\n", hr);
        return hr;
    }
#endif

    //
    // Get output buffer
    //
    hr = FxRequest->RetrieveOutputMemory(&memory);
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, "RetrieveOutputMemory failed %!hresult!\n", hr);
        return hr;        
    }
    reportBuffer = (PHIDMINI_INPUT_REPORT) memory->GetDataBuffer(&outBufferCb);
    SAFE_RELEASE(memory);
    
    reportSize = sizeof(HIDMINI_INPUT_REPORT);
    if (outBufferCb < reportSize)
    {
        hr = HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! Insufficient report buffer size %!hresult!\n", hr);
        return hr;
    }

	reportBuffer->ReportId = CONTROL_COLLECTION_REPORT_ID;
#if TBD
	reportBuffer->Data = m_OutputReport;
#endif

    Trace(TRACE_LEVEL_INFORMATION, "Returning %d in input report\n", m_OutputReport);

    //
    // Report how many bytes were copied
    //
    FxRequest->SetInformation(reportSize);
    hr = S_OK;
    
    return hr;
}


HRESULT
CMyQueue::SetOutputReport(
    _In_ IWDFIoRequest2 *FxRequest
    )
/*++

Routine Description:

    Handles IOCTL_UMDF_HID_SET_OUTPUT_REPORT for all the collection.

Arguments:

    Request - Pointer to Request Packet.

Return Value:

    NT status code.

--*/
{
    HRESULT hr;
    IWDFMemory *memory = NULL;
    SIZE_T inBufferCb, reportSize;
    UCHAR reportId; 
    SIZE_T pOutBufferSize;
    PHIDMINI_OUTPUT_REPORT reportBuffer = NULL;
    
    // *************************************************************************
    // IOCTL_UMDF_HID_SET_OUTPUT_REPORT
    // Input report ID:
    //     UMDF driver retrieves report ID associated with this collection 
    //     through pOutBufferSize parameter passed to a call to 
    //     IWDFRequest::GetDeviceIoControlParameters DDI.
    // Input report buffer and length: 
    //     UMDF driver calls IWDFRequest::GetInputMemory() to get memory 
    //     buffer and its length. The buffer contains feature report data that
    //     driver transfers to the device. 
    //
    // *************************************************************************

    Trace(TRACE_LEVEL_INFORMATION, "SetOutputReport\n");

    //
    // Get report ID if needed.
    //
    FxRequest->GetDeviceIoControlParameters(NULL, NULL, &pOutBufferSize);
    reportId = (UCHAR) pOutBufferSize;

    if (reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        //
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        hr = HRESULT_FROM_NT(STATUS_INVALID_PARAMETER);
        Trace(TRACE_LEVEL_INFORMATION, 
            "Unknown request, %!hresult!\n", hr);
        return hr;
    }

    //
    // Get input report buffer. 
    //
    hr = FxRequest->RetrieveInputMemory(&memory);
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, "RetrieveInputMemory failed %!hresult!\n", hr);
        return hr;        
    }
    
    reportBuffer = (PHIDMINI_OUTPUT_REPORT)memory->GetDataBuffer(&inBufferCb);
    SAFE_RELEASE(memory);

    //
    // before touching buffer make sure buffer is big enough.
    //
    reportSize = sizeof(PHIDMINI_OUTPUT_REPORT);
    Trace(TRACE_LEVEL_INFORMATION, "Expected report size: %I64d\n", reportSize);
    if (inBufferCb < reportSize) 
    {
        hr = HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! Unexpected buffer size %I64d, expected %I64d %!hresult!\n", 
            inBufferCb, reportSize, hr);
        return hr;
    }

    m_OutputReport = reportBuffer->Data;
    Trace(TRACE_LEVEL_INFORMATION, "Received %d in output report\n", reportBuffer->Data);

    //
    // Report how many bytes were copied
    //
    FxRequest->SetInformation(reportSize);
    hr = S_OK;

    return hr;
}


HRESULT
CMyQueue::WriteReport(
    _In_ IWDFIoRequest2 *FxRequest
    )
/*++

Routine Description:

    Handles IOCTL_HID_WRITE_REPORT all the collection.

Arguments:

    Request - Pointer to  Request Packet.

Return Value:

    NT status code.

--*/
{
    HRESULT hr;
    IWDFMemory *memory = NULL;
    SIZE_T inBufferCb, reportSize;
    UCHAR reportId; 
    SIZE_T pOutBufferSize;
    PHIDMINI_OUTPUT_REPORT outputReport = NULL;
    
    // *************************************************************************
    // IOCTL_HID_WRITE_REPORT
    // Input report ID:
    //     UMDF driver retrieves report ID associated with this collection 
    //     through pOutBufferSize parameter passed to a call to 
    //     IWDFRequest::GetDeviceIoControlParameters DDI.
    // Input report buffer and length: 
    //     UMDF driver calls IWDFRequest::GetInputMemory() to get memory 
    //     buffer and its length. The buffer contains feature report data that
    //     driver transfers to the device. 
    //
    // *************************************************************************

    Trace(TRACE_LEVEL_INFORMATION, "WriteReport\n");

    //
    // Get report ID if needed.
    //
    FxRequest->GetDeviceIoControlParameters(NULL, NULL, &pOutBufferSize);
    reportId = (UCHAR) pOutBufferSize;

    if (reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        //
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        hr = HRESULT_FROM_NT(STATUS_INVALID_PARAMETER);
        Trace(TRACE_LEVEL_INFORMATION, 
            "Unknown request, %!hresult!\n", hr);
        return hr;
    }

    //
    // Get input report buffer. 
    //
    hr = FxRequest->RetrieveInputMemory(&memory);
    if (FAILED(hr)) {
        Trace(TRACE_LEVEL_ERROR, "RetrieveINputMemory failed %!hresult!\n", hr);
        return hr;        
    }
    outputReport = (PHIDMINI_OUTPUT_REPORT) memory->GetDataBuffer(&inBufferCb);
    SAFE_RELEASE(memory);

    //
    // make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_OUTPUT_REPORT);  
    if (inBufferCb < reportSize) 
    {
        hr =  HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! Unexpected buffer size %!hresult!\n", hr);
        return hr;
    }

    //
    // Store the device data in device extension. 
    //
    m_Device->m_DeviceData = outputReport->Data;

    //
    // set status and information
    //
    FxRequest->SetInformation(reportSize);
    hr = S_OK;

    return hr;
}

HRESULT
CMyQueue::GetString(
    _In_ IWDFIoRequest2 *FxRequest
    )
/*++

Routine Description:

    Handles IOCTL_HID_GET_INDEXED_STRING and IOCTL_HID_GET_STRING.

Arguments:

    Request - Pointer to Request Packet.

Return Value:

    NT status code.

--*/
{
    HRESULT hr;
    IWDFMemory *memory = NULL;
    SIZE_T inBufferCb, outBufferCb, stringSizeCb;
    PVOID inBuffer, outBuffer;
    ULONG languageId, stringIndex, stringId;
    BOOLEAN isIndexedString = FALSE;
    ULONG ioctl;
    PWSTR string = NULL;

    // *************************************************************************
    // IOCTL_HID_GET_INDEXED_STRING/IOCTL_HID_GET_STRING
    // Input:
    //     UMDF driver calls IWDFRequest::GetInputMemory() to get the buffer 
    //     that contains an INT value that describes the string to be retrieved.
    //     The most significant two bytes of the INT value contain the language
    //     ID (for example, a value of 1033 indicates English). 
    //     The least significant two bytes of the INT value contain the string 
    //     index in cae of IOCTL_HID_GET_INDEXED_STRING. 
    //     In case of IOCTL_HID_GET_STRING, the two least significant bytes 
    //     contain one of the following three constant values: 
    //     HID_STRING_ID_IMANUFACTURER, HID_STRING_ID_IPRODUCT,
    //     HID_STRING_ID_ISERIALNUMBER.
    // Output:
    //     UMDF driver calls IWDFRequest::GetOutputMemory() to get the output 
    //     buffer and fill it with the retrieved string (a NULL-terminated wide 
    //     character string). 
    //
    // *************************************************************************

    Trace(TRACE_LEVEL_INFORMATION, "GetIndexedString\n");

    //
    // Get input report buffer. 
    //
    hr = FxRequest->RetrieveInputMemory(&memory);
    if (FAILED(hr)) 
    {
        Trace(TRACE_LEVEL_ERROR, "RetrieveINputMemory failed %!hresult!\n", hr);
        return hr;        
    }
    inBuffer = memory->GetDataBuffer(&inBufferCb);
    SAFE_RELEASE(memory);

    //
    // make sure buffer is big enough.
    //
    if (inBufferCb < sizeof(ULONG)) 
    {
        hr =  HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! Unexpected input buffer size %!hresult!\n", hr);
        return hr;
    }

    FxRequest->GetDeviceIoControlParameters(&ioctl, NULL, NULL);
    if (ioctl == IOCTL_HID_GET_INDEXED_STRING) 
    {
        isIndexedString = TRUE;
    }

    //
    // The most significant two bytes of the INT value contain the language
    // ID (for example, a value of 1033 indicates English). 
    //
    languageId = (*(PULONG)inBuffer) >> 16;
    UNREFERENCED_PARAMETER(languageId);

    if (isIndexedString)
    {
        //
        // The least significant two bytes of the INT value contain the string index.
        //
        stringIndex = ((*(PULONG)inBuffer) & 0x0ffff);
        if (stringIndex != VHIDMINI_DEVICE_STRING_INDEX) 
        {
            hr = E_INVALIDARG;
            Trace(TRACE_LEVEL_ERROR, "%!FUNC! Unexpected string index %!hresult!\n", hr);
            return hr;
        }

        stringSizeCb = sizeof(VHIDMINI_DEVICE_STRING);
        string = VHIDMINI_DEVICE_STRING;
    }
    else 
    {
        //
        // The least significant two bytes of the INT value contain the string Id.
        //
        stringId = ((*(PULONG)inBuffer) & 0x0ffff);

        switch (stringId){
        case HID_STRING_ID_IMANUFACTURER:
            stringSizeCb = sizeof(VHIDMINI_MANUFACTURER_STRING);
            string = VHIDMINI_MANUFACTURER_STRING;
            break;
        case HID_STRING_ID_IPRODUCT:
            stringSizeCb = sizeof(VHIDMINI_PRODUCT_STRING);
            string = VHIDMINI_PRODUCT_STRING;
            break;
        case HID_STRING_ID_ISERIALNUMBER:
            stringSizeCb = sizeof(VHIDMINI_SERIAL_NUMBER_STRING);
            string = VHIDMINI_SERIAL_NUMBER_STRING;
            break;
        default:
            stringSizeCb = 0;
            string = NULL;
            break;
        }
    }

    //
    // Get output buffer 
    //
    hr = FxRequest->RetrieveOutputMemory(&memory);
    if (FAILED(hr)) 
    {
        Trace(TRACE_LEVEL_ERROR, "RetrieveOutputMemory failed %!hresult!\n", hr);
        return hr;        
    }
    outBuffer = (PUCHAR) memory->GetDataBuffer(&outBufferCb);
    SAFE_RELEASE(memory);

    if (outBufferCb < stringSizeCb) 
    {
        hr = HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
        Trace(TRACE_LEVEL_ERROR, 
            "%!FUNC! Unexpected output buffer size %!hresult!\n", hr);
        return hr;
    }

    CopyMemory(outBuffer, string, stringSizeCb);

    //
    // set status and information
    //
    FxRequest->SetInformation(stringSizeCb);
    hr = S_OK;

    return hr;
}

HRESULT
CMyManualQueue::CreateInstance(
    _In_ PCMyDevice Device,
    _Out_ CMyManualQueue **Queue
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
    CMyManualQueue *queue;

    HRESULT hr = S_OK;

    //
    // Allocate a new instance of the device class.
    //
    queue = new CMyManualQueue(Device);

    if (NULL == queue)
    {
        hr = E_OUTOFMEMORY;
    }

    //
    // Initialize the instance.
    //
    if (SUCCEEDED(hr)) 
    {
        hr = queue->Initialize(Device->GetFxDevice());
    }

    if (SUCCEEDED(hr)) 
    {
        queue->AddRef();
        *Queue = queue;
    }

    if (NULL != queue)
    {
        queue->Release();
    }

    return hr;
}

HRESULT
CMyManualQueue::QueryInterface(
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

    if (IsEqualIID(InterfaceId, __uuidof(IObjectCleanup))) {
        AddRef();
        *Object = static_cast<IObjectCleanup *>(this);
        hr = S_OK; 
    }
    else 
    {
        hr = CUnknown::QueryInterface(InterfaceId, Object);
    }

    return hr;
}

HRESULT
CMyManualQueue::Initialize(
    _In_ IWDFDevice *FxDevice
    )
/*++
 
  Routine Description:

    This method initializes the device callback object.  Any operations which
    need to be performed before the caller can use the callback object, but 
    which couldn't be done in the constructor becuase they could fail would
    be placed here.

  Arguments:

    FxDevice - the device which this Queue is for.

  Return Value:

    status.

--*/
{
    IWDFIoQueue     *fxQueue;
    HRESULT hr;

    //
    // Create the framework queue
    //
    IUnknown *unknown = QueryIUnknown();
    hr = FxDevice->CreateIoQueue(
                        unknown,
                        FALSE,                        // bDefaultQueue
                        WdfIoQueueDispatchManual, 
                        FALSE,                       // bPowerManaged
                        TRUE,                        // bAllowZeroLengthRequests
                        &fxQueue 
                        );
    if (FAILED(hr))
    {
        Trace(
            TRACE_LEVEL_ERROR, 
            "%!FUNC!: Could not create manual I/O queue, %!hresult!",
            hr
            );
    }

    if (SUCCEEDED(hr)) 
    {
        m_FxQueue = fxQueue;
        
        fxQueue->Release();
    }

    unknown->Release();

	// Get Default I/O target.
	if (SUCCEEDED(hr))
	{
		FxDevice->GetDefaultIoTarget(&m_FxIoTarget);
	}

	if (SUCCEEDED(hr))
    {
        //
        // create a threadpool timer
        //
        m_Timer = CreateThreadpoolTimer(_TimerCallback,
                                        this,
                                        NULL  // use default threadpool
                                        );
        if (m_Timer == NULL) {
            hr = E_OUTOFMEMORY;
            Trace(TRACE_LEVEL_ERROR, 
                "Failed to allocate timer %!hresult!", hr);
        }
		m_InterruptThread = CreateThread(0, 0, InterruptThread, this, 0, 0);
    }

	m_pGesture = new CGesture();
	m_pGesture->SetEventCallback((void*)this, OnGestureEvent);

    return hr;
}

//
// IRequestCallbackRequestCompletion
//
void
STDMETHODCALLTYPE
CMyManualQueue::OnCompletion(
_In_ IWDFIoRequest               * FxRequest,
_In_ IWDFIoTarget                * /* FxTarget */,
_In_ IWDFRequestCompletionParams * /* Params */,
_In_ void *Context                         /* Context */
)
{
	IWDFRequestCompletionParams * CompletionParams;

	FxRequest->GetCompletionParams(&CompletionParams);

	if (CompletionParams->GetCompletedRequestType() == WdfRequestDeviceIoControl)
	{
		IWDFMemory *FxOutputMemory;
		FxOutputMemory = (IWDFMemory *)Context;

		m_pGesture->InjectTouchPoint((HID_TOUCH_REPORT *)FxOutputMemory->GetDataBuffer(NULL));

		// Release the request and free output memory.
		FxRequest->DeleteWdfObject();

		m_nIoRequests--;
		SetEvent(m_hCompletionEvent);
	}

	CompletionParams->Release();
}

VOID 
CMyManualQueue::_TimerCallback(
    _Inout_      PTP_CALLBACK_INSTANCE Instance,
    _Inout_      PVOID Context,
    _Inout_      PTP_TIMER Timer
    )
{
    PCMyManualQueue This = (PCMyManualQueue) Context;
    HRESULT hr;
    IWDFIoRequest *fxRequest = NULL;
    IWDFMemory *memory = NULL;
    PVOID buffer;
    SIZE_T bufferSizeCb;
    ULONG readReportSizeCb = sizeof(HIDMINI_INPUT_REPORT);
    PHIDMINI_INPUT_REPORT readReport;
    
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(Timer);

    Trace(TRACE_LEVEL_ERROR, "_TimerCallback CMyQueue 0x%p\n", This);

#if 0
    //
    // see if we have a request in manual queue
    //
    hr = This->GetQueue()->RetrieveNextRequest(&fxRequest);
    if (SUCCEEDED(hr)) {
        IWDFIoRequest2 *fxRequest2;
        
        Trace(TRACE_LEVEL_INFORMATION, "retrieved read request from manual queue CMyManualQueue 0x%p\n", This);

        hr = fxRequest->QueryInterface(IID_PPV_ARGS(&fxRequest2));
        if (FAILED(hr)){
            Trace(TRACE_LEVEL_ERROR, "QueryInterface failed %!hresult!", hr);
            fxRequest->Complete(hr);
            fxRequest->Release();
            return;
        }
        fxRequest2->Release();

        Trace(TRACE_LEVEL_INFORMATION, "EffectiveIoType: %d\n", fxRequest2->GetEffectiveIoType());

        hr = fxRequest2->RetrieveOutputMemory(&memory);
        if (FAILED(hr)) {
            Trace(TRACE_LEVEL_ERROR, "RetrieveINputMemory failed %!hresult!", hr);
            fxRequest2->Complete(hr);
            fxRequest2->Release();
            return;
        }

        buffer = memory->GetDataBuffer(&bufferSizeCb);
        memory->Release();
        
        if (bufferSizeCb < readReportSizeCb) 
        {
            hr = HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
            Trace(TRACE_LEVEL_ERROR, 
                "%!FUNC! Insufficient read report buffer size %!hresult!", hr);
        }
        else
        {
            //
            //Create input report
            //
            readReport = (PHIDMINI_INPUT_REPORT)buffer;
            readReport->ReportId = CONTROL_FEATURE_REPORT_ID;
            readReport->Data = This->m_Device->m_DeviceData;
            
            //
            // Report how many bytes were copied
            //
            fxRequest2->SetInformation(readReportSizeCb);
            hr = S_OK;
        }

        fxRequest2->Complete(hr);
        fxRequest2->Release();
    }
#endif
    return;
}

void
CMyManualQueue::OnCleanup(
    IN IWDFObject*  pWdfObject
    )
{
    UNREFERENCED_PARAMETER(pWdfObject);
    
    if (m_Timer != NULL) {
        //
        // Prevent new queues. If pftDueTime parameter is NULL, the timer object
        // will cease to queue new callbacks (but callbacks already queued will
        // still occur).
        //
        SetThreadpoolTimer(
            m_Timer,  
            NULL,  // PFILETIME pftDueTime,
            0,     // DWORD msPeriod,
            0      // DWORD msWindowLength
            );

        //
        // wait for timer callback. 
        //
        WaitForThreadpoolTimerCallbacks(m_Timer,
                                        FALSE    // donot cancel pending 
                                        );
        //
        // close timer callback
        //
        CloseThreadpoolTimer(m_Timer);

        m_Timer = NULL;

		if (NULL != m_InterruptThread)
			CloseHandle(m_InterruptThread);

		delete m_pGesture;

    }
}

DWORD WINAPI CMyManualQueue::InterruptThread( LPVOID lpParam )
{
	UNREFERENCED_PARAMETER(lpParam);

	PCMyManualQueue This= (CMyManualQueue *)lpParam;

	Trace(TRACE_LEVEL_INFORMATION, "InterruptThread started...\n");
	This->DoMainIo();

	return 0;
}

BOOL CMyManualQueue::DoMainIo()
{
	Trace(TRACE_LEVEL_INFORMATION, "DoMainIo()+++\n");

	BlockTouch(m_PointingMode ? TRUE:FALSE);	// Request to block multi-touch.

	for (;;)
	{
		if (FALSE == ProcessRawTouch())
		{
			break;
		}
	}

	Trace(TRACE_LEVEL_INFORMATION, "DoMainIo()---\n");
	return TRUE;
}

/*
This is called in the main thread context of the driver.
*/
BOOL CMyManualQueue::ProcessRawTouch()
{
	HRESULT hr;
	IWDFDriver *FxDriver;
	IWDFDevice *FxDevice;
	CComPtr<IWDFIoRequest>      pIoRequest;

	Trace(TRACE_LEVEL_INFORMATION, "ProcessRawTouch()+++\n");
	FxDriver = m_Device->GetFxDriver();
	FxDevice = m_Device->GetFxDevice();

	if (m_nIoRequests >= MAX_IO_REQUEST)
	{
		Trace(TRACE_LEVEL_INFORMATION, "m_nIoRequests reached the maximum(%d). Now hold.\n", m_nIoRequests);
		// If maximum requests are maded, wait for any request to be completed.
		WaitForSingleObject(m_hCompletionEvent, INFINITE);
		Trace(TRACE_LEVEL_INFORMATION, "m_nIoRequests=%d\n", m_nIoRequests);
	}

	hr = FxDevice->CreateRequest(
		NULL,
		NULL,
		&pIoRequest
		);

	if (FAILED(hr))
	{
		Trace(TRACE_LEVEL_ERROR, "Error in CMyRemoteTarget::CreateRequest. hr=0x%x\n", hr);
		return FALSE;
	}

	// Create a buffer for the read request

	CComPtr<IWDFMemory> pOutputMemory;
	hr = FxDriver->CreateWdfMemory(sizeof(HID_TOUCH_REPORT),
		NULL,
		pIoRequest,		// Set IoRequest as the parent to be freed when IoRequest is deleted.
		&pOutputMemory);

	// Mark that the request has been sent, so we don't try to send 
	// again before the completion routine runs.
	m_IoctlInProgress = true;

	hr = m_FxIoTarget->FormatRequestForIoctl(pIoRequest,
		(ULONG)IOCTL_SELFTEST_GET_INPUT_REPORT,
		NULL,
		NULL,
		NULL,
		pOutputMemory,
		NULL
		);

	if (SUCCEEDED(hr))
	{
		pIoRequest->SetCompletionCallback(this, (void *)pOutputMemory);

		hr = pIoRequest->Send(m_FxIoTarget, 0, 0);		// Send requests asynchronously so that multiple requests are made concurrently.
		m_nIoRequests++;
	}
	else
	{
		Trace(TRACE_LEVEL_ERROR, "Error in Sending IOCTL_SELFTEST_GET_INPUT_REPORT.\n");
		m_IoctlInProgress = false;
		return FALSE;
	}

	Trace(TRACE_LEVEL_INFORMATION, "ProcessRawTouch()---\n");
	return TRUE;
}

HRESULT CMyManualQueue::BlockTouch(UINT32 fBlock)
{
	HRESULT hr;
	IWDFDriver *FxDriver;
	IWDFDevice *FxDevice;
	CComPtr<IWDFIoRequest>      pIoRequest;

	Trace(TRACE_LEVEL_INFORMATION, "BlockTouch()+++\n");

	FxDriver = m_Device->GetFxDriver();
	FxDevice = m_Device->GetFxDevice();

	hr = FxDevice->CreateRequest(
		NULL,
		NULL,
		&pIoRequest
		);

	if (FAILED(hr))
	{
		Trace(TRACE_LEVEL_ERROR, "Error in BlockTouch - CreateRequest. hr=0x%x\n", hr);
		return hr;
	}

	// Create a buffer for the read request
	CComPtr<IWDFMemory> pInputMemory;

	hr = FxDriver->CreatePreallocatedWdfMemory((BYTE *)&fBlock, sizeof(UINT32),
		NULL,
		pIoRequest,		// Set IoRequest as the parent to be freed when IoRequest is deleted.
		&pInputMemory);

	hr = m_FxIoTarget->FormatRequestForIoctl(
		pIoRequest,
		(ULONG)IOCTL_SELFTEST_BLOCK_TOUCH_REPORT,
		NULL,
		pInputMemory,
		NULL,
		NULL,
		NULL
		);

	if (SUCCEEDED(hr))
	{
		hr = pIoRequest->Send(m_FxIoTarget, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS, 0);		// Send requests synchronously.
	}
	else
	{
		Trace(TRACE_LEVEL_ERROR, "Error in formatting IOCTL_SELFTEST_BLOCK_TOUCH_REPORT.\n");
		return hr;
	}

	pIoRequest->DeleteWdfObject();

	Trace(TRACE_LEVEL_INFORMATION, "BlockTouch()---\n");

	return hr;
}

void CMyManualQueue::TogglePointingMode()
{
	if (m_PointingMode == TRUE)
	{
		m_PointingMode = FALSE;
	}
	else
	{
		m_PointingMode = TRUE;
	}

	BlockTouch(m_PointingMode ? TRUE:FALSE);	// Request to block multi-touch.
	m_TogglePending = FALSE;
}

void CMyManualQueue::CompleteInputReport(CGesture *pGesture)
{
	HRESULT hr;
	IWDFIoRequest *fxRequest = NULL;
	IWDFMemory *memory = NULL;
	PVOID buffer;
	SIZE_T bufferSizeCb;
	ULONG readReportSizeCb = sizeof(HIDMINI_INPUT_REPORT);
	PHIDMINI_INPUT_REPORT readReport;

	Trace(TRACE_LEVEL_VERBOSE, "CompleteInputReport++\n");

	//
	// see if we have a request in manual queue
	//
	hr = m_FxQueue->RetrieveNextRequest(&fxRequest);
	if (SUCCEEDED(hr)) {
		IWDFIoRequest2 *fxRequest2;

		Trace(TRACE_LEVEL_VERBOSE, "retrieved read request from manual queue \n");

		hr = fxRequest->QueryInterface(IID_PPV_ARGS(&fxRequest2));
		if (FAILED(hr)){
			Trace(TRACE_LEVEL_ERROR, "QueryInterface failed %!hresult!", hr);
			fxRequest->Complete(hr);
			fxRequest->Release();
			return;
		}
		fxRequest2->Release();

		Trace(TRACE_LEVEL_VERBOSE, "EffectiveIoType: %d\n", fxRequest2->GetEffectiveIoType());

		hr = fxRequest2->RetrieveOutputMemory(&memory);
		if (FAILED(hr)) {
			Trace(TRACE_LEVEL_ERROR, "RetrieveINputMemory failed %!hresult!", hr);
			fxRequest2->Complete(hr);
			fxRequest2->Release();
			return;
		}

		buffer = memory->GetDataBuffer(&bufferSizeCb);
		memory->Release();

		if (bufferSizeCb < readReportSizeCb)
		{
			hr = HRESULT_FROM_NT(STATUS_INVALID_BUFFER_SIZE);
			Trace(TRACE_LEVEL_ERROR,
				"%!FUNC! Insufficient read report buffer size %!hresult!", hr);
		}
		else
		{
			//
			//Create input report
			//
			readReport = (PHIDMINI_INPUT_REPORT)buffer;
			memset(readReport, 0, sizeof(HIDMINI_INPUT_REPORT));
			readReport->ReportId = REPORTID_MOUSE;
			PHID_MOUSE_REPORT hidMouse = NULL;

			hidMouse = &(readReport->MouseReport);
			hidMouse->InputReport.wXData = pGesture->CurrentMouseX;
			hidMouse->InputReport.wYData = pGesture->CurrentMouseY;
			hidMouse->InputReport.bButtons = pGesture->ButtonState;
			hidMouse->InputReport.cWheel = pGesture->CurrentWheel;
			//
			// Report how many bytes were copied
			//
			fxRequest2->SetInformation(readReportSizeCb);
			hr = S_OK;
		}

		fxRequest2->Complete(hr);
		fxRequest2->Release();
	}

	return;
}

/*
The handler is called back when there's a Gesture event. There are two contexts of the thread:

1. Called by the context of the thread that called InjectTouchPoint().
2. Called by the context of the ShortTap timer thread when the timer is expired.
*/
void CALLBACK CMyManualQueue::OnGestureEvent(_Inout_ void *pContext)
{
	CMyManualQueue *This = (CMyManualQueue *)pContext;

	if (This->m_pGesture->IsToggleEvent())
	{
		This->TogglePointingMode();
		This->m_pGesture->ClearGestureState();
	}

	if (This->m_PointingMode)	/* If it's Pointing mode. */
	{
		// Report the pointing event to ManualQueue.
		This->CompleteInputReport(This->m_pGesture);
	}
}

