// Copyright Epic Games, Inc. All Rights Reserved.

#include "DialogueWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Layout/Margin.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "TimerManager.h"

FDialogueStyle::FDialogueStyle()
{
	// Default to a readable font; other defaults are set as in-class initializers.
	Font = FCoreStyle::GetDefaultFontStyle("Regular", 24);
}

UDialogueWidget::UDialogueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UDialogueWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// Transparent root that pins the dialogue box to the bottom of the screen.
		UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueRoot"));
		Root->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		Root->SetHorizontalAlignment(HAlign_Fill);
		Root->SetVerticalAlignment(VAlign_Bottom);
		Root->SetPadding(FMargin(120.0f, 0.0f, 120.0f, 90.0f));

		// Swappable image panel behind the text.
		UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueBox"));
		Box->SetPadding(FMargin(32.0f));
		BackgroundBorder = Box;

		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DialogueText"));
		Text->SetAutoWrapText(true);

		Box->SetContent(Text);
		Root->SetContent(Box);
		WidgetTree->RootWidget = Root;
		DialogueText = Text;

		ApplyStyleToWidgets();
	}

	return Super::RebuildWidget();
}

void UDialogueWidget::ApplyStyle(const FDialogueStyle& InStyle)
{
	Style = InStyle;
	ApplyStyleToWidgets();
}

void UDialogueWidget::ApplyStyleToWidgets()
{
	if (BackgroundBorder)
	{
		// Draw the texture as a 9-slice Box so corners are never stretched.
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = Style.BackgroundDrawMargin;
		Brush.TintColor = FSlateColor(Style.BackgroundTint);
		Brush.SetResourceObject(Style.BackgroundImage);
		if (Style.BackgroundImage)
		{
			Brush.ImageSize = FVector2D(Style.BackgroundImage->GetSizeX(), Style.BackgroundImage->GetSizeY());
		}

		BackgroundBorder->SetBrush(Brush);
	}

	if (DialogueText)
	{
		DialogueText->SetFont(Style.Font);
		DialogueText->SetColorAndOpacity(Style.TextColor);
	}
}

void UDialogueWidget::TypeLine(const FText& Line, float CharactersPerSecond)
{
	FullText = Line.ToString();
	RevealedChars = 0;
	CharsPerSecond = FMath::Max(CharactersPerSecond, 1.0f);

	if (DialogueText)
	{
		DialogueText->SetText(FText::GetEmpty());
	}

	// Nothing to animate for an empty line.
	if (FullText.IsEmpty())
	{
		bTyping = false;
		return;
	}

	bTyping = true;

	if (UWorld* World = GetWorld())
	{
		StartTimeSeconds = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(RevealTimerHandle, this, &UDialogueWidget::RevealTick, 1.0f / 60.0f, true);
	}
}

void UDialogueWidget::CompleteLine()
{
	RevealedChars = FullText.Len();
	bTyping = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevealTimerHandle);
	}

	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(FullText));
	}
}

void UDialogueWidget::RevealTick()
{
	UWorld* World = GetWorld();
	if (!World || !bTyping)
	{
		return;
	}

	const int32 Length = FullText.Len();
	const float Elapsed = World->GetTimeSeconds() - StartTimeSeconds;
	const int32 Target = FMath::Clamp(FMath::FloorToInt(Elapsed * CharsPerSecond), 0, Length);

	if (Target != RevealedChars)
	{
		RevealedChars = Target;
		if (DialogueText)
		{
			DialogueText->SetText(FText::FromString(FullText.Left(RevealedChars)));
		}
	}

	if (RevealedChars >= Length)
	{
		bTyping = false;
		World->GetTimerManager().ClearTimer(RevealTimerHandle);
	}
}
