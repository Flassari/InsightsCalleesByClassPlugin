// Copyright Epic Games, Inc. All Rights Reserved.

#include "CalleeByClassExtender.h"

#include "Features/IModularFeatures.h"
#include "Insights/ITimingViewExtender.h"
#include "Insights/IUnrealInsightsModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "InsightsCalleeByClass"

class FInsightsCalleeByClassModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Extender = MakeUnique<FCalleeByClassExtender>();

		// Register as a timing view extender to receive session lifecycle events
		// and timing event / selection change notifications.
		IModularFeatures::Get().RegisterModularFeature(
			UE::Insights::Timing::TimingViewExtenderFeatureName, Extender.Get());

		// Register the minor tab inside the Timing Profiler major tab window.
		// Use LoadModuleChecked (not IsModuleLoaded) so the module is loaded if not yet,
		// matching the pattern used by all other Insights plugins.
		IUnrealInsightsModule& InsightsModule =
			FModuleManager::LoadModuleChecked<IUnrealInsightsModule>("TraceInsights");

		ExtensionDelegateHandle =
			InsightsModule.OnRegisterMajorTabExtension(FInsightsManagerTabs::TimingProfilerTabId)
			.AddRaw(Extender.Get(), &FCalleeByClassExtender::RegisterTimingProfilerLayoutExtensions);

		// Force-invoke the tab when the Timing Profiler major tab is created and
		// reposition it right after "Callees" in the workspace menu.
		FCalleeByClassExtender* ExtenderRaw = Extender.Get();
		MajorTabCreatedHandle =
			InsightsModule.OnMajorTabCreated().AddLambda(
				[ExtenderRaw](const FName& InMajorTabId, TSharedRef<FTabManager> InTabManager)
				{
					if (InMajorTabId == FInsightsManagerTabs::TimingProfilerTabId)
					{
						ExtenderRaw->InvokeAndReorder(InTabManager);
					}
				});
	}

	virtual void ShutdownModule() override
	{
		if (IUnrealInsightsModule* InsightsModule = FModuleManager::GetModulePtr<IUnrealInsightsModule>("TraceInsights"))
		{
			FOnRegisterMajorTabExtensions* Delegate =
				InsightsModule->FindMajorTabLayoutExtension(FInsightsManagerTabs::TimingProfilerTabId);
			if (Delegate)
			{
				Delegate->Remove(ExtensionDelegateHandle);
			}

			InsightsModule->OnMajorTabCreated().Remove(MajorTabCreatedHandle);
		}
		ExtensionDelegateHandle.Reset();
		MajorTabCreatedHandle.Reset();

		if (Extender.IsValid())
		{
			IModularFeatures::Get().UnregisterModularFeature(
				UE::Insights::Timing::TimingViewExtenderFeatureName, Extender.Get());
			Extender.Reset();
		}
	}

private:
	TUniquePtr<FCalleeByClassExtender> Extender;
	FDelegateHandle ExtensionDelegateHandle;
	FDelegateHandle MajorTabCreatedHandle;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FInsightsCalleeByClassModule, InsightsCalleeByClass)
