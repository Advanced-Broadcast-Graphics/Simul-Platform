#include "Platform/Core/BaseMouseHandler.h"

using namespace platform;
using namespace core;

BaseMouseHandler::BaseMouseHandler()
	:fDeltaX(0)
	,fDeltaY(0)
	,MouseX(0)
	,MouseY(0)
	, view_width(750)
	, view_height(750)
	,mouseButtons(core::NoButton)
{
}

BaseMouseHandler::~BaseMouseHandler()
{
}

void BaseMouseHandler::mouseRelease(int button,int x,int y)
{
	*((int*)&mouseButtons)&=(~button);
	MouseX=x;
	MouseY = y;
	if (mouseEvent)
		mouseEvent(MouseEventType::Release, (MouseButtons)button, x, y);
	UpdateViews();
}

void BaseMouseHandler::mousePress(int button,int x,int y)
{
	*((int*)&mouseButtons)|=button;
	MouseX=x;
	MouseY = y;
	if (mouseEvent)
		mouseEvent(MouseEventType::Press, (MouseButtons)button, x, y);
	UpdateViews();
}

void BaseMouseHandler::mouseDoubleClick(int b,int x,int y)
{
	MouseX=x;
	MouseY = y;
	if (mouseEvent)
		mouseEvent(MouseEventType::DoubleClick, (MouseButtons)b, x, y);
	UpdateViews();
}
void BaseMouseHandler::UpdateViews()
{
	if(updateViews)
		updateViews();
}

void BaseMouseHandler::ValuesChanged()
{
	if (valuesChanged)
		valuesChanged();
}

void BaseMouseHandler::mouseMove(int x,int y)
{
	int dx=x-MouseX;
	int dy=y-MouseY;
	fDeltaX=dx/float(view_width);
	fDeltaY=dy/float(view_height);
	MouseX=x;
	MouseY = y;
	if (mouseEvent)
		mouseEvent(MouseEventType::Move, MouseButtons::NoButton, x, y);
	UpdateViews();
}

void BaseMouseHandler::getMousePosition(int &x,int &y) const
{
	x=MouseX;
	y=MouseY;
}

void BaseMouseHandler::mouseWheel(int x,int y)
{
	if(mouseEvent)
		mouseEvent(MouseEventType::Wheel, MouseButtons::NoButton, x, y);
	UpdateViews();
}

void BaseMouseHandler::SetViewSize(int w, int h)
{
	view_width = w;
	view_height= h;
}

void BaseMouseHandler::KeyboardProc(unsigned int /*nChar*/, bool bKeyDown, bool )
{
	if(bKeyDown)
	{
	}
	if(updateViews)
		UpdateViews();
}

MouseButtons	BaseMouseHandler::getMouseButtons() const
{
	return  mouseButtons;
}