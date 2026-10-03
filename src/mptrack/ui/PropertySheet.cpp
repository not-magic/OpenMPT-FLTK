// MFC replacement on FLTK. Dialog that shows several pages (tabs), each a dialog of its own.

#include "stdafx.h"
#include "PropertySheet.h"
#include "StandardIds.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

constexpr int kTabBarHeight = 26;
constexpr int kButtonAreaHeight = 40;
constexpr int kMargin = 8;
constexpr uint32 kApplyButtonId = 0x3021;

std::string ToUtf8(const mpt::ustring &text)
{
	return mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
}

}  // namespace


class PropertySheet::TabBar : public Fl_Widget
{
public:
	TabBar(int x, int y, int width, int height, PropertySheet &sheet)
	    : Fl_Widget(x, y, width, height)
	    , m_sheet(sheet)
	{
	}

	void draw() override
	{
		fl_push_clip(x(), y(), w(), h());
		fl_color(FL_BACKGROUND_COLOR);
		fl_rectf(x(), y(), w(), h());
		fl_font(FL_HELVETICA, 12);
		int tabX = x();
		for(int i = 0; i < m_sheet.GetPageCount(); ++i)
		{
			const std::string title = ToUtf8(m_sheet.GetPage(i)->m_title);
			const int width = static_cast<int>(fl_width(title.c_str())) + 20;
			const bool isActive = (i == m_sheet.GetActiveIndex());
			draw_box(isActive ? FL_UP_BOX : FL_THIN_UP_BOX, tabX, y() + (isActive ? 0 : 3), width, h() - (isActive ? 0 : 3), FL_BACKGROUND_COLOR);
			fl_color(FL_FOREGROUND_COLOR);
			fl_draw(title.c_str(), tabX + 10, y() + h() / 2 + fl_height() / 2 - fl_descent() + 1);
			tabX += width;
		}
		fl_pop_clip();
	}

	int handle(int event) override
	{
		if(event == FL_PUSH)
		{
			fl_font(FL_HELVETICA, 12);
			int tabX = x();
			for(int i = 0; i < m_sheet.GetPageCount(); ++i)
			{
				const int width = static_cast<int>(fl_width(ToUtf8(m_sheet.GetPage(i)->m_title).c_str())) + 20;
				if(Fl::event_x() >= tabX && Fl::event_x() < tabX + width)
				{
					m_sheet.SetActivePage(i);
					return 1;
				}
				tabX += width;
			}
			return 1;
		}
		return Fl_Widget::handle(event);
	}

private:
	PropertySheet &m_sheet;
};


UI_MESSAGE_MAP_BEGIN(PropertyPage, Dialog)
UI_MESSAGE_MAP_END()

UI_MESSAGE_MAP_BEGIN(PropertySheet, Dialog)
	UI_COMMAND(kApplyButtonId, &PropertySheet::OnApply)
UI_MESSAGE_MAP_END()


bool PropertyPage::OnSetActive()
{
	return true;
}


bool PropertyPage::OnKillActive()
{
	return UpdateData(true);
}


bool PropertyPage::OnApply()
{
	m_isPageModified = false;
	return true;
}


void PropertyPage::SetModified(bool isModified)
{
	m_isPageModified = isModified;
	if(PropertySheet *sheet = GetParentSheet())
		sheet->PageModified();
}


PropertySheet *PropertyPage::GetParentSheet() const
{
	for(Wnd *wnd = GetParent(); wnd != nullptr; wnd = wnd->GetParent())
	{
		if(PropertySheet *sheet = dynamic_cast<PropertySheet *>(wnd))
			return sheet;
	}
	return nullptr;
}


void PropertyPage::OnOK()
{
	// The sheet closes itself; the page only reads its controls
	UpdateData(true);
}


void PropertyPage::OnCancel()
{
}


bool PropertyPage::OnInitDialog()
{
	const DialogTemplate *dialogTemplate = FindDialogTemplate(m_templateId);
	if(dialogTemplate)
		m_title = mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, std::string(dialogTemplate->caption));
	return Dialog::OnInitDialog();
}


PropertySheet::PropertySheet(const mpt::ustring &caption, Wnd *, uint32 selectedPage)
    : m_caption(caption)
    , m_initialIndex(static_cast<int>(selectedPage))
{
}


PropertySheet::~PropertySheet()
{
	// Pages are owned by the caller; detach them so that they are not deleted with the sheet
	for(PropertyPage *page : m_pages)
	{
		if(page->GetWidget()->parent())
			page->GetWidget()->parent()->remove(page->GetWidget());
	}
}


void PropertySheet::AddPage(PropertyPage *page)
{
	m_pages.push_back(page);
	// The caption of the template is the tab title
	if(const DialogTemplate *dialogTemplate = FindDialogTemplate(page->m_templateId))
		page->m_title = mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, std::string(dialogTemplate->caption));
}


PropertyPage *PropertySheet::GetPage(int index) const
{
	if(index < 0 || index >= GetPageCount())
		return nullptr;
	return m_pages[index];
}


PropertyPage *PropertySheet::GetActivePage() const
{
	return GetPage(m_activeIndex);
}


void PropertySheet::BuildFrame()
{
	if(m_isBuilt)
		return;
	m_isBuilt = true;
	// Create all pages to measure them
	int contentWidth = 200;
	int contentHeight = 100;
	for(PropertyPage *page : m_pages)
	{
		if(const DialogTemplate *dialogTemplate = FindDialogTemplate(page->m_templateId))
		{
			contentWidth = std::max(contentWidth, DialogUnitsToPixelsX(dialogTemplate->width));
			contentHeight = std::max(contentHeight, DialogUnitsToPixelsY(dialogTemplate->height));
		}
	}
	const int width = contentWidth + 2 * kMargin;
	const int height = kTabBarHeight + contentHeight + kButtonAreaHeight + kMargin;
	resize(0, 0, width, height);
	begin();
	m_tabBar = new TabBar(kMargin, kMargin / 2, width - 2 * kMargin, kTabBarHeight, *this);
	m_content = new Fl_Group(kMargin, kMargin / 2 + kTabBarHeight, contentWidth, contentHeight);
	m_content->box(FL_UP_FRAME);
	m_content->end();
	const int buttonY = height - kButtonAreaHeight + 6;
	m_okButton = new Button(width - 3 * 80 - 2 * kMargin - 2 * 4, buttonY, 80, 26, "OK");
	m_okButton->argument(IDOK);
	m_okButton->SetKind(Button::Kind::DefaultPush);
	m_cancelButton = new Button(width - 2 * 80 - kMargin - 4, buttonY, 80, 26, "Cancel");
	m_cancelButton->argument(IDCANCEL);
	m_applyButton = new Button(width - 80 - kMargin, buttonY, 80, 26, "Apply");
	m_applyButton->argument(kApplyButtonId);
	m_applyButton->deactivate();
	end();
}


void PropertySheet::ShowPage(int index)
{
	PropertyPage *page = GetPage(index);
	if(page == nullptr)
		return;
	if(!page->m_isPageCreated)
	{
		page->m_isPageCreated = true;
		page->CreateChild(page->m_templateId, *this, 0, 0);
		m_content->add(page->GetWidget());
		page->GetWidget()->position(m_content->x(), m_content->y());
	}
	page->GetWidget()->show();
}


bool PropertySheet::SetActivePage(int index)
{
	if(index < 0 || index >= GetPageCount() || index == m_activeIndex)
		return index == m_activeIndex;
	if(PropertyPage *current = GetActivePage())
	{
		if(!current->OnKillActive())
			return false;
		current->GetWidget()->hide();
	}
	m_activeIndex = index;
	ShowPage(index);
	if(PropertyPage *page = GetActivePage())
		page->OnSetActive();
	if(m_tabBar)
		m_tabBar->redraw();
	return true;
}


intptr_t PropertySheet::DoModal()
{
	m_templateId = 0;
	BuildFrame();
	m_isModal = true;
	m_isDone = false;
	m_result = IDCANCEL;
	m_frame = new Fl_Double_Window(w(), h(), ToUtf8(m_caption).c_str());
	m_frame->copy_label(ToUtf8(m_caption).c_str());
	m_frame->callback(
	    [](Fl_Widget *, void *data)
	    {
		    static_cast<PropertySheet *>(data)->OnCancel();
	    },
	    this);
	m_frame->add(this);
	this->resize(0, 0, m_frame->w(), m_frame->h());
	m_frame->end();
	m_frame->set_modal();
	SetActivePage(std::clamp(m_initialIndex, 0, std::max(GetPageCount() - 1, 0)));
	CenterWindow(nullptr);
	m_frame->show();
	while(!m_isDone && m_frame->shown())
		Fl::wait();
	DestroyFrame();
	return m_result;
}


void PropertySheet::PageModified()
{
	UpdateApplyButton();
}


void PropertySheet::UpdateApplyButton()
{
	if(m_applyButton == nullptr)
		return;
	bool isModified = false;
	for(const PropertyPage *page : m_pages)
		isModified = isModified || page->IsPageModified();
	if(isModified)
		m_applyButton->activate();
	else
		m_applyButton->deactivate();
}


void PropertySheet::OnApply()
{
	for(PropertyPage *page : m_pages)
	{
		if(page->m_isPageCreated && page->IsPageModified())
		{
			page->OnOK();
			page->OnApply();
		}
	}
	UpdateApplyButton();
}


void PropertySheet::OnOK()
{
	if(PropertyPage *current = GetActivePage())
	{
		if(!current->OnKillActive())
			return;
	}
	for(PropertyPage *page : m_pages)
	{
		if(page->m_isPageCreated)
		{
			page->OnOK();
			page->OnApply();
		}
	}
	Finish(IDOK);
}


void PropertySheet::OnCancel()
{
	Finish(IDCANCEL);
}


}  // namespace ui


OPENMPT_NAMESPACE_END
