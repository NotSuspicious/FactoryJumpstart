// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelTransitionSubsystem.h"
#include "SlideTransitionWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

void ULevelTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Bound once; gated by bAwaitingLoad so only our own loads trigger a reveal.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULevelTransitionSubsystem::HandlePostLoadMap);
}

void ULevelTransitionSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	Super::Deinitialize();
}

void ULevelTransitionSubsystem::TransitionToLevel(TSoftObjectPtr<UWorld> Level, FSlideTransitionSettings Settings, TSubclassOf<USlideTransitionWidget> WidgetClass)
{
	if (Level.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("TransitionToLevel: target level is unset."));
		return;
	}

	// Ignore re-entrant requests while a transition is already running.
	if (bBusy)
	{
		return;
	}

	PendingLevel = Level;
	PendingSettings = Settings;
	PendingWidgetClass = WidgetClass;

	USlideTransitionWidget* TransitionWidget = EnsureWidget(WidgetClass);
	if (!TransitionWidget)
	{
		// No widget available; fall back to a plain load so the game still progresses.
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(GameInstance, Level);
		}
		return;
	}

	bBusy = true;

	TransitionWidget->ApplySettings(PendingSettings);
	TransitionWidget->AddToViewport(TransitionZOrder);

	TransitionWidget->OnSlideInComplete.BindUObject(this, &ULevelTransitionSubsystem::HandleSlideInComplete);
	TransitionWidget->PlaySlideIn(PendingSettings.ExitDuration);
}

void ULevelTransitionSubsystem::PlayOpenTransition(FSlideTransitionSettings Settings, TSubclassOf<USlideTransitionWidget> WidgetClass)
{
	if (bBusy)
	{
		return;
	}

	USlideTransitionWidget* TransitionWidget = EnsureWidget(WidgetClass);
	if (!TransitionWidget)
	{
		return;
	}

	bBusy = true;
	PendingSettings = Settings;

	TransitionWidget->ApplySettings(Settings);
	TransitionWidget->SnapToCovered();
	TransitionWidget->AddToViewport(TransitionZOrder);

	TransitionWidget->OnSlideOutComplete.BindUObject(this, &ULevelTransitionSubsystem::HandleSlideOutComplete);
	TransitionWidget->PlaySlideOut(Settings.EntryDuration);
}

void ULevelTransitionSubsystem::HandleSlideInComplete()
{
	// The panel now fully covers the screen; safe to start the (blocking) load.
	// The covered frame is held on screen for the duration of the load.
	bAwaitingLoad = true;

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(GameInstance, PendingLevel);
	}
}

void ULevelTransitionSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// Only react to the load we requested (ignores the initial map and others).
	if (!bAwaitingLoad)
	{
		return;
	}
	bAwaitingLoad = false;

	// LoadMap removed all viewport widgets, but our widget UObject persists.
	// Re-add it already covering, before the first frame of the new map renders.
	USlideTransitionWidget* TransitionWidget = EnsureWidget(PendingWidgetClass);
	if (!TransitionWidget)
	{
		bBusy = false;
		return;
	}

	TransitionWidget->ApplySettings(PendingSettings);
	TransitionWidget->SnapToCovered();
	TransitionWidget->AddToViewport(TransitionZOrder);

	TransitionWidget->OnSlideOutComplete.BindUObject(this, &ULevelTransitionSubsystem::HandleSlideOutComplete);
	TransitionWidget->PlaySlideOut(PendingSettings.EntryDuration);
}

void ULevelTransitionSubsystem::HandleSlideOutComplete()
{
	if (Widget)
	{
		Widget->OnSlideInComplete.Unbind();
		Widget->OnSlideOutComplete.Unbind();
		Widget->RemoveFromParent();
	}

	bBusy = false;
}

USlideTransitionWidget* ULevelTransitionSubsystem::EnsureWidget(TSubclassOf<USlideTransitionWidget> WidgetClass)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	if (!WidgetClass)
	{
		WidgetClass = USlideTransitionWidget::StaticClass();
	}

	// Recreate if missing or if the requested class changed.
	if (!Widget || Widget->GetClass() != WidgetClass)
	{
		// Owned by the GameInstance (not the world), so it survives OpenLevel.
		Widget = CreateWidget<USlideTransitionWidget>(GameInstance, WidgetClass);
	}

	return Widget;
}
