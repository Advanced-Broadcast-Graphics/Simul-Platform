#ifndef PLATFORM_CORE_MOUSEHANDLER_H
#define PLATFORM_CORE_MOUSEHANDLER_H

#include <functional>
#include "Platform/Core/Export.h"
#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
namespace platform
{
	namespace core
	{
		enum MouseButtons
		{
			NoButton = 0,
			LeftButton = 1,
			RightButton = 2,
			MiddleButton = 4,
			Button4 = 8,
			Button5 = 16,
		};
		enum class KeyboardModifiers
		{
			NoModifier = 0,
			Shift = 1,
			Alt = 2,
			Ctrl = 4,
		};
		enum class MouseEventType
		{
			None = 0,
			Press = 1,
			Release= 2,
			DoubleClick= 4,
			Move=8,
			Wheel
		};
		// A simple render delegate, it will usually be a function partially bound with std::bind.
		typedef std::function<void()> VoidFnDelegate;
		typedef std::function<void(MouseEventType, MouseButtons, int, int)> MouseFnDelegate;
		typedef std::function<void(int, bool )> KeyboardFnDelegate;
		class PLATFORM_CORE_EXPORT BaseMouseHandler
		{
		public:
			BaseMouseHandler();
			virtual ~BaseMouseHandler();
			virtual void mousePress(int button, int x, int y);
			virtual void mouseRelease(int button, int x, int y);
			virtual void mouseDoubleClick(int button, int x, int y);
			virtual void mouseMove(int x, int y);
			virtual void getMousePosition(int &x, int &y) const;
			virtual void mouseWheel(int delta, int modifiers);
			virtual void KeyboardProc(unsigned int nChar, bool bKeyDown, bool bAltDown);
			virtual MouseButtons getMouseButtons() const;
			void SetViewSize(int w, int h);
			VoidFnDelegate updateViews;
			VoidFnDelegate valuesChanged;
			MouseFnDelegate mouseEvent;
			KeyboardFnDelegate keyboardEvent;
		protected:
			float fDeltaX, fDeltaY;
			int MouseX, MouseY;
			int view_width;
			int view_height;

			MouseButtons mouseButtons;
			void UpdateViews();
			void ValuesChanged();
		};
	}
}
#endif
