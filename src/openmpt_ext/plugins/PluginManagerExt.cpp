/*
 * PluginManagerExt.cpp
 * --------------------
 * Purpose: Plugin library management that libopenmpt declares but only defines in the tracker build.
 * Notes  : Ported from upstream's PluginManager.cpp. External (VST) plugins are not supported, so
 *          AddPlugin() never finds a plugin and the plugin cache is not written.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "../soundlib/plugins/PluginManager.h"

#include "AbstractVstEditor.h"
#include "PluginUi.h"
#include "openmpt_ext/sndlib/TrackerCriticalSection.h"
#include "../soundlib/plugins/PlugInterface.h"

OPENMPT_NAMESPACE_BEGIN

void VSTPluginLib::WriteToCache() const
{
}


std::vector<VSTPluginLib *> CVstPluginManager::AddPlugin(const mpt::PathString &dllPath, bool, bool, bool *fileFound, uint32)
{
	if(fileFound != nullptr)
		*fileFound = FileSystem::IsFile(dllPath);
	return {};
}


bool CVstPluginManager::RemovePlugin(VSTPluginLib *pFactory)
{
	for(const_iterator p = begin(); p != end(); ++p)
	{
		VSTPluginLib *plug = p->get();
		if(plug != pFactory)
			continue;

		TrackerCriticalSection cs;
		while(plug->pPluginsList != nullptr)
		{
			IMixPlugin *pluginInstance = plug->pPluginsList;
			PluginUi(*pluginInstance).CloseEditor();
			plug->RemovePluginInstanceFromList(*pluginInstance);
			pluginInstance->Release();
		}
		pluginList.erase(p);
		return true;
	}
	return false;
}


void CVstPluginManager::OnIdle()
{
	for(auto &factory : pluginList)
	{
		for(IMixPlugin *plugin = factory->pPluginsList; plugin != nullptr; plugin = plugin->GetNextInstance())
		{
			plugin->Idle();
			if(CAbstractVstEditor *editor = PluginUi(*plugin).GetEditor(); editor && editor->IsWindow())
				editor->UpdateParamDisplays();
		}
	}
}

OPENMPT_NAMESPACE_END
