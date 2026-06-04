// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "DialogueWidget.generated.h"

class UBorder;
class UTextBlock;
class UTexture2D;

/** Editor-editable visual style for the dialogue box. */
USTRUCT(BlueprintType)
struct FDialogueStyle
{
	GENERATED_BODY()

	FDialogueStyle();

	/** Background panel image. Leave unset to use a solid BackgroundTint fill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<UTexture2D> BackgroundImage = nullptr;

	/** Tint multiplied over the panel image (and the fill color when no image is set). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FLinearColor BackgroundTint = FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);

	/**
	 * 9-slice border margins (0..1 fraction of the image per side). The image is
	 * drawn as a Box so the corners stay crisp and only the middle stretches —
	 * no corner stretching. Tune to match your texture's border art.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FMargin BackgroundDrawMargin = FMargin(0.5f);

	/** Font used for the dialogue text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FSlateFontInfo Font;

	/** Color of the dialogue text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FSlateColor TextColor = FSlateColor(FLinearColor::White);
};

/**
 * Simple typewriter dialogue box: reveals a single line of text character by
 * character. The owning UDialogueSubsystem feeds it one line at a time and asks
 * whether it is still typing.
 *
 * Built entirely in C++ (no Blueprint asset required); subclass with a Blueprint
 * if you want to restyle it, or set the Style in the GameMode's DialogueSettings.
 */
UCLASS()
class RATCHETITUPGAMEJAM_API UDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UDialogueWidget(const FObjectInitializer& ObjectInitializer);

	/** Visual style: panel image/tint, font, and text color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueStyle Style;

	/** Applies a style at runtime (also updates the live widgets if already built). */
	void ApplyStyle(const FDialogueStyle& InStyle);

	/** Begins revealing Line at the given speed. */
	void TypeLine(const FText& Line, float CharactersPerSecond);

	/** Instantly reveals the whole current line. */
	void CompleteLine();

	/** True while characters are still being revealed. */
	bool IsTyping() const { return bTyping; }

protected:

	/** Builds the dialogue box (background + wrapped text) as the widget root. */
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:

	/** Reveals characters based on elapsed time; driven by a looping timer. */
	void RevealTick();

	/** Pushes the current Style onto the background border and text block. */
	void ApplyStyleToWidgets();

	/** Border whose brush is the swappable background image. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> BackgroundBorder = nullptr;

	/** Text block that displays the (partially revealed) line. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogueText = nullptr;

	FString FullText;
	int32 RevealedChars = 0;
	float CharsPerSecond = 40.0f;
	float StartTimeSeconds = 0.0f;
	bool bTyping = false;

	FTimerHandle RevealTimerHandle;
};
