/*
 * CommonDialogs.cpp
 * -----------------
 * Purpose: Dialogs for choosing a colour or a font.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "CommonDialogs.h"
#include "StandardIds.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Spinner.H>

#include <string>
#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

bool ChooseColor(ColorRef &color, const mpt::ustring &title)
{
	uchar red = static_cast<uchar>(GetRValue(color));
	uchar green = static_cast<uchar>(GetGValue(color));
	uchar blue = static_cast<uchar>(GetBValue(color));
	const std::string caption = title.empty() ? std::string("Choose Colour") : mpt::transcode<std::string>(mpt::common_encoding::utf8, title);
	if(fl_color_chooser(caption.c_str(), red, green, blue, 1) == 0)
		return false;
	color = RGB(red, green, blue);
	return true;
}


namespace
{

struct FontFamily
{
	std::string name;
	Fl_Font face;
};

std::vector<FontFamily> ListFontFamilies()
{
	std::vector<FontFamily> families;
	const Fl_Font count = Fl::set_fonts(nullptr);
	for(Fl_Font i = 0; i < count; ++i)
	{
		int attributes = 0;
		const char *name = Fl::get_font_name(i, &attributes);
		if(name != nullptr && attributes == 0)
			families.push_back({name, i});
	}
	return families;
}

class FontPreview : public Fl_Box
{
public:
	using Fl_Box::Fl_Box;
};

}  // namespace


FontDialog::FontDialog(const mpt::ustring &faceName, int32 size, bool isBold, bool isItalic)
    : m_faceName(faceName)
    , m_size(size)
    , m_isBold(isBold)
    , m_isItalic(isItalic)
{
}


intptr_t FontDialog::DoModal()
{
	const std::vector<FontFamily> families = ListFontFamilies();
	const std::string wantedName = mpt::transcode<std::string>(mpt::common_encoding::utf8, m_faceName);

	Fl_Double_Window window(380, 340, "Font");
	Fl_Hold_Browser browser(10, 10, 220, 250);
	for(const FontFamily &family : families)
		browser.add(family.name.c_str());
	Fl_Spinner sizeSpinner(300, 10, 70, 24, "Size:");
	sizeSpinner.type(FL_INT_INPUT);
	sizeSpinner.minimum(4);
	sizeSpinner.maximum(72);
	sizeSpinner.value(std::max(4, m_size / 10));
	Fl_Check_Button boldCheck(240, 50, 100, 24, "Bold");
	boldCheck.value(m_isBold ? 1 : 0);
	Fl_Check_Button italicCheck(240, 80, 100, 24, "Italic");
	italicCheck.value(m_isItalic ? 1 : 0);
	Fl_Box preview(10, 270, 360, 30, "AaBbYyZz 0123456789");
	preview.box(FL_DOWN_BOX);
	Fl_Button okButton(210, 308, 75, 24, "OK");
	Fl_Button cancelButton(295, 308, 75, 24, "Cancel");
	window.end();

	intptr_t result = IDCANCEL;
	bool isDone = false;

	const auto updatePreview = [&]()
	{
		const int line = browser.value();
		Fl_Font face = FL_HELVETICA;
		if(line >= 1 && line <= static_cast<int>(families.size()))
			face = families[line - 1].face;
		if(boldCheck.value())
			face += 1;
		if(italicCheck.value())
			face += 2;
		preview.labelfont(face);
		preview.labelsize(static_cast<Fl_Fontsize>(sizeSpinner.value()) * 96 / 72);
		preview.redraw();
	};

	for(size_t i = 0; i < families.size(); ++i)
	{
		if(families[i].name == wantedName)
		{
			browser.value(static_cast<int>(i) + 1);
			break;
		}
	}
	if(browser.value() == 0 && !families.empty())
		browser.value(1);

	struct Context
	{
		decltype(updatePreview) *update;
		bool *isDone;
		intptr_t *result;
	} context{&updatePreview, &isDone, &result};
	const auto onChanged = [](Fl_Widget *, void *data) { (*static_cast<Context *>(data)->update)(); };
	browser.callback(onChanged, &context);
	sizeSpinner.callback(onChanged, &context);
	boldCheck.callback(onChanged, &context);
	italicCheck.callback(onChanged, &context);
	okButton.callback([](Fl_Widget *, void *data)
	                  {
		                  Context *ctx = static_cast<Context *>(data);
		                  *ctx->result = IDOK;
		                  *ctx->isDone = true;
	                  },
	                  &context);
	cancelButton.callback([](Fl_Widget *, void *data) { *static_cast<Context *>(data)->isDone = true; }, &context);
	window.callback([](Fl_Widget *, void *data) { *static_cast<Context *>(data)->isDone = true; }, &context);
	updatePreview();

	window.set_modal();
	window.show();
	while(!isDone && window.shown())
		Fl::wait();
	window.hide();

	if(result == IDOK)
	{
		const int line = browser.value();
		if(line >= 1 && line <= static_cast<int>(families.size()))
			m_faceName = mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, families[line - 1].name);
		m_size = static_cast<int32>(sizeSpinner.value()) * 10;
		m_isBold = boldCheck.value() != 0;
		m_isItalic = italicCheck.value() != 0;
	}
	return result;
}

}  // namespace ui


OPENMPT_NAMESPACE_END
