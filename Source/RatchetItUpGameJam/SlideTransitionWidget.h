// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/KismetMathLibrary.h" // EEasingFunc
#include "SlideTransitionWidget.generated.h"

class UCurveFloat;
class UImage;

/** Direction a solid-color panel travels across the screen during a wipe. */
UENUM(BlueprintType)
enum class ESlideDirection : uint8
{
	/** Panel enters from the left and exits to the right. */
	LeftToRight,
	/** Panel enters from the right and exits to the left. */
	RightToLeft
};

/** Look & feel of a slide transition; passed from the GameMode to the subsystem. */
USTRUCT(BlueprintType)
struct FSlideTransitionSettings
{
	GENERATED_BODY()

	/** Solid color the sliding panel is filled with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition")
	FLinearColor Color = FLinearColor::Black;

	/** Direction the panel sweeps (consistent for both the in and out wipe). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition")
	ESlideDirection Direction = ESlideDirection::LeftToRight;

	/** Easing applied to the slide. Ignored when EasingCurve is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition")
	TEnumAsByte<EEasingFunc::Type> Easing = EEasingFunc::EaseInOut;

	/** Blend exponent for easing modes that use it (EaseIn/Out/InOut). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition", meta = (ClampMin = "0.0"))
	float EasingBlendExp = 2.0f;

	/** Optional curve (domain & range 0..1) that overrides Easing when set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition")
	TObjectPtr<UCurveFloat> EasingCurve = nullptr;

	/** Seconds for the slide-on (cover) wipe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition", meta = (ClampMin = "0.0"))
	float ExitDuration = 0.5f;

	/** Seconds for the slide-off (reveal) wipe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition", meta = (ClampMin = "0.0"))
	float EntryDuration = 0.5f;
};

/**
 * Full-screen solid-color panel that slides horizontally across the screen.
 *
 *  - PlaySlideIn()  drives the panel from off-screen until it fully covers the view.
 *  - PlaySlideOut() drives the panel from full cover until it is off-screen.
 *
 * Built entirely in C++ (no Blueprint asset required) and designed to be reused
 * (owned by the GameInstance) across level loads rather than recreated.
 */
UCLASS()
class RATCHETITUPGAMEJAM_API USlideTransitionWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	USlideTransitionWidget(const FObjectInitializer& ObjectInitializer);

	/** Solid color the sliding panel is filled with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition")
	FLinearColor SlideColor = FLinearColor::Black;

	/** Direction the panel travels (consistent for both the in and out wipe). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition")
	ESlideDirection SlideDirection = ESlideDirection::LeftToRight;

	/** Easing applied to the slide. Ignored when EasingCurve is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition")
	TEnumAsByte<EEasingFunc::Type> EasingFunction = EEasingFunc::EaseInOut;

	/** Blend exponent for easing modes that use it (EaseIn/Out/InOut). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition", meta = (ClampMin = "0.0"))
	float EasingBlendExp = 2.0f;

	/**
	 * Optional curve asset (expected domain & range 0..1) that fully defines the
	 * easing. When set, it overrides EasingFunction/EasingBlendExp.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide Transition")
	TObjectPtr<UCurveFloat> EasingCurve = nullptr;

	/** Copies the look & feel from a settings struct onto this widget. */
	void ApplySettings(const FSlideTransitionSettings& Settings);

	/** Snaps the panel to fully cover the screen with no animation. */
	void SnapToCovered();

	/** Slides the panel in until it fully covers the screen, over Duration seconds. */
	void PlaySlideIn(float Duration);

	/** Slides the panel off-screen to reveal the level over Duration seconds. */
	void PlaySlideOut(float Duration);

	/** Fired once PlaySlideIn() has finished (panel fully covers the screen). */
	FSimpleDelegate OnSlideInComplete;

	/** Fired once PlaySlideOut() has finished (panel fully off-screen). */
	FSimpleDelegate OnSlideOutComplete;

protected:

	/** Builds the full-screen colored panel as the widget's root. */
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:

	enum class ESlidePhase : uint8 { Idle, SlidingIn, SlidingOut };

	/** Advances the slide; driven by a looping timer so it does not depend on widget tick settings. */
	void UpdateSlide();

	/** Starts the slide timer and applies the current color to the panel. */
	void BeginSlide(ESlidePhase NewPhase, float Duration);

	/** Returns the panel's X translation (in slate units) for a given progress and screen width. */
	float ComputeTranslationX(float Alpha, float Width) const;

	/** Shapes raw 0..1 progress through the configured easing function or curve. */
	float ApplyEasing(float Alpha) const;

	/** Root image used as the solid color panel. */
	UPROPERTY(Transient)
	TObjectPtr<UImage> PanelImage = nullptr;

	ESlidePhase Phase = ESlidePhase::Idle;
	float StartTimeSeconds = 0.0f;
	float SlideDuration = 0.0f;

	/** Last known screen width; falls back to a safely-off-screen value before geometry is valid. */
	float CachedWidth = 5000.0f;

	FTimerHandle SlideTimerHandle;
};
