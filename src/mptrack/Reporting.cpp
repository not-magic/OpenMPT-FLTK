/*
 * Reporting.cpp
 * -------------
 * Purpose: A class for showing notifications, prompts, etc...
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/Reporting.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "Reporting.h"
#include <FL/fl_ask.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


namespace
{

enum class MessageKind
{
	Plain,
	Information,
	Warning,
	Error,
	Confirm,
	ConfirmCancel,
	RetryCancel,
};

constexpr MessageKind LogLevelToKind(LogLevel level)
{
	switch(level)
	{
	case LogDebug: return MessageKind::Plain;
	case LogNotification: return MessageKind::Plain;
	case LogInformation: return MessageKind::Information;
	case LogWarning: return MessageKind::Warning;
	case LogError: return MessageKind::Error;
	}
	return MessageKind::Plain;
}


mpt::ustring GetTitle()
{
	return UL_("Open ModPlug Tracker");
}


mpt::ustring FillEmptyCaption(const mpt::ustring &caption, LogLevel level)
{
	if(!caption.empty())
		return caption;
	mpt::ustring result = GetTitle() + UL_(" - ");
	switch(level)
	{
	case LogDebug: result += UL_("Debug"); break;
	case LogNotification: result += UL_("Notification"); break;
	case LogInformation: result += UL_("Information"); break;
	case LogWarning: result += UL_("Warning"); break;
	case LogError: result += UL_("Error"); break;
	}
	return result;
}


mpt::ustring FillEmptyCaption(const mpt::ustring &caption)
{
	return caption.empty() ? GetTitle() : caption;
}


// Returns the index of the pressed button, in the order the buttons are listed for the message kind
int ShowMessageImpl(const mpt::ustring &text, const mpt::ustring &caption, MessageKind kind)
{
	const std::string title = mpt::transcode<std::string>(mpt::common_encoding::utf8, caption.empty() ? GetTitle() : caption);
	std::string body = mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
	body.erase(std::remove(body.begin(), body.end(), '\r'), body.end());
	fl_message_title(title.c_str());
	switch(kind)
	{
	case MessageKind::Plain:
	case MessageKind::Information:
		fl_message("%s", body.c_str());
		return 0;
	case MessageKind::Warning:
	case MessageKind::Error:
		fl_alert("%s", body.c_str());
		return 0;
	case MessageKind::Confirm:
		return fl_choice("%s", "Yes", "No", nullptr, body.c_str());
	case MessageKind::ConfirmCancel:
		return fl_choice("%s", "Yes", "No", "Cancel", body.c_str());
	case MessageKind::RetryCancel:
		return fl_choice("%s", "Retry", "Cancel", nullptr, body.c_str());
	}
	return 0;
}

}  // namespace


void Reporting::Notification(const AnyStringLocale &text, const ui::Wnd *parent)
{
	ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ustring(), LogNotification), LogLevelToKind(LogNotification));
}
void Reporting::Notification(const AnyStringLocale &text, const AnyStringLocale &caption, const ui::Wnd *parent)
{
	ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption), LogNotification), LogLevelToKind(LogNotification));
}


void Reporting::Information(const AnyStringLocale &text, const ui::Wnd *parent)
{
	ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ustring(), LogInformation), LogLevelToKind(LogInformation));
}
void Reporting::Information(const AnyStringLocale &text, const AnyStringLocale &caption, const ui::Wnd *parent)
{
	ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption), LogInformation), LogLevelToKind(LogInformation));
}


void Reporting::Warning(const AnyStringLocale &text, const ui::Wnd *parent)
{
	ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ustring(), LogWarning), LogLevelToKind(LogWarning));
}
void Reporting::Warning(const AnyStringLocale &text, const AnyStringLocale &caption, const ui::Wnd *parent)
{
	ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption), LogWarning), LogLevelToKind(LogWarning));
}


void Reporting::Error(const AnyStringLocale &text, const ui::Wnd *parent)
{
	ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ustring(), LogError), LogLevelToKind(LogError));
}
void Reporting::Error(const AnyStringLocale &text, const AnyStringLocale &caption, const ui::Wnd *parent)
{
	ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption), LogError), LogLevelToKind(LogError));
}


void Reporting::Message(LogLevel level, const AnyStringLocale &text, const ui::Wnd *parent)
{
	ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ustring(), level), LogLevelToKind(level));
}
void Reporting::Message(LogLevel level, const AnyStringLocale &text, const AnyStringLocale &caption, const ui::Wnd *parent)
{
	ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption), level), LogLevelToKind(level));
}


ConfirmAnswer Reporting::Confirm(const AnyStringLocale &text, bool showCancel, bool defaultNo, const ui::Wnd *parent)
{
	return Confirm(mpt::ToUnicode(text), GetTitle() + UL_(" - Confirmation"), showCancel, defaultNo, parent);
}


ConfirmAnswer Reporting::Confirm(const AnyStringLocale &text, const AnyStringLocale &caption, bool showCancel, bool defaultNo, const ui::Wnd *parent)
{
	MPT_UNUSED(parent);
	MPT_UNUSED(defaultNo);
	const int result = ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption)), showCancel ? MessageKind::ConfirmCancel : MessageKind::Confirm);
	switch(result)
	{
	case 0:
		return cnfYes;
	case 1:
		return cnfNo;
	default:
		return cnfCancel;
	}
}


RetryAnswer Reporting::RetryCancel(const AnyStringLocale &text, const ui::Wnd *parent)
{
	return RetryCancel(mpt::ToUnicode(text), GetTitle(), parent);
}


RetryAnswer Reporting::RetryCancel(const AnyStringLocale &text, const AnyStringLocale &caption, const ui::Wnd *parent)
{
	MPT_UNUSED(parent);
	const int result = ShowMessageImpl(mpt::ToUnicode(text), FillEmptyCaption(mpt::ToUnicode(caption)), MessageKind::RetryCancel);
	return result == 0 ? rtyRetry : rtyCancel;
}


OPENMPT_NAMESPACE_END
