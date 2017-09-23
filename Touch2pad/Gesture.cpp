#include "internal.h"
#if defined(EVENT_TRACING)
#include "interrupt.tmh"
#endif
#include "Gesture.h"

UINT32 GetDistance(int a, int b)
{
	if (a >= b)
	{
		return a - b;
	}
	else
	{
		return b - a;
	}
}

ULONG GetDistance(CTouchPoint a, CTouchPoint b)
{
	ULONG distance;

	distance = GetDistance(a.x, b.x) + GetDistance(a.y, b.y);
	return distance;
}


//
// Implementions of CShortTapTimer.
//
CShortTapTimer::CShortTapTimer()
{
	HANDLE			m_TimerThread;

#if TIMER_EMULATION
	m_Count = 0;
#else
	m_TickCount = 0;
#endif
	m_Threshold = 0;
	m_pfnCallback = NULL;

	m_TimerThread = CreateThread(0, 0, TimerThread, this, 0, 0);
	CloseHandle(m_TimerThread);

	m_StartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
};

CShortTapTimer::~CShortTapTimer()
{
	CloseHandle(m_StartEvent);
}

DWORD WINAPI CShortTapTimer::TimerThread(LPVOID lpParam)
{
	UNREFERENCED_PARAMETER(lpParam);

	CShortTapTimer *This = (CShortTapTimer *)lpParam;

	Trace(TRACE_LEVEL_INFORMATION, "TimerThread started...\n");

	for (;;)
	{
#if TIMER_EMULATION /* Timer emulation for source-level debugging. */
		Sleep(1);
		if (This->m_Threshold != 0)
		{
			This->IncrementTimer();
			if (This->m_Count >= This->m_Threshold)
			{
				This->StopTimer();
				(*This->m_pfnCallback)(This->m_Context);
			}
		}
#else
		DWORD dwWait = 0;

		dwWait = WaitForSingleObject(This->m_StartEvent, INFINITE);
		if (dwWait != WAIT_OBJECT_0)
		{
			Trace(TRACE_LEVEL_ERROR, "TimerThread: Error in WaitForSingleObject.\n");
		}
		Sleep(This->m_Threshold);
		if (This->m_pfnCallback != NULL)
		{
			// Run call-back function only if the timer is still active.
			(*This->m_pfnCallback)(This->m_Context);
		}
#endif
	}

	return 0;
}

void CShortTapTimer::StartTimer(UINT32 threshold, PFN_SHORT_TAP_CALLBACK callback, PVOID context)
{
#if TIMER_EMULATION
	m_Count = 0;
#else
	m_TickCount = GetTickCount64();
#endif
	m_Threshold = threshold;
	m_pfnCallback = callback;
	m_Context = context;

	if (FALSE == SetEvent(m_StartEvent))
	{
		Trace(TRACE_LEVEL_ERROR, "StartTimer: Error in SetEvent().");
	};
}


void CShortTapTimer::StopTimer()
{
#if TIMER_EMULATION
	m_Count = 0;
#else
	m_TickCount = 0;
#endif
	m_Threshold = 0;
	m_pfnCallback = NULL;
}

#if TIMER_EMULATION
void CShortTapTimer::IncrementTimer()
{
	m_Count+=1;
}
#endif

/* returns TRUE if the timer is expired or stopped. */
BOOL CShortTapTimer::IsStopped()
{
	if (m_Threshold == 0)
	{
		return TRUE;	// Default state with no timer set is considered as stopped.
	}

#if TIMER_EMULATION
	if (m_Count >= m_Threshold)
	{
		return TRUE;	// Timer is expired.
	}
#else
	if (GetTickCount64() - m_TickCount >= m_Threshold)
	{
		return TRUE;	// Timer is expired.
	}
#endif
	else
	{
		return FALSE;
	}
}

//
// Implementions of CGesture.
//
CGesture::CGesture()
{
	PreviousTouchpadX = PreviousTouchpadY = 0;
	PreviousContactState = 0;
	LastTouchpadPressure = 0;
	LastTouchTick = 0;
	CurrentMouseX = CurrentMouseX = 0;
	CurrentWheel = 0;
	ButtonState = 0;

	m_fPositionChanged = FALSE;
	m_fContactCountChanged = FALSE;
	m_ContactCount = 0;
	m_MaxContactCount = 0;
	m_ShortTapCount = 0;
	m_GestureState = GESTURE_STATE_NONE;
	m_fPositionChanged = FALSE;
	m_fLastRelease = 0;

	for (int i = 0; i < MAX_TOUCH_POINT; i++)
	{
		m_PrevContactArray[i].id = 0;
		m_PrevContactArray[i].x = 0;
		m_PrevContactArray[i].y = 0;
		m_PrevContactArray[i].down = 0;
		m_PrevContactArray[i].tick = 0;

		m_ContactArray[i].id = 0;
		m_ContactArray[i].x = 0;
		m_ContactArray[i].y = 0;
		m_ContactArray[i].down = 0;
		m_ContactArray[i].tick = 0;
	}
}
/*
	Called back when the timer gets expired.
*/
void CGesture::OnTimeout(void *pContext)
{
	CGesture *This = (CGesture *)pContext;
	Trace(TRACE_LEVEL_ERROR, "OnTimeout\n");

	This->UpdateButtonState();
}

void CGesture::UpdateButtonState()
{
	if (m_MaxContactCount == 1)
	{
		if (m_ShortTapCount == 1)
		{
			m_GestureState = GESTURE_STATE_ONE_FINGER_MOVE;
		}
		if (m_ShortTapCount == 2)
		{
			m_GestureState = GESTURE_STATE_ONE_FINGER_SINGLE_TAP;

			UpdateLButtonPress(TRUE);
			Sleep(50);
			UpdateLButtonPress(FALSE);

			m_GestureState = GESTURE_STATE_NONE;
		}
		if (m_ShortTapCount == 3)
		{
			m_GestureState = GESTURE_STATE_ONE_FINGER_DOUBLE_TAP_HOLD;

			UpdateLButtonPress(TRUE);
		}
		if (m_ShortTapCount == 4)
		{
			m_GestureState = GESTURE_STATE_ONE_FINGER_DOUBLE_TAP;

			UpdateLButtonPress(TRUE);
			Sleep(50);
			UpdateLButtonPress(FALSE);
			Sleep(50);
			UpdateLButtonPress(TRUE);
			Sleep(50);
			UpdateLButtonPress(FALSE);

			m_GestureState = GESTURE_STATE_NONE;
		}

	}
	if (m_MaxContactCount == 2)
	{
		if (m_ShortTapCount == 1)
		{
			m_GestureState = GESTURE_STATE_TWO_FINGER_MOVE;
		}
		if (m_ShortTapCount == 2)
		{
			m_GestureState = GESTURE_STATE_TWO_FINGER_SINGLE_TAP;

			UpdateRButtonPress(TRUE);
			Sleep(50);
			UpdateRButtonPress(FALSE);

			m_GestureState = GESTURE_STATE_NONE;
		}
		if (m_ShortTapCount == 3)
		{
			m_GestureState = GESTURE_STATE_TWO_FINGER_DOUBLE_TAP;

			UpdateRButtonPress(TRUE);
			Sleep(50);
			UpdateRButtonPress(FALSE);
			Sleep(50);
			UpdateRButtonPress(TRUE);
			m_MaxContactCount = 0;	// Clear MaxContactCount because it's not cleared 
			// when the finger is released in order for the OnTimeout() to use.
		}
		if (m_ShortTapCount == 4)
		{
			m_GestureState = GESTURE_STATE_TWO_FINGER_DOUBLE_TAP;

			UpdateLButtonPress(TRUE);
			Sleep(50);
			UpdateLButtonPress(FALSE);
			Sleep(50);
			UpdateLButtonPress(TRUE);
			Sleep(50);
			UpdateLButtonPress(FALSE);
		}
	}

	m_ShortTapCount = 0;
}

BOOL CGesture::IsInShortTapRange(CTouchPoint firstTap, CTouchPoint currentTap)
{
	if (m_ShortTapCount == 2)
	{
		if (GetDistance(currentTap, firstTap) > cShortMoveTolerance)
		{
			Trace(TRACE_LEVEL_ERROR, "Out-of-range at count 2.\n");
			return FALSE;
		}
	}
	if (m_ShortTapCount == 3 || m_ShortTapCount == 4)
	{
		if (GetDistance(currentTap, firstTap) > cShortMoveRange)
		{
			Trace(TRACE_LEVEL_ERROR, "Out-of-range at count %d.\n", m_ShortTapCount);
			return FALSE;
		}
	}

	return TRUE;
}

void CGesture::InjectTouchPoint(_In_ HID_TOUCH_REPORT *pTouchReport)
{
	CTouchPoint currentContact;

	currentContact.id = pTouchReport->ContactId;		// ID of current finger.
	currentContact.x = pTouchReport->wXData;
	currentContact.y = pTouchReport->wYData;
	currentContact.down = pTouchReport->bStatus;
	currentContact.tick = GetTickCount64();

	//char cDown = (currentContact.down == 1) ? 'D' : 'U';
	//Trace(TRACE_LEVEL_FATAL, "%ld:%c:[%d](%d,%d)\n", GetTickCount(), cDown, currentContact.id, currentContact.x, currentContact.y);

// Update contact status & update also the ShortTap Timer.
	// Back-up previous ContactArray to be able to compare with the new array.
	m_PrevContactArray = m_ContactArray;

	// Get position and down status of the current finger.
	m_ContactArray[currentContact.id] = currentContact;

	m_fContactCountChanged = FALSE;	// Initialize for this iteration.

	if (currentContact.down == TRUE)
	{	// Down event
		if (m_PrevContactArray[currentContact.id].down == FALSE)
		{  // The finger was previously UP.
			m_ContactCount++;
			Trace(TRACE_LEVEL_ERROR, "ContactCount=%d\n", m_ContactCount);

			m_fContactCountChanged = TRUE;
			if (m_ContactCount == 1 )
			{
				if (m_ShortTapTimer.IsStopped() == TRUE)
				{  // It's first contact after ShortTapTimer is stopped.
					m_FirstContact = m_ContactArray[currentContact.id];
					m_ShortTapTimer.StartTimer(cTickShortTap, CGesture::OnTimeout, this);
				}
				else
				{
					// It's first contact during ShortTap duration.
				}
			}

			if (m_MaxContactCount < m_ContactCount)
			{
				m_MaxContactCount = m_ContactCount;
			}
		}
	}
	else
	{	// Up event
		if (m_PrevContactArray[currentContact.id].down == TRUE)
		{  // The finger was previously DOWN.
			m_ContactCount--;
			Trace(TRACE_LEVEL_ERROR, "ContactCount=%d\n", m_ContactCount);
			m_fContactCountChanged = TRUE;
			if (m_ContactCount == 0)
			{
				m_fLastRelease = TRUE;
			}
		}
	}

// Deal with One finger operation.
	if (currentContact.id == 0 && m_MaxContactCount == 1)
	{	
		if (m_ShortTapTimer.IsStopped() == FALSE)
		{	// ShortTap Timer is NOT stopped yet.
			if (FALSE == IsInShortTapRange(m_FirstContact, currentContact))
			{	// The current tap is out of the ShortTap range.
				Trace(TRACE_LEVEL_ERROR, "Stopped by move.\n");
				m_ShortTapTimer.StopTimer();	// No harm to stop timer multiple times.
			}

			// Count all the Tapping during ShortTap duration.
			if (m_fContactCountChanged == TRUE)
			{
				m_ShortTapCount++;
				Trace(TRACE_LEVEL_ERROR, "m_ShortTapCount=%d\n", m_ShortTapCount);
			}
		}
		else
		{	// ShortTap Timer is already stopped.	
			Trace(TRACE_LEVEL_INFORMATION, "Already stopped by timeout.\n");
			if (m_fLastRelease==TRUE)
			{
				if (m_GestureState == GESTURE_STATE_ONE_FINGER_DOUBLE_TAP_HOLD)
				{  // This is the case that the double tap was on hold when the ShortTap timer is expired.
					Trace(TRACE_LEVEL_INFORMATION, "End of One finger double tap.\n");
					Sleep(300);	// Give some delay just in case OnTimeout() call-back still emulates the LButton events.
					// In order to remove this delay, queuing mechanism is required to serialize the LButton events
					// from both Gesture context and OnTimeout() context.
					UpdateLButtonPress(FALSE);
					m_GestureState = GESTURE_STATE_NONE;
					m_MaxContactCount = 0;	// Clear MaxContactCount because it's not cleared 
				}
				if (m_GestureState == GESTURE_STATE_ONE_FINGER_MOVE)
				{
					m_GestureState = GESTURE_STATE_NONE;
				}
			}
		}
	}

	// Deal with Two finger operation. Focus only on the 1st finger.
	if (currentContact.id == 0 && m_MaxContactCount == 2)
	{
		if (m_ShortTapTimer.IsStopped() == FALSE)
		{	// ShortTap Timer is NOT stopped yet.
			if (FALSE == IsInShortTapRange(m_FirstContact, currentContact))
			{	// The current tap is out of the ShortTap range.
				Trace(TRACE_LEVEL_ERROR, "Stopped by 1st finger move.\n");
				m_ShortTapTimer.StopTimer();	// No harm to stop timer multiple times.
			}

			// Count all the Tapping during ShortTap duration.
			if (m_fContactCountChanged == TRUE)
			{
				m_ShortTapCount++;
				Trace(TRACE_LEVEL_ERROR, "m_ShortTapCount=%d\n", m_ShortTapCount);
			}
		}
		else
		{	// ShortTap Timer is already stopped.	
			Trace(TRACE_LEVEL_INFORMATION, "Already stopped by timeout.\n");
			if (m_fLastRelease == TRUE)
			{
				if (m_GestureState == GESTURE_STATE_TWO_FINGER_DOUBLE_TAP)
				{  // This is the case that the double tap was on hold when the ShortTap timer is expired.
					Sleep(300);	// Give some delay just in case OnTimeout() call-back still emulates the RButton events.
					// In order to remove this delay, queuing mechanism is required to serialize the RButton events
					// from both Gesture context and OnTimeout() context.
					UpdateRButtonPress(FALSE);
					m_GestureState = GESTURE_STATE_NONE;
				}
				if (m_GestureState == GESTURE_STATE_TWO_FINGER_MOVE)
				{
					m_GestureState = GESTURE_STATE_NONE;
				}
			}
		}
	}

// Report One finger move event.
	if (currentContact.id == 0 && m_MaxContactCount==1 )
	{
		if (m_fContactCountChanged)
		{
			UpdateCursor(currentContact.x, currentContact.y, TRUE);	// New position is registered.
		}
		else
		{
			UpdateCursor(currentContact.x, currentContact.y, FALSE); // Move event can be sent even within ShortTap duration.
		}
	}

// Deal with Two finger move event. Focus only on the 1st finger regarding move and down/up.
	if (currentContact.id == 0 && m_MaxContactCount == 2)
	{
		if (m_fContactCountChanged)
		{
			UpdateScroll(currentContact.x, currentContact.y, TRUE);	// New position is registered.
		}
		else
		{
			UpdateScroll(currentContact.x, currentContact.y, FALSE); // Scroll event can be sent even within ShortTap duration.
		}
	}

	if (m_MaxContactCount == 4 && m_fLastRelease == TRUE)
	{
		Trace(TRACE_LEVEL_INFORMATION, "TogglePointingMode.\n");
		m_GestureState = GESTURE_STATE_TOGGLE;
		PostGestureEvent();
		m_MaxContactCount = 0;
	}

	
// Contact Status should be cleared when it's a release of last finger.
	if (m_fLastRelease == TRUE)
	{
		Trace(TRACE_LEVEL_INFORMATION, "ClearContactStatus.\n");
		ClearContactStatus();
		if (m_ShortTapTimer.IsStopped() != FALSE)
		{
			Trace(TRACE_LEVEL_INFORMATION, "m_MaxContactCount=0\n");
			m_MaxContactCount = 0;
		}
		else
		{	// Do not clear m_MaxContactCount for OnTimeout call-back to use it later.
			// OnTimeout call-back will clear it after use it.
		}
	}
}


// Only contact status must be cleared.
// First contact and Counter must NOT be cleared.
// m_MaxContactCount should be cleared separately as well.
void CGesture::ClearContactStatus()
{
	m_fContactCountChanged = FALSE;
	m_ContactCount = 0;
	m_fLastRelease = FALSE;

	for (int i = 0; i < MAX_TOUCH_POINT; i++)
	{
		m_PrevContactArray[i].id = 0;
		m_PrevContactArray[i].x = 0;
		m_PrevContactArray[i].y = 0;
		m_PrevContactArray[i].down = 0;
		m_PrevContactArray[i].tick = 0;

		m_ContactArray[i].id = 0;
		m_ContactArray[i].x = 0;
		m_ContactArray[i].y = 0;
		m_ContactArray[i].down = 0;
		m_ContactArray[i].tick = 0;
	}
}

void CGesture::UpdateCursor(int x, int y, BOOL newstroke)
{
	if (newstroke == TRUE)
	{
		Trace(TRACE_LEVEL_INFORMATION, "UpdateCursor - New stroke.\n");

		PreviousTouchpadX = x;
		PreviousTouchpadY = y;
		return;
	}

	INT32 Delta = abs(x - PreviousTouchpadX) + abs(y - PreviousTouchpadY);
	INT32 scale;

	if (Delta > 1000)
	{
		scale = 8;
		Trace(TRACE_LEVEL_VERBOSE, "FASTEST ");
	}
	else if (Delta > 100)
	{
		scale = 4;
		Trace(TRACE_LEVEL_VERBOSE, "FAST ");
	}
	else
	{
		scale = 1;
	}

	if (Delta == 0)
	{	// No move, no need to report.
		Trace(TRACE_LEVEL_VERBOSE, "No move.\n");
		return;
	}

#if ABSOLUTE_ASIX
	CurrentMouseX += scale*(x - PreviousTouchpadX);
	if (CurrentMouseX < 0) CurrentMouseX = 0;
	if (CurrentMouseX >= MAX_MOUSE_X) CurrentMouseX = MAX_MOUSE_X - 1;
#else
	CurrentMouseX = scale*(x - PreviousTouchpadX);
	if (CurrentMouseX < -1024) CurrentMouseX = -1024;
	if (CurrentMouseX > 1023) CurrentMouseX = 1023;
#endif

#if ABSOLUTE_ASIX
	CurrentMouseY += scale*(y - PreviousTouchpadY);
	if (CurrentMouseY < 0) CurrentMouseY = 0;
	if (CurrentMouseY >= MAX_MOUSE_Y) CurrentMouseY = MAX_MOUSE_Y - 1;
#else
	CurrentMouseY = scale*(y - PreviousTouchpadY);
	if (CurrentMouseY < -1024) CurrentMouseY = -1024;
	if (CurrentMouseY > 1023) CurrentMouseY = 1023;
#endif

	Trace(TRACE_LEVEL_VERBOSE, "(%d, %d)\n", CurrentMouseX, CurrentMouseY );

	PreviousTouchpadX = x;
	PreviousTouchpadY = y;

	PostGestureEvent();
}

void CGesture::UpdateLButtonPress(BOOL down)
{
	if (down == TRUE)
	{
		ButtonState |= 0x1;
		Trace(TRACE_LEVEL_INFORMATION, "LButton pressed.\n");
	}
	else
	{
		ButtonState &= ~0x1;
		Trace(TRACE_LEVEL_INFORMATION, "LButton released.\n");
	}
	PostGestureEvent();
}

void CGesture::UpdateRButtonPress(BOOL down)
{
	if (down == TRUE)
	{
		ButtonState |= 0x2;
		Trace(TRACE_LEVEL_INFORMATION, "RButton pressed.\n");
	}
	else
	{
		ButtonState &= ~0x2;
		Trace(TRACE_LEVEL_INFORMATION, "RButton released.\n");
	}
	PostGestureEvent();
}

void CGesture::UpdateScroll(int x, int y, BOOL newstroke)
{
	y = MAX_MOUSE_Y - y;

	if (newstroke == TRUE)
	{
		Trace(TRACE_LEVEL_INFORMATION, "UpdateScroll - New stroke.\n");

		PreviousTouchpadX = x;
		PreviousTouchpadY = y;
		return;
	}

	INT32 Delta = abs(y - PreviousTouchpadY);

	if (Delta == 0)
	{	// No move, no need to report.
		Trace(TRACE_LEVEL_INFORMATION, "No scroll.\n");
		return;
	}

	CurrentWheel = (PreviousTouchpadY - y);
	if (CurrentWheel < -127) CurrentWheel = -127;
	if (CurrentWheel > 127) CurrentWheel = 127;

	Trace(TRACE_LEVEL_INFORMATION, "{%d}\n", CurrentWheel);

	PreviousTouchpadX = x;
	PreviousTouchpadY = y;

	PostGestureEvent();
}

void CGesture::PostGestureEvent()
{
	(*m_pfnEventCallback)(m_pContext);
}

void CGesture::ClearGestureState()
{
	m_GestureState = GESTURE_STATE_NONE;
}

BOOL CGesture::IsToggleEvent()
{
	return (m_GestureState == GESTURE_STATE_TOGGLE) ? TRUE : FALSE;
}

void CGesture::SetEventCallback(void *pContext, PFN_GESTURE_EVENT_CALLBACK pfnCallback)
{
	m_pContext = pContext;
	m_pfnEventCallback = pfnCallback;
}