// MFC replacement on FLTK. Application framework: documents, views, child frames with tabbed document area,
// main window and application object.

#include "stdafx.h"
#include "Framework.h"
#include "Dialog.h"
#include "Controls.h"
#include "Menu.h"
#include "Notify.h"
#include "StandardIds.h"

#include <FL/Fl.H>
#include <FL/Fl_Tabs.H>
#include <FL/fl_draw.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


const MessageMap View::messageMap{&Wnd::messageMap};
const MessageMap ChildFrameBase::messageMap{&Wnd::messageMap};
const MessageMap MainFrameBase::messageMap{&Wnd::messageMap};
const MessageMap AppBase::messageMap{&CommandTarget::messageMap};

AppBase *AppBase::s_instance = nullptr;


namespace
{

constexpr int kTabStripHeight = 24;
constexpr int kTabBorderHeight = 3;
constexpr int kTabLabelSize = 12;
constexpr int kTabClosePadding = 4;
// Fl_Tabs internals
constexpr int kTabBorder = 2;
constexpr int kTabExtraSpace = 10;
constexpr int kTabCloseGap = 2;
constexpr int kTabDrawRight = 1;
constexpr int kTabDrawSelected = 2;

}  // namespace


// The menu bar updates the state of its items before it opens a menu
class FrameMenuBar : public Fl_Menu_Bar
{
public:
	FrameMenuBar(int x, int y, int width, int height, MainFrameBase &frame)
	    : Fl_Menu_Bar(x, y, width, height)
	    , m_frame(frame)
	{
	}

	int handle(int event) override
	{
		if(event == FL_PUSH || event == FL_SHORTCUT)
			m_frame.UpdateMenuState();
		return Fl_Menu_Bar::handle(event);
	}

private:
	MainFrameBase &m_frame;
};


// Tab of one child frame; the frame itself is not a child of the tab widget
class DocumentTab : public Fl_Group
{
public:
	DocumentTab(ChildFrameBase &frame)
	    : Fl_Group(0, 0, 0, 0)
	    , m_frame(frame)
	{
		end();
		box(FL_NO_BOX);
		labelsize(kTabLabelSize);
		when(FL_WHEN_CLOSED);
		callback(OnCloseTab);
	}

	ChildFrameBase &GetFrame() const noexcept { return m_frame; }

	void UpdateLabel()
	{
		const std::string escapedTitle = EscapeLabel(mpt::transcode<std::string>(mpt::common_encoding::utf8, m_frame.GetTitle()));
		if(label() == nullptr || escapedTitle != label())
			copy_label(escapedTitle.c_str());
	}

private:
	static std::string EscapeLabel(const std::string &text)
	{
		std::string escaped;
		escaped.reserve(text.size());
		for(const char c : text)
		{
			if(c == '@' || c == '&')
				escaped += c;
			escaped += c;
		}
		return escaped;
	}

	static void OnCloseTab(Fl_Widget *widget, void *)
	{
		if(Fl::callback_reason() != FL_REASON_CLOSED)
			return;
		ChildFrameBase &frame = static_cast<DocumentTab *>(widget)->GetFrame();
		if(View *view = frame.GetActiveView())
			view->PostCommand(ID_FILE_CLOSE);
		else if(Document *document = frame.GetActiveDocument())
			document->OnFileClose();
	}

	ChildFrameBase &m_frame;
};


// Header of the tabbed document area
class DocumentTabs : public Fl_Tabs
{
public:
	DocumentTabs(int x, int y, int width, int height, MainFrameBase &frame)
	    : Fl_Tabs(x, y, width, height)
	    , m_frame(frame)
	{
		end();
		box(FL_THIN_UP_BOX);
		labelsize(kTabLabelSize);
		handle_overflow(OVERFLOW_PULLDOWN);
		callback(OnSelectTab, this);
	}

	// Matches the tabs to the child frames of the main window
	void Sync(int x, int y, int width, int tabHeight)
	{
		const std::vector<ChildFrameBase *> &frames = m_frame.GetChildFrames();
		bool isInSync = (static_cast<std::size_t>(children()) == frames.size());
		for(std::size_t i = 0; isInSync && i < frames.size(); ++i)
			isInSync = (&static_cast<DocumentTab *>(child(static_cast<int>(i)))->GetFrame() == frames[i]);
		if(!isInSync)
		{
			clear();
			begin();
			for(ChildFrameBase *frame : frames)
				new DocumentTab(*frame);
			end();
		}
		resize(x, y, width, tabHeight + kTabBorderHeight);
		for(int i = 0; i < children(); ++i)
		{
			DocumentTab *tab = static_cast<DocumentTab *>(child(i));
			tab->resize(x, y + tabHeight, width, kTabBorderHeight);
			tab->UpdateLabel();
			if(&tab->GetFrame() == m_frame.MDIGetActive())
				value(tab);
		}
		if(frames.size() < 2)
			hide();
		else
			show();
		redraw();
	}

	int tab_positions() override
	{
		const int selectedIndex = Fl_Tabs::tab_positions();
		int shift = 0;
		for(int i = 0; i < tab_count; ++i)
		{
			tab_pos[i] += shift;
			tab_width[i] += 2 * kTabClosePadding;
			shift += 2 * kTabClosePadding;
		}
		tab_pos[tab_count] += shift;
		return selectedIndex;
	}

	int hit_close(Fl_Widget *widget, int eventX, int eventY) override
	{
		if(eventY < y() || eventY >= y() + tab_height())
			return 0;
		for(int i = 0; i < tab_count; ++i)
		{
			if(child(i) != widget)
				continue;
			const int tabX = x() + tab_pos[i] + tab_offset;
			return (eventX >= tabX) && (eventX < tabX + labelsize() / 2 + kTabExtraSpace / 2 + kTabCloseGap + 2 * kTabClosePadding);
		}
		return 0;
	}

	void draw_tab(int x1, int x2, int width, int height, Fl_Widget *widget, int flags, int what) override
	{
		if(height < 0)
		{
			Fl_Tabs::draw_tab(x1, x2, width, height, widget, flags, what);
			return;
		}
		x1 += tab_offset;
		x2 += tab_offset;
		const bool isSelected = (what == kTabDrawSelected);
		if(x2 < x1 + width && what == kTabDrawRight)
			x1 = x2 - width;
		const int yOffset = isSelected ? 0 : kTabBorder;
		const int tabHeight = height + Fl::box_dh(box());
		const Fl_Boxtype tabBox = (widget == push() && !isSelected) ? fl_down(box()) : box();
		const Fl_Color tabColor = isSelected ? selection_color() : widget->selection_color();
		draw_box(tabBox, x1, y() + yOffset, width, tabHeight + 10 - yOffset, tabColor);

		const Fl_Color oldLabelColor = widget->labelcolor();
		widget->labelcolor(isSelected ? labelcolor() : oldLabelColor);
		const int symbolSize = labelsize() / 2;
		const int symbolY = y() + yOffset / 2 + (tabHeight - symbolSize) / 2;
		Fl_Color closeColor = fl_contrast(FL_GRAY_RAMP + 0, tabColor);
		if(!active_r())
			closeColor = fl_inactive(closeColor);
		fl_draw_symbol("@3+", x1 + kTabExtraSpace / 2 + kTabClosePadding, symbolY, symbolSize, symbolSize, closeColor);
		const int labelOffset = symbolSize + kTabCloseGap + 2 * kTabClosePadding;
		widget->draw_label(x1 + labelOffset, y() + yOffset, width - labelOffset, tabHeight - yOffset, tab_align());
		widget->labelcolor(oldLabelColor);

		if(Fl::focus() == this && widget->visible())
			draw_focus(tabBox, x1, y(), width, tabHeight, tabColor);
	}

	void draw() override
	{
		for(int i = 0; i < children(); ++i)
			static_cast<DocumentTab *>(child(i))->UpdateLabel();
		Fl_Tabs::draw();
	}

	int handle(int event) override
	{
		const int result = Fl_Tabs::handle(event);
		ChildFrameBase *frame = m_frame.MDIGetActive();
		if(Fl::focus() == this && frame != nullptr)
		{
			if(View *view = frame->GetActiveView())
				view->SetFocus();
		}
		return result;
	}

private:
	static void OnSelectTab(Fl_Widget *, void *data)
	{
		DocumentTabs *tabs = static_cast<DocumentTabs *>(data);
		if(DocumentTab *tab = static_cast<DocumentTab *>(tabs->value()))
			tabs->m_frame.MDIActivate(&tab->GetFrame());
	}

	MainFrameBase &m_frame;
};


UI_MESSAGE_MAP_BEGIN(Document, CommandTarget)
	UI_COMMAND(ID_FILE_CLOSE, &Document::OnFileClose)
	UI_COMMAND(ID_FILE_SAVE, &Document::OnFileSave)
	UI_COMMAND(ID_FILE_SAVE_AS, &Document::OnFileSaveAs)
UI_MESSAGE_MAP_END()


Document::~Document()
{
	if(m_template)
		m_template->RemoveDocument(this);
}


void Document::SetPathName(const mpt::PathString &path, bool addToMostRecentlyUsed)
{
	m_pathName = path;
	SetTitle(path.GetFilename().ToUnicode());
	if(addToMostRecentlyUsed && AppBase::GetApp())
		AppBase::GetApp()->AddToRecentFileList(path);
}


void Document::SetTitle(const mpt::ustring &title)
{
	m_title = title;
	UpdateFrameCounts();
}


void Document::SetModifiedFlag(bool isModified)
{
	m_isModified = isModified;
}


void Document::AddView(View *view)
{
	if(std::find(m_views.begin(), m_views.end(), view) == m_views.end())
	{
		m_views.push_back(view);
		OnChangedViewList();
	}
}


void Document::RemoveView(View *view)
{
	const auto it = std::find(m_views.begin(), m_views.end(), view);
	if(it != m_views.end())
	{
		m_views.erase(it);
		OnChangedViewList();
	}
}


void Document::UpdateAllViews(View *sender, LParam hint, HintObject *hintObject)
{
	const std::vector<View *> views = m_views;
	for(View *view : views)
	{
		if(view != sender)
			view->OnUpdate(sender, hint, hintObject);
	}
}


bool Document::OnNewDocument()
{
	DeleteContents();
	m_isModified = false;
	return true;
}


bool Document::OnOpenDocument(const mpt::PathString &)
{
	DeleteContents();
	m_isModified = false;
	return true;
}


bool Document::OnSaveDocument(const mpt::PathString &)
{
	m_isModified = false;
	return true;
}


void Document::OnCloseDocument()
{
	// Close all frames that show this document
	while(!m_views.empty())
	{
		View *view = m_views.back();
		ChildFrameBase *frame = view->GetParentFrame();
		RemoveView(view);
		if(frame)
		{
			if(MainFrameBase *mainFrame = frame->GetMDIFrame())
				mainFrame->RemoveChildFrame(frame);
			frame->DestroyWindow();
			Fl::delete_widget(frame->GetWidget());
		}
	}
	if(m_template)
		m_template->RemoveDocument(this);
	m_template = nullptr;
	if(m_isAutoDelete)
		delete this;
}


void Document::DeleteContents()
{
}


bool Document::SaveModified()
{
	return true;
}


bool Document::DoSave(const mpt::PathString &path, bool shouldReplace)
{
	if(!OnSaveDocument(path))
		return false;
	if(shouldReplace)
		SetPathName(path);
	return true;
}


bool Document::DoFileSave()
{
	return DoSave(m_pathName, true);
}


void Document::OnChangedViewList()
{
}


void Document::UpdateFrameCounts()
{
	for(View *view : m_views)
	{
		if(ChildFrameBase *frame = view->GetParentFrame())
			frame->OnUpdateFrameTitle(true);
	}
}


void Document::PreCloseFrame(ChildFrameBase *)
{
}


DocTemplate::DocTemplate(uint32 resourceId, DocumentFactory documentFactory, FrameFactory frameFactory)
    : m_resourceId(resourceId)
    , m_documentFactory(std::move(documentFactory))
    , m_frameFactory(std::move(frameFactory))
{
}


Document *DocTemplate::OpenDocumentFile(const mpt::PathString &path, bool addToMostRecentlyUsed, bool isVisible)
{
	Document *document = m_documentFactory();
	if(document == nullptr)
		return nullptr;
	AddDocument(document);
	if(path.empty())
		SetDefaultTitle(document);
	const bool isOpened = path.empty() ? document->OnNewDocument() : document->OnOpenDocument(path);
	if(!isOpened)
	{
		RemoveDocument(document);
		delete document;
		return nullptr;
	}
	ChildFrameBase *frame = CreateNewFrame(*document);
	if(frame == nullptr)
	{
		RemoveDocument(document);
		delete document;
		return nullptr;
	}
	if(path.empty())
		++m_nUntitledCount;
	else
		document->SetPathName(path, addToMostRecentlyUsed);
	frame->InitialUpdateFrame(document, isVisible);
	return document;
}


void DocTemplate::AddDocument(Document *document)
{
	m_documents.push_back(document);
	document->SetDocTemplate(this);
}


void DocTemplate::RemoveDocument(Document *document)
{
	const auto it = std::find(m_documents.begin(), m_documents.end(), document);
	if(it != m_documents.end())
		m_documents.erase(it);
}


bool DocTemplate::CloseAllDocuments(bool)
{
	const std::vector<Document *> documents = m_documents;
	for(Document *document : documents)
	{
		if(!document->SaveModified())
			return false;
		document->OnCloseDocument();
	}
	return true;
}


void DocTemplate::SetDefaultTitle(Document *document)
{
	if(m_nUntitledCount == 0)
		document->SetTitle(UL_("untitled"));
	else
		document->SetTitle(UL_("untitled") + mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, std::to_string(m_nUntitledCount + 1)));
}


ChildFrameBase *DocTemplate::CreateNewFrame(Document &document)
{
	ChildFrameBase *frame = m_frameFactory(document);
	if(frame == nullptr)
		return nullptr;
	if(AppBase *app = AppBase::GetApp())
	{
		if(MainFrameBase *mainFrame = app->GetMainWnd())
			mainFrame->AddChildFrame(frame);
	}
	frame->SetFrameDocument(&document);
	frame->OnCreateViews(document);
	return frame;
}


View::View(int x, int y, int width, int height)
    : Panel(x, y, width, height)
{
	m_isFocusable = true;
}


View::~View()
{
	if(m_document)
		m_document->RemoveView(this);
}


void View::SetDocument(Document *document)
{
	if(m_document)
		m_document->RemoveView(this);
	m_document = document;
	if(m_document)
		m_document->AddView(this);
}


ChildFrameBase *Wnd::GetParentFrame() const
{
	for(Wnd *wnd = GetParent(); wnd != nullptr; wnd = wnd->GetParent())
	{
		if(ChildFrameBase *frame = dynamic_cast<ChildFrameBase *>(wnd))
			return frame;
	}
	return nullptr;
}


void View::OnInitialUpdate()
{
	OnUpdate(nullptr, 0, nullptr);
}


void View::OnUpdate(View *, LParam, HintObject *)
{
	Invalidate();
}


void View::OnActivateView(bool, View *, View *)
{
}


void View::InitialUpdate()
{
	OnInitialUpdate();
}


bool View::OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result)
{
	if(Wnd::OnCmdMsg(id, code, extra, result))
		return true;
	if(m_document && m_document->OnCmdMsg(id, code, extra, result))
		return true;
	return false;
}


void View::OnDraw(Painter *)
{
}


const MessageMap FormView::messageMap{&View::messageMap};


class FormView::Form : public Dialog
{
public:
	Form(FormView &owner) : m_owner(owner) {}
	void DoDataExchange(DataExchange *dx) override { m_owner.DoDataExchange(dx); }
	void OnOK() override {}
	void OnCancel() override {}
	bool OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result) override
	{
		return m_owner.OnCmdMsg(id, code, extra, result);
	}
	void OnHScroll(uint32 code, uint32 position, Wnd *scrollBar) override { m_owner.CallHScroll(code, position, scrollBar); }
	void OnVScroll(uint32 code, uint32 position, Wnd *scrollBar) override { m_owner.CallVScroll(code, position, scrollBar); }

private:
	FormView &m_owner;
};


FormView::FormView(uint32 templateId)
    : m_templateId(templateId)
{
}


FormView::~FormView() = default;


void FormView::OnInitialUpdate()
{
	if(!m_form)
	{
		m_form = new Form(*this);
		m_form->CreateChild(m_templateId, *this, x(), y());
	}
	View::OnInitialUpdate();
}


void FormView::DoDataExchange(DataExchange *)
{
}


bool FormView::UpdateData(bool isSaveAndValidate)
{
	return m_form ? m_form->UpdateData(isSaveAndValidate) : false;
}


Size FormView::GetFormSize() const
{
	return m_form ? Size(m_form->w(), m_form->h()) : Size();
}


bool FormView::OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result)
{
	return View::OnCmdMsg(id, code, extra, result);
}


void FormView::OnSize(uint32 type, int width, int height)
{
	View::OnSize(type, width, height);
}


ScrollView::ScrollView(int x, int y, int width, int height)
    : View(x, y, width, height)
{
	begin();
	m_horizontalBar = new Fl_Scrollbar(x, y + height - kBarSize, width - kBarSize, kBarSize);
	m_horizontalBar->type(FL_HORIZONTAL);
	m_horizontalBar->callback(BarCallback, this);
	m_horizontalBar->hide();
	m_verticalBar = new Fl_Scrollbar(x + width - kBarSize, y, kBarSize, height - kBarSize);
	m_verticalBar->type(FL_VERTICAL);
	m_verticalBar->callback(BarCallback, this);
	m_verticalBar->hide();
	end();
}


Rect ScrollView::GetClientRect() const
{
	int width = w();
	int height = h();
	if(m_verticalBar && m_verticalBar->visible())
		width -= kBarSize;
	if(m_horizontalBar && m_horizontalBar->visible())
		height -= kBarSize;
	return Rect(0, 0, std::max(width, 0), std::max(height - GetNonClientTop(), 0));
}


void ScrollView::SetScrollSizes(Size total, Size page, Size line)
{
	m_total = total;
	m_page = page;
	m_line = line;
	UpdateBars();
}


void ScrollView::ScrollToPosition(Point position)
{
	const Size area = GetClientRect().Dimensions();
	position.x = std::clamp(position.x, 0, std::max(m_total.cx - area.cx, 0));
	position.y = std::clamp(position.y, 0, std::max(m_total.cy - area.cy, 0));
	if(position == m_position)
		return;
	m_position = position;
	UpdateBars();
	OnScrollChanged();
	Invalidate();
}


bool ScrollView::OnScrollBy(Size scrollSize, bool isDoScroll)
{
	const Size area = GetClientRect().Dimensions();
	Point target(m_position.x + scrollSize.cx, m_position.y + scrollSize.cy);
	target.x = std::clamp(target.x, 0, std::max(m_total.cx - area.cx, 0));
	target.y = std::clamp(target.y, 0, std::max(m_total.cy - area.cy, 0));
	if(target == m_position)
		return false;
	if(isDoScroll)
		ScrollToPosition(target);
	return true;
}


bool ScrollView::OnScroll(uint32 code, uint32 position, bool isDoScroll)
{
	// The low byte is the horizontal code, the high byte the vertical one
	const uint32 horizontalCode = code & 0xFF;
	const uint32 verticalCode = (code >> 8) & 0xFF;
	Size delta;
	if(horizontalCode == ScrollThumbTrack || horizontalCode == ScrollThumbPosition)
		delta.cx = static_cast<int>(position) - m_position.x;
	if(verticalCode == ScrollThumbTrack || verticalCode == ScrollThumbPosition)
		delta.cy = static_cast<int>(position) - m_position.y;
	return OnScrollBy(delta, isDoScroll);
}


int ScrollView::SetScrollPos(int bar, int position, bool)
{
	const int previous = GetScrollPos(bar);
	if(bar == 0)
		ScrollToPosition(Point(position, m_position.y));
	else
		ScrollToPosition(Point(m_position.x, position));
	return previous;
}


int ScrollView::GetScrollPos(int bar) const
{
	return bar == 0 ? m_position.x : m_position.y;
}


bool ScrollView::GetScrollInfo(int bar, int &minimum, int &maximum, int &page, int &position, int &trackPosition) const
{
	const Fl_Scrollbar *scrollBar = (bar == 0) ? m_horizontalBar : m_verticalBar;
	if(scrollBar == nullptr || !scrollBar->visible())
		return false;
	const Size area = GetClientRect().Dimensions();
	minimum = 0;
	maximum = (bar == 0) ? m_total.cx : m_total.cy;
	page = (bar == 0) ? area.cx : area.cy;
	position = GetScrollPos(bar);
	trackPosition = m_trackPosition[bar == 0 ? 0 : 1];
	return true;
}


int ScrollView::GetScrollLimit(int bar) const
{
	const Size area = GetClientRect().Dimensions();
	return bar == 0 ? std::max(m_total.cx - area.cx, 0) : std::max(m_total.cy - area.cy, 0);
}


bool ScrollView::IsBarVisible(int bar) const
{
	const Fl_Scrollbar *scrollBar = (bar == 0) ? m_horizontalBar : m_verticalBar;
	return scrollBar && scrollBar->visible();
}


void ScrollView::ShowScrollBar(int bar, bool isVisible)
{
	Fl_Scrollbar *scrollBar = (bar == 0) ? m_horizontalBar : m_verticalBar;
	if(scrollBar == nullptr)
		return;
	if(isVisible)
		scrollBar->show();
	else
		scrollBar->hide();
}


void ScrollView::OnSize(uint32 type, int width, int height)
{
	View::OnSize(type, width, height);
	UpdateBars();
}


void ScrollView::resize(int x, int y, int width, int height)
{
	View::resize(x, y, width, height);
	UpdateBars();
}


void ScrollView::draw()
{
	if(damage() & ~FL_DAMAGE_CHILD)
	{
		DispatchDraw();
		if(m_verticalBar->visible() && m_barInsets.cy > 0)
			draw_box(FL_FLAT_BOX, x() + w() - kBarSize, y() + GetNonClientTop(), kBarSize, m_barInsets.cy, FL_BACKGROUND_COLOR);
		if(m_horizontalBar->visible() && m_barInsets.cx > 0)
			draw_box(FL_FLAT_BOX, x(), y() + h() - kBarSize, m_barInsets.cx, kBarSize, FL_BACKGROUND_COLOR);
	}
	draw_children();
	// Child windows created by the view are added after the bars and must not cover them
	draw_child(*m_horizontalBar);
	draw_child(*m_verticalBar);
}


bool ScrollView::OnMouseWheel(uint32, int16 delta, Point)
{
	const int lines = std::max(m_line.cy, 1);
	return OnScrollBy(Size(0, -delta / 120 * lines * 3), true);
}


void ScrollView::OnScrollChanged()
{
}


void ScrollView::SetBarInsets(Size insets)
{
	m_barInsets = insets;
	UpdateBars();
}


void ScrollView::UpdateBars()
{
	if(m_isUpdatingBars || m_horizontalBar == nullptr || m_verticalBar == nullptr)
		return;
	m_isUpdatingBars = true;
	// Whether a bar is needed depends on the other bar taking space
	const int clientHeight = h() - GetNonClientTop();
	bool isHorizontal = m_total.cx > w();
	bool isVertical = m_total.cy > clientHeight;
	isHorizontal = m_total.cx > w() - (isVertical ? kBarSize : 0);
	isVertical = m_total.cy > clientHeight - (isHorizontal ? kBarSize : 0);
	int viewWidth = w() - (isVertical ? kBarSize : 0);
	int viewHeight = clientHeight - (isHorizontal ? kBarSize : 0);
	m_position.x = std::clamp(m_position.x, 0, std::max(m_total.cx - viewWidth, 0));
	m_position.y = std::clamp(m_position.y, 0, std::max(m_total.cy - viewHeight, 0));
	if(isHorizontal)
	{
		m_horizontalBar->resize(x() + m_barInsets.cx, y() + h() - kBarSize, std::max(viewWidth - m_barInsets.cx, 0), kBarSize);
		m_horizontalBar->scrollvalue(m_position.x, std::max(viewWidth, 1), 0, std::max(m_total.cx, 1));
		m_horizontalBar->linesize(std::max(m_line.cx, 1));
		m_horizontalBar->show();
	} else
	{
		m_horizontalBar->hide();
	}
	if(isVertical)
	{
		m_verticalBar->resize(x() + w() - kBarSize, y() + GetNonClientTop() + m_barInsets.cy, kBarSize, std::max(viewHeight - m_barInsets.cy, 0));
		m_verticalBar->scrollvalue(m_position.y, std::max(viewHeight, 1), 0, std::max(m_total.cy, 1));
		m_verticalBar->linesize(std::max(m_line.cy, 1));
		m_verticalBar->show();
	} else
	{
		m_verticalBar->hide();
	}
	m_isUpdatingBars = false;
}


void ScrollView::BarCallback(Fl_Widget *widget, void *data)
{
	ScrollView *view = static_cast<ScrollView *>(data);
	if(view->m_isUpdatingBars)
		return;
	const bool isHorizontal = (widget == view->m_horizontalBar);
	const int value = static_cast<Fl_Scrollbar *>(widget)->value();
	view->m_trackPosition[isHorizontal ? 0 : 1] = value;
	const uint32 code = isHorizontal ? ScrollThumbTrack : (ScrollThumbTrack << 8);
	view->OnScroll(code, static_cast<uint32>(value), true);
}


ChildFrameBase::ChildFrameBase(int x, int y, int width, int height)
    : Panel(x, y, width, height)
{
}


ChildFrameBase::~ChildFrameBase()
{
	if(MainFrameBase *mainFrame = GetMDIFrame())
		mainFrame->RemoveChildFrame(this);
}


Document *ChildFrameBase::GetActiveDocument() const
{
	return m_activeView ? m_activeView->GetDocument() : m_frameDocument;
}


void ChildFrameBase::SetActiveView(View *view, bool isNotify)
{
	if(view == m_activeView)
		return;
	View *previous = m_activeView;
	m_activeView = view;
	if(isNotify)
	{
		if(previous)
			previous->OnActivateView(false, view, previous);
		if(view)
			view->OnActivateView(true, view, previous);
	}
}


void ChildFrameBase::OnCreateViews(Document &)
{
}


void ChildFrameBase::InitialUpdateFrame(Document *document, bool isMakeVisible)
{
	MPT_UNUSED(document);
	if(Fl_Group *group = GetWidget()->as_group())
	{
		for(int i = 0; i < group->children(); ++i)
		{
			if(View *view = dynamic_cast<View *>(group->child(i)))
				view->InitialUpdate();
		}
	}
	if(isMakeVisible)
		ActivateFrame();
}


void ChildFrameBase::ActivateFrame(int)
{
	if(MainFrameBase *mainFrame = GetMDIFrame())
		mainFrame->MDIActivate(this);
}


void ChildFrameBase::RecalcLayout(bool)
{
}


void ChildFrameBase::OnUpdateFrameTitle(bool)
{
	if(Document *document = GetActiveDocument())
		m_title = document->GetTitle();
	if(MainFrameBase *mainFrame = GetMDIFrame())
		mainFrame->GetWidget()->redraw();
}


MainFrameBase *ChildFrameBase::GetMDIFrame() const
{
	for(Wnd *wnd = GetParent(); wnd != nullptr; wnd = wnd->GetParent())
	{
		if(MainFrameBase *frame = dynamic_cast<MainFrameBase *>(wnd))
			return frame;
	}
	if(AppBase *app = AppBase::GetApp())
		return app->GetMainWnd();
	return nullptr;
}


bool ChildFrameBase::OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result)
{
	if(m_activeView && m_activeView->OnCmdMsg(id, code, extra, result))
		return true;
	return Wnd::OnCmdMsg(id, code, extra, result);
}


MainFrameBase::MainFrameBase(int width, int height, const char *title)
    : WndT<Fl_Double_Window>(0, 0, width, height, title)
{
	box(FL_FLAT_BOX);
	color(FL_BACKGROUND_COLOR);
	m_isCustomPaint = false;
	begin();
	m_menuBar = new FrameMenuBar(0, 0, width, m_menuBarHeight, *this);
	m_documentArea = new Fl_Group(0, m_menuBarHeight, width, height - m_menuBarHeight);
	m_documentArea->box(FL_FLAT_BOX);
	m_documentArea->end();
	end();
	resizable(m_documentArea);
	callback(
	    [](Fl_Widget *, void *data)
	    {
		    static_cast<MainFrameBase *>(data)->OnClose();
	    },
	    this);
	menuCommandHandler = [this](uint32 id)
	{
		RouteCommand(id, 0, nullptr);
	};
}


MainFrameBase::~MainFrameBase()
{
	menuCommandHandler = nullptr;
}


void MainFrameBase::AddChildFrame(ChildFrameBase *frame)
{
	m_frames.push_back(frame);
	m_documentArea->add(frame);
	frame->GetWidget()->hide();
	MDIActivate(frame);
	RecalcLayout();
}


void MainFrameBase::RemoveChildFrame(ChildFrameBase *frame)
{
	const auto it = std::find(m_frames.begin(), m_frames.end(), frame);
	if(it == m_frames.end())
		return;
	const bool isActive = (m_activeFrame == frame);
	m_frames.erase(it);
	m_documentArea->remove(frame);
	if(isActive)
	{
		m_activeFrame = nullptr;
		if(m_frames.empty())
			OnUpdateFrameTitle(true);
		else
			MDIActivate(m_frames.back());
	}
	RecalcLayout();
	redraw();
}


void MainFrameBase::MDIActivate(ChildFrameBase *frame)
{
	if(frame == m_activeFrame)
		return;
	if(m_activeFrame)
		m_activeFrame->GetWidget()->hide();
	m_activeFrame = frame;
	if(m_activeFrame)
	{
		m_activeFrame->GetWidget()->show();
		RecalcLayout();
		if(View *view = m_activeFrame->GetActiveView())
			view->SetFocus();
	}
	OnUpdateFrameTitle(true);
	redraw();
}


void MainFrameBase::MDINext()
{
	if(m_frames.size() < 2 || m_activeFrame == nullptr)
		return;
	const auto it = std::find(m_frames.begin(), m_frames.end(), m_activeFrame);
	const std::size_t index = static_cast<std::size_t>(it - m_frames.begin());
	MDIActivate(m_frames[(index + 1) % m_frames.size()]);
}


void MainFrameBase::MDIPrev()
{
	if(m_frames.size() < 2 || m_activeFrame == nullptr)
		return;
	const auto it = std::find(m_frames.begin(), m_frames.end(), m_activeFrame);
	const std::size_t index = static_cast<std::size_t>(it - m_frames.begin());
	MDIActivate(m_frames[(index + m_frames.size() - 1) % m_frames.size()]);
}


void MainFrameBase::SetMenu(const Menu &menu)
{
	m_menu = menu;
	UpdateMenuState();
}


void MainFrameBase::UpdateMenuState()
{
	UpdateMenuItems(m_menu, *this);
	std::vector<Fl_Menu_Item> items;
	std::vector<std::unique_ptr<std::string>> labels;
	m_menu.BuildFltkItems(items, labels);
	items.push_back(Fl_Menu_Item{});
	m_menuBar->copy(items.data());
	m_menuLabels = std::move(labels);
}


void MainFrameBase::RecalcLayout(bool)
{
	const int width = w();
	const int height = h();
	m_menuBar->resize(0, 0, width, m_menuBarHeight);
	int top = m_menuBarHeight;
	int bottom = height;
	// Bars docked by the derived class take their space first
	for(const DockedBar &bar : m_bars)
	{
		Fl_Widget *widget = bar.window->GetWidget();
		if(!widget->visible())
			continue;
		switch(bar.side)
		{
		case DockSide::Top:
			widget->resize(0, top, width, bar.size);
			top += bar.size;
			break;
		case DockSide::Bottom:
			widget->resize(0, bottom - bar.size, width, bar.size);
			bottom -= bar.size;
			break;
		default:
			break;
		}
	}
	int left = 0;
	int right = width;
	for(const DockedBar &bar : m_bars)
	{
		Fl_Widget *widget = bar.window->GetWidget();
		if(!widget->visible())
			continue;
		if(bar.side == DockSide::Left)
		{
			widget->resize(left, top, bar.size, bottom - top);
			left += bar.size;
		} else if(bar.side == DockSide::Right)
		{
			widget->resize(right - bar.size, top, bar.size, bottom - top);
			right -= bar.size;
		}
	}
	m_documentArea->resize(left, top, right - left, bottom - top);
	const bool hasTabs = m_frames.size() > 1;
	const int tabHeight = hasTabs ? kTabStripHeight : 0;
	const int frameTop = hasTabs ? top + tabHeight + kTabBorderHeight : top;
	if(m_tabs == nullptr)
	{
		m_documentArea->begin();
		m_tabs = new DocumentTabs(left, top, right - left, tabHeight, *this);
		m_documentArea->end();
	}
	static_cast<DocumentTabs *>(m_tabs)->Sync(left, top, right - left, tabHeight);
	for(ChildFrameBase *frame : m_frames)
		frame->GetWidget()->resize(left, frameTop, right - left, std::max(bottom - frameTop, 0));
	redraw();
}


void MainFrameBase::DockBar(Wnd *bar, DockSide side, int size)
{
	m_bars.push_back(DockedBar{bar, side, size});
	add(bar->GetWidget());
	RecalcLayout();
}


void MainFrameBase::UpdateDockedBar(const Wnd *bar, DockSide side, int size)
{
	for(DockedBar &docked : m_bars)
	{
		if(docked.window == bar)
		{
			docked.side = side;
			docked.size = size;
		}
	}
}


void MainFrameBase::ShowControlBar(Wnd *bar, bool isVisible)
{
	bar->ShowWindow(isVisible);
	RecalcLayout();
}


void MainFrameBase::UpdateFrameTitleForDocument(const mpt::ustring &documentTitle)
{
	const mpt::ustring title = documentTitle.empty() ? m_title : documentTitle + UL_(" - ") + m_title;
	copy_label(mpt::transcode<std::string>(mpt::common_encoding::utf8, title).c_str());
}


void MainFrameBase::OnUpdateFrameTitle(bool)
{
	UpdateFrameTitleForDocument(m_activeFrame ? m_activeFrame->GetTitle() : mpt::ustring{});
}


void MainFrameBase::OnClose()
{
	hide();
}


void MainFrameBase::OnSize(uint32, int, int)
{
	RecalcLayout();
}


bool MainFrameBase::PreTranslateMessage(int)
{
	return false;
}


bool MainFrameBase::OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result)
{
	if(m_activeFrame && m_activeFrame->OnCmdMsg(id, code, extra, result))
		return true;
	if(Wnd::OnCmdMsg(id, code, extra, result))
		return true;
	if(AppBase *app = AppBase::GetApp())
		return app->OnCmdMsg(id, code, extra, result);
	return false;
}


AppBase::AppBase()
{
	s_instance = this;
}


AppBase::~AppBase()
{
	if(s_instance == this)
		s_instance = nullptr;
}


bool AppBase::InitInstance()
{
	return true;
}


int AppBase::ExitInstance()
{
	return 0;
}


int AppBase::Run()
{
	SetIdleHandler([this](int idleCount) { return OnIdle(idleCount); });
	return Fl::run();
}


bool AppBase::OnIdle(int)
{
	return false;
}


void AppBase::AddDocTemplate(std::unique_ptr<DocTemplate> docTemplate)
{
	m_template = std::move(docTemplate);
}


Document *AppBase::OpenDocumentFile(const mpt::PathString &path, bool addToMostRecentlyUsed)
{
	if(!m_template)
		return nullptr;
	return m_template->OpenDocumentFile(path, addToMostRecentlyUsed, true);
}


void AppBase::AddToRecentFileList(const mpt::PathString &path)
{
	const auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), path);
	if(it != m_recentFiles.end())
		m_recentFiles.erase(it);
	m_recentFiles.insert(m_recentFiles.begin(), path);
	if(m_recentFiles.size() > 10)
		m_recentFiles.resize(10);
}


void AppBase::RemoveRecentFile(std::size_t index)
{
	if(index < m_recentFiles.size())
		m_recentFiles.erase(m_recentFiles.begin() + index);
}


void AppBase::CloseAllDocuments(bool isEnding)
{
	if(m_template)
		m_template->CloseAllDocuments(isEnding);
}


void AppBase::QuitApplication()
{
	if(m_mainWindow)
		m_mainWindow->hide();
	Fl::hide_all_windows();
}


void AppBase::SetCommandLine(int argc, char **argv)
{
	m_arguments.assign(argv, argv + argc);
}


}  // namespace ui


OPENMPT_NAMESPACE_END
