// MFC replacement on FLTK. Dialogs for choosing a colour or a font.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../Geometry.h"
#include "../UiTypes.h"

OPENMPT_NAMESPACE_BEGIN


namespace ui
{

// Lets the user pick a colour. Returns false if the dialog was cancelled.
bool ChooseColor(ColorRef &color, const mpt::ustring &title = {});


// Lets the user pick one of the installed fonts
class FontDialog
{
public:
	// The size is given in tenths of a point
	FontDialog(const mpt::ustring &faceName, int32 size, bool isBold, bool isItalic);

	// Returns IDOK or IDCANCEL
	intptr_t DoModal();

	mpt::ustring GetFaceName() const { return m_faceName; }
	int32 GetSize() const noexcept { return m_size; }
	bool IsBold() const noexcept { return m_isBold; }
	bool IsItalic() const noexcept { return m_isItalic; }

private:
	mpt::ustring m_faceName;
	int32 m_size;
	bool m_isBold;
	bool m_isItalic;
};

}  // namespace ui


OPENMPT_NAMESPACE_END
