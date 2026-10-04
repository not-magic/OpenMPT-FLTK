// MFC replacement on FLTK. Dialog that shows several pages (tabs), each a dialog of its own.

#include "stdafx.h"
#include "PropertySheet.h"
#include "StandardIds.h"

#include <FL/Fl.H>
#include <FL/Fl_Tabs.H>
#include <FL/fl_draw.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

constexpr int kTabBarHeight = 26;
constexpr int kTabLabelSize = 12;
constexpr int kTabLabelPadding = 20;
constexpr int kPagePadding = 6;
constexpr int kButtonAreaHeight = 40;
constexpr int kMargin = 8;
constexpr uint32 kApplyButtonId = 0x3021;

std::string ToUtf8(const mpt::ustring &text)
{
	return mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
}

}  // namespace


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
	// The tab group between page and sheet is not a Wnd, so walk the FLTK parents
	for(Fl_Group *group = parent(); group != nullptr; group = group->parent())
	{
		if(PropertySheet *sheet = dynamic_cast<PropertySheet *>(group))
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
	// Pages are owned by the caller and may already be destroyed, so only detach what is still attached
	for(Fl_Group *tabGroup : m_tabGroups)
	{
		while(tabGroup->children() > 0)
			tabGroup->remove(0);
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
	int contentWidth = 200;
	int contentHeight = 100;
	for(PropertyPage *page : m_pages)
	{
		if(const DialogTemplate *dialogTemplate = FindDialogTemplate(page->m_templateId))
		{
			contentWidth = std::max(contentWidth, DialogUnitsToPixelsX(dialogTemplate->width) + 2 * kPagePadding);
			contentHeight = std::max(contentHeight, DialogUnitsToPixelsY(dialogTemplate->height) + 2 * kPagePadding);
		}
	}
	fl_font(FL_HELVETICA, kTabLabelSize);
	int tabsWidth = 0;
	for(const PropertyPage *page : m_pages)
		tabsWidth += static_cast<int>(fl_width(ToUtf8(page->m_title).c_str())) + kTabLabelPadding;
	contentWidth = std::max(contentWidth, tabsWidth);
	const int width = contentWidth + 2 * kMargin;
	const int height = kTabBarHeight + contentHeight + kButtonAreaHeight + kMargin;
	resize(0, 0, width, height);
	begin();
	m_tabs = new Fl_Tabs(kMargin, kMargin / 2, contentWidth, kTabBarHeight + contentHeight);
	m_tabs->labelsize(kTabLabelSize);
	m_tabs->callback(
	    [](Fl_Widget *, void *data)
	    {
		    static_cast<PropertySheet *>(data)->OnTabSelected();
	    },
	    this);
	for(const PropertyPage *page : m_pages)
	{
		Fl_Group *tabGroup = new Fl_Group(kMargin, kMargin / 2 + kTabBarHeight, contentWidth, contentHeight);
		tabGroup->copy_label(ToUtf8(page->m_title).c_str());
		tabGroup->labelsize(kTabLabelSize);
		tabGroup->end();
		tabGroup->hide();
		m_tabGroups.push_back(tabGroup);
	}
	m_tabs->end();
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
	Fl_Group *tabGroup = m_tabGroups[index];
	if(!page->m_isPageCreated)
	{
		page->m_isPageCreated = true;
		page->CreateChild(page->m_templateId, *this, 0, 0);
		tabGroup->add(page->GetWidget());
		Fl_Widget *pageWidget = page->GetWidget();
		pageWidget->box(FL_NO_BOX);
		pageWidget->position(tabGroup->x() + std::max(kPagePadding, (tabGroup->w() - pageWidget->w()) / 2), tabGroup->y() + kPagePadding);
	}
	page->GetWidget()->show();
	m_tabs->value(tabGroup);
}


void PropertySheet::OnTabSelected()
{
	const auto it = std::find(m_tabGroups.begin(), m_tabGroups.end(), m_tabs->value());
	if(it == m_tabGroups.end())
		return;
	if(!SetActivePage(static_cast<int>(it - m_tabGroups.begin())) && m_activeIndex >= 0)
		m_tabs->value(m_tabGroups[m_activeIndex]);
}


bool PropertySheet::SetActivePage(int index)
{
	if(index < 0 || index >= GetPageCount() || index == m_activeIndex)
		return index == m_activeIndex;
	if(PropertyPage *current = GetActivePage())
	{
		if(!current->OnKillActive())
			return false;
	}
	m_activeIndex = index;
	ShowPage(index);
	if(PropertyPage *page = GetActivePage())
		page->OnSetActive();
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
