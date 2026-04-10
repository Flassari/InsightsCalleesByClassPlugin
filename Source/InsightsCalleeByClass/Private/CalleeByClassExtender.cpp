#include "CalleeByClassExtender.h"

#include "SCalleesByClassView.h"

#include "Framework/Docking/LayoutExtender.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Textures/SlateIcon.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

// TraceInsights
#include "Insights/IUnrealInsightsModule.h"
#include "Insights/ViewModels/ITimingEvent.h"
#include "Insights/ViewModels/ThreadTrackEvent.h"

// TraceServices
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/TimingProfiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogCalleeByClass, Log, All);

const FName FCalleeByClassExtender::TabId("InsightsCalleeByClass");

#define LOCTEXT_NAMESPACE "InsightsCalleeByClass"

////////////////////////////////////////////////////////////////////////////////////////////////////

FCalleeByClassExtender::FCalleeByClassExtender()
{
}

FCalleeByClassExtender::~FCalleeByClassExtender()
{
	// Delegates are unbound in OnEndSession; nothing extra needed here.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tab registration
////////////////////////////////////////////////////////////////////////////////////////////////////

void FCalleeByClassExtender::RegisterTimingProfilerLayoutExtensions(FInsightsMajorTabExtender& InOutExtender)
{
	FMinorTabConfig& TabConfig = InOutExtender.AddMinorTabConfig();
	TabConfig.TabId          = FCalleeByClassExtender::TabId;
	TabConfig.TabLabel       = LOCTEXT("TabLabel",   "Callees by Class");
	TabConfig.TabTooltip     = LOCTEXT("TabTooltip", "Groups the direct callees of the selected timer by class name, "
	                                                  "showing aggregated inclusive/exclusive time per class.");
	TabConfig.TabIcon        = FSlateIcon(FName("InsightsStyle"), "Icons.CalleesView");
	TabConfig.WorkspaceGroup = InOutExtender.GetWorkspaceGroup();
	TabConfig.OnSpawnTab     = FOnSpawnTab::CreateRaw(this, &FCalleeByClassExtender::SpawnTab);
	WeakWorkspaceGroup       = InOutExtender.GetWorkspaceGroup();

	// Place the tab in the default layout immediately after the Callees tab.
	InOutExtender.GetLayoutExtender().ExtendLayout(
		FTimingProfilerTabs::CalleesID,
		ELayoutExtensionPosition::After,
		FTabManager::FTab(FCalleeByClassExtender::TabId, ETabState::OpenedTab)
	);
}

TSharedRef<SDockTab> FCalleeByClassExtender::SpawnTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SCalleesByClassView> ViewWidget = SNew(SCalleesByClassView);
	WeakView = ViewWidget;

	return SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			ViewWidget
		];
}

void FCalleeByClassExtender::InvokeAndReorder(TSharedRef<FTabManager> InTabManager)
{
	InTabManager->TryInvokeTab(TabId);

	// Reposition our entry immediately after "Callees" in the workspace menu.
	// Items are registered in order, so ours ends up last; we fix that here.
	TSharedPtr<FWorkspaceItem> Group = WeakWorkspaceGroup.Pin();
	TSharedPtr<FTabSpawnerEntry> OurSpawner = InTabManager->FindTabSpawnerFor(TabId);
	if (!Group.IsValid() || !OurSpawner.IsValid())
	{
		return;
	}

	const TArray<TSharedRef<FWorkspaceItem>>& Children = Group->GetChildItems();

	// Upcast to FWorkspaceItem so we can compare against the children array.
	TSharedRef<FWorkspaceItem> OurEntry = OurSpawner.ToSharedRef();

	// Find the index of the "Callees" entry.
	int32 CalleesIdx = INDEX_NONE;
	for (int32 i = 0; i < Children.Num(); ++i)
	{
		if (Children[i]->GetFName() == FName("Callees"))
		{
			CalleesIdx = i;
			break;
		}
	}
	if (CalleesIdx == INDEX_NONE)
	{
		return;
	}

	// Collect all entries that follow "Callees" (excluding our own, wherever it sits).
	TArray<TSharedRef<FWorkspaceItem>> ToReinsert;
	for (int32 i = CalleesIdx + 1; i < Children.Num(); ++i)
	{
		if (Children[i] != OurEntry)
		{
			ToReinsert.Add(Children[i]);
		}
	}

	// Remove our entry and all entries that come after "Callees".
	Group->RemoveItem(OurEntry);
	for (TSharedRef<FWorkspaceItem>& Item : ToReinsert)
	{
		Group->RemoveItem(Item);
	}

	// Re-add: ours first (right after "Callees"), then the rest in original order.
	Group->AddItem(OurEntry);
	for (TSharedRef<FWorkspaceItem>& Item : ToReinsert)
	{
		Group->AddItem(Item);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ITimingViewExtender
////////////////////////////////////////////////////////////////////////////////////////////////////

void FCalleeByClassExtender::OnBeginSession(UE::Insights::Timing::ITimingViewSession& InSession)
{
	if (InSession.GetName() != FInsightsManagerTabs::TimingProfilerTabId)
	{
		return;
	}

	Session = &InSession;

	SelectedEventHandle = InSession.OnSelectedEventChanged().AddRaw(
		this, &FCalleeByClassExtender::OnSelectedEventChanged);

	SelectionChangedHandle = InSession.OnSelectionChanged().AddRaw(
		this, &FCalleeByClassExtender::OnSelectionChanged);
}

void FCalleeByClassExtender::OnEndSession(UE::Insights::Timing::ITimingViewSession& InSession)
{
	if (InSession.GetName() != FInsightsManagerTabs::TimingProfilerTabId)
	{
		return;
	}

	InSession.OnSelectedEventChanged().Remove(SelectedEventHandle);
	InSession.OnSelectionChanged().Remove(SelectionChangedHandle);
	SelectedEventHandle.Reset();
	SelectionChangedHandle.Reset();

	Session            = nullptr;
	SelectedTimerId    = InvalidTimerId;
	SelectionStartTime = 0.0;
	SelectionEndTime   = 0.0;
	bNeedsRefresh      = false;

	if (TSharedPtr<SCalleesByClassView> View = WeakView.Pin())
	{
		View->SetShowHint(true);
	}
}

void FCalleeByClassExtender::Tick(const UE::Insights::Timing::FTimingViewExtenderTickParams& Params)
{
	if (!bNeedsRefresh)
	{
		return;
	}

	TimeSinceRefresh += Params.DeltaTime;
	if (TimeSinceRefresh < RefreshCooldownSec)
	{
		return;
	}
	TimeSinceRefresh = 0.0f;
	bNeedsRefresh    = false;

	if (SelectedTimerId == InvalidTimerId || SelectionStartTime >= SelectionEndTime)
	{
		if (TSharedPtr<SCalleesByClassView> View = WeakView.Pin())
		{
			View->SetShowHint(true);
		}
		return;
	}

	if (Params.AnalysisSession)
	{
		RefreshView(*Params.AnalysisSession);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Delegate callbacks
////////////////////////////////////////////////////////////////////////////////////////////////////

void FCalleeByClassExtender::OnSelectedEventChanged(const TSharedPtr<const ITimingEvent> InSelectedEvent)
{
	if (InSelectedEvent.IsValid() && InSelectedEvent->Is<FThreadTrackEvent>())
	{
		const FThreadTrackEvent& TrackEvent = InSelectedEvent->As<FThreadTrackEvent>();
		const uint32 NewTimerId = TrackEvent.GetTimerId();
		if (NewTimerId != SelectedTimerId)
		{
			SelectedTimerId  = NewTimerId;
			bNeedsRefresh    = true;
			TimeSinceRefresh = 0.0f;
		}
	}
}

void FCalleeByClassExtender::OnSelectionChanged(UE::Insights::Timing::ETimeChangedFlags /*Flags*/, double StartTime, double EndTime)
{
	if (StartTime != SelectionStartTime || EndTime != SelectionEndTime)
	{
		SelectionStartTime = StartTime;
		SelectionEndTime   = EndTime;
		bNeedsRefresh      = true;
		TimeSinceRefresh   = 0.0f;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Data refresh
////////////////////////////////////////////////////////////////////////////////////////////////////

void FCalleeByClassExtender::RefreshView(const TraceServices::IAnalysisSession& InAnalysisSession)
{
	TSharedPtr<SCalleesByClassView> View = WeakView.Pin();
	if (!View.IsValid())
	{
		return;
	}

	TraceServices::FAnalysisSessionReadScope ReadScope(InAnalysisSession);

	const TraceServices::ITimingProfilerProvider* Provider = TraceServices::ReadTimingProfilerProvider(InAnalysisSession);
	if (!Provider)
	{
		View->SetShowHint(true);
		return;
	}

	// Build the butterfly (callee tree) for the current time range.
	TraceServices::FCreateButterflyParams Params;
	Params.IntervalStart  = SelectionStartTime;
	Params.IntervalEnd    = SelectionEndTime;
	// Include all CPU threads; no GPU filter needed for callee grouping.
	Params.CpuThreadFilter = [](uint32) { return true; };

	TUniquePtr<TraceServices::ITimingProfilerButterfly> Butterfly(Provider->CreateButterfly(Params));
	if (!Butterfly)
	{
		View->SetShowHint(true);
		return;
	}

	const TraceServices::FTimingProfilerButterflyNode& Root = Butterfly->GenerateCalleesTree(SelectedTimerId);

	// Resolve the selected timer's display name for the root row.
	FString TimerName = TEXT("(unknown)");
	if (Root.Timer && Root.Timer->Name)
	{
		TimerName = FString(Root.Timer->Name);
	}

	// Aggregate direct callees (depth 1) by class name.
	// Timer names use the format "ClassName/FunctionName"; split on the first '/'.
	TMap<FString, TSharedPtr<FCalleeRow>> Groups;

	for (const TraceServices::FTimingProfilerButterflyNode* Callee : Root.Children)
	{
		if (!Callee || !Callee->Timer)
		{
			continue;
		}

		const FString FullName = Callee->Timer->Name ? FString(Callee->Timer->Name) : TEXT("(unknown)");

		// Split on the first '/' to get class vs function.
		int32 SlashIdx = INDEX_NONE;
		FullName.FindChar(TEXT('/'), SlashIdx);

		const FString ClassName    = (SlashIdx != INDEX_NONE) ? FullName.Left(SlashIdx)          : FullName;
		const FString FunctionPart = (SlashIdx != INDEX_NONE) ? FullName.Mid(SlashIdx + 1)        : FString();

		// Find or create the group row.
		TSharedPtr<FCalleeRow>* ExistingGroup = Groups.Find(ClassName);
		TSharedPtr<FCalleeRow>  Group;
		if (ExistingGroup)
		{
			Group = *ExistingGroup;
		}
		else
		{
			Group = MakeShared<FCalleeRow>();
			Group->bIsGroup     = true;
			Group->DisplayName  = ClassName;
			Group->FullName     = ClassName;
			Groups.Add(ClassName, Group);
		}

		Group->Count         += Callee->Count;
		Group->InclusiveTime += Callee->InclusiveTime;
		Group->ExclusiveTime += Callee->ExclusiveTime;

		// Add a leaf row for this specific callee.
		TSharedPtr<FCalleeRow> Leaf = MakeShared<FCalleeRow>();
		Leaf->bIsGroup      = false;
		Leaf->FullName      = FullName;
		Leaf->DisplayName   = FunctionPart.IsEmpty() ? FullName : FunctionPart;
		Leaf->Count         = Callee->Count;
		Leaf->InclusiveTime = Callee->InclusiveTime;
		Leaf->ExclusiveTime = Callee->ExclusiveTime;
		Group->Children.Add(Leaf);
	}

	// Collect groups into a flat array for the widget.
	TArray<TSharedPtr<FCalleeRow>> GroupArray;
	GroupArray.Reserve(Groups.Num());
	for (auto& Pair : Groups)
	{
		GroupArray.Add(Pair.Value);
	}

	View->SetData(TimerName, Root.Count, Root.InclusiveTime, Root.ExclusiveTime, MoveTemp(GroupArray));
}

#undef LOCTEXT_NAMESPACE
