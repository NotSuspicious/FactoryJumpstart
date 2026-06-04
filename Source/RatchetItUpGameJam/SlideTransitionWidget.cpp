// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlideTransitionWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "TimerManager.h"

USlideTransitionWidget::USlideTransitionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> USlideTransitionWidget::RebuildWidget()
{
	// Build a single full-screen image tinted with SlideColor as the root.
	// A default (textureless) image brush paints a solid block across whatever
	// geometry it is given, so it fills the screen when added to the viewport.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		if (UImage* Panel = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SlidePanel")))
		{
			Panel->SetColorAndOpacity(SlideColor);
			PanelImage = Panel;
			WidgetTree->RootWidget = Panel;
		}
	}

	return Super::RebuildWidget();
}

void USlideTransitionWidget::ApplySettings(const FSlideTransitionSettings& Settings)
{
	SlideColor = Settings.Color;
	SlideDirection = Settings.Direction;
	EasingFunction = Settings.Easing;
	EasingBlendExp = Settings.EasingBlendExp;
	EasingCurve = Settings.EasingCurve;

	if (PanelImage)
	{
		PanelImage->SetColorAndOpacity(SlideColor);
	}
}

void USlideTransitionWidget::SnapToCovered()
{
	// Used when re-adding the persistent widget after a level load: it must
	// already fully cover the screen before the first frame is presented.
	Phase = ESlidePhase::SlidingOut; // so ComputeTranslationX uses the "covering at alpha 0" branch
	SetRenderTranslation(FVector2D::ZeroVector);
}

void USlideTransitionWidget::BeginSlide(ESlidePhase NewPhase, float Duration)
{
	Phase = NewPhase;
	SlideDuration = FMath::Max(Duration, KINDA_SMALL_NUMBER);

	// Keep the panel color in sync in case settings changed between uses.
	if (PanelImage)
	{
		PanelImage->SetColorAndOpacity(SlideColor);
	}

	if (UWorld* World = GetWorld())
	{
		StartTimeSeconds = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(SlideTimerHandle, this, &USlideTransitionWidget::UpdateSlide, 1.0f / 120.0f, true);
	}
}

void USlideTransitionWidget::PlaySlideIn(float Duration)
{
	// Start fully off-screen on the entering side so there is no flash of cover.
	Phase = ESlidePhase::SlidingIn;
	SetRenderTranslation(FVector2D(ComputeTranslationX(0.0f, CachedWidth), 0.0f));

	BeginSlide(ESlidePhase::SlidingIn, Duration);
}

void USlideTransitionWidget::PlaySlideOut(float Duration)
{
	// Start fully covering the screen, then wipe away.
	Phase = ESlidePhase::SlidingOut;
	SetRenderTranslation(FVector2D::ZeroVector);

	BeginSlide(ESlidePhase::SlidingOut, Duration);
}

float USlideTransitionWidget::ApplyEasing(float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	// A curve asset, when provided, fully defines the easing.
	if (EasingCurve)
	{
		return EasingCurve->GetFloatValue(Alpha);
	}

	return UKismetMathLibrary::Ease(0.0f, 1.0f, Alpha, EasingFunction, EasingBlendExp);
}

float USlideTransitionWidget::ComputeTranslationX(float Alpha, float Width) const
{
	const float Eased = ApplyEasing(Alpha);

	// Sign controls travel direction; the panel always moves the same way for
	// both the in and out wipes so the motion reads as a single sweep.
	const float Sign = (SlideDirection == ESlideDirection::LeftToRight) ? 1.0f : -1.0f;

	if (Phase == ESlidePhase::SlidingIn)
	{
		// From off-screen (-Sign * Width) to fully covering (0).
		return FMath::Lerp(-Sign * Width, 0.0f, Eased);
	}

	// SlidingOut: from fully covering (0) to off-screen (+Sign * Width).
	return FMath::Lerp(0.0f, Sign * Width, Eased);
}

void USlideTransitionWidget::UpdateSlide()
{
	UWorld* World = GetWorld();
	if (!World || Phase == ESlidePhase::Idle)
	{
		return;
	}

	// Use the actual painted width so the translation matches screen pixels at any resolution/DPI.
	const float Width = GetCachedGeometry().GetLocalSize().X;
	if (Width > 0.0f)
	{
		CachedWidth = Width;
	}

	const float Elapsed = World->GetTimeSeconds() - StartTimeSeconds;
	const float Alpha = FMath::Clamp(Elapsed / SlideDuration, 0.0f, 1.0f);

	SetRenderTranslation(FVector2D(ComputeTranslationX(Alpha, CachedWidth), 0.0f));

	if (Alpha >= 1.0f)
	{
		const ESlidePhase Finished = Phase;
		Phase = ESlidePhase::Idle;
		World->GetTimerManager().ClearTimer(SlideTimerHandle);

		if (Finished == ESlidePhase::SlidingIn)
		{
			OnSlideInComplete.ExecuteIfBound();
		}
		else
		{
			OnSlideOutComplete.ExecuteIfBound();
		}
	}
}
