// MFC replacement on FLTK. Dialog that shows several pages (tabs), each a dialog of its own.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Controls.h"
#include "Dialog.h"

#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

class PropertySheet;


class PropertyPage : public Dialog
{
public:
	PropertyPage() = default;
	explicit PropertyPage(uint32 templateId) : Dialog(templateId) { }

	// Called when the page is shown. Return false to refuse switching to the page.
	virtual bool OnSetActive();
	// Called when another page is about to be shown. Return false to stay on this page.
	virtual bool OnKillActive();
	// Called when the changes of the page should be applied
	virtual bool OnApply();

	void SetModified(bool isModified = true);
	bool IsPageModified() const noexcept { return m_isPageModified; }
	PropertySheet *GetParentSheet() const;

	// A page does not close itself through the buttons of the sheet
	void OnOK() override;
	void OnCancel() override;
	bool OnInitDialog() override;

	UI_DECLARE_MESSAGE_MAP()

private:
	friend class PropertySheet;
	mpt::ustring m_title;
	bool m_isPageModified = false;
	bool m_isPageCreated = false;
};


class PropertySheet : public Dialog
{
public:
	PropertySheet(const mpt::ustring &caption, Wnd *parent = nullptr, uint32 selectedPage = 0);
	~PropertySheet() override;

	void AddPage(PropertyPage *page);
	int GetPageCount() const { return static_cast<int>(m_pages.size()); }
	PropertyPage *GetPage(int index) const;
	PropertyPage *GetActivePage() const;
	int GetActiveIndex() const noexcept { return m_activeIndex; }
	bool SetActivePage(int index);
	void SetTitle(const mpt::ustring &title) { m_caption = title; }

	// The sheet is shown modally and ends when OK or Cancel is pressed
	intptr_t DoModal();
	void OnOK() override;
	void OnCancel() override;
	// Applies the pages without closing the sheet
	void OnApply();
	void UpdateApplyButton();
	void PageModified();

	UI_DECLARE_MESSAGE_MAP()

private:
	class TabBar;

	void BuildFrame();
	void ShowPage(int index);

	std::vector<PropertyPage *> m_pages;
	mpt::ustring m_caption;
	int m_activeIndex = -1;
	int m_initialIndex = 0;
	Fl_Group *m_content = nullptr;
	Fl_Widget *m_tabBar = nullptr;
	Button *m_okButton = nullptr;
	Button *m_cancelButton = nullptr;
	Button *m_applyButton = nullptr;
	bool m_isBuilt = false;
};

}  // namespace ui


OPENMPT_NAMESPACE_END
