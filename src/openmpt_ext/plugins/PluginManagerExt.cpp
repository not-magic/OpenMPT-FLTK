// Members of openmpt/soundlib/plugins/PluginManager.cpp that libopenmpt declares but only defines
// in the tracker build. External (VST) plugins are not supported.

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
