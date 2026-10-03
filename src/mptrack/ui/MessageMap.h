/*
 * MessageMap.h
 * ------------
 * Purpose: Per-class tables that route commands, notifications and user messages to member functions.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../UiTypes.h"

#include <functional>
#include <type_traits>
#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

class CommandTarget;
class Wnd;


// User-defined message ranges
constexpr uint32 MsgUser = 0x0400;
constexpr uint32 MsgApp = 0x8000;


// Handle passed to command-state updaters to describe how a command is presented
class CmdUI
{
public:
	virtual ~CmdUI() = default;
	virtual void Enable(bool isEnabled = true) = 0;
	virtual void SetCheck(int state = 1) = 0;
	virtual void SetRadio(bool isOn = true) = 0;
	virtual void SetText(const mpt::ustring &text) = 0;

	uint32 id = 0;
};


// ID matched by UI_NOTIFY_REFLECT entries, which handle the notifications a control sends itself
constexpr uint32 NotifyReflectId = 0xFFFFFF00;

// Identifies the sender of a control notification
struct NotifyHeader
{
	Wnd *from = nullptr;
	uint32 id = 0;
	uint32 code = 0;
	void *extra = nullptr;
};


struct MessageArgs
{
	WParam wParam = 0;
	LParam lParam = 0;
};


enum class MapKind : uint8
{
	Command,
	CommandRange,
	Notify,
	NotifyRange,
	UpdateUi,
	Message,
};

struct MapEntry
{
	MapKind kind = MapKind::Command;
	uint32 id = 0;
	uint32 idLast = 0;
	uint32 code = 0;  // notification code or message ID
	// Returns true if handled; result receives the message / notification result
	std::function<bool(CommandTarget *, uint32 id, void *extra, LResult *result)> call;
};


class MessageMap
{
public:
	explicit MessageMap(const MessageMap *base) : m_base(base) { }

	template <typename T, typename Fn>
	void AddCommand(uint32 id, Fn fn)
	{
		Add<T>(MapKind::Command, id, id, 0, fn);
	}
	template <typename T, typename Fn>
	void AddCommandRange(uint32 id, uint32 idLast, Fn fn)
	{
		Add<T>(MapKind::CommandRange, id, idLast, 0, fn);
	}
	template <typename T, typename Fn>
	void AddNotify(uint32 id, uint32 code, Fn fn)
	{
		Add<T>(MapKind::Notify, id, id, code, fn);
	}
	template <typename T, typename Fn>
	void AddNotifyRange(uint32 id, uint32 idLast, uint32 code, Fn fn)
	{
		Add<T>(MapKind::NotifyRange, id, idLast, code, fn);
	}
	template <typename T, typename Fn>
	void AddUpdateUi(uint32 id, Fn fn)
	{
		Add<T>(MapKind::UpdateUi, id, id, 0, fn);
	}
	template <typename T, typename Fn>
	void AddMessage(uint32 message, Fn fn)
	{
		Add<T>(MapKind::Message, message, message, message, fn);
	}

	const MessageMap *GetBase() const noexcept { return m_base; }
	const std::vector<MapEntry> &GetEntries() const noexcept { return m_entries; }

private:
	template <typename T, typename Fn>
	void Add(MapKind kind, uint32 id, uint32 idLast, uint32 code, Fn fn)
	{
		MapEntry entry;
		entry.kind = kind;
		entry.id = id;
		entry.idLast = idLast;
		entry.code = code;
		entry.call = [fn](CommandTarget *target, uint32 senderId, void *extra, LResult *result) -> bool
		{
			T *self = static_cast<T *>(target);
			if constexpr(std::is_invocable_v<Fn, T *, WParam, LParam>)
			{
				const MessageArgs *args = static_cast<const MessageArgs *>(extra);
				const LResult value = (self->*fn)(args->wParam, args->lParam);
				if(result)
					*result = value;
			} else if constexpr(std::is_invocable_v<Fn, T *, CmdUI *>)
			{
				(self->*fn)(static_cast<CmdUI *>(extra));
			} else if constexpr(std::is_invocable_v<Fn, T *, NotifyHeader *, LResult *>)
			{
				LResult dummy = 0;
				(self->*fn)(static_cast<NotifyHeader *>(extra), result ? result : &dummy);
			} else if constexpr(std::is_invocable_v<Fn, T *, uint32>)
			{
				(self->*fn)(senderId);
			} else
			{
				(self->*fn)();
			}
			return true;
		};
		m_entries.push_back(std::move(entry));
	}

	const MessageMap *m_base;
	std::vector<MapEntry> m_entries;
};


}  // namespace ui


OPENMPT_NAMESPACE_END


#define UI_DECLARE_MESSAGE_MAP() \
protected: \
	static const ui::MessageMap messageMap; \
	const ui::MessageMap *GetMessageMap() const override;

#define UI_MESSAGE_MAP_BEGIN(theClass, baseClass) \
	const ui::MessageMap *theClass::GetMessageMap() const { return &messageMap; } \
	const ui::MessageMap theClass::messageMap = []() -> ui::MessageMap { \
		using ThisClass = theClass; \
		ui::MessageMap map(&baseClass::messageMap);

#define UI_MESSAGE_MAP_END() \
		return map; \
	}();

#define UI_COMMAND(id, fn) map.AddCommand<ThisClass>(id, fn);
#define UI_COMMAND_RANGE(id, idLast, fn) map.AddCommandRange<ThisClass>(id, idLast, fn);
#define UI_NOTIFY(code, id, fn) map.AddNotify<ThisClass>(id, code, fn);
#define UI_NOTIFY_REFLECT(code, fn) map.AddNotify<ThisClass>(ui::NotifyReflectId, code, fn);
#define UI_NOTIFY_RANGE(code, id, idLast, fn) map.AddNotifyRange<ThisClass>(id, idLast, code, fn);
#define UI_UPDATE_COMMAND(id, fn) map.AddUpdateUi<ThisClass>(id, fn);
#define UI_MESSAGE(message, fn) map.AddMessage<ThisClass>(message, fn);
