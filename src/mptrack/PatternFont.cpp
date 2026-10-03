// FLTK port of openmpt/mptrack/PatternFont.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "PatternFont.h"
#include "Mptrack.h"
#include "Mainfrm.h"
#include "TrackerSettings.h"
#include "../soundlib/Tables.h"

OPENMPT_NAMESPACE_BEGIN

//////////////////////////////////////////////
// Font Definitions

// Medium Font (Default)
static constexpr PATTERNFONT gDefaultPatternFont = 
{
	nullptr,
	nullptr,
	92,13,	// Column Width & Height
	0,0,	// Clear location
	130,8,	// Space Location.
	{20, 20, 24, 9, 15},		// Element Widths
	{0, 0, 0, 0, 0},			// Padding pixels contained in element width
	20,13,	// Numbers 0-F (hex)
	30,13,	// Numbers 10-29 (dec)
	64,26,	// A-M#
	78,26,	// N-Z?										// MEDIUM FONT !!!
	0, 0,
	{ 7, 5 }, 8,	// Note & Octave Width
	42,13,			// Volume Column Effects
	8,8,
	-1,
	8,		// 8+7 = 15
	-3, -1, 12,
	1,		// Margin for first digit of PC event parameter number
	2,		// Margin for first digit of PC event parameter value
	1,		// Margin for second digit of parameter
	13,		// Vertical spacing between letters in the bitmap
};


//////////////////////////////////////////////////
// Small Font

static constexpr PATTERNFONT gSmallPatternFont =
{
	nullptr,
	nullptr,
	70,11,	// Column Width & Height
	92,0,	// Clear location
	130,8,	// Space Location.
	{16, 14, 18, 7, 11},		// Element Widths
	{0, 0, 0, 0, 0},			// Padding pixels contained in element width
	108,13,	// Numbers 0-F (hex)
	120,13,	// Numbers 10-29 (dec)
	142,26,	// A-M#
	150,26,	// N-Z?										// SMALL FONT !!!
	92, 0,	// Notes
	{ 5, 5 }, 6,	// Note & Octave Width
	132,13,		// Volume Column Effects
	6,5,
	-1,
	6,		// 8+7 = 15
	-3,	1, 9,	// InstrOfs + nInstrHiWidth
	1,		// Margin for first digit of PC event parameter number
	2,		// Margin for first digit of PC event parameter value
	1,		// Margin for second digit of parameter
	13,		// // Vertical spacing between letters in the bitmap
};

// NOTE: See also CViewPattern::DrawNote() when changing stuff here
// or adding new fonts - The custom tuning note names might require
// some additions there.

const PATTERNFONT *PatternFont::currentFont = nullptr;

static MODPLUGDIB customFontBitmap;
static MODPLUGDIB customFontBitmapASCII;

static void DrawChar(ui::Painter &dc, char32_t ch, int x, int y, int w, int h)
{
	Rect rect(x, y, x + w, y + h);
	dc.DrawText(mpt::ustring(1, mpt::unsafe_char_convert<mpt::uchar>(ch)), rect, ui::TextCenter | ui::TextVCenter | ui::TextSingleLine | ui::TextNoPrefix);
}

template<typename char_t>
static void DrawString(ui::Painter &dc, const char_t *text, int len, int x, int y, int w, int h)
{
	for(int i = 0; i < len; i++)
	{
		DrawChar(dc, static_cast<char32_t>(static_cast<std::make_unsigned_t<char_t>>(text[i])), x, y, w, h);
		x += w;
	}
}

// Converts rendered black-on-white text into a 16-colour DIB (index 0 = black, 15 = white)
static void BitmapToDib(const ui::Bitmap &bitmap, int dibWidth, MODPLUGDIB &dib)
{
	dib.width = dibWidth;
	dib.height = bitmap.GetHeight();
	dib.pixels.assign(static_cast<size_t>(dib.width) * dib.height, 15);
	for(int y = 0; y < bitmap.GetHeight(); y++)
	{
		for(int x = 0; x < bitmap.GetWidth() && x < dibWidth; x++)
		{
			const ColorRef color = bitmap.GetPixel(x, y);
			const int luminance = (GetRValue(color) + GetGValue(color) + GetBValue(color)) / 3;
			dib.pixels[static_cast<size_t>(y) * dib.width + x] = (luminance > 127) ? 15 : 0;
		}
	}
	dib.palette[0] = RGB(0x00, 0x00, 0x00);
	dib.palette[15] = RGB(0xFF, 0xFF, 0xFF);
}

void PatternFont::UpdateFont(WindowHandle hwnd)
{
	const int dpi = ui::GetDpiForWindow(hwnd);
	FontSetting font = TrackerSettings::Instance().patternFont;
	const PATTERNFONT *builtinFont = nullptr;
	if(font.name == PATTERNFONT_SMALL || font.name.empty())
	{
		builtinFont = &gSmallPatternFont;
	} else if(font.name == PATTERNFONT_LARGE)
	{
		builtinFont = &gDefaultPatternFont;
	}

	const int builtInFontSize = Util::muldivr(std::min(font.size + 1, 11), dpi, 96);
	if(builtinFont != nullptr && builtInFontSize < 2)
	{
		currentFont = builtinFont;
		return;
	}

	static PATTERNFONT pf{};
	currentFont = &pf;

	static FontSetting previousFont;
	static int previousDPI = 0;
	if(previousFont == font && previousDPI == dpi)
	{
		// Nothing to do
		return;
	}
	previousFont = font;
	previousDPI = dpi;
	DeleteFontData();
	pf.dib = &customFontBitmap;
	pf.dibASCII = nullptr;

	// Upscale built-in font?
	if(builtinFont != nullptr)
	{
		// Copy and scale original 4-bit bitmap
		const MODPLUGDIB &original = *CMainFrame::bmpNotes;
		customFontBitmap.width = original.width * builtInFontSize;
		customFontBitmap.height = original.height * builtInFontSize;
		std::copy(std::begin(original.palette), std::end(original.palette), std::begin(customFontBitmap.palette));
		customFontBitmap.pixels.resize(static_cast<size_t>(customFontBitmap.width) * customFontBitmap.height);
		for(int y = 0; y < customFontBitmap.height; y++)
		{
			const uint8 *sourceRow = &original.pixels[static_cast<size_t>(y / builtInFontSize) * original.width];
			uint8 *targetRow = &customFontBitmap.pixels[static_cast<size_t>(y) * customFontBitmap.width];
			for(int x = 0; x < customFontBitmap.width; x++)
				targetRow[x] = sourceRow[x / builtInFontSize];
		}
		pf.nWidth = (builtinFont->nWidth - 4) * builtInFontSize + 4;
		pf.nHeight = builtinFont->nHeight * builtInFontSize;
		pf.nClrX = builtinFont->nClrX * builtInFontSize;
		pf.nClrY = builtinFont->nClrY * builtInFontSize;
		pf.nSpaceX = builtinFont->nSpaceX * builtInFontSize;
		pf.nSpaceY = builtinFont->nSpaceY * builtInFontSize;
		for(std::size_t i = 0; i < std::size(pf.nEltWidths); i++)
		{
			pf.nEltWidths[i] = builtinFont->nEltWidths[i] * builtInFontSize;
			pf.padding[i] = builtinFont->padding[i] * builtInFontSize;
		}
		pf.nNumX = builtinFont->nNumX * builtInFontSize;
		pf.nNumY = builtinFont->nNumY * builtInFontSize;
		pf.nNum10X = builtinFont->nNum10X * builtInFontSize;
		pf.nNum10Y = builtinFont->nNum10Y * builtInFontSize;
		pf.nAlphaAM_X = builtinFont->nAlphaAM_X * builtInFontSize;
		pf.nAlphaAM_Y = builtinFont->nAlphaAM_Y * builtInFontSize;
		pf.nAlphaNZ_X = builtinFont->nAlphaNZ_X * builtInFontSize;
		pf.nAlphaNZ_Y = builtinFont->nAlphaNZ_Y * builtInFontSize;
		pf.nNoteX = builtinFont->nNoteX * builtInFontSize;
		pf.nNoteY = builtinFont->nNoteY * builtInFontSize;
		pf.nNoteWidth[0] = builtinFont->nNoteWidth[0] * builtInFontSize;
		pf.nNoteWidth[1] = builtinFont->nNoteWidth[1] * builtInFontSize;
		pf.nOctaveWidth = builtinFont->nOctaveWidth * builtInFontSize;
		pf.nVolX = builtinFont->nVolX * builtInFontSize;
		pf.nVolY = builtinFont->nVolY * builtInFontSize;
		pf.nVolCmdWidth = builtinFont->nVolCmdWidth * builtInFontSize;
		pf.nVolHiWidth = builtinFont->nVolHiWidth * builtInFontSize;
		pf.nCmdOfs = builtinFont->nCmdOfs * builtInFontSize;
		pf.nParamHiWidth = builtinFont->nParamHiWidth * builtInFontSize;
		pf.nInstrOfs = builtinFont->nInstrOfs * builtInFontSize;
		pf.nInstr10Ofs = builtinFont->nInstr10Ofs * builtInFontSize;
		pf.nInstrHiWidth = builtinFont->nInstrHiWidth * builtInFontSize;
		pf.pcParamMargin = builtinFont->pcParamMargin * builtInFontSize;
		pf.pcValMargin = builtinFont->pcValMargin * builtInFontSize;
		pf.paramLoMargin = builtinFont->paramLoMargin * builtInFontSize;
		pf.spacingY = builtinFont->spacingY * builtInFontSize;

		// Create 4-pixel border
		for(int y = pf.nClrY; y < pf.nClrY + pf.nHeight; y++)
		{
			uint8 *border = &pf.dib->pixels[static_cast<size_t>(y) * pf.dib->width + pf.nClrX + pf.nWidth - 4];
			border[0] = 0xE; border[1] = 0xC; border[2] = 0xC; border[3] = 0x4;
		}

		return;
	}

	// Create our own font!
	const int fontSize = Util::muldivr(font.size, dpi, 720);
	const ui::Font gdiFont = ui::CreateFont(font.name, fontSize, font.flags[FontSetting::Bold], font.flags[FontSetting::Italic], true);
	int charWidth = 0, charHeight = 0;
	{
		ui::Painter measure;
		measure.SetFont(gdiFont);
		const Size extent = measure.GetTextExtent(UL_("W"));
		charWidth = extent.cx;
		charHeight = extent.cy;
	}
	const int spacing = charWidth / 4;
	const int width = charWidth * 12 + spacing * 2 + 4, height = charHeight * 21;

	pf.nWidth = width;					// Column Width & Height, including 4-pixels border
	pf.nHeight = charHeight;
	pf.nClrX = pf.nClrY = 0;			// Clear (empty note) location
	pf.nSpaceX = charWidth * 7;			// White location (must be big enough)
	pf.nSpaceY = charHeight;
	pf.nEltWidths[0] = charWidth * 3;	// Note
	pf.padding[0] = 0;
	pf.nEltWidths[1] = charWidth * 3 + spacing;	// Instr
	pf.padding[1] = spacing;
	pf.nEltWidths[2] = charWidth * 3 + spacing;	// Volume
	pf.padding[2] = spacing;
	pf.nEltWidths[3] = charWidth;		// Command letter
	pf.padding[3] = 0;
	pf.nEltWidths[4] = charWidth * 2;	// Command param
	pf.padding[4] = 0;
	pf.nNumX = charWidth * 3;			// Vertically-oriented numbers 0x00-0x0F
	pf.nNumY = charHeight;
	pf.nNum10X = charWidth * 4;			// Numbers 10-29
	pf.nNum10Y = charHeight;
	pf.nAlphaAM_X = charWidth * 6;		// Letters A-M +#
	pf.nAlphaAM_Y = charHeight * 2;
	pf.nAlphaNZ_X = charWidth * 7;		// Letters N-Z +?
	pf.nAlphaNZ_Y = charHeight * 2;
	pf.nNoteX = 0;						// Notes ..., C-, C#, ...
	pf.nNoteY = 0;
	pf.nNoteWidth[0] = charWidth;		// Total width of note (C#)
	pf.nNoteWidth[1] = charWidth;		// Total width of note (C#)
	pf.nOctaveWidth = charWidth;		// Octave Width
	pf.nVolX = charWidth * 8;			// Volume Column Effects
	pf.nVolY = charHeight;
	pf.nVolCmdWidth = charWidth;		// Width of volume effect
	pf.nVolHiWidth = charWidth;			// Width of first character in volume parameter
	pf.nCmdOfs = 0;						// XOffset (-xxx) around the command letter
	pf.nParamHiWidth = charWidth;
	pf.nInstrOfs = -charWidth;
	pf.nInstr10Ofs = 0;
	pf.nInstrHiWidth = charWidth * 2;
	pf.pcParamMargin = 0;
	pf.pcValMargin = 0;
	pf.paramLoMargin = 0;				// Margin for second digit of parameter
	pf.spacingY = charHeight;

	{
		ui::OffscreenBuffer buffer(width, height);
		buffer.Begin();
		{
			ui::Painter dc;
			dc.SetFont(gdiFont);
			dc.FillSolidRect(0, 0, width - 4, height, RGB(0xFF, 0xFF, 0xFF));
			dc.SetTextColor(RGB(0x00, 0x00, 0x00));
			dc.SetBkTransparent(true);

		// Empty cells (dots - i-th bit set = dot in the i-th column of a cell)
		const uint8 dots[5] = { 1 | 2 | 4, 2 | 4, 2 | 4, 1, 1 | 2 };
		const auto dotStr = TrackerSettings::Instance().patternFontDot.Get();
		auto dotChar = dotStr.empty() ? UC_(' ') : dotStr[0];
		for(int cell = 0, offset = 0; cell < static_cast<int>(std::size(dots)); cell++)
		{
			uint8 dot = dots[cell];
			for(int i = 0; dot != 0; i++)
			{
				if(dot & 1) DrawChar(dc, dotChar, pf.nClrX + offset + i * charWidth, pf.nClrY, charWidth, charHeight);
				dot >>= 1;
			}
			offset += pf.nEltWidths[cell];
		}

		// Notes
		for(int i = 0; i < 12; i++)
		{
			DrawString(dc, NoteNamesSharp[i], 2, pf.nNoteX, pf.nNoteY + (i + 1) * charHeight, charWidth, charHeight);
		}
		DrawString(dc, "^^", 2, pf.nNoteX, pf.nNoteY + 13 * charHeight, charWidth, charHeight);
		DrawString(dc, "==", 2, pf.nNoteX, pf.nNoteY + 14 * charHeight, charWidth, charHeight);
		DrawString(dc, "PC", 2, pf.nNoteX, pf.nNoteY + 15 * charHeight, charWidth, charHeight);
		DrawString(dc, "PCs", 3, pf.nNoteX, pf.nNoteY + 16 * charHeight, charWidth, charHeight);
		DrawString(dc, "~~", 2, pf.nNoteX, pf.nNoteY + 17 * charHeight, charWidth, charHeight);

		// Hex chars
		const char hexChars[] = "0123456789ABCDEF";
		for(int i = 0; i < 16; i++)
		{
			DrawChar(dc, hexChars[i], pf.nNumX, pf.nNumY + i * charHeight, charWidth, charHeight);
		}
		// Double-digit numbers
		for(int i = 0; i < 20; i++)
		{
			char s[2];
			s[0] = char('1' + i / 10);
			s[1] = char('0' + i % 10);
			DrawString(dc, s, 2, pf.nNum10X, pf.nNum10Y + i * charHeight, charWidth, charHeight);
		}

		// Volume commands
		const char volEffects[]= " vpcdabuhlrgfe:o";
		static_assert(mpt::array_size<decltype(volEffects)>::size - 1 == MAX_VOLCMDS);
		for(int i = 0; i < MAX_VOLCMDS; i++)
		{
			DrawChar(dc, volEffects[i], pf.nVolX, pf.nVolY + i * charHeight, charWidth, charHeight);
		}

		// Letters A-Z
		for(int i = 0; i < 13; i++)
		{
			DrawChar(dc, char('A' + i), pf.nAlphaAM_X, pf.nAlphaAM_Y + i * charHeight, charWidth, charHeight);
			DrawChar(dc, char('N' + i), pf.nAlphaNZ_X, pf.nAlphaNZ_Y + i * charHeight, charWidth, charHeight);
		}
		// Special chars
		DrawChar(dc, '#', pf.nAlphaAM_X, pf.nAlphaAM_Y + 13 * charHeight, charWidth, charHeight);
		DrawChar(dc, '?', pf.nAlphaNZ_X, pf.nAlphaNZ_Y + 13 * charHeight, charWidth, charHeight);
		DrawChar(dc, 'b', pf.nAlphaAM_X, pf.nAlphaAM_Y + 14 * charHeight, charWidth, charHeight);
		DrawChar(dc, '\\', pf.nAlphaNZ_X, pf.nAlphaNZ_Y + 14 * charHeight, charWidth, charHeight);
		DrawChar(dc, '-', pf.nAlphaAM_X, pf.nAlphaAM_Y + 15 * charHeight, charWidth, charHeight);
		DrawChar(dc, ':', pf.nAlphaNZ_X, pf.nAlphaNZ_Y + 15 * charHeight, charWidth, charHeight);
		DrawChar(dc, '+', pf.nAlphaAM_X, pf.nAlphaAM_Y + 16 * charHeight, charWidth, charHeight);
		DrawChar(dc, '*', pf.nAlphaNZ_X, pf.nAlphaNZ_Y + 16 * charHeight, charWidth, charHeight);
		DrawChar(dc, 'd', pf.nAlphaAM_X, pf.nAlphaAM_Y + 17 * charHeight, charWidth, charHeight);


		}
		const ui::Bitmap rendered = buffer.ReadPixels();
		buffer.End();
		BitmapToDib(rendered, (width + 7) & ~7, customFontBitmap);
		// Create 4-pixel border
		for(int y = 0; y < height; y++)
		{
			uint8 *border = &customFontBitmap.pixels[static_cast<size_t>(y) * customFontBitmap.width + width - 4];
			border[0] = 0xE; border[1] = 0xC; border[2] = 0xC; border[3] = 0x4;
		}
	}

	{
		pf.dibASCII = &customFontBitmapASCII;
		const int asciiWidth = (charWidth * 128 + 7) & ~7;
		ui::OffscreenBuffer buffer(asciiWidth, charHeight);
		buffer.Begin();
		{
			ui::Painter dc;
			dc.SetFont(gdiFont);
			dc.FillSolidRect(0, 0, asciiWidth, charHeight, RGB(0xFF, 0xFF, 0xFF));
			dc.SetTextColor(RGB(0x00, 0x00, 0x00));
			dc.SetBkTransparent(true);
			for(uint32 c = 32; c < 128; ++c)
			{
				DrawChar(dc, c, charWidth * c, 0, charWidth, charHeight);
			}
		}
		const ui::Bitmap rendered = buffer.ReadPixels();
		buffer.End();
		BitmapToDib(rendered, asciiWidth, customFontBitmapASCII);
	}
}


void PatternFont::DeleteFontData()
{
	customFontBitmap.pixels.clear();
	customFontBitmapASCII.pixels.clear();
}

OPENMPT_NAMESPACE_END
