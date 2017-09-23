#pragma once

#define MAX_TOUCH_POINT	10

#define NM_TOUCH_CONTACT_TO_TOGGLE 4 // Number of touch contacts to toggle blocking of multi-touch.

enum GESTURE_STATE_TYPE
{
	GESTURE_STATE_NONE = 0,
	GESTURE_STATE_ONE_FINGER_MOVE,
	GESTURE_STATE_ONE_FINGER_SINGLE_TAP,
	GESTURE_STATE_ONE_FINGER_DOUBLE_TAP,
	GESTURE_STATE_ONE_FINGER_DOUBLE_TAP_HOLD,	/* Double Tap and hold the press */
	GESTURE_STATE_TWO_FINGER_SINGLE_TAP,
	GESTURE_STATE_TWO_FINGER_DOUBLE_TAP,
	GESTURE_STATE_TWO_FINGER_MOVE,
	GESTURE_STATE_LEFT_EDGE,
	GESTURE_STATE_RIGHT_EDGE,
	GESTURE_STATE_TOP_EDGE,
	GESTURE_STATE_BOTTOM_EDGE,
	GESTURE_STATE_TOGGLE,
	GESTURE_STATE_MAX
};

class CTouchPoint
{
public:
	int x;
	int y;
	int down;
	ULONGLONG tick;
	int id;
public:
	CTouchPoint & operator=(const CTouchPoint &rhs)
	{
		this->x = rhs.x;
		this->y = rhs.y;
		this->down = rhs.down;
		this->tick = rhs.tick;
		this->id = rhs.id;
		return *this;
	}
};

// Get 1-dimension distance
UINT32 GetDistance(int a, int b);
// Get 2-dimension distance
ULONG GetDistance(CTouchPoint a, CTouchPoint b);

typedef void (*PFN_SHORT_TAP_CALLBACK)(void *pContext);
typedef void(*PFN_GESTURE_EVENT_CALLBACK)(void *pContext);

class CShortTapTimer
{
private:
#if TIMER_EMULATION
	UINT32 m_Count;
#else
	ULONGLONG m_TickCount;
#endif
	UINT32 m_Threshold;
	PFN_SHORT_TAP_CALLBACK m_pfnCallback;
	PVOID m_Context;

	HANDLE	m_StartEvent;	// Notify timer started.

public:
	CShortTapTimer();
	~CShortTapTimer();

	void StartTimer(UINT32 threshold, PFN_SHORT_TAP_CALLBACK callback, PVOID context);		//	threshold : time duration to expire.
#if TIMER_EMULATION
	void IncrementTimer();
#endif
	void StopTimer();
	BOOL IsStopped();
	static DWORD WINAPI TimerThread(LPVOID lpParam);
};

class CContactArray
{
public:
	CTouchPoint points[MAX_TOUCH_POINT];

	CContactArray & operator=(const CContactArray &rhs)
	{
		for (int i = 0; i < MAX_TOUCH_POINT; i++)
		{
			this->points[i] = rhs.points[i];
		}
		return *this;
	}

	CTouchPoint &operator[](const int index)
	{
		return points[index];
	}

};

class CGesture
{
public:
	CContactArray m_PrevContactArray;
	CContactArray m_ContactArray;

	BOOL m_fContactCountChanged;
	BOOL m_fPositionChanged;
	int m_ContactCount;
	int m_MaxContactCount;
	BOOL m_fLastRelease;
	int m_ShortTapCount;	// How many taps during ShortTap duration, 0: No tap, 1: Single tap, 2: Double tap.
	BOOL m_LastRelease;

	CShortTapTimer m_ShortTapTimer;	// Timer for Short Tap duration

	CTouchPoint m_FirstContact;		// First contact information for single tapping.

	int cTickShortTap = 300;	// How much tick to consider as short tap. 300ms
	int cShortMoveTolerance = 100; // Minimum move required to be considered as short move. The short move will be considered as tap unless the move is farther than this.
	int cShortMoveRange = 1000;

	GESTURE_STATE_TYPE m_GestureState;

	INT32   CurrentMouseX;
	INT32   CurrentMouseY;
	INT32   CurrentWheel;
	INT8	ButtonState;
private:
	// To maintain mouse point.
	INT32   PreviousTouchpadX;
	INT32   PreviousTouchpadY;
	INT8    PreviousContactState;
	INT32	LastTouchpadPressure;
	DWORD	LastTouchTick;
	PFN_GESTURE_EVENT_CALLBACK m_pfnEventCallback;	// Event callback to be called in order to notify.
	void	*m_pContext;		// Context for event-callback.

public:
	CGesture();
	virtual ~CGesture()
	{}

	BOOL IsInShortTapRange(CTouchPoint firstTap, CTouchPoint currentTap);

	void InjectTouchPoint(_In_ HID_TOUCH_REPORT *pTouchReport);
	BOOL IsToggleEvent();
	void ClearContactStatus();

	void UpdateCursor(int x, int y, BOOL newstroke);
	void UpdateScroll(int x, int y, BOOL newstroke);
	void UpdateLButtonPress(BOOL down);
	void UpdateRButtonPress(BOOL down);
	void UpdateButtonState();

	void SetEventCallback(void *pContext, PFN_GESTURE_EVENT_CALLBACK pfnCallback);

	static void OnTimeout(void *pContext);
	void PostGestureEvent();
	void ClearGestureState();
};
