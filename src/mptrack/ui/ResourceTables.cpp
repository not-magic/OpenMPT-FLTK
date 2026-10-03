/*
 * ResourceTables.cpp
 * ------------------
 * Purpose: Lookup of the generated static resources.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ResourceTables.h"


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


const BinaryResource *FindBinaryResource(uint32 id)
{
	for(std::size_t i = 0; i < binaryResourceCount; ++i)
	{
		if(binaryResources[i].id == id)
			return &binaryResources[i];
	}
	return nullptr;
}


const char *FindStringResource(uint32 id)
{
	for(std::size_t i = 0; i < stringResourceCount; ++i)
	{
		if(stringResources[i].id == id)
			return stringResources[i].text;
	}
	return nullptr;
}


const DialogTemplate *FindDialogTemplate(uint32 id)
{
	for(std::size_t i = 0; i < dialogTemplateCount; ++i)
	{
		if(dialogTemplates[i].id == id)
			return &dialogTemplates[i];
	}
	return nullptr;
}


const MenuTemplate *FindMenuTemplate(uint32 id)
{
	for(std::size_t i = 0; i < menuTemplateCount; ++i)
	{
		if(menuTemplates[i].id == id)
			return &menuTemplates[i];
	}
	return nullptr;
}


}  // namespace ui


OPENMPT_NAMESPACE_END
