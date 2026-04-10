#include "SCalleesByClassView.h"

#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"

// TraceInsightsCore
#include "InsightsCore/Common/InsightsCoreStyle.h"
#include "InsightsCore/Common/TimeUtils.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "InsightsCalleeByClass"

////////////////////////////////////////////////////////////////////////////////////////////////////
// SCalleesByClassRow
////////////////////////////////////////////////////////////////////////////////////////////////////

void SCalleesByClassRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
{
	Item = InArgs._Item;
	RootInclusiveTime = InArgs._RootInclusiveTime;
	SMultiColumnTableRow<TSharedPtr<FCalleeRow>>::Construct(FSuperRowType::FArguments(), InOwnerTable);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SCalleesByClassRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	auto MakeCell = [](TSharedRef<SWidget> InWidget, EHorizontalAlignment HAlign = HAlign_Left) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.Padding(FMargin(4.f, 2.f))
			.HAlign(HAlign)
			[
				InWidget
			];
	};

	auto MakeText = [](const FString& InStr) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(FText::FromString(InStr))
			.TextStyle(UE::Insights::FInsightsCoreStyle::Get(), TEXT("TreeTable.Tooltip"));
	};

	if (ColumnName == CalleeByClassColumns::Class)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SExpanderArrow, SharedThis(this))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4.f, 2.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->DisplayName))
				.TextStyle(UE::Insights::FInsightsCoreStyle::Get(), TEXT("TreeTable.Tooltip"))
				.ToolTipText(FText::FromString(Item->FullName))
			];
	}

	if (ColumnName == CalleeByClassColumns::Count)
	{
		return MakeCell(MakeText(FString::Printf(TEXT("%llu"), Item->Count)), HAlign_Right);
	}

	if (ColumnName == CalleeByClassColumns::Incl)
	{
		return MakeCell(MakeText(UE::Insights::FormatTimeAuto(Item->InclusiveTime)), HAlign_Right);
	}

	if (ColumnName == CalleeByClassColumns::Pct)
	{
		FString Str = TEXT("—");
		if (!Item->bIsRoot && RootInclusiveTime > 0.0)
		{
			const double Pct = (Item->InclusiveTime / RootInclusiveTime) * 100.0;
			Str = FString::Printf(TEXT("%.1f%%"), Pct);
		}
		return MakeCell(MakeText(Str), HAlign_Right);
	}

	if (ColumnName == CalleeByClassColumns::Excl)
	{
		return MakeCell(MakeText(UE::Insights::FormatTimeAuto(Item->ExclusiveTime)), HAlign_Right);
	}

	return SNullWidget::NullWidget;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////
// SCalleesByClassView
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SCalleesByClassView::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// Tree view
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			BuildTree()
		]

		// Hint bar at the bottom (same visual as the Callees tab)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f)
		[
			SAssignNew(HintBar, SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.05f, 0.1f, 0.2f, 1.0f))
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Margin(FMargin(4.0f, 1.0f, 4.0f, 1.0f))
				.Text(LOCTEXT("SelectionWarning", "Please select a time range and a timer!"))
				.ColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.5f, 1.0f))
			]
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SCalleesByClassView::BuildTree()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.Padding(0.f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FCalleeRow>>)
			.TreeItemsSource(&RootItems)
			.OnGenerateRow(this, &SCalleesByClassView::OnGenerateRow)
			.OnGetChildren(this, &SCalleesByClassView::OnGetChildren)
			.SelectionMode(ESelectionMode::Single)
			.HeaderRow(BuildHeaderRow())
		];
}

TSharedRef<SHeaderRow> SCalleesByClassView::BuildHeaderRow()
{
	auto MakeColumn = [this](const FName& ColumnId, const FText& Label, float FillWidth, EHorizontalAlignment HAlign = HAlign_Left) -> SHeaderRow::FColumn::FArguments
	{
		return SHeaderRow::Column(ColumnId)
			.DefaultLabel(Label)
			.FillWidth(FillWidth)
			.HAlignHeader(HAlign)
			.HAlignCell(HAlign)
			.SortMode(this, &SCalleesByClassView::GetColumnSortMode, ColumnId)
			.OnSort(this, &SCalleesByClassView::OnSortModeChanged);
	};

	return SNew(SHeaderRow)
		+ MakeColumn(CalleeByClassColumns::Class, LOCTEXT("ColClass", "Callees by Class"), 0.36f)
		+ MakeColumn(CalleeByClassColumns::Count, LOCTEXT("ColCount", "Count"),     0.14f, HAlign_Right)
		+ MakeColumn(CalleeByClassColumns::Incl,  LOCTEXT("ColIncl",  "Incl"), 0.18f, HAlign_Right)
		+ MakeColumn(CalleeByClassColumns::Pct,   LOCTEXT("ColPct",   "% Parent"), 0.14f, HAlign_Right)
		+ MakeColumn(CalleeByClassColumns::Excl,  LOCTEXT("ColExcl",  "Excl"),     0.18f, HAlign_Right);
}

void SCalleesByClassView::SetData(const FString& InTimerName, uint64 InRootCount, double InRootInclusiveTime, double InRootExclusiveTime, TArray<TSharedPtr<FCalleeRow>> InGroups)
{
	RootInclusiveTime = InRootInclusiveTime;

	// Sort groups by current sort settings and each group's leaves by inclusive time.
	SortGroups(InGroups);
	for (TSharedPtr<FCalleeRow>& Group : InGroups)
	{
		Group->Children.Sort([](const TSharedPtr<FCalleeRow>& A, const TSharedPtr<FCalleeRow>& B)
		{
			return A->InclusiveTime > B->InclusiveTime;
		});
	}

	// Build the root row wrapping all groups.
	TSharedPtr<FCalleeRow> RootRow = MakeShared<FCalleeRow>();
	RootRow->bIsRoot     = true;
	RootRow->DisplayName = InTimerName;
	RootRow->FullName    = InTimerName;
	RootRow->Count       = InRootCount;
	RootRow->InclusiveTime = InRootInclusiveTime;
	RootRow->ExclusiveTime = InRootExclusiveTime;
	RootRow->Children    = MoveTemp(InGroups);

	RootItems = { RootRow };

	SetShowHint(RootRow->Children.IsEmpty());
	TreeView->RequestTreeRefresh();
	TreeView->SetItemExpansion(RootRow, true);
}

void SCalleesByClassView::SetShowHint(bool bShow)
{
	HintBar->SetVisibility(bShow ? EVisibility::Visible : EVisibility::Collapsed);
}

TSharedRef<ITableRow> SCalleesByClassView::OnGenerateRow(TSharedPtr<FCalleeRow> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SCalleesByClassRow, OwnerTable)
		.Item(Item)
		.RootInclusiveTime(RootInclusiveTime);
}

void SCalleesByClassView::OnGetChildren(TSharedPtr<FCalleeRow> Item, TArray<TSharedPtr<FCalleeRow>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

void SCalleesByClassView::OnSortModeChanged(EColumnSortPriority::Type /*Priority*/, const FName& ColumnId, EColumnSortMode::Type NewMode)
{
	SortColumn = ColumnId;
	SortMode = NewMode;
	SortRootItems();
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

EColumnSortMode::Type SCalleesByClassView::GetColumnSortMode(const FName ColumnId) const
{
	return (ColumnId == SortColumn) ? SortMode : EColumnSortMode::None;
}

void SCalleesByClassView::SortGroups(TArray<TSharedPtr<FCalleeRow>>& Groups)
{
	const bool bDescending = (SortMode == EColumnSortMode::Descending);
	const FName Col = SortColumn;

	Groups.Sort([Col, bDescending](const TSharedPtr<FCalleeRow>& A, const TSharedPtr<FCalleeRow>& B)
	{
		if (Col == CalleeByClassColumns::Class)
		{
			const int32 Cmp = A->DisplayName.Compare(B->DisplayName);
			return bDescending ? (Cmp > 0) : (Cmp < 0);
		}

		double VA = 0.0, VB = 0.0;
		if (Col == CalleeByClassColumns::Count)
		{
			VA = (double)A->Count;
			VB = (double)B->Count;
		}
		else if (Col == CalleeByClassColumns::Excl)
		{
			VA = A->ExclusiveTime;
			VB = B->ExclusiveTime;
		}
		else
		{
			VA = A->InclusiveTime;
			VB = B->InclusiveTime;
		}
		return bDescending ? (VA > VB) : (VA < VB);
	});
}

void SCalleesByClassView::SortRootItems()
{
	if (RootItems.Num() == 1 && RootItems[0]->bIsRoot)
	{
		SortGroups(RootItems[0]->Children);
	}
}

#undef LOCTEXT_NAMESPACE
