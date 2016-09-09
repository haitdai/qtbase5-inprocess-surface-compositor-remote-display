#include "ScreenScrollController.h"

ScreenScrollController& ScreenScrollController::Instance()
{
	static ScreenScrollController s_pInstance;
	return s_pInstance;
}

ScreenScrollController::ScreenScrollController()
{
	m_touchPointDeltaY = 0;
	m_trueScreenWidth = DUBHE_PORTRAIT_SCREEN_TRUE_WIDTH;
	m_trueScreenHeight = DUBHE_PORTRAIT_SCREEN_TRUE_HEIGHT;
	m_qtScreenWidth = DUBHE_PORTRAIT_SCREEN_TRUE_WIDTH;
	m_qtScreenHeight = DUBHE_PORTRAIT_SCREEN_TRUE_HEIGHT;
	m_validQtScreenHeight = DUBHE_PORTRAIT_SCREEN_TRUE_HEIGHT;
	m_topfixedRectHeight = 0;
	m_bottomfixedRectHeight = 0;
	m_scrollRectStartY = 0;
	m_waveImageTopLeft = QPoint(0, 0);
	m_viewId = 0;
	m_waveId = 0;
}

ScreenScrollController::~ScreenScrollController()
{
}

ScreenScrollController::ScRects::ScRects()
{
	m_count = 0;
}

void ScreenScrollController::GetScrollRects(ScRects &mainRects, ScRects &waveRects) const
{
	const QMrOfs::WV_ID viewId = m_viewId;
	const QMrOfs::WV_ID waveId = m_waveId;
	const int trueScreenWidth = m_trueScreenWidth;
	const int trueScreenHeight = m_trueScreenHeight;
	const int validQtScreenHeight = m_validQtScreenHeight;
	const int topFixedRectHeight = m_topfixedRectHeight;
	const int bottomfixedRectHeight = m_bottomfixedRectHeight;
	const int scrollRectStartY = m_scrollRectStartY;
	const int scrollRectHeight = trueScreenHeight - topFixedRectHeight - bottomfixedRectHeight;
	const int waveScrollRectStartY = scrollRectStartY - m_waveImageTopLeft.y();
	const int waveScrollRectHeight = trueScreenHeight - topFixedRectHeight;
	//主屏区域
	mainRects.m_count = 2;
	mainRects.m_source[0] = QRect(0, 0, trueScreenWidth, topFixedRectHeight);
	mainRects.m_target[0] = mainRects.m_source[0];
	mainRects.m_source[1] = QRect(0, scrollRectStartY, trueScreenWidth, scrollRectHeight);
	mainRects.m_target[1] = QRect(0, topFixedRectHeight, trueScreenWidth, scrollRectHeight);
	if (0 < bottomfixedRectHeight)
	{
		//有底部固定显示块
		mainRects.m_count = 3;
		mainRects.m_source[2] = QRect(0, validQtScreenHeight - bottomfixedRectHeight, trueScreenWidth, bottomfixedRectHeight);
		mainRects.m_target[2] = QRect(0, trueScreenHeight - bottomfixedRectHeight, trueScreenWidth, bottomfixedRectHeight);
	}
	//波形屏区域
	waveRects.m_count = 1;
	waveRects.m_viewId = viewId;
	waveRects.m_waveId = waveId;
	
	waveRects.m_source[0] = QRect(0, waveScrollRectStartY, trueScreenWidth, waveScrollRectHeight);
	waveRects.m_target[0] = QRect(0, topFixedRectHeight, trueScreenWidth, waveScrollRectHeight);
}

void ScreenScrollController::ConvertTouchPoint(qreal &/*x*/, qreal &y, bool touchPointPressed)
{
	///////////////////////////////////////////////
	//顶部固定区里的点不需要转换
	//顶部固定区外的点需要加上DeltaY
	//按下时DeltaY保持不变!
	///////////////////////////////////////////////
	const int topFixedRectBottom = m_topfixedRectHeight;
	const int bottomFixedRectTop = m_trueScreenHeight-m_bottomfixedRectHeight;
	const int scrollRectStartY = m_scrollRectStartY;
	const int bottomfixedRectStartY = m_validQtScreenHeight-m_bottomfixedRectHeight;
	if (touchPointPressed)
	{
		//按下
		m_touchPointDeltaY = 0;
		if (topFixedRectBottom < (int)y)
		{
			m_touchPointDeltaY = scrollRectStartY-topFixedRectBottom;
			if (bottomFixedRectTop < (int)y)
			{
				m_touchPointDeltaY = bottomfixedRectStartY-bottomFixedRectTop;
			}
		}
	}
	y += m_touchPointDeltaY;
	return;
}

void ScreenScrollController::CalcScrollBar(int barHeight, int &boxHeight, int &boxPos) const
{
	const int scrollRectHeight = m_trueScreenHeight - m_topfixedRectHeight;//滚动块高
	const int scrollScreenHeight = m_validQtScreenHeight - m_topfixedRectHeight;//滚动屏幕高
	const int scrollRang = scrollScreenHeight - scrollRectHeight;//滚动范围
	const int scrollPos = m_scrollRectStartY - m_topfixedRectHeight;//滚动位置
	if ( (0 == scrollScreenHeight) || (scrollRectHeight>=scrollScreenHeight) )
	{
		//不能滚动,滚动条满屏!
		boxHeight = barHeight;
		boxPos = 0;
		return;
	}
	boxHeight = (scrollRectHeight*barHeight)/scrollScreenHeight;
	boxPos = (scrollPos*(barHeight-boxHeight))/scrollRang;
}

void ScreenScrollController::SetScreenSize(int trueWidth, int trueHeight, int qtWidth, int qtHeight)
{
	m_trueScreenWidth = trueWidth;
	m_trueScreenHeight = trueHeight;
	m_qtScreenWidth = qtWidth;
	m_qtScreenHeight = qtHeight;
	//校正QT屏最大高度
	if (qtHeight < m_validQtScreenHeight)
	{
		m_validQtScreenHeight = qtHeight;
	}
	//校正固定块高度(固定块不大于高度的1/3)
	const int maxFixedHeight = trueHeight/3;
	if (maxFixedHeight < m_topfixedRectHeight)
	{
		m_topfixedRectHeight = maxFixedHeight;
	}
	if (maxFixedHeight < m_bottomfixedRectHeight)
	{
		m_bottomfixedRectHeight = maxFixedHeight;
	}
	//校正滚动位置
	const int maxScrollY = qtHeight-(trueHeight-m_topfixedRectHeight);
	if (maxScrollY < m_scrollRectStartY)
	{
		m_scrollRectStartY = maxScrollY;
	}
}

void ScreenScrollController::SetValidQtScreenHeight(int height)
{
	m_validQtScreenHeight = height;
}

void ScreenScrollController::SetFixedRectHeight(int height, bool top)
{
	if (top)
	{
		m_topfixedRectHeight = height;
	}
	else
	{
		m_bottomfixedRectHeight = height;
	}
}

void ScreenScrollController::SetScrollRectStartY(int y)
{
	m_scrollRectStartY = y;
}

void ScreenScrollController::SetWaveImageTopLeft(QPoint topLeft)
{
	m_waveImageTopLeft = topLeft;
}

void ScreenScrollController::SetMrOfsWaveId(QMrOfs::WV_ID waveId)
{
	m_waveId = waveId;
}

void ScreenScrollController::SetMrOfsViewId(QMrOfs::WV_ID viewId)
{
	m_viewId = viewId;
}


