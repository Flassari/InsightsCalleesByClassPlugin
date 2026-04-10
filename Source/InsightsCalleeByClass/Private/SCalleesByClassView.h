#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

class SBorder;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Column name constants

namespace CalleeByClassColumns
{
	static const FName Class = "Class";
	static const FName Count = "Count";
	static const FName Incl  = "Incl";
	static const FName Pct   = "Pct";
	static const FName Excl  = "Excl";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Data model

/**
 * Represents one row in the tree view.
 * - Root row  (bIsRoot=true):  the selected timer; children = class groups
 * - Group rows (bIsGroup=true): one per class, children = individual callees
 * - Leaf rows (bIsGroup=false): one per unique callee function within a class
 */
struct FCalleeRow
{
	bool    bIsRoot  = false;
	bool    bIsGroup = false;
	FString DisplayName;  // timer name for root, class name for groups, function-part for leaves
	FString FullName;     // full timer name (e.g. "UMyClass/FunctionName")
	uint64  Count    = 0;
	double  InclusiveTime = 0.0;  // seconds
	double  ExclusiveTime = 0.0;  // seconds

	TArray<TSharedPtr<FCalleeRow>> Children;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class SCalleesByClassView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCalleesByClassView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Replace the displayed data. Call from the game thread. */
	void SetData(const FString& InTimerName, uint64 InRootCount, double InRootInclusiveTime, double InRootExclusiveTime, TArray<TSharedPtr<FCalleeRow>> InGroups);

	/** Show or hide the hint bar at the bottom. */
	void SetShowHint(bool bShow);

private:
	TSharedRef<SWidget> BuildTree();
	TSharedRef<SHeaderRow> BuildHeaderRow();

	// STreeView callbacks
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FCalleeRow> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FCalleeRow> Item, TArray<TSharedPtr<FCalleeRow>>& OutChildren);

	// Sort
	void OnSortModeChanged(EColumnSortPriority::Type Priority, const FName& ColumnId, EColumnSortMode::Type NewMode);
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
	void SortGroups(TArray<TSharedPtr<FCalleeRow>>& Groups);
	void SortRootItems();

	TSharedPtr<STreeView<TSharedPtr<FCalleeRow>>> TreeView;
	TSharedPtr<SBorder>                           HintBar;

	TArray<TSharedPtr<FCalleeRow>> RootItems;
	double RootInclusiveTime = 0.0;

	FName             SortColumn = CalleeByClassColumns::Incl;
	EColumnSortMode::Type SortMode = EColumnSortMode::Descending;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** One row widget inside the tree view. */
class SCalleesByClassRow : public SMultiColumnTableRow<TSharedPtr<FCalleeRow>>
{
public:
	SLATE_BEGIN_ARGS(SCalleesByClassRow) {}
		SLATE_ARGUMENT(TSharedPtr<FCalleeRow>, Item)
		SLATE_ARGUMENT(double, RootInclusiveTime)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable);
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FCalleeRow> Item;
	double RootInclusiveTime = 0.0;
};
