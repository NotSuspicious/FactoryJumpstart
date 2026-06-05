// Copyright Epic Games, Inc. All Rights Reserved.

#include "SharedMaterialController.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"

ASharedMaterialController::ASharedMaterialController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASharedMaterialController::BeginPlay()
{
	Super::BeginPlay();
	ApplySharedMaterial();
}

void ASharedMaterialController::ApplySharedMaterial()
{
	if (!BaseMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("SharedMaterialController: BaseMaterial is not set."));
		return;
	}

	// Create ONE dynamic instance, owned by this actor, shared by all targets.
	SharedMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (!SharedMaterial)
	{
		return;
	}

	TArray<UStaticMeshComponent*> Targets;
	CollectTargets(Targets);

	for (UStaticMeshComponent* Mesh : Targets)
	{
		if (Mesh)
		{
			Mesh->SetMaterial(MaterialSlot, SharedMaterial);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SharedMaterialController: shared MID applied to %d mesh component(s)."), Targets.Num());
}

void ASharedMaterialController::CollectTargets(TArray<UStaticMeshComponent*>& OutTargets) const
{
	OutTargets.Reset();

	// Static meshes on every actor carrying the tag.
	if (!TargetActorTag.IsNone())
	{
		TArray<AActor*> TaggedActors;
		UGameplayStatics::GetAllActorsWithTag(this, TargetActorTag, TaggedActors);

		for (AActor* Actor : TaggedActors)
		{
			if (!Actor)
			{
				continue;
			}

			TArray<UStaticMeshComponent*> Meshes;
			Actor->GetComponents<UStaticMeshComponent>(Meshes);
			OutTargets.Append(Meshes);
		}
	}

	// Explicitly-picked extras.
	for (const TObjectPtr<UStaticMeshComponent>& Mesh : ExtraTargets)
	{
		if (Mesh)
		{
			OutTargets.AddUnique(Mesh);
		}
	}
}

void ASharedMaterialController::SetScalarParameter(FName ParameterName, float Value)
{
	if (SharedMaterial)
	{
		SharedMaterial->SetScalarParameterValue(ParameterName, Value);
	}
}

void ASharedMaterialController::SetVectorParameter(FName ParameterName, FLinearColor Value)
{
	if (SharedMaterial)
	{
		SharedMaterial->SetVectorParameterValue(ParameterName, Value);
	}
}

void ASharedMaterialController::SetTextureParameter(FName ParameterName, UTexture* Value)
{
	if (SharedMaterial)
	{
		SharedMaterial->SetTextureParameterValue(ParameterName, Value);
	}
}
