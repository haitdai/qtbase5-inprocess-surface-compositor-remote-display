#ifndef SCREEN_SCROLL_CONTROLLER_H
#define SCREEN_SCROLL_CONTROLLER_H

#include <QPainter>
#include <QtGui/qmrofs.h>

/////////////////////////////////////////////////
//遥测屏幕尺寸定义
/////////////////////////////////////////////////
const int DUBHE_PORTRAIT_SCREEN_TRUE_WIDTH = 320;
const int DUBHE_PORTRAIT_SCREEN_TRUE_HEIGHT = 480;
const int DUBHE_PORTRAIT_SCREEN_QT_HEIGHT = 2040;
const int DUBHE_LANDSCAPE_SCREEN_QT_HEIGHT = 1400;

class Q_GUI_EXPORT ScreenScrollController
{
public:
	static ScreenScrollController& Instance();

private:
	ScreenScrollController();
	~ScreenScrollController();

public:
	//获取屏幕合成区域
	class ScRects
	{
	public:
		ScRects();		

	public:
		static const int MAX_COUNT = 5; 
		int m_count;//区块个数
		QRect m_source[MAX_COUNT];//Image上的位置
		QRect m_target[MAX_COUNT];//显示屏上的位置
		QMrOfs::WV_ID  m_viewId;//view id(MrOfs ids)
		QMrOfs::WV_ID  m_waveId;//wave id(MrOfs ids)
	};
	//mainRects : 主屏(第一层)合成区块
	//waveRects : 波形屏(第二层)合成区块
	void GetScrollRects(ScRects &mainRects, ScRects &waveRects) const;

public:
	//计算滚动条
	//barHeight : 输入参数, 给定滚动条的高度
	//boxHeight : 输出参数, 计算出的滚动条滑块高度
	//boxPos : 输出参数, 计算出滚动条滑块位置(顶点位置)
	//
	//根据boxHeight/boxTopPosition就可以画出滚动条的滑块!!!
	//
	void CalcScrollBar(int barHeight, int &boxHeight, int &boxPos) const;

public:
	//触摸坐标转换
	void ConvertTouchPoint(qreal &/*x*/, qreal &y, bool touchPointPressed);

public:
	//设置屏幕尺寸
	void SetScreenSize(int trueWidth, int trueHeight, int qtWidth, int qtHeight);
	//设置有效QT屏幕高度
	void SetValidQtScreenHeight(int height);
	//设置固定区域高
	//top:顶部(true),底部(false)
	void SetFixedRectHeight(int height, bool top);
	//设置滚动Rect开始Y坐标
	void SetScrollRectStartY(int y);

public:
	//设置波形图片坐标
	void SetWaveImageTopLeft(QPoint topLeft);
	//设置MrOfs ids
	void SetMrOfsWaveId(QMrOfs::WV_ID waveId);
	void SetMrOfsViewId(QMrOfs::WV_ID viewId);
	
private:
	int m_touchPointDeltaY;///触摸点偏移

private:
	int m_trueScreenWidth;//真实的屏幕宽度
	int m_trueScreenHeight;//真实的屏幕高度
	int m_qtScreenWidth;//QT屏幕宽度
	int m_qtScreenHeight;//QT屏幕高度
	int m_validQtScreenHeight;//有效QT屏幕高度
	int m_topfixedRectHeight;//顶部固定显示块高度
	int m_bottomfixedRectHeight;//底部固定显示块高度
	int m_scrollRectStartY;//滚动块开始位置(高)

private:
	QPoint m_waveImageTopLeft;//波形图片左上角坐标
	QMrOfs::WV_ID  m_viewId;//view id(MrOfs ids)
	QMrOfs::WV_ID  m_waveId;//wave id(MrOfs ids)
};

#endif


