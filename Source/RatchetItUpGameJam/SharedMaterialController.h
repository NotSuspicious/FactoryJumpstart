// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SharedMaterialController.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture;

/**
 * Drives one shared Dynamic Material Instance across many static meshes.
 *
 * Place one in the level, set BaseMaterial, tag your duplicated meshes with
 * TargetActorTag, and on BeginPlay every tagged mesh is switched to the SAME
 * MID. Changing a parameter (SetScalar/Vector/Texture) then updates all of them
 * at once.
 */
UCLASS()
class RATCHETITUPGAMEJAM_API ASharedMaterialController : public AActor
{
	GENERATED_BODY()

public:

	ASharedMaterialController();

	/** Base material (or material instance) the shared MID is created from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared Material")
	TObjectPtr<UMaterialInterface> BaseMaterial;

	/** Material slot index overridden on each target mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared Material")
	int32 MaterialSlot = 0;

	/** Every actor with this tag has its static meshes switched to the shared MID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shared Material")
	FName TargetActorTag = TEXT("SharedMaterial");

	/** Optional extra mesh components to include (pick instances in the level). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Shared Material")
	TArray<TObjectPtr<UStaticMeshComponent>> ExtraTargets;

	/** The single dynamic material instance shared by every target. */
	UPROPERTY(BlueprintReadOnly, Category = "Shared Material")
	TObjectPtr<UMaterialInstanceDynamic> SharedMaterial;

	/** Creates the shared MID and assigns it to all targets. Call again if meshes spawn later. */
	UFUNCTION(BlueprintCallable, Category = "Shared Material")
	void ApplySharedMaterial();

	/** Set a scalar parameter on the shared MID (affects every assigned mesh). */
	UFUNCTION(BlueprintCallable, Category = "Shared Material")
	void SetScalarParameter(FName ParameterName, float Value);

	/** Set a vector/color parameter on the shared MID (affects every assigned mesh). */
	UFUNCTION(BlueprintCallable, Category = "Shared Material")
	void SetVectorParameter(FName ParameterName, FLinearColor Value);

	/** Set a texture parameter on the shared MID (affects every assigned mesh). */
	UFUNCTION(BlueprintCallable, Category = "Shared Material")
	void SetTextureParameter(FName ParameterName, UTexture* Value);

protected:

	virtual void BeginPlay() override;

private:

	/** Gathers static mesh components from tagged actors + ExtraTargets. */
	void CollectTargets(TArray<UStaticMeshComponent*>& OutTargets) const;
};
