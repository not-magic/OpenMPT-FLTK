// MFC replacement on FLTK. Application framework: documents, views, child frames with tabbed document area,
// main window and application object.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../../common/mptPathString.h"
#include "Menu.h"
#include "Wnd.h"

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Scrollbar.H>

#include <functional>
#include <memory>
#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

class DocTemplate;
class View;
class ChildFrameBase;
class DataExchange;
class MainFrameBase;


class Document : public CommandTarget
{
public:
	Document() = default;
	~Document() override;

	const mpt::PathString &GetPathName() const noexcept { return m_pathName; }
	virtual void SetPathName(const mpt::PathString &path, bool addToMostRecentlyUsed = true);
	mpt::ustring GetTitle() const { return m_title; }
	virtual void SetTitle(const mpt::ustring &title);
	bool IsModified() const noexcept { return m_isModified; }
	virtual void SetModifiedFlag(bool isModified = true);

	void AddView(View *view);
	void RemoveView(View *view);
	const std::vector<View *> &GetViews() const noexcept { return m_views; }
	// Informs all views except the sender; the hint is passed on untouched
	void UpdateAllViews(View *sender, LParam hint = 0, HintObject *hintObject = nullptr);

	DocTemplate *GetDocTemplate() const noexcept { return m_template; }
	void SetDocTemplate(DocTemplate *docTemplate) noexcept { m_template = docTemplate; }

	virtual bool OnNewDocument();
	virtual bool OnOpenDocument(const mpt::PathString &path);
	virtual bool OnSaveDocument(const mpt::PathString &path);
	virtual void OnCloseDocument();
	virtual void DeleteContents();
	// Asks the user whether to save pending changes. Returns false if closing should be cancelled.
	virtual bool SaveModified();
	virtual bool DoSave(const mpt::PathString &path, bool shouldReplace = true);
	virtual bool DoFileSave();
	// Closes the document after asking whether to save pending changes
	void OnFileClose() { if(SaveModified()) OnCloseDocument(); }
	void OnFileSave() { DoFileSave(); }
	void OnFileSaveAs() { DoSave(mpt::PathString{}); }
	virtual void OnChangedViewList();
	virtual void UpdateFrameCounts();
	virtual void PreCloseFrame(ChildFrameBase *frame);

	UI_DECLARE_MESSAGE_MAP()

	bool m_isAutoDelete = true;

private:
	mpt::PathString m_pathName;
	mpt::ustring m_title;
	std::vector<View *> m_views;
	DocTemplate *m_template = nullptr;
	bool m_isModified = false;
};


// Creates documents, their frames and views, and keeps track of open documents
class DocTemplate
{
public:
	using DocumentFactory = std::function<Document *()>;
	using FrameFactory = std::function<ChildFrameBase *(Document &)>;

	DocTemplate(uint32 resourceId, DocumentFactory documentFactory, FrameFactory frameFactory);
	virtual ~DocTemplate() = default;

	uint32 GetResourceId() const noexcept { return m_resourceId; }
	virtual Document *OpenDocumentFile(const mpt::PathString &path, bool addToMostRecentlyUsed = true, bool isVisible = true);
	virtual void AddDocument(Document *document);
	virtual void RemoveDocument(Document *document);
	const std::vector<Document *> &GetDocuments() const noexcept { return m_documents; }
	virtual bool CloseAllDocuments(bool isEnding);
	// Names a new document "untitled", "untitled2", ...
	virtual void SetDefaultTitle(Document *document);
	// Creates a frame with its views for the document
	ChildFrameBase *CreateNewFrame(Document &document);

protected:
	std::vector<Document *> m_documents;
	int m_nUntitledCount = 0;

private:
	uint32 m_resourceId;
	DocumentFactory m_documentFactory;
	FrameFactory m_frameFactory;
};


// One window onto a document
class View : public Panel
{
public:
	View(int x = 0, int y = 0, int width = 0, int height = 0);
	~View() override;

	Document *GetDocument() const noexcept { return m_document; }
	void SetDocument(Document *document);

	virtual void OnInitialUpdate();
	virtual void OnUpdate(View *sender, LParam hint, HintObject *hintObject);
	// Draws the view; Painter coordinates start at the top left of the client area
	virtual void OnDraw(Painter *painter);
	virtual void OnActivateView(bool isActivated, View *activatedView, View *deactivatedView);
	// Called when the view is about to be shown the first time
	void InitialUpdate();

	bool OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result = nullptr) override;

	static const MessageMap messageMap;

protected:
	const MessageMap *GetMessageMap() const override { return &messageMap; }
	void OnPaint(Painter &painter) override { OnDraw(&painter); }

private:
	Document *m_document = nullptr;
};


// A view that shows the controls of a dialog template
class FormView : public View
{
public:
	explicit FormView(uint32 templateId);
	~FormView() override;

	bool UpdateData(bool isSaveAndValidate = true);
	// Size of the dialog template shown by the view
	Size GetFormSize() const;
	virtual void DoDataExchange(DataExchange *dx);
	void OnInitialUpdate() override;
	bool OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result = nullptr) override;

	static const MessageMap messageMap;

protected:
	const MessageMap *GetMessageMap() const override { return &messageMap; }
	void OnSize(uint32 type, int width, int height) override;

private:
	class Form;
	Form *m_form = nullptr;
	uint32 m_templateId;
};


// A view with scroll bars that shows a part of a larger virtual area
class ScrollView : public View
{
public:
	ScrollView(int x = 0, int y = 0, int width = 0, int height = 0);

	// Size of the visible area without the scroll bars
	using Panel::GetClientRect;
	Rect GetClientRect() const override;

	void SetScrollSizes(Size total, Size page = {}, Size line = {});
	Size GetTotalSize() const noexcept { return m_total; }
	Point GetScrollPosition() const noexcept { return m_position; }
	void ScrollToPosition(Point position);
	Point GetDeviceScrollPosition() const noexcept { return m_position; }
	// Moves the view of the area; the content is not redrawn by this function
	virtual bool OnScrollBy(Size scrollSize, bool isDoScroll = true);
	// A scroll bar was used; code is one of the ScrollCode values
	virtual bool OnScroll(uint32 code, uint32 position, bool isDoScroll = true);

	int SetScrollPos(int bar, int position, bool isRedraw = true);
	int GetScrollPos(int bar) const;
	bool GetScrollInfo(int bar, int &minimum, int &maximum, int &page, int &position, int &trackPosition) const;
	bool IsBarVisible(int bar) const;
	// Largest scroll position of a bar
	int GetScrollLimit(int bar) const;
	void ShowScrollBar(int bar, bool isVisible);
	// Leaves the start of the bars free: horizontal bar left inset (cx), vertical bar top inset (cy)
	void SetBarInsets(Size insets);

	void resize(int x, int y, int width, int height) override;
	void draw() override;

protected:
	void OnSize(uint32 type, int width, int height) override;
	bool OnMouseWheel(uint32 flags, int16 delta, Point point) override;
	virtual void OnScrollChanged();

	Size m_total;
	Size m_page;
	Size m_line;
	Point m_position;
	Size m_barInsets;

	static constexpr int kBarSize = 16;

private:
	void UpdateBars();
	static void BarCallback(Fl_Widget *widget, void *data);

	Fl_Scrollbar *m_horizontalBar = nullptr;
	Fl_Scrollbar *m_verticalBar = nullptr;
	int m_trackPosition[2] = {0, 0};
	bool m_isUpdatingBars = false;
};


// Bars of scroll views
constexpr int SB_HORZ = 0;
constexpr int SB_VERT = 1;


// Container of the views of one document, shown as one tab of the main window
class ChildFrameBase : public Panel
{
public:
	ChildFrameBase(int x = 0, int y = 0, int width = 0, int height = 0);
	~ChildFrameBase() override;

	Document *GetActiveDocument() const;
	View *GetActiveView() const noexcept { return m_activeView; }
	virtual void SetActiveView(View *view, bool isNotify = true);
	virtual void InitialUpdateFrame(Document *document, bool isMakeVisible);
	// Creates the views of the frame once it is part of the main window and knows its document
	virtual void OnCreateViews(Document &document);
	void SetFrameDocument(Document *document) noexcept { m_frameDocument = document; }
	virtual void ActivateFrame(int command = -1);
	virtual void RecalcLayout(bool isNotify = true);
	void SetTitle(const mpt::ustring &title) { m_title = title; }
	mpt::ustring GetTitle() const { return m_title; }
	virtual void OnUpdateFrameTitle(bool isAddToTitle);
	MainFrameBase *GetMDIFrame() const;
	bool OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result = nullptr) override;

	static const MessageMap messageMap;

protected:
	const MessageMap *GetMessageMap() const override { return &messageMap; }

private:
	Document *m_frameDocument = nullptr;
	View *m_activeView = nullptr;
	mpt::ustring m_title;
};


// The main window: menu bar, areas for control bars, tabbed document area and status bar
class MainFrameBase : public WndT<Fl_Double_Window>
{
public:
	MainFrameBase(int width = 1024, int height = 768, const char *title = nullptr);
	~MainFrameBase() override;

	// Document area
	void AddChildFrame(ChildFrameBase *frame);
	void RemoveChildFrame(ChildFrameBase *frame);
	ChildFrameBase *MDIGetActive() const noexcept { return m_activeFrame; }
	void MDIActivate(ChildFrameBase *frame);
	void MDINext();
	void MDIPrev();
	const std::vector<ChildFrameBase *> &GetChildFrames() const noexcept { return m_frames; }

	Menu *GetMenu() noexcept { return &m_menu; }
	void SetMenu(const Menu &menu);
	virtual void RecalcLayout(bool isNotify = true);
	void ShowControlBar(Wnd *bar, bool isVisible);

	Fl_Group *GetDocumentArea() noexcept { return m_documentArea; }
	// Size of the area that holds the document windows
	Rect GetMDIClientRect() const { return Rect(0, 0, m_documentArea->w(), m_documentArea->h()); }

	enum class DockSide : uint8
	{
		Top,
		Bottom,
		Left,
		Right,
	};
	// Attaches a bar at one side of the document area; bars at the same side stack in the order they were added
	void DockBar(Wnd *bar, DockSide side, int size);
	// Changes the side and size of a docked bar; takes effect with the next RecalcLayout
	void UpdateDockedBar(const Wnd *bar, DockSide side, int size);

	// Name of the application in the title bar; the title of the active document is shown in front of it
	void SetTitle(const mpt::ustring &title) { m_title = title; }
	mpt::ustring GetTitle() const { return m_title; }
	void UpdateFrameTitleForDocument(const mpt::ustring &documentTitle);
	virtual void OnUpdateFrameTitle(bool isAddToTitle);
	// The user asked to close the window; the default closes it
	virtual void OnClose();

	// Updates the enabled and checked state of menu items
	void UpdateMenuState();

	bool OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result = nullptr) override;

	static const MessageMap messageMap;

protected:
	const MessageMap *GetMessageMap() const override { return &messageMap; }

	void OnSize(uint32 type, int width, int height) override;
	bool PreTranslateMessage(int event) override;

	struct DockedBar
	{
		Wnd *window;
		DockSide side;
		int size;
	};

	Fl_Menu_Bar *m_menuBar = nullptr;
	Fl_Group *m_documentArea = nullptr;
	Fl_Widget *m_tabs = nullptr;
	std::vector<DockedBar> m_bars;
	Menu m_menu;
	std::vector<std::unique_ptr<std::string>> m_menuLabels;
	ChildFrameBase *m_activeFrame = nullptr;
	std::vector<ChildFrameBase *> m_frames;
	mpt::ustring m_title = UL_("OpenMPT");
	int m_menuBarHeight = 22;
};


// Application object
class AppBase : public CommandTarget
{
public:
	AppBase();
	~AppBase() override;

	virtual bool InitInstance();
	virtual int ExitInstance();
	virtual int Run();
	// Return true to be called again as soon as possible
	virtual bool OnIdle(int idleCount);

	MainFrameBase *GetMainWnd() const noexcept { return m_mainWindow; }
	void SetMainWnd(MainFrameBase *window) noexcept { m_mainWindow = window; }

	void AddDocTemplate(std::unique_ptr<DocTemplate> docTemplate);
	DocTemplate *GetDocTemplate() const noexcept { return m_template.get(); }
	virtual Document *OpenDocumentFile(const mpt::PathString &path, bool addToMostRecentlyUsed = true);
	virtual void AddToRecentFileList(const mpt::PathString &path);
	const std::vector<mpt::PathString> &GetRecentFileList() const noexcept { return m_recentFiles; }
	void SetRecentFileList(std::vector<mpt::PathString> files) { m_recentFiles = std::move(files); }
	void RemoveRecentFile(std::size_t index);
	// Closes all documents and the main window; the run loop then ends
	virtual void CloseAllDocuments(bool isEnding);
	void QuitApplication();

	static AppBase *GetApp() noexcept { return s_instance; }

	void SetCommandLine(int argc, char **argv);
	const std::vector<std::string> &GetCommandLine() const noexcept { return m_arguments; }

	static const MessageMap messageMap;

protected:
	const MessageMap *GetMessageMap() const override { return &messageMap; }

	std::vector<mpt::PathString> m_recentFiles;
	std::vector<std::string> m_arguments;
	std::unique_ptr<DocTemplate> m_template;
	MainFrameBase *m_mainWindow = nullptr;

private:
	static AppBase *s_instance;
};


}  // namespace ui


OPENMPT_NAMESPACE_END
