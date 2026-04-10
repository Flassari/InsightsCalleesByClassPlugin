#pragma once

#include "CoreMinimal.h"

#include "Insights/ITimingViewExtender.h"
#include "Insights/ITimingViewSession.h"

class SCalleesByClassView;
struct FInsightsMajorTabExtender;
class FSpawnTabArgs;
class SDockTab;
class FTabManager;
class FWorkspaceItem;

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Implements ITimingViewExtender to:
 *   1) Subscribe to timing event selection and time range selection in the Timing View.
 *   2) When either changes, query ITimingProfilerProvider::CreateButterfly() for the
 *      direct callees of the selected timer and push aggregated class-level data to
 *      the SCalleesByClassView tab widget.
 *
 * Tab registration is handled externally: the module calls
 * RegisterTimingProfilerLayoutExtensions() when the Timing Profiler major tab is being built.
 *
 * NOTE: Only timing events clicked in the Timing View drive this tab (not selections made
 * in the Timers list), because timer-list selection has no public engine delegate.
 */
class FCalleeByClassExtender : public UE::Insights::Timing::ITimingViewExtender
{
public:
	/** Tab ID used to spawn/invoke the minor tab. */
	static const FName TabId;

	FCalleeByClassExtender();
	virtual ~FCalleeByClassExtender();

	/** Called by the module to wire up the minor tab in the Timing Profiler window. */
	void RegisterTimingProfilerLayoutExtensions(FInsightsMajorTabExtender& InOutExtender);

	/** Invokes the tab and repositions it right after "Callees" in the workspace menu. */
	void InvokeAndReorder(TSharedRef<FTabManager> InTabManager);

	// ~Begin ITimingViewExtender
	virtual void OnBeginSession(UE::Insights::Timing::ITimingViewSession& InSession) override;
	virtual void OnEndSession(UE::Insights::Timing::ITimingViewSession& InSession) override;
	virtual void Tick(const UE::Insights::Timing::FTimingViewExtenderTickParams& Params) override;
	// ~End ITimingViewExtender

private:
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	void OnSelectedEventChanged(const TSharedPtr<const ITimingEvent> InSelectedEvent);
	void OnSelectionChanged(UE::Insights::Timing::ETimeChangedFlags Flags, double StartTime, double EndTime);

	void RefreshView(const TraceServices::IAnalysisSession& InAnalysisSession);

	// Session (non-owning)
	UE::Insights::Timing::ITimingViewSession* Session = nullptr;

	FDelegateHandle SelectedEventHandle;
	FDelegateHandle SelectionChangedHandle;

	// Current state
	static constexpr uint32 InvalidTimerId = uint32(-1);
	uint32 SelectedTimerId    = InvalidTimerId;
	double SelectionStartTime = 0.0;
	double SelectionEndTime   = 0.0;

	// Dirty flag — set when timer or range changes, cleared after refresh
	bool  bNeedsRefresh     = false;
	// Throttle: seconds since last refresh
	float TimeSinceRefresh  = 0.0f;
	static constexpr float RefreshCooldownSec = 0.25f;

	// Weak reference to the spawned tab widget
	TWeakPtr<SCalleesByClassView> WeakView;

	// Workspace group for menu ordering
	TWeakPtr<FWorkspaceItem> WeakWorkspaceGroup;
};
