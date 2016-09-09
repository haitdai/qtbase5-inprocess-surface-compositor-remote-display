#ifndef SCREEN_SCROLL_CONTROLLER_H
#define SCREEN_SCROLL_CONTROLLER_H

#include <QPainter>
#include <QtGui/qmrofs.h>

/////////////////////////////////////////////////
//ң����Ļ�ߴ綨��
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
	//��ȡ��Ļ�ϳ�����
	class ScRects
	{
	public:
		ScRects();		

	public:
		static const int MAX_COUNT = 5; 
		int m_count;//�������
		QRect m_source[MAX_COUNT];//Image�ϵ�λ��
		QRect m_target[MAX_COUNT];//��ʾ���ϵ�λ��
		QMrOfs::WV_ID  m_viewId;//view id(MrOfs ids)
		QMrOfs::WV_ID  m_waveId;//wave id(MrOfs ids)
	};
	//mainRects : ����(��һ��)�ϳ�����
	//waveRects : ������(�ڶ���)�ϳ�����
	void GetScrollRects(ScRects &mainRects, ScRects &waveRects) const;

public:
	//���������
	//barHeight : �������, �����������ĸ߶�
	//boxHeight : �������, ������Ĺ���������߶�
	//boxPos : �������, ���������������λ��(����λ��)
	//
	//����boxHeight/boxTopPosition�Ϳ��Ի����������Ļ���!!!
	//
	void CalcScrollBar(int barHeight, int &boxHeight, int &boxPos) const;

public:
	//��������ת��
	void ConvertTouchPoint(qreal &/*x*/, qreal &y, bool touchPointPressed);

public:
	//������Ļ�ߴ�
	void SetScreenSize(int trueWidth, int trueHeight, int qtWidth, int qtHeight);
	//������ЧQT��Ļ�߶�
	void SetValidQtScreenHeight(int height);
	//���ù̶������
	//top:����(true),�ײ�(false)
	void SetFixedRectHeight(int height, bool top);
	//���ù���Rect��ʼY����
	void SetScrollRectStartY(int y);

public:
	//���ò���ͼƬ����
	void SetWaveImageTopLeft(QPoint topLeft);
	//����MrOfs ids
	void SetMrOfsWaveId(QMrOfs::WV_ID waveId);
	void SetMrOfsViewId(QMrOfs::WV_ID viewId);
	
private:
	int m_touchPointDeltaY;///������ƫ��

private:
	int m_trueScreenWidth;//��ʵ����Ļ���
	int m_trueScreenHeight;//��ʵ����Ļ�߶�
	int m_qtScreenWidth;//QT��Ļ���
	int m_qtScreenHeight;//QT��Ļ�߶�
	int m_validQtScreenHeight;//��ЧQT��Ļ�߶�
	int m_topfixedRectHeight;//�����̶���ʾ��߶�
	int m_bottomfixedRectHeight;//�ײ��̶���ʾ��߶�
	int m_scrollRectStartY;//�����鿪ʼλ��(��)

private:
	QPoint m_waveImageTopLeft;//����ͼƬ���Ͻ�����
	QMrOfs::WV_ID  m_viewId;//view id(MrOfs ids)
	QMrOfs::WV_ID  m_waveId;//wave id(MrOfs ids)
};

#endif


