#pragma once
#ifndef MENUWIDGETS_H
#define MENUWIDGETS_H

#include <list>
#include <vector>
#include <string>
#include <cstdint>


typedef void (*funcptr_t)();


namespace GUI
{

	class Item
	{
	public:
		int32_t		x0, y0,
					x1, y1;

		funcptr_t	ev_lclick;
		funcptr_t	ev_rclick;
		funcptr_t	ev_mouseover;
		funcptr_t	ev_mouseleave;

		virtual void draw() = 0;

	protected:
		virtual ~Item() {};
	};


	class Image : public Item
	{
	public:
		Image();
		~Image();

		void draw();

	private:
		void*		m_image;
	};


	class Button : public Item
	{
	public:

		Button();
		~Button();

		void draw();

	private:
		//NOTE: the image data is the 'off' variant above the 'on' variant inside the image data at 16-bit 5551 BGRA
		void*		m_image;
	};


	class CheckButton : public Item
	{
	public:

		CheckButton();
		~CheckButton();

		void draw();

		bool isChecked() const;

	private:
		//NOTE: the image data is the 'off' variant above the 'on' variant inside the image data at 16-bit 5551 BGRA
		void*		m_image;
		bool		m_checked;
	};


	class ListBox : public Item
	{
	public:

		ListBox() : selected_index(0) {}
		~ListBox();

		int getSelectionIndex() const;
		std::string getSelection() const;

		void setSelectionIndex(size_t index);

		void draw();

	protected:
		std::vector<std::string>	content;
		int				selected_index;
	};

}; //namespace GUI


#endif // MENUWIDGETS_H
